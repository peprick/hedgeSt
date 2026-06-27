#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

struct GbmParameters {
    double initialStockPrice = 100.0;
    double drift = 0.05;
    double volatility = 0.20;
    double maturityYears = 1.0;
    int steps = 252;
    unsigned long long seed = 42;
};

struct EuropeanCallOption {
    double strike = 100.0;
    double maturityYears = 1.0;
};

struct MonteCarloSettings {
    int paths = 100000;
    unsigned long long seed = 20260626;
};

struct HedgeSimulationSettings {
    int paths = 10000;
    unsigned long long seed = 20260627;
};

struct QLearningSettings {
    int trainingEpisodes = 12000;
    int evaluationPaths = 5000;
    double learningRate = 0.08;
    double rewardDiscount = 0.99;
    double epsilonStart = 0.35;
    double epsilonEnd = 0.03;
    unsigned long long trainingSeed = 20260628;
    unsigned long long evaluationSeed = 20260629;
};

struct MonteCarloResult {
    double discountedPrice = 0.0;
    double averagePayoff = 0.0;
    double discountedStdError = 0.0;
    int paths = 0;
};

struct BlackScholesResult {
    double callPrice = 0.0;
    double callDelta = 0.0;
    double d1 = 0.0;
    double d2 = 0.0;
};

struct DeltaHedgeResult {
    double optionPremium = 0.0;
    double initialDelta = 0.0;
    double finalStockPrice = 0.0;
    double optionPayoff = 0.0;
    double totalTransactionCosts = 0.0;
    double hedgingPnl = 0.0;
    int rebalances = 0;
};

struct HedgeSimulationSummary {
    std::string label;
    int paths = 0;
    int rebalanceEverySteps = 1;
    double deltaRebalanceBand = 0.0;
    double averageRebalances = 0.0;
    double averagePnl = 0.0;
    double pnlStdDev = 0.0;
    double worstPnl = 0.0;
    double bestPnl = 0.0;
    double pnlPercentile5 = 0.0;
    double valueAtRisk95 = 0.0;
    double conditionalValueAtRisk95 = 0.0;
    double averageTransactionCosts = 0.0;
    double lossProbability = 0.0;
};

struct HedgingState {
    double logMoneyness = 0.0;
    double timeToMaturity = 0.0;
    double currentHedge = 0.0;
    double blackScholesDelta = 0.0;
    double deltaGap = 0.0;
};

struct HedgingAction {
    double targetHedge = 0.0;
};

struct HedgingStepResult {
    HedgingState nextState;
    double reward = 0.0;
    bool done = false;
};

struct HedgingEnvironmentDemo {
    double totalReward = 0.0;
    double finalPnl = 0.0;
    double totalTransactionCosts = 0.0;
    int steps = 0;
};

struct EpisodeDecision {
    int stateIndex = 0;
    int actionIndex = 0;
    double reward = 0.0;
};

struct QLearningResult {
    HedgeSimulationSummary evaluationSummary;
    double averageTrainingReward = 0.0;
    int trainingEpisodes = 0;
    std::vector<double> qTable;
};

void validateGbmParameters(const GbmParameters& params) {
    if (params.initialStockPrice <= 0.0) {
        throw std::invalid_argument("initialStockPrice must be positive");
    }
    if (params.volatility < 0.0) {
        throw std::invalid_argument("volatility cannot be negative");
    }
    if (params.maturityYears <= 0.0) {
        throw std::invalid_argument("maturityYears must be positive");
    }
    if (params.steps <= 0) {
        throw std::invalid_argument("steps must be positive");
    }
}

void validateEuropeanCall(const EuropeanCallOption& option) {
    if (option.strike <= 0.0) {
        throw std::invalid_argument("strike must be positive");
    }
    if (option.maturityYears <= 0.0) {
        throw std::invalid_argument("maturityYears must be positive");
    }
}

std::vector<double> simulateGbmPath(const GbmParameters& params, std::mt19937_64& rng) {
    validateGbmParameters(params);

    std::normal_distribution<double> standardNormal(0.0, 1.0);

    const double dt = params.maturityYears / static_cast<double>(params.steps);
    const double diffusionScale = params.volatility * std::sqrt(dt);
    const double driftTerm = (params.drift - 0.5 * params.volatility * params.volatility) * dt;

    std::vector<double> path;
    path.reserve(static_cast<std::size_t>(params.steps) + 1);

    double stockPrice = params.initialStockPrice;
    path.push_back(stockPrice);

    for (int step = 1; step <= params.steps; ++step) {
        const double z = standardNormal(rng);
        stockPrice *= std::exp(driftTerm + diffusionScale * z);
        path.push_back(stockPrice);
    }

    return path;
}

std::vector<double> simulateGbmPath(const GbmParameters& params) {
    std::mt19937_64 rng(params.seed);
    return simulateGbmPath(params, rng);
}

double europeanCallPayoff(double stockAtMaturity, double strike) {
    if (strike < 0.0) {
        throw std::invalid_argument("strike cannot be negative");
    }
    return std::max(stockAtMaturity - strike, 0.0);
}

double simulateGbmTerminalPrice(
    double initialStockPrice,
    double drift,
    double volatility,
    double maturityYears,
    std::mt19937_64& rng
) {
    std::normal_distribution<double> standardNormal(0.0, 1.0);
    const double z = standardNormal(rng);
    const double exponent =
        (drift - 0.5 * volatility * volatility) * maturityYears
        + volatility * std::sqrt(maturityYears) * z;

    return initialStockPrice * std::exp(exponent);
}

