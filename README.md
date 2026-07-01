# StochEdge

StochEdge is a math-heavy quant engineering project that compares classical option
hedging against a small reinforcement-learning-style hedger.

The core stochastic calculus and simulation work lives in C++. A Java Spring
Boot backend wraps the C++ engine, exposes the experiment through HTTP, and
serves a frontend dashboard for exploring the output.

No Docker.

## What It Does

- Simulates stock paths with Geometric Brownian Motion.
- Prices a European call option with Monte Carlo simulation.
- Computes the Black-Scholes call price and delta.
- Runs delta hedging experiments with transaction costs.
- Compares daily, weekly, monthly, and no-trade-band hedging strategies.
- Reports risk metrics such as mean P&L, standard deviation, VaR, CVaR, loss
  probability, worst P&L, and transaction costs.
- Trains and evaluates a tabular Q-learning-style hedger with hold, partial
  rebalance, and full rebalance actions.
- Exposes the C++ experiment result through a Spring Boot API.
- Serves a dashboard with pricing cards, risk charts, strategy tables, and raw
  JSON output.

## Project Shape

```text
cpp-engine/                         C++ stochastic simulation, pricing, hedging, and RL engine
backend-spring/                     Java Spring Boot API wrapper around the C++ engine
backend-spring/src/main/resources/static/
                                    Browser dashboard served by Spring
README.md                           Project guide
```

## Big Idea

Imagine you sell someone an option. The option can become expensive for you if
the stock moves. Hedging means you keep buying or selling pieces of the stock so
your risk stays controlled.

The project compares:

- a formula-based hedge from Black-Scholes delta;
- slower or cheaper hedge schedules;
- no-trade bands that avoid tiny costly trades;
- a learning agent that tries to decide when to hedge.

The C++ engine does the math many times on random stock paths. The Spring app
acts like a clean backend layer and serves the frontend that turns the results
into charts, tables, and readable metrics.

## Prerequisites

- Java 17
- Apache Maven 3.9+
- MSYS2 UCRT64 toolchain, including `g++`, `cmake`, and `ninja`

On this machine, the MSYS2 runtime path is:

```powershell
C:\msys64\ucrt64\bin
```

If the C++ executable fails to start from PowerShell or Spring, add that folder
to PATH before running commands:

```powershell
$env:Path = $env:Path + ';C:\msys64\ucrt64\bin'
```

## Build And Run The C++ Engine

```powershell
cd C:\Users\Sagarnil\quant1\cpp-engine
cmake -S . -B build -G Ninja
cmake --build build
.\build\stochedge_engine.exe
```

To get machine-readable JSON for the Spring backend:

```powershell
.\build\stochedge_engine.exe --json
```

The engine can also run with custom experiment inputs:

```powershell
.\build\stochedge_engine.exe --spot 120 --strike 110 --volatility 0.30 --mc-paths 50000 --rl-train 20000
```

## Build And Run The Spring Backend

```powershell
cd C:\Users\Sagarnil\quant1\backend-spring
mvn test
mvn package -DskipTests
java -jar target\stochedge-backend-0.1.0-SNAPSHOT.jar
```

Open the dashboard:

```text
http://localhost:8080
```

The backend expects this C++ engine path by default:

```text
../cpp-engine/build/stochedge_engine.exe
```

You can change it in:

```text
backend-spring/src/main/resources/application.properties
```

The same file also controls engine arguments:

```properties
quant.engine.arguments=--json
```

## API

Health check:

```powershell
Invoke-RestMethod -Uri http://localhost:8080/api/health
```

Run the quant experiment:

```powershell
Invoke-RestMethod -Method Post -Uri http://localhost:8080/api/experiments/run
```

The experiment response includes:

- market assumptions;
- Monte Carlo price;
- Black-Scholes price and delta;
- one-path delta hedge run;
- strategy risk summaries;
- Q-learning hedger evaluation.

## Frontend Dashboard

The dashboard is served directly by Spring, so there is no separate Node or npm
frontend process.

It includes:

- backend health status;
- Run Experiment button;
- Black-Scholes vs Monte Carlo pricing cards;
- market assumption summary;
- strategy leaderboard by CVaR tail risk;
- sortable strategy comparison chart and table;
- Q-learning agent vs daily delta hedge comparison;
- RL action-count summary;
- raw JSON view for debugging and learning.

## Current Result Snapshot

The default run uses:

- initial stock price: `100`
- strike: `100`
- risk-free rate: `5%`
- volatility: `20%`
- maturity: `1` year
- trading steps: `252`
- transaction cost rate: `0.1%`

The engine currently reports a Black-Scholes call price near `10.450584` and
compares six hedging strategies.

## Next Improvements

- Split the C++ engine into pricing, simulation, hedging, and learning modules.
- Add configurable experiment inputs through the Spring API.
- Add Heston stochastic-volatility simulation.
- Store experiment runs in a database.
- Add downloadable experiment reports.

This is an educational research project, not financial advice.
