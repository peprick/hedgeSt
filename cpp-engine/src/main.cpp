#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

struct GbmParameters {
    double initialStockPrice = 100.0;
    double drift = 0.05;
    double volatility = 0.20;
    double maturityYears = 1.0;
    int steps = 252;
    unsigned long long seed = 42;
};

std::vector<double> simulateGbmPath(const GbmParameters& params) {
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

int main() {
    const GbmParameters params;
    const double strike = 100.0;

    const std::vector<double> path = simulateGbmPath(params);
    const double finalStockPrice = path.back();
    const double payoff = europeanCallPayoff(finalStockPrice, strike);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "StochHedge first step: GBM path + European call payoff\n\n";
    std::cout << "S0       : " << params.initialStockPrice << '\n';
    std::cout << "mu       : " << params.drift << '\n';
    std::cout << "sigma    : " << params.volatility << '\n';
    std::cout << "T years  : " << params.maturityYears << '\n';
    std::cout << "steps    : " << params.steps << '\n';
    std::cout << "strike K : " << strike << "\n\n";

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

    return 0;
}