MonteCarloResult priceEuropeanCallMonteCarlo(
    const GbmParameters& market,
    const EuropeanCallOption& option,
    double riskFreeRate,
    const MonteCarloSettings& settings
) {
    validateGbmParameters(market);
    validateEuropeanCall(option);
    if (settings.paths <= 1) {
        throw std::invalid_argument("Monte Carlo paths must be greater than 1");
    }

    std::mt19937_64 rng(settings.seed);

    double payoffSum = 0.0;
    double payoffSquareSum = 0.0;

    for (int pathIndex = 0; pathIndex < settings.paths; ++pathIndex) {
        const double terminalStockPrice = simulateGbmTerminalPrice(
            market.initialStockPrice,
            riskFreeRate,
            market.volatility,
            option.maturityYears,
            rng
        );
        const double payoff = europeanCallPayoff(terminalStockPrice, option.strike);
        payoffSum += payoff;
        payoffSquareSum += payoff * payoff;
    }

    const double pathCount = static_cast<double>(settings.paths);
    const double averagePayoff = payoffSum / pathCount;
    const double payoffVariance =
        (payoffSquareSum - pathCount * averagePayoff * averagePayoff)
        / (pathCount - 1.0);
    const double payoffStdError = std::sqrt(std::max(payoffVariance, 0.0) / pathCount);
    const double discountFactor = std::exp(-riskFreeRate * option.maturityYears);

    return MonteCarloResult{
        discountFactor * averagePayoff,
        averagePayoff,
        discountFactor * payoffStdError,
        settings.paths
    };
}

double normalCdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

BlackScholesResult priceEuropeanCallBlackScholes(
    double stockPrice,
    const EuropeanCallOption& option,
    double riskFreeRate,
    double volatility
) {
    if (stockPrice <= 0.0) {
        throw std::invalid_argument("stockPrice must be positive");
    }
    validateEuropeanCall(option);
    if (volatility <= 0.0) {
        throw std::invalid_argument("volatility must be positive for Black-Scholes");
    }

    const double sqrtT = std::sqrt(option.maturityYears);
    const double d1 =
        (std::log(stockPrice / option.strike)
        + (riskFreeRate + 0.5 * volatility * volatility) * option.maturityYears)
        / (volatility * sqrtT);
    const double d2 = d1 - volatility * sqrtT;
    const double discountFactor = std::exp(-riskFreeRate * option.maturityYears);

    const double callPrice =
        stockPrice * normalCdf(d1)
        - option.strike * discountFactor * normalCdf(d2);
    const double callDelta = normalCdf(d1);

    return BlackScholesResult{callPrice, callDelta, d1, d2};
}

HedgingState buildHedgingState(
    double stockPrice,
    const EuropeanCallOption& option,
    double currentHedge,
    double riskFreeRate,
    double volatility
) {
    const BlackScholesResult optionSnapshot = priceEuropeanCallBlackScholes(
        stockPrice,
        option,
        riskFreeRate,
        volatility
    );

    return HedgingState{
        std::log(stockPrice / option.strike),
        option.maturityYears,
        currentHedge,
        optionSnapshot.callDelta,
        std::abs(optionSnapshot.callDelta - currentHedge)
    };
}

HedgingStepResult applyHedgingAction(
    const HedgingState& state,
    const HedgingAction& action,
    double stockPrice,
    double transactionCostRate
) {
    const double tradeSize = action.targetHedge - state.currentHedge;
    const double transactionCost = transactionCostRate * stockPrice * std::abs(tradeSize);

    HedgingState nextState = state;
    nextState.currentHedge = action.targetHedge;

    return HedgingStepResult{
        nextState,
        -transactionCost,
        false
    };
}

int bucketize(double value, const std::vector<double>& cutoffs) {
    int bucket = 0;
    while (bucket < static_cast<int>(cutoffs.size()) && value > cutoffs[static_cast<std::size_t>(bucket)]) {
        ++bucket;
    }
    return bucket;
}

int hedgingStateIndex(const HedgingState& state) {
    const int moneynessBucket = bucketize(state.logMoneyness, {-0.10, -0.03, 0.03, 0.10});
    const int timeBucket = bucketize(state.timeToMaturity, {0.10, 0.30, 0.60});
    const int hedgeBucket = bucketize(state.currentHedge, {0.20, 0.40, 0.60, 0.80});
    const int deltaBucket = bucketize(state.blackScholesDelta, {0.20, 0.40, 0.60, 0.80});
    const int deltaGapBucket = bucketize(state.deltaGap, {0.02, 0.05, 0.10, 0.20});

    constexpr int timeBuckets = 4;
    constexpr int hedgeBuckets = 5;
    constexpr int deltaBuckets = 5;
    constexpr int deltaGapBuckets = 5;

    return ((((moneynessBucket * timeBuckets) + timeBucket) * hedgeBuckets + hedgeBucket)
        * deltaBuckets + deltaBucket) * deltaGapBuckets + deltaGapBucket;
}

int hedgingStateCount() {
    constexpr int moneynessBuckets = 5;
    constexpr int timeBuckets = 4;
    constexpr int hedgeBuckets = 5;
    constexpr int deltaBuckets = 5;
    constexpr int deltaGapBuckets = 5;
    return moneynessBuckets * timeBuckets * hedgeBuckets * deltaBuckets * deltaGapBuckets;
}

int qLearningActionCount() {
    return 2;
}

double qLearningTargetHedge(const HedgingState& state, int actionIndex) {
    switch (actionIndex) {
        case 0:
            return state.currentHedge;
        case 1:
            return state.blackScholesDelta;
        default:
            throw std::invalid_argument("invalid Q-learning action index");
    }
}

int bestActionIndex(const std::vector<double>& qTable, int stateIndex) {
    int bestIndex = 0;
    const int actionCount = qLearningActionCount();
    double bestValue = qTable[static_cast<std::size_t>(stateIndex) * actionCount];

    for (int actionIndex = 1; actionIndex < actionCount; ++actionIndex) {
        const double candidate = qTable[
            static_cast<std::size_t>(stateIndex) * actionCount
            + static_cast<std::size_t>(actionIndex)
        ];
        if (candidate > bestValue) {
            bestValue = candidate;
            bestIndex = actionIndex;
        }
    }

    return bestIndex;
}

