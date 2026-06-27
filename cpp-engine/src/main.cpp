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

int main() {
    const GbmParameters params;
    const EuropeanCallOption option;
    const double riskFreeRate = 0.05;
    const double transactionCostRate = 0.001;
    const MonteCarloSettings monteCarloSettings;
    const HedgeSimulationSettings hedgeSimulationSettings;

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
    std::cout << "Monte Carlo minus formula     : "
              << monteCarloPrice.discountedPrice - blackScholes.callPrice << '\n';

    printSeparator("One-path delta hedge demo");
    std::cout << "sold option for premium        : " << deltaHedge.optionPremium << '\n';
    std::cout << "initial stock hedge delta      : " << deltaHedge.initialDelta << '\n';
    std::cout << "number of rebalances           : " << deltaHedge.rebalances << '\n';
    std::cout << "final stock price              : " << deltaHedge.finalStockPrice << '\n';
    std::cout << "option payoff paid at maturity : " << deltaHedge.optionPayoff << '\n';
    std::cout << "total transaction costs        : " << deltaHedge.totalTransactionCosts << '\n';
    std::cout << "final hedging P&L              : " << deltaHedge.hedgingPnl << '\n';

    printSeparator("Many-path delta hedge comparison");
    printHedgeSummary(dailyHedgeSummary);
    printHedgeSummary(weeklyHedgeSummary);
    printHedgeSummary(monthlyHedgeSummary);
    printHedgeSummary(band005HedgeSummary);
    printHedgeSummary(band010HedgeSummary);

    return 0;
}
