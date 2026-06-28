#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
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
    double lossAversion = 0.35;
    unsigned long long trainingSeed = 20260628;
    unsigned long long evaluationSeed = 20260629;
};

struct ExperimentConfig {
    GbmParameters market;
    EuropeanCallOption option;
    double riskFreeRate = 0.05;
    double transactionCostRate = 0.001;
    MonteCarloSettings monteCarlo;
    HedgeSimulationSettings hedging;
    QLearningSettings qLearning;
};

struct StrategyConfig {
    std::string label;
    int rebalanceEverySteps = 1;
    double deltaRebalanceBand = 0.0;
};

struct RuntimeOptions {
    ExperimentConfig config;
    bool json = false;
    bool help = false;
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

struct HedgingEnvironmentRun {
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
    std::vector<int> evaluationActionCounts;
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

void validateMonteCarloSettings(const MonteCarloSettings& settings) {
    if (settings.paths <= 1) {
        throw std::invalid_argument("Monte Carlo paths must be greater than 1");
    }
}

void validateHedgeSimulationSettings(const HedgeSimulationSettings& settings) {
    if (settings.paths <= 1) {
        throw std::invalid_argument("hedge simulation paths must be greater than 1");
    }
}

void validateQLearningSettings(const QLearningSettings& settings) {
    if (settings.trainingEpisodes <= 0) {
        throw std::invalid_argument("RL training episodes must be positive");
    }
    if (settings.evaluationPaths <= 1) {
        throw std::invalid_argument("RL evaluation paths must be greater than 1");
    }
    if (settings.learningRate <= 0.0 || settings.learningRate > 1.0) {
        throw std::invalid_argument("RL learning rate must be in (0, 1]");
    }
    if (settings.rewardDiscount < 0.0 || settings.rewardDiscount > 1.0) {
        throw std::invalid_argument("RL reward discount must be in [0, 1]");
    }
    if (settings.epsilonStart < 0.0 || settings.epsilonStart > 1.0) {
        throw std::invalid_argument("RL epsilon start must be in [0, 1]");
    }
    if (settings.epsilonEnd < 0.0 || settings.epsilonEnd > 1.0) {
        throw std::invalid_argument("RL epsilon end must be in [0, 1]");
    }
    if (settings.lossAversion < 0.0) {
        throw std::invalid_argument("RL loss aversion cannot be negative");
    }
}

void validateExperimentConfig(const ExperimentConfig& config) {
    validateGbmParameters(config.market);
    validateEuropeanCall(config.option);
    validateMonteCarloSettings(config.monteCarlo);
    validateHedgeSimulationSettings(config.hedging);
    validateQLearningSettings(config.qLearning);
    if (config.riskFreeRate < -1.0) {
        throw std::invalid_argument("riskFreeRate is unrealistically low");
    }
    if (config.transactionCostRate < 0.0) {
        throw std::invalid_argument("transactionCostRate cannot be negative");
    }
    if (std::abs(config.market.maturityYears - config.option.maturityYears) > 1e-12) {
        throw std::invalid_argument("market and option maturity must match");
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
    validateMonteCarloSettings(settings);

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
    return 4;
}

double qLearningTargetHedge(const HedgingState& state, int actionIndex) {
    const double deltaMove = state.blackScholesDelta - state.currentHedge;
    switch (actionIndex) {
        case 0:
            return state.currentHedge;
        case 1:
            return state.currentHedge + 0.25 * deltaMove;
        case 2:
            return state.currentHedge + 0.50 * deltaMove;
        case 3:
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

HedgingEnvironmentRun runBlackScholesPolicyEnvironment(
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

    return HedgingEnvironmentRun{
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
    std::vector<EpisodeDecision>* decisions,
    std::vector<int>* actionCounts
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
        if (actionCounts != nullptr) {
            ++(*actionCounts)[static_cast<std::size_t>(actionIndex)];
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
            &decisions,
            nullptr
        );

        const double lossPenalty = settings.lossAversion * std::max(0.0, -hedge.hedgingPnl);
        double returnEstimate = -std::abs(hedge.hedgingPnl) - lossPenalty;
        trainingRewardSum += returnEstimate;

        const double episodeLearningRate =
            settings.learningRate / (1.0 + 0.50 * progress);
        for (auto decision = decisions.rbegin(); decision != decisions.rend(); ++decision) {
            returnEstimate = decision->reward + settings.rewardDiscount * returnEstimate;
            const std::size_t qIndex =
                static_cast<std::size_t>(decision->stateIndex) * static_cast<std::size_t>(actionCount)
                + static_cast<std::size_t>(decision->actionIndex);
            qTable[qIndex] += episodeLearningRate * (returnEstimate - qTable[qIndex]);
        }
    }

    std::mt19937_64 evaluationRng(settings.evaluationSeed);
    std::vector<DeltaHedgeResult> evaluationResults;
    evaluationResults.reserve(static_cast<std::size_t>(settings.evaluationPaths));
    std::vector<int> evaluationActionCounts(static_cast<std::size_t>(actionCount), 0);

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
            nullptr,
            &evaluationActionCounts
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
        qTable,
        evaluationActionCounts
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
    validateHedgeSimulationSettings(settings);
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

bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string optionValue(int& index, int argc, char* argv[], const std::string& argument) {
    const std::size_t equalsPosition = argument.find('=');
    if (equalsPosition != std::string::npos) {
        return argument.substr(equalsPosition + 1);
    }
    if (index + 1 >= argc) {
        throw std::invalid_argument("missing value for " + argument);
    }
    ++index;
    return argv[index];
}

double parseDoubleOption(int& index, int argc, char* argv[], const std::string& argument) {
    const std::string value = optionValue(index, argc, argv, argument);
    std::size_t parsedCharacters = 0;
    const double parsed = std::stod(value, &parsedCharacters);
    if (parsedCharacters != value.size()) {
        throw std::invalid_argument("invalid numeric value for " + argument + ": " + value);
    }
    return parsed;
}

int parseIntOption(int& index, int argc, char* argv[], const std::string& argument) {
    const std::string value = optionValue(index, argc, argv, argument);
    std::size_t parsedCharacters = 0;
    const int parsed = std::stoi(value, &parsedCharacters);
    if (parsedCharacters != value.size()) {
        throw std::invalid_argument("invalid integer value for " + argument + ": " + value);
    }
    return parsed;
}

unsigned long long parseSeedOption(int& index, int argc, char* argv[], const std::string& argument) {
    const std::string value = optionValue(index, argc, argv, argument);
    std::size_t parsedCharacters = 0;
    const unsigned long long parsed = std::stoull(value, &parsedCharacters);
    if (parsedCharacters != value.size()) {
        throw std::invalid_argument("invalid seed value for " + argument + ": " + value);
    }
    return parsed;
}

void applyBaseSeed(ExperimentConfig& config, unsigned long long seed) {
    config.market.seed = seed;
    config.monteCarlo.seed = seed + 1;
    config.hedging.seed = seed + 2;
    config.qLearning.trainingSeed = seed + 3;
    config.qLearning.evaluationSeed = seed + 4;
}

RuntimeOptions parseRuntimeOptions(int argc, char* argv[]) {
    RuntimeOptions options;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];

        if (argument == "--json") {
            options.json = true;
        } else if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--spot" || startsWith(argument, "--spot=")) {
            options.config.market.initialStockPrice = parseDoubleOption(index, argc, argv, argument);
        } else if (argument == "--strike" || startsWith(argument, "--strike=")) {
            options.config.option.strike = parseDoubleOption(index, argc, argv, argument);
        } else if (argument == "--drift" || startsWith(argument, "--drift=")) {
            options.config.market.drift = parseDoubleOption(index, argc, argv, argument);
        } else if (argument == "--rate" || startsWith(argument, "--rate=")) {
            options.config.riskFreeRate = parseDoubleOption(index, argc, argv, argument);
        } else if (argument == "--volatility" || argument == "--sigma"
            || startsWith(argument, "--volatility=") || startsWith(argument, "--sigma=")) {
            options.config.market.volatility = parseDoubleOption(index, argc, argv, argument);
        } else if (argument == "--transaction-cost" || startsWith(argument, "--transaction-cost=")) {
            options.config.transactionCostRate = parseDoubleOption(index, argc, argv, argument);
        } else if (argument == "--maturity" || startsWith(argument, "--maturity=")) {
            const double maturity = parseDoubleOption(index, argc, argv, argument);
            options.config.market.maturityYears = maturity;
            options.config.option.maturityYears = maturity;
        } else if (argument == "--steps" || startsWith(argument, "--steps=")) {
            options.config.market.steps = parseIntOption(index, argc, argv, argument);
        } else if (argument == "--mc-paths" || startsWith(argument, "--mc-paths=")) {
            options.config.monteCarlo.paths = parseIntOption(index, argc, argv, argument);
        } else if (argument == "--hedge-paths" || startsWith(argument, "--hedge-paths=")) {
            options.config.hedging.paths = parseIntOption(index, argc, argv, argument);
        } else if (argument == "--rl-train" || startsWith(argument, "--rl-train=")) {
            options.config.qLearning.trainingEpisodes = parseIntOption(index, argc, argv, argument);
        } else if (argument == "--rl-eval" || startsWith(argument, "--rl-eval=")) {
            options.config.qLearning.evaluationPaths = parseIntOption(index, argc, argv, argument);
        } else if (argument == "--rl-loss-aversion" || startsWith(argument, "--rl-loss-aversion=")) {
            options.config.qLearning.lossAversion = parseDoubleOption(index, argc, argv, argument);
        } else if (argument == "--seed" || startsWith(argument, "--seed=")) {
            applyBaseSeed(options.config, parseSeedOption(index, argc, argv, argument));
        } else if (argument == "--path-seed" || startsWith(argument, "--path-seed=")) {
            options.config.market.seed = parseSeedOption(index, argc, argv, argument);
        } else if (argument == "--mc-seed" || startsWith(argument, "--mc-seed=")) {
            options.config.monteCarlo.seed = parseSeedOption(index, argc, argv, argument);
        } else if (argument == "--hedge-seed" || startsWith(argument, "--hedge-seed=")) {
            options.config.hedging.seed = parseSeedOption(index, argc, argv, argument);
        } else if (argument == "--rl-train-seed" || startsWith(argument, "--rl-train-seed=")) {
            options.config.qLearning.trainingSeed = parseSeedOption(index, argc, argv, argument);
        } else if (argument == "--rl-eval-seed" || startsWith(argument, "--rl-eval-seed=")) {
            options.config.qLearning.evaluationSeed = parseSeedOption(index, argc, argv, argument);
        } else {
            throw std::invalid_argument("unknown argument: " + argument);
        }
    }

    validateExperimentConfig(options.config);
    return options;
}

void printUsage() {
    std::cout
        << "StochEdge quant engine\n\n"
        << "Usage:\n"
        << "  stochedge_engine [--json] [options]\n\n"
        << "Core options:\n"
        << "  --spot VALUE              Initial stock price\n"
        << "  --strike VALUE            European call strike\n"
        << "  --drift VALUE             Real-world GBM drift for sample path display\n"
        << "  --rate VALUE              Risk-free pricing rate\n"
        << "  --volatility VALUE        Annualized volatility\n"
        << "  --transaction-cost VALUE  Proportional stock trade cost\n"
        << "  --maturity VALUE          Maturity in years\n"
        << "  --steps VALUE             Time steps per path\n"
        << "  --mc-paths VALUE          Monte Carlo pricing paths\n"
        << "  --hedge-paths VALUE       Paths per hedge strategy\n"
        << "  --rl-train VALUE          RL training episodes\n"
        << "  --rl-eval VALUE           RL evaluation paths\n"
        << "  --rl-loss-aversion VALUE  Extra penalty applied to negative RL P&L\n"
        << "  --seed VALUE              Base seed for all random streams\n"
        << "  --help                    Show this help\n";
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

std::string qLearningActionLabel(int actionIndex);

void printJsonActionCounts(const std::vector<int>& actionCounts) {
    std::cout << "[";
    for (std::size_t index = 0; index < actionCounts.size(); ++index) {
        if (index > 0) {
            std::cout << ",";
        }
        std::cout
            << "{"
            << "\"action\":\"" << jsonEscape(qLearningActionLabel(static_cast<int>(index))) << "\","
            << "\"count\":" << actionCounts[index]
            << "}";
    }
    std::cout << "]";
}

void printJsonReport(
    const ExperimentConfig& config,
    const MonteCarloResult& monteCarloPrice,
    const BlackScholesResult& blackScholes,
    const DeltaHedgeResult& deltaHedge,
    const HedgingEnvironmentRun& environmentRun,
    const QLearningResult& qLearningResult,
    const std::vector<HedgeSimulationSummary>& strategySummaries
) {
    const double monteCarloPriceError = monteCarloPrice.discountedPrice - blackScholes.callPrice;
    const GbmParameters& params = config.market;
    const EuropeanCallOption& option = config.option;

    std::cout << std::fixed << std::setprecision(6);
    std::cout
        << "{"
        << "\"project\":\"StochEdge\","
        << "\"market\":{"
        << "\"initialStockPrice\":" << params.initialStockPrice << ","
        << "\"drift\":" << params.drift << ","
        << "\"riskFreeRate\":" << config.riskFreeRate << ","
        << "\"volatility\":" << params.volatility << ","
        << "\"transactionCostRate\":" << config.transactionCostRate << ","
        << "\"maturityYears\":" << option.maturityYears << ","
        << "\"steps\":" << params.steps << ","
        << "\"strike\":" << option.strike
        << "},";

    std::cout
        << "\"settings\":{"
        << "\"monteCarloPaths\":" << config.monteCarlo.paths << ","
        << "\"hedgePaths\":" << config.hedging.paths << ","
        << "\"rlTrainingEpisodes\":" << config.qLearning.trainingEpisodes << ","
        << "\"rlEvaluationPaths\":" << config.qLearning.evaluationPaths << ","
        << "\"rlLossAversion\":" << config.qLearning.lossAversion
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
        << "\"rlEnvironment\":{"
        << "\"policy\":\"Black-Scholes delta\","
        << "\"state\":\"log(S/K), time left, current hedge, BS delta, delta gap\","
        << "\"action\":\"choose target hedge adjustment\","
        << "\"stepReward\":\"negative transaction cost\","
        << "\"finalRewardPenalty\":\"negative absolute final P&L plus loss-aversion penalty\","
        << "\"steps\":" << environmentRun.steps << ","
        << "\"totalTransactionCosts\":" << environmentRun.totalTransactionCosts << ","
        << "\"finalPnl\":" << environmentRun.finalPnl << ","
        << "\"totalReward\":" << environmentRun.totalReward
        << "},";

    std::cout
        << "\"learnedHedger\":{"
        << "\"algorithm\":\"tabular Q-learning with partial hedge actions\","
        << "\"trainingEpisodes\":" << qLearningResult.trainingEpisodes << ","
        << "\"averageTrainingFinalReward\":" << qLearningResult.averageTrainingReward << ","
        << "\"actionMenu\":\"hold, move 25%, move 50%, move to Black-Scholes delta\","
        << "\"evaluationActionCounts\":";
    printJsonActionCounts(qLearningResult.evaluationActionCounts);
    std::cout << ","
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
            return "move25";
        case 2:
            return "move50";
        case 3:
            return "move100";
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
    std::cout << "Action labels: hold, move25, move50, move100 toward Black-Scholes delta\n\n";
    printPolicyGapRow("under-hedged, mid-life", qTable, 0.50, 0.50, true);
    printPolicyGapRow("over-hedged, mid-life ", qTable, 0.50, 0.50, false);
    printPolicyGapRow("under-hedged, near end", qTable, 0.50, 0.08, true);
    printPolicyGapRow("over-hedged, near end ", qTable, 0.50, 0.08, false);
}

std::vector<StrategyConfig> defaultStrategyConfigs(int steps) {
    const int weeklyRebalance = std::max(1, steps / 52);
    const int monthlyRebalance = std::max(1, steps / 12);

    return {
        {"Daily rebalance", 1, 0.0},
        {"Weekly rebalance", weeklyRebalance, 0.0},
        {"Monthly rebalance", monthlyRebalance, 0.0},
        {"Daily check + 0.05 delta no-trade band", 1, 0.05},
        {"Daily check + 0.10 delta no-trade band", 1, 0.10}
    };
}

void printActionCounts(const std::vector<int>& actionCounts) {
    std::cout << "evaluation action counts       : ";
    for (std::size_t index = 0; index < actionCounts.size(); ++index) {
        if (index > 0) {
            std::cout << ", ";
        }
        std::cout << qLearningActionLabel(static_cast<int>(index)) << "=" << actionCounts[index];
    }
    std::cout << '\n';
}

int main(int argc, char* argv[]) {
    RuntimeOptions runtime;
    try {
        runtime = parseRuntimeOptions(argc, argv);
    } catch (const std::exception& exception) {
        std::cerr << "Configuration error: " << exception.what() << "\n\n";
        printUsage();
        return 2;
    }

    if (runtime.help) {
        printUsage();
        return 0;
    }

    const ExperimentConfig& config = runtime.config;
    const GbmParameters& params = config.market;
    const EuropeanCallOption& option = config.option;

    const std::vector<double> path = simulateGbmPath(params);
    const double finalStockPrice = path.back();
    const double payoff = europeanCallPayoff(finalStockPrice, option.strike);

    const MonteCarloResult monteCarloPrice = priceEuropeanCallMonteCarlo(
        params,
        option,
        config.riskFreeRate,
        config.monteCarlo
    );
    const BlackScholesResult blackScholes = priceEuropeanCallBlackScholes(
        params.initialStockPrice,
        option,
        config.riskFreeRate,
        params.volatility
    );
    const DeltaHedgeResult deltaHedge = runOnePathDeltaHedge(
        path,
        option,
        config.riskFreeRate,
        params.volatility,
        config.transactionCostRate,
        1,
        0.0
    );
    const HedgingEnvironmentRun environmentRun = runBlackScholesPolicyEnvironment(
        path,
        option,
        config.riskFreeRate,
        params.volatility,
        config.transactionCostRate
    );

    std::vector<HedgeSimulationSummary> strategySummaries;
    for (const StrategyConfig& strategy : defaultStrategyConfigs(params.steps)) {
        strategySummaries.push_back(runDeltaHedgeMonteCarlo(
            params,
            option,
            config.riskFreeRate,
            params.volatility,
            config.transactionCostRate,
            strategy.rebalanceEverySteps,
            strategy.deltaRebalanceBand,
            strategy.label,
            config.hedging
        ));
    }

    const QLearningResult qLearningResult = trainAndEvaluateQHedger(
        params,
        option,
        config.riskFreeRate,
        params.volatility,
        config.transactionCostRate,
        config.qLearning
    );
    strategySummaries.push_back(qLearningResult.evaluationSummary);

    if (runtime.json) {
        printJsonReport(
            config,
            monteCarloPrice,
            blackScholes,
            deltaHedge,
            environmentRun,
            qLearningResult,
            strategySummaries
        );
        return 0;
    }

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "StochEdge: GBM + Monte Carlo + Black-Scholes + RL hedging\n";

    printSeparator("Market and option inputs");
    std::cout << "S0       : " << params.initialStockPrice << '\n';
    std::cout << "mu       : " << params.drift << "  (real-world path drift)\n";
    std::cout << "r        : " << config.riskFreeRate << "  (risk-free pricing drift)\n";
    std::cout << "sigma    : " << params.volatility << '\n';
    std::cout << "cost     : " << config.transactionCostRate << "  (0.001 means 0.10% per stock trade)\n";
    std::cout << "T years  : " << option.maturityYears << '\n';
    std::cout << "steps    : " << params.steps << '\n';
    std::cout << "strike K : " << option.strike << '\n';
    std::cout << "MC paths : " << config.monteCarlo.paths << '\n';
    std::cout << "hedge paths : " << config.hedging.paths << '\n';
    std::cout << "RL train/eval paths : "
              << config.qLearning.trainingEpisodes << " / "
              << config.qLearning.evaluationPaths << '\n';

    printSeparator("One possible stock path");
    std::cout << "Sampled path points:\n";
    std::cout << "step, stock_price\n";

    const int printEvery = std::max(1, params.steps / 12);
    for (std::size_t i = 0; i < path.size(); i += static_cast<std::size_t>(printEvery)) {
        std::cout << i << ", " << path[i] << '\n';
    }
    if ((path.size() - 1) % static_cast<std::size_t>(printEvery) != 0) {
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

    printSeparator("One-path delta hedge run");
    std::cout << "sold option for premium        : " << deltaHedge.optionPremium << '\n';
    std::cout << "initial stock hedge delta      : " << deltaHedge.initialDelta << '\n';
    std::cout << "number of rebalances           : " << deltaHedge.rebalances << '\n';
    std::cout << "final stock price              : " << deltaHedge.finalStockPrice << '\n';
    std::cout << "option payoff paid at maturity : " << deltaHedge.optionPayoff << '\n';
    std::cout << "total transaction costs        : " << deltaHedge.totalTransactionCosts << '\n';
    std::cout << "final hedging P&L              : " << deltaHedge.hedgingPnl << '\n';

    printSeparator("RL environment run");
    std::cout << "policy                         : Black-Scholes delta\n";
    std::cout << "state                          : log(S/K), time left, current hedge, BS delta, delta gap\n";
    std::cout << "action                         : choose target hedge adjustment\n";
    std::cout << "step reward                    : negative transaction cost\n";
    std::cout << "final reward penalty           : negative absolute final P&L plus loss-aversion penalty\n";
    std::cout << "environment steps              : " << environmentRun.steps << '\n';
    std::cout << "total transaction costs        : " << environmentRun.totalTransactionCosts << '\n';
    std::cout << "final P&L                      : " << environmentRun.finalPnl << '\n';
    std::cout << "total reward                   : " << environmentRun.totalReward << '\n';

    printSeparator("Many-path delta hedge comparison");
    for (std::size_t index = 0; index + 1 < strategySummaries.size(); ++index) {
        printHedgeSummary(strategySummaries[index]);
    }

    printSeparator("Learned hedger");
    std::cout << "algorithm                      : tabular Q-learning with partial hedge actions\n";
    std::cout << "training episodes              : " << qLearningResult.trainingEpisodes << '\n';
    std::cout << "average training final reward  : " << qLearningResult.averageTrainingReward << '\n';
    std::cout << "action menu                    : hold, move25, move50, move100\n";
    printActionCounts(qLearningResult.evaluationActionCounts);
    printHedgeSummary(qLearningResult.evaluationSummary);

    printSeparator("Strategy leaderboard by tail risk");
    printStrategyLeaderboard(strategySummaries);

    printSeparator("Learned policy snapshot");
    printLearnedPolicySnapshot(qLearningResult.qTable);

    return 0;
}