int epsilonGreedyActionIndex(
    const std::vector<double>& qTable,
    int stateIndex,
    double epsilon,
    std::mt19937_64& rng
) {
    std::uniform_real_distribution<double> probability(0.0, 1.0);
    if (probability(rng) < epsilon) {
        std::uniform_int_distribution<int> actionPicker(0, qLearningActionCount() - 1);
        return actionPicker(rng);
    }

    return bestActionIndex(qTable, stateIndex);
}

DeltaHedgeResult runOnePathDeltaHedge(
    const std::vector<double>& stockPath,
    const EuropeanCallOption& option,
    double riskFreeRate,
    double volatility,
    double transactionCostRate,
    int rebalanceEverySteps,
    double deltaRebalanceBand
) {
    if (stockPath.size() < 2) {
        throw std::invalid_argument("stockPath must contain at least two prices");
    }
    if (transactionCostRate < 0.0) {
        throw std::invalid_argument("transactionCostRate cannot be negative");
    }
    if (rebalanceEverySteps <= 0) {
        throw std::invalid_argument("rebalanceEverySteps must be positive");
    }
    if (deltaRebalanceBand < 0.0) {
        throw std::invalid_argument("deltaRebalanceBand cannot be negative");
    }

    validateEuropeanCall(option);

    const int totalSteps = static_cast<int>(stockPath.size()) - 1;
    const double dt = option.maturityYears / static_cast<double>(totalSteps);

    const BlackScholesResult initialOption = priceEuropeanCallBlackScholes(
        stockPath.front(),
        option,
        riskFreeRate,
        volatility
    );

    double cash = initialOption.callPrice;
    double stockPosition = initialOption.callDelta;
    double totalTransactionCosts = transactionCostRate * stockPath.front() * std::abs(stockPosition);
    cash -= stockPosition * stockPath.front();
    cash -= totalTransactionCosts;

    int rebalances = 0;

    for (int step = 1; step < totalSteps; ++step) {
        const double stockPrice = stockPath[static_cast<std::size_t>(step)];
        const double remainingMaturity = option.maturityYears - static_cast<double>(step) * dt;
        cash *= std::exp(riskFreeRate * dt);

        if (step % rebalanceEverySteps != 0) {
            continue;
        }

        const EuropeanCallOption remainingOption{option.strike, remainingMaturity};
        const BlackScholesResult nextOption = priceEuropeanCallBlackScholes(
            stockPrice,
            remainingOption,
            riskFreeRate,
            volatility
        );

        const double tradeSize = nextOption.callDelta - stockPosition;
        if (std::abs(tradeSize) < deltaRebalanceBand) {
            continue;
        }

        const double tradeCost = transactionCostRate * stockPrice * std::abs(tradeSize);
        cash -= tradeSize * stockPrice;
        cash -= tradeCost;
        totalTransactionCosts += tradeCost;
        stockPosition = nextOption.callDelta;
        ++rebalances;
    }

    cash *= std::exp(riskFreeRate * dt);

    const double finalStockPrice = stockPath.back();
    const double liquidationCost = transactionCostRate * finalStockPrice * std::abs(stockPosition);
    cash += stockPosition * finalStockPrice;
    cash -= liquidationCost;
    totalTransactionCosts += liquidationCost;

    const double optionPayoff = europeanCallPayoff(finalStockPrice, option.strike);
    cash -= optionPayoff;

    return DeltaHedgeResult{
        initialOption.callPrice,
        initialOption.callDelta,
        finalStockPrice,
        optionPayoff,
        totalTransactionCosts,
        cash,
        rebalances
    };
}

HedgingEnvironmentDemo runBlackScholesPolicyEnvironmentDemo(
    const std::vector<double>& stockPath,
    const EuropeanCallOption& option,
    double riskFreeRate,
    double volatility,
    double transactionCostRate
) {
    if (stockPath.size() < 2) {
        throw std::invalid_argument("stockPath must contain at least two prices");
    }

    const int totalSteps = static_cast<int>(stockPath.size()) - 1;
    const double dt = option.maturityYears / static_cast<double>(totalSteps);

    const BlackScholesResult initialOption = priceEuropeanCallBlackScholes(
        stockPath.front(),
        option,
        riskFreeRate,
        volatility
    );

    double cash = initialOption.callPrice;
    double currentHedge = 0.0;
    double totalReward = 0.0;
    double totalTransactionCosts = 0.0;

    for (int step = 0; step < totalSteps; ++step) {
        const double stockPrice = stockPath[static_cast<std::size_t>(step)];
        const double remainingMaturity = option.maturityYears - static_cast<double>(step) * dt;
        const EuropeanCallOption remainingOption{option.strike, remainingMaturity};

        if (step > 0) {
            cash *= std::exp(riskFreeRate * dt);
        }

        const HedgingState state = buildHedgingState(
            stockPrice,
            remainingOption,
            currentHedge,
            riskFreeRate,
            volatility
        );
        const HedgingAction action{state.blackScholesDelta};
        const HedgingStepResult stepResult = applyHedgingAction(
            state,
            action,
            stockPrice,
            transactionCostRate
        );

        const double tradeSize = stepResult.nextState.currentHedge - currentHedge;
        const double transactionCost = -stepResult.reward;
        cash -= tradeSize * stockPrice;
        cash -= transactionCost;
        totalReward += stepResult.reward;
        totalTransactionCosts += transactionCost;
        currentHedge = stepResult.nextState.currentHedge;
    }

    const double finalStockPrice = stockPath.back();
    const double liquidationCost = transactionCostRate * finalStockPrice * std::abs(currentHedge);
    cash += currentHedge * finalStockPrice;
    cash -= liquidationCost;
    totalReward -= liquidationCost;
    totalTransactionCosts += liquidationCost;

    const double optionPayoff = europeanCallPayoff(finalStockPrice, option.strike);
    cash -= optionPayoff;
    totalReward -= std::abs(cash);

    return HedgingEnvironmentDemo{
        totalReward,
        cash,
        totalTransactionCosts,
        totalSteps
    };
}

