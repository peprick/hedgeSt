#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
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

struct MonteCarloResult {
    double discountedPrice = 0.0;
    double averagePayoff = 0.0;
    double discountedStdError = 0.0;
    int paths = 0;
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
    if (option.strike < 0.0) {
        throw std::invalid_argument("strike cannot be negative");
    }
    if (option.maturityYears <= 0.0) {
        throw std::invalid_argument("maturityYears must be positive");
    }
}

std::vector<double> simulateGbmPath(const GbmParameters& params) {
    validateGbmParameters(params);

    std::mt19937_64 rng(params.seed);
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

void printSeparator(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

int main() {
    const GbmParameters params;
    const EuropeanCallOption option;
    const double riskFreeRate = 0.05;
    const MonteCarloSettings monteCarloSettings;

    const std::vector<double> path = simulateGbmPath(params);
    const double finalStockPrice = path.back();
    const double payoff = europeanCallPayoff(finalStockPrice, option.strike);

    const MonteCarloResult monteCarloPrice = priceEuropeanCallMonteCarlo(
        params,
        option,
        riskFreeRate,
        monteCarloSettings
    );

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "StochHedge: GBM path + Monte Carlo call pricing\n";

    printSeparator("Market and option inputs");
    std::cout << "S0       : " << params.initialStockPrice << '\n';
    std::cout << "mu       : " << params.drift << "  (real-world path demo drift)\n";
    std::cout << "r        : " << riskFreeRate << "  (risk-free pricing drift)\n";
    std::cout << "sigma    : " << params.volatility << '\n';
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

    return 0;
}
