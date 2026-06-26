# StochHedge

StochHedge is a math-first quant research project. The first goal is to understand
stochastic stock paths, option payoff, and hedging mechanics in C++. Later we will
wrap the engine with Java Spring APIs and add reinforcement learning hedging.

No Docker.

## Current Step

The current C++ program does two things:

1. Simulates a stock path using Geometric Brownian Motion.
2. Takes the final stock price at maturity.
3. Computes the payoff of a European call option.
4. Runs many simulated terminal stock prices.
5. Estimates the call option price with Monte Carlo.

This is the foundation for option pricing, delta hedging, P&L simulation, and RL.

## Project Shape

```text
cpp-engine/       C++ stochastic calculus and quant math engine
docs/learning/    Short notes explaining the math and finance as we build
backend-spring/   Java Spring API layer, added later
```

## Build Later

This machine does not currently expose a C++ compiler or CMake on PATH. Once a
compiler is available, either of these will work.

With CMake:

```powershell
cd C:\Users\Sagarnil\quant1\cpp-engine
cmake -S . -B build
cmake --build build
.\build\Debug\stoch_hedge_first_step.exe
```

With MinGW g++:

```powershell
cd C:\Users\Sagarnil\quant1\cpp-engine
g++ -std=c++20 -O2 -Wall -Wextra src\main.cpp -o stoch_hedge_first_step.exe
.\stoch_hedge_first_step.exe
```

With MSVC Developer PowerShell:

```powershell
cd C:\Users\Sagarnil\quant1\cpp-engine
cl /EHsc /std:c++20 src\main.cpp /Fe:stoch_hedge_first_step.exe
.\stoch_hedge_first_step.exe
```