DeltaHedgeResult runQPolicyOnePath(
    const std::vector<double>& stockPath,
    const EuropeanCallOption& option,
    double riskFreeRate,
    double volatility,
    double transactionCostRate,
    const std::vector<double>& qTable,
    double epsilon,
    std::mt19937_64& rng,
    std::vector<EpisodeDecision>* decisions
) {
    if (stockPath.size() < 2) {
        throw std::invalid_argument("stockPath must contain at least two prices");
    }

    const int totalSteps = static_cast<int>(stockPath.size()) - 1;
    const double dt = option.maturityYears / static_cast<double>(totalSteps);
    const BlackScholesResult initialOption = priceEuropeanCallBlackScholes(
        stockPath.front(),
        option,
        riskFreeRate,
        volatility
    );

    double cash = initialOption.callPrice;
    double currentHedge = 0.0;
    double totalTransactionCosts = 0.0;
    int rebalances = 0;

    for (int step = 0; step < totalSteps; ++step) {
        const double stockPrice = stockPath[static_cast<std::size_t>(step)];
        const double remainingMaturity = option.maturityYears - static_cast<double>(step) * dt;
        const EuropeanCallOption remainingOption{option.strike, remainingMaturity};

        if (step > 0) {
            cash *= std::exp(riskFreeRate * dt);
        }

        const HedgingState state = buildHedgingState(
            stockPrice,
            remainingOption,
            currentHedge,
            riskFreeRate,
            volatility
        );
        const int stateIndex = hedgingStateIndex(state);
        const int actionIndex = epsilonGreedyActionIndex(qTable, stateIndex, epsilon, rng);
        const double targetHedge = qLearningTargetHedge(state, actionIndex);
        const double tradeSize = targetHedge - currentHedge;
        const double transactionCost = transactionCostRate * stockPrice * std::abs(tradeSize);

        cash -= tradeSize * stockPrice;
        cash -= transactionCost;
        totalTransactionCosts += transactionCost;
        if (std::abs(tradeSize) > 1e-12) {
            ++rebalances;
        }

        if (decisions != nullptr) {
            decisions->push_back(EpisodeDecision{stateIndex, actionIndex, -transactionCost});
        }

        currentHedge = targetHedge;
    }

    cash *= std::exp(riskFreeRate * dt);

    const double finalStockPrice = stockPath.back();
    const double liquidationCost = transactionCostRate * finalStockPrice * std::abs(currentHedge);
    cash += currentHedge * finalStockPrice;
    cash -= liquidationCost;
    totalTransactionCosts += liquidationCost;

    const double optionPayoff = europeanCallPayoff(finalStockPrice, option.strike);
    cash -= optionPayoff;

    return DeltaHedgeResult{
        initialOption.callPrice,
        initialOption.callDelta,
        finalStockPrice,
        optionPayoff,
        totalTransactionCosts,
        cash,
        rebalances
    };
}

HedgeSimulationSummary summarizeHedgeResults(
    const std::string& label,
    int rebalanceEverySteps,
    double deltaRebalanceBand,
    const std::vector<DeltaHedgeResult>& results
) {
    if (results.size() < 2) {
        throw std::invalid_argument("at least two hedge results are required");
    }

    double pnlSum = 0.0;
    double pnlSquareSum = 0.0;
    double transactionCostSum = 0.0;
    double rebalanceSum = 0.0;
    int losingPaths = 0;
    double worstPnl = std::numeric_limits<double>::infinity();
    double bestPnl = -std::numeric_limits<double>::infinity();
    std::vector<double> pnls;
    pnls.reserve(results.size());

    for (const DeltaHedgeResult& result : results) {
        pnlSum += result.hedgingPnl;
        pnlSquareSum += result.hedgingPnl * result.hedgingPnl;
        transactionCostSum += result.totalTransactionCosts;
        rebalanceSum += static_cast<double>(result.rebalances);
        worstPnl = std::min(worstPnl, result.hedgingPnl);
        bestPnl = std::max(bestPnl, result.hedgingPnl);
        pnls.push_back(result.hedgingPnl);
        if (result.hedgingPnl < 0.0) {
            ++losingPaths;
        }
    }

    const double pathCount = static_cast<double>(results.size());
    const double averagePnl = pnlSum / pathCount;
    const double pnlVariance =
        (pnlSquareSum - pathCount * averagePnl * averagePnl)
        / (pathCount - 1.0);

    std::sort(pnls.begin(), pnls.end());
    const int tailCount = std::max(1, static_cast<int>(std::ceil(0.05 * pathCount)));
    const double pnlPercentile5 = pnls[static_cast<std::size_t>(tailCount - 1)];

    double tailPnlSum = 0.0;
    for (int index = 0; index < tailCount; ++index) {
        tailPnlSum += pnls[static_cast<std::size_t>(index)];
    }

    const double averageTailPnl = tailPnlSum / static_cast<double>(tailCount);

    return HedgeSimulationSummary{
        label,
        static_cast<int>(results.size()),
        rebalanceEverySteps,
        deltaRebalanceBand,
        rebalanceSum / pathCount,
        averagePnl,
        std::sqrt(std::max(pnlVariance, 0.0)),
        worstPnl,
        bestPnl,
        pnlPercentile5,
        std::max(0.0, -pnlPercentile5),
        std::max(0.0, -averageTailPnl),
        transactionCostSum / pathCount,
        static_cast<double>(losingPaths) / pathCount
    };
}

QLearningResult trainAndEvaluateQHedger(
    const GbmParameters& market,
    const EuropeanCallOption& option,
    double riskFreeRate,
    double volatility,
    double transactionCostRate,
    const QLearningSettings& settings
) {
    const int actionCount = qLearningActionCount();
    std::vector<double> qTable(
        static_cast<std::size_t>(hedgingStateCount()) * static_cast<std::size_t>(actionCount),
        0.0
    );

    GbmParameters pricingWorld = market;
    pricingWorld.drift = riskFreeRate;

    std::mt19937_64 trainingRng(settings.trainingSeed);
    double trainingRewardSum = 0.0;

    for (int episode = 0; episode < settings.trainingEpisodes; ++episode) {
        const double progress = static_cast<double>(episode)
            / static_cast<double>(std::max(1, settings.trainingEpisodes - 1));
        const double epsilon =
            settings.epsilonStart
            + (settings.epsilonEnd - settings.epsilonStart) * progress;

        const std::vector<double> path = simulateGbmPath(pricingWorld, trainingRng);
        std::vector<EpisodeDecision> decisions;
        decisions.reserve(static_cast<std::size_t>(pricingWorld.steps));

        const DeltaHedgeResult hedge = runQPolicyOnePath(
            path,
            option,
            riskFreeRate,
            volatility,
            transactionCostRate,
            qTable,
            epsilon,
            trainingRng,
            &decisions
        );

        double returnEstimate = -std::abs(hedge.hedgingPnl);
        trainingRewardSum += returnEstimate;

        for (auto decision = decisions.rbegin(); decision != decisions.rend(); ++decision) {
            returnEstimate = decision->reward + settings.rewardDiscount * returnEstimate;
            const std::size_t qIndex =
                static_cast<std::size_t>(decision->stateIndex) * static_cast<std::size_t>(actionCount)
                + static_cast<std::size_t>(decision->actionIndex);
            qTable[qIndex] += settings.learningRate * (returnEstimate - qTable[qIndex]);
        }
    }

    std::mt19937_64 evaluationRng(settings.evaluationSeed);
    std::vector<DeltaHedgeResult> evaluationResults;
    evaluationResults.reserve(static_cast<std::size_t>(settings.evaluationPaths));

    for (int pathIndex = 0; pathIndex < settings.evaluationPaths; ++pathIndex) {
        const std::vector<double> path = simulateGbmPath(pricingWorld, evaluationRng);
        evaluationResults.push_back(runQPolicyOnePath(
            path,
            option,
            riskFreeRate,
            volatility,
            transactionCostRate,
            qTable,
            0.0,
            evaluationRng,
            nullptr
        ));
    }

    return QLearningResult{
        summarizeHedgeResults(
            "Q-learning hedger",
            1,
            0.0,
            evaluationResults
        ),
        trainingRewardSum / static_cast<double>(settings.trainingEpisodes),
        settings.trainingEpisodes,
        qTable
    };
}

HedgeSimulationSummary runDeltaHedgeMonteCarlo(
    const GbmParameters& market,
    const EuropeanCallOption& option,
    double riskFreeRate,
    double volatility,
    double transactionCostRate,
    int rebalanceEverySteps,
    double deltaRebalanceBand,
    const std::string& label,
    const HedgeSimulationSettings& settings
) {
    validateGbmParameters(market);
    validateEuropeanCall(option);
    if (settings.paths <= 1) {
        throw std::invalid_argument("hedge simulation paths must be greater than 1");
    }
    if (rebalanceEverySteps <= 0) {
        throw std::invalid_argument("rebalanceEverySteps must be positive");
    }
    if (deltaRebalanceBand < 0.0) {
        throw std::invalid_argument("deltaRebalanceBand cannot be negative");
    }

    GbmParameters pricingWorld = market;
    pricingWorld.drift = riskFreeRate;

    std::mt19937_64 rng(settings.seed);

    double pnlSum = 0.0;
    double pnlSquareSum = 0.0;
    double transactionCostSum = 0.0;
    double rebalanceSum = 0.0;
    int losingPaths = 0;
    double worstPnl = std::numeric_limits<double>::infinity();
    double bestPnl = -std::numeric_limits<double>::infinity();
    std::vector<double> pnls;
    pnls.reserve(static_cast<std::size_t>(settings.paths));

    for (int pathIndex = 0; pathIndex < settings.paths; ++pathIndex) {
        const std::vector<double> path = simulateGbmPath(pricingWorld, rng);
        const DeltaHedgeResult hedge = runOnePathDeltaHedge(
            path,
            option,
            riskFreeRate,
            volatility,
            transactionCostRate,
            rebalanceEverySteps,
            deltaRebalanceBand
        );

        pnlSum += hedge.hedgingPnl;
        pnlSquareSum += hedge.hedgingPnl * hedge.hedgingPnl;
        pnls.push_back(hedge.hedgingPnl);
        transactionCostSum += hedge.totalTransactionCosts;
        rebalanceSum += static_cast<double>(hedge.rebalances);
        worstPnl = std::min(worstPnl, hedge.hedgingPnl);
        bestPnl = std::max(bestPnl, hedge.hedgingPnl);
        if (hedge.hedgingPnl < 0.0) {
            ++losingPaths;
        }
    }

    const double pathCount = static_cast<double>(settings.paths);
    const double averagePnl = pnlSum / pathCount;
    const double pnlVariance =
        (pnlSquareSum - pathCount * averagePnl * averagePnl)
        / (pathCount - 1.0);

    std::sort(pnls.begin(), pnls.end());
    const int tailCount = std::max(1, static_cast<int>(std::ceil(0.05 * pathCount)));
    const double pnlPercentile5 = pnls[static_cast<std::size_t>(tailCount - 1)];

    double tailPnlSum = 0.0;
    for (int index = 0; index < tailCount; ++index) {
        tailPnlSum += pnls[static_cast<std::size_t>(index)];
    }

    const double averageTailPnl = tailPnlSum / static_cast<double>(tailCount);
    const double valueAtRisk95 = std::max(0.0, -pnlPercentile5);
    const double conditionalValueAtRisk95 = std::max(0.0, -averageTailPnl);

    return HedgeSimulationSummary{
        label,
        settings.paths,
        rebalanceEverySteps,
        deltaRebalanceBand,
        rebalanceSum / pathCount,
        averagePnl,
        std::sqrt(std::max(pnlVariance, 0.0)),
        worstPnl,
        bestPnl,
        pnlPercentile5,
        valueAtRisk95,
        conditionalValueAtRisk95,
        transactionCostSum / pathCount,
        static_cast<double>(losingPaths) / pathCount
    };
}

void printSeparator(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

bool hasArgument(int argc, char* argv[], const std::string& expected) {
    for (int index = 1; index < argc; ++index) {
        if (expected == argv[index]) {
            return true;
        }
    }
    return false;
}

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        switch (character) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += character;
                break;
        }
    }
    return escaped;
}

void printJsonHedgeSummary(const HedgeSimulationSummary& summary) {
    std::cout
        << "{"
        << "\"label\":\"" << jsonEscape(summary.label) << "\","
        << "\"paths\":" << summary.paths << ","
        << "\"rebalanceEverySteps\":" << summary.rebalanceEverySteps << ","
        << "\"deltaRebalanceBand\":" << summary.deltaRebalanceBand << ","
        << "\"averageRebalances\":" << summary.averageRebalances << ","
        << "\"averagePnl\":" << summary.averagePnl << ","
        << "\"pnlStdDev\":" << summary.pnlStdDev << ","
        << "\"worstPnl\":" << summary.worstPnl << ","
        << "\"bestPnl\":" << summary.bestPnl << ","
        << "\"pnlPercentile5\":" << summary.pnlPercentile5 << ","
        << "\"valueAtRisk95\":" << summary.valueAtRisk95 << ","
        << "\"conditionalValueAtRisk95\":" << summary.conditionalValueAtRisk95 << ","
        << "\"averageTransactionCosts\":" << summary.averageTransactionCosts << ","
        << "\"lossProbability\":" << summary.lossProbability
        << "}";
}

void printJsonReport(
    const GbmParameters& params,
    const EuropeanCallOption& option,
    double riskFreeRate,
    double transactionCostRate,
    const MonteCarloResult& monteCarloPrice,
    const BlackScholesResult& blackScholes,
    const DeltaHedgeResult& deltaHedge,
    const HedgingEnvironmentDemo& environmentDemo,
    const QLearningResult& qLearningResult,
    const std::vector<HedgeSimulationSummary>& strategySummaries
) {
    const double monteCarloPriceError = monteCarloPrice.discountedPrice - blackScholes.callPrice;

    std::cout << std::fixed << std::setprecision(6);
    std::cout
        << "{"
        << "\"project\":\"StochHedge\","
        << "\"market\":{"
        << "\"initialStockPrice\":" << params.initialStockPrice << ","
        << "\"drift\":" << params.drift << ","
        << "\"riskFreeRate\":" << riskFreeRate << ","
        << "\"volatility\":" << params.volatility << ","
        << "\"transactionCostRate\":" << transactionCostRate << ","
        << "\"maturityYears\":" << option.maturityYears << ","
        << "\"steps\":" << params.steps << ","
        << "\"strike\":" << option.strike
        << "},";

    std::cout
        << "\"pricing\":{"
        << "\"monteCarlo\":{"
        << "\"paths\":" << monteCarloPrice.paths << ","
        << "\"averagePayoff\":" << monteCarloPrice.averagePayoff << ","
        << "\"discountedOptionPrice\":" << monteCarloPrice.discountedPrice << ","
        << "\"discountedStandardError\":" << monteCarloPrice.discountedStdError
        << "},"
        << "\"blackScholes\":{"
        << "\"d1\":" << blackScholes.d1 << ","
        << "\"d2\":" << blackScholes.d2 << ","
        << "\"callPrice\":" << blackScholes.callPrice << ","
        << "\"callDelta\":" << blackScholes.callDelta
        << "},"
        << "\"monteCarloMinusFormula\":" << monteCarloPriceError << ","
        << "\"withinTwoStandardErrors\":"
        << (std::abs(monteCarloPriceError) <= 2.0 * monteCarloPrice.discountedStdError ? "true" : "false")
        << "},";

    std::cout
        << "\"onePathDeltaHedge\":{"
        << "\"optionPremium\":" << deltaHedge.optionPremium << ","
        << "\"initialDelta\":" << deltaHedge.initialDelta << ","
        << "\"finalStockPrice\":" << deltaHedge.finalStockPrice << ","
        << "\"optionPayoff\":" << deltaHedge.optionPayoff << ","
        << "\"totalTransactionCosts\":" << deltaHedge.totalTransactionCosts << ","
        << "\"hedgingPnl\":" << deltaHedge.hedgingPnl << ","
        << "\"rebalances\":" << deltaHedge.rebalances
        << "},";

    std::cout
        << "\"rlEnvironmentDemo\":{"
        << "\"policy\":\"Black-Scholes delta\","
        << "\"state\":\"log(S/K), time left, current hedge, BS delta, delta gap\","
        << "\"action\":\"choose target hedge\","
        << "\"stepReward\":\"negative transaction cost\","
        << "\"finalRewardPenalty\":\"negative absolute final P&L\","
        << "\"steps\":" << environmentDemo.steps << ","
        << "\"totalTransactionCosts\":" << environmentDemo.totalTransactionCosts << ","
        << "\"finalPnl\":" << environmentDemo.finalPnl << ","
        << "\"totalReward\":" << environmentDemo.totalReward
        << "},";

    std::cout
        << "\"learnedHedger\":{"
        << "\"algorithm\":\"tabular Q-learning style update\","
        << "\"trainingEpisodes\":" << qLearningResult.trainingEpisodes << ","
        << "\"averageTrainingFinalReward\":" << qLearningResult.averageTrainingReward << ","
        << "\"actionMenu\":\"hold, move to Black-Scholes delta\","
        << "\"evaluation\":";
    printJsonHedgeSummary(qLearningResult.evaluationSummary);
    std::cout << "},";

    std::cout << "\"strategies\":[";
    for (std::size_t index = 0; index < strategySummaries.size(); ++index) {
        if (index > 0) {
            std::cout << ",";
        }
        printJsonHedgeSummary(strategySummaries[index]);
    }
    std::cout << "]}";
}

void printHedgeSummary(const HedgeSimulationSummary& summary) {
    std::cout << "\n" << summary.label << '\n';
    std::cout << "paths                         : " << summary.paths << '\n';
    std::cout << "rebalance every steps          : " << summary.rebalanceEverySteps << '\n';
    std::cout << "delta no-trade band            : " << summary.deltaRebalanceBand << '\n';
    std::cout << "average rebalances             : " << summary.averageRebalances << '\n';
    std::cout << "average hedging P&L            : " << summary.averagePnl << '\n';
    std::cout << "P&L standard deviation         : " << summary.pnlStdDev << '\n';
    std::cout << "worst P&L                      : " << summary.worstPnl << '\n';
    std::cout << "best P&L                       : " << summary.bestPnl << '\n';
    std::cout << "5% P&L percentile              : " << summary.pnlPercentile5 << '\n';
    std::cout << "95% VaR loss amount            : " << summary.valueAtRisk95 << '\n';
    std::cout << "95% CVaR loss amount           : " << summary.conditionalValueAtRisk95 << '\n';
    std::cout << "average transaction costs      : " << summary.averageTransactionCosts << '\n';
    std::cout << "loss probability               : " << summary.lossProbability << '\n';
}

std::string qLearningActionLabel(int actionIndex) {
    switch (actionIndex) {
        case 0:
            return "hold";
        case 1:
            return "move";
        default:
            return "unknown";
    }
}

void printStrategyLeaderboard(std::vector<HedgeSimulationSummary> summaries) {
    std::sort(
        summaries.begin(),
        summaries.end(),
        [](const HedgeSimulationSummary& left, const HedgeSimulationSummary& right) {
            if (left.conditionalValueAtRisk95 == right.conditionalValueAtRisk95) {
                return left.averagePnl > right.averagePnl;
            }
            return left.conditionalValueAtRisk95 < right.conditionalValueAtRisk95;
        }
    );

    std::cout << std::left
              << std::setw(42) << "strategy"
              << std::right
              << std::setw(8) << "paths"
              << std::setw(12) << "avg pnl"
              << std::setw(12) << "cvar95"
              << std::setw(12) << "cost"
              << std::setw(12) << "trades"
              << '\n';

    for (const HedgeSimulationSummary& summary : summaries) {
        std::cout << std::left
                  << std::setw(42) << summary.label
                  << std::right
                  << std::setw(8) << summary.paths
                  << std::setw(12) << summary.averagePnl
                  << std::setw(12) << summary.conditionalValueAtRisk95
                  << std::setw(12) << summary.averageTransactionCosts
                  << std::setw(12) << summary.averageRebalances
                  << '\n';
    }
}

void printPolicyGapRow(
    const std::string& label,
    const std::vector<double>& qTable,
    double currentHedge,
    double timeToMaturity,
    bool deltaAboveCurrent
) {
    const std::vector<double> gaps{0.01, 0.04, 0.08, 0.15, 0.30};

    std::cout << label << " | current hedge " << currentHedge
              << " | T " << timeToMaturity << '\n';
    std::cout << "gap       : ";
    for (const double gap : gaps) {
        std::cout << std::setw(8) << gap;
    }
    std::cout << "\naction    : ";

    for (const double gap : gaps) {
        const double rawDelta = deltaAboveCurrent
            ? currentHedge + gap
            : currentHedge - gap;
        const HedgingState state{
            0.0,
            timeToMaturity,
            currentHedge,
            std::clamp(rawDelta, 0.0, 1.0),
            gap
        };
        const int actionIndex = bestActionIndex(qTable, hedgingStateIndex(state));
        std::cout << std::setw(8) << qLearningActionLabel(actionIndex);
    }

    std::cout << "\n\n";
}

void printLearnedPolicySnapshot(const std::vector<double>& qTable) {
    std::cout << "Action labels: hold = do not trade, move = rebalance to Black-Scholes delta\n\n";
    printPolicyGapRow("under-hedged, mid-life", qTable, 0.50, 0.50, true);
    printPolicyGapRow("over-hedged, mid-life ", qTable, 0.50, 0.50, false);
    printPolicyGapRow("under-hedged, near end", qTable, 0.50, 0.08, true);
    printPolicyGapRow("over-hedged, near end ", qTable, 0.50, 0.08, false);
}

int main(int argc, char* argv[]) {
    const GbmParameters params;
    const EuropeanCallOption option;
    const double riskFreeRate = 0.05;
    const double transactionCostRate = 0.001;
    const MonteCarloSettings monteCarloSettings;
    const HedgeSimulationSettings hedgeSimulationSettings;
    const QLearningSettings qLearningSettings;

    const std::vector<double> path = simulateGbmPath(params);
    const double finalStockPrice = path.back();
    const double payoff = europeanCallPayoff(finalStockPrice, option.strike);

    const MonteCarloResult monteCarloPrice = priceEuropeanCallMonteCarlo(
        params,
        option,
        riskFreeRate,
        monteCarloSettings
    );
    const BlackScholesResult blackScholes = priceEuropeanCallBlackScholes(
        params.initialStockPrice,
        option,
        riskFreeRate,
        params.volatility
    );
    const DeltaHedgeResult deltaHedge = runOnePathDeltaHedge(
        path,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate,
        1,
        0.0
    );
    const HedgingEnvironmentDemo environmentDemo = runBlackScholesPolicyEnvironmentDemo(
        path,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate
    );
    const HedgeSimulationSummary dailyHedgeSummary = runDeltaHedgeMonteCarlo(
        params,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate,
        1,
        0.0,
        "Daily rebalance",
        hedgeSimulationSettings
    );
    const HedgeSimulationSummary weeklyHedgeSummary = runDeltaHedgeMonteCarlo(
        params,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate,
        5,
        0.0,
        "Weekly-ish rebalance",
        hedgeSimulationSettings
    );
    const HedgeSimulationSummary monthlyHedgeSummary = runDeltaHedgeMonteCarlo(
        params,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate,
        21,
        0.0,
        "Monthly-ish rebalance",
        hedgeSimulationSettings
    );
    const HedgeSimulationSummary band005HedgeSummary = runDeltaHedgeMonteCarlo(
        params,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate,
        1,
        0.05,
        "Daily check + 0.05 delta no-trade band",
        hedgeSimulationSettings
    );
    const HedgeSimulationSummary band010HedgeSummary = runDeltaHedgeMonteCarlo(
        params,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate,
        1,
        0.10,
        "Daily check + 0.10 delta no-trade band",
        hedgeSimulationSettings
    );
    const QLearningResult qLearningResult = trainAndEvaluateQHedger(
        params,
        option,
        riskFreeRate,
        params.volatility,
        transactionCostRate,
        qLearningSettings
    );
    const std::vector<HedgeSimulationSummary> strategySummaries{
        dailyHedgeSummary,
        weeklyHedgeSummary,
        monthlyHedgeSummary,
        band005HedgeSummary,
        band010HedgeSummary,
        qLearningResult.evaluationSummary
    };

    if (hasArgument(argc, argv, "--json")) {
        printJsonReport(
            params,
            option,
            riskFreeRate,
            transactionCostRate,
            monteCarloPrice,
            blackScholes,
            deltaHedge,
            environmentDemo,
            qLearningResult,
            strategySummaries
        );
        return 0;
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "StochHedge: GBM + Monte Carlo + Black-Scholes\n";

    printSeparator("Market and option inputs");
    std::cout << "S0       : " << params.initialStockPrice << '\n';
    std::cout << "mu       : " << params.drift << "  (real-world path demo drift)\n";
    std::cout << "r        : " << riskFreeRate << "  (risk-free pricing drift)\n";
    std::cout << "sigma    : " << params.volatility << '\n';
    std::cout << "cost     : " << transactionCostRate << "  (0.001 means 0.10% per stock trade)\n";
    std::cout << "T years  : " << option.maturityYears << '\n';
    std::cout << "steps    : " << params.steps << '\n';
    std::cout << "strike K : " << option.strike << '\n';

    printSeparator("One possible stock path");
    std::cout << "Sampled path points:\n";
    std::cout << "step, stock_price\n";

    const int printEvery = 21;
    for (std::size_t i = 0; i < path.size(); i += printEvery) {
        std::cout << i << ", " << path[i] << '\n';
    }
    if ((path.size() - 1) % printEvery != 0) {
        std::cout << (path.size() - 1) << ", " << finalStockPrice << '\n';
    }

    std::cout << "\nFinal stock price S_T : " << finalStockPrice << '\n';
    std::cout << "Call payoff max(S_T - K, 0): " << payoff << '\n';

    printSeparator("Monte Carlo pricing");
    std::cout << "paths                         : " << monteCarloPrice.paths << '\n';
    std::cout << "average payoff E[payoff]      : " << monteCarloPrice.averagePayoff << '\n';
    std::cout << "discounted option price       : " << monteCarloPrice.discountedPrice << '\n';
    std::cout << "discounted standard error     : " << monteCarloPrice.discountedStdError << '\n';

    printSeparator("Black-Scholes formula check");
    std::cout << "d1                            : " << blackScholes.d1 << '\n';
    std::cout << "d2                            : " << blackScholes.d2 << '\n';
    std::cout << "formula call price            : " << blackScholes.callPrice << '\n';
    std::cout << "formula call delta            : " << blackScholes.callDelta << '\n';
    const double monteCarloPriceError = monteCarloPrice.discountedPrice - blackScholes.callPrice;
    std::cout << "Monte Carlo minus formula     : " << monteCarloPriceError << '\n';
    std::cout << "within 2 standard errors      : "
              << (std::abs(monteCarloPriceError) <= 2.0 * monteCarloPrice.discountedStdError ? "yes" : "no")
              << '\n';

    printSeparator("One-path delta hedge demo");
    std::cout << "sold option for premium        : " << deltaHedge.optionPremium << '\n';
    std::cout << "initial stock hedge delta      : " << deltaHedge.initialDelta << '\n';
    std::cout << "number of rebalances           : " << deltaHedge.rebalances << '\n';
    std::cout << "final stock price              : " << deltaHedge.finalStockPrice << '\n';
    std::cout << "option payoff paid at maturity : " << deltaHedge.optionPayoff << '\n';
    std::cout << "total transaction costs        : " << deltaHedge.totalTransactionCosts << '\n';
    std::cout << "final hedging P&L              : " << deltaHedge.hedgingPnl << '\n';

    printSeparator("RL-style environment demo");
    std::cout << "policy                         : Black-Scholes delta\n";
    std::cout << "state                          : log(S/K), time left, current hedge, BS delta, delta gap\n";
    std::cout << "action                         : choose target hedge\n";
    std::cout << "step reward                    : negative transaction cost\n";
    std::cout << "final reward penalty           : negative absolute final P&L\n";
    std::cout << "environment steps              : " << environmentDemo.steps << '\n';
    std::cout << "total transaction costs        : " << environmentDemo.totalTransactionCosts << '\n';
    std::cout << "final P&L                      : " << environmentDemo.finalPnl << '\n';
    std::cout << "total reward                   : " << environmentDemo.totalReward << '\n';

    printSeparator("Many-path delta hedge comparison");
    printHedgeSummary(dailyHedgeSummary);
    printHedgeSummary(weeklyHedgeSummary);
    printHedgeSummary(monthlyHedgeSummary);
    printHedgeSummary(band005HedgeSummary);
    printHedgeSummary(band010HedgeSummary);

    printSeparator("First learned hedger");
    std::cout << "algorithm                      : tabular Q-learning style update\n";
    std::cout << "training episodes              : " << qLearningResult.trainingEpisodes << '\n';
    std::cout << "average training final reward  : " << qLearningResult.averageTrainingReward << '\n';
    std::cout << "action menu                    : hold, move to Black-Scholes delta\n";
    printHedgeSummary(qLearningResult.evaluationSummary);

    printSeparator("Strategy leaderboard by tail risk");
    printStrategyLeaderboard(strategySummaries);

    printSeparator("Learned policy snapshot");
    printLearnedPolicySnapshot(qLearningResult.qTable);

    return 0;
}
