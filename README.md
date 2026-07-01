# StochEdge

StochEdge is a quant engineering project that compares classical option hedging
against a small reinforcement-learning-style hedger.

The stochastic calculus and simulation work lives in C++. A Java Spring Boot
backend wraps the C++ engine, exposes the experiment through HTTP, and serves a
browser dashboard for exploring the output. There is no Docker setup and no
separate Node frontend process.

This is an educational research project, not financial advice.

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
- Serves a dashboard with pricing cards, risk charts, strategy tables, and raw
  JSON output.

## Project Layout

```text
cpp-engine/                         C++ stochastic simulation, pricing, hedging, and RL engine
backend-spring/                     Java Spring Boot API wrapper around the C++ engine
backend-spring/src/main/resources/static/
                                    Browser dashboard served by Spring
README.md                           Project guide
```

## Prerequisites

You need these tools before building from a fresh clone:

- Git
- Java 17
- Apache Maven 3.9+
- CMake 3.20+
- A C++20 compiler

Ninja is optional on macOS, but recommended on Windows when using MSYS2.

## macOS Setup

Install Apple command line tools if you do not already have a compiler:

```bash
xcode-select --install
```

If you use Homebrew, install the rest of the toolchain with:

```bash
brew install cmake maven openjdk@17
```

Make sure Java 17 is active:

```bash
java -version
mvn -version
cmake --version
```

If Homebrew installed Java but `java` is not on PATH, follow the `openjdk@17`
message printed by Homebrew, or set `JAVA_HOME` to the installed JDK.

## Windows Setup

Install:

- Java 17, such as Eclipse Temurin 17 or Microsoft Build of OpenJDK 17
- Apache Maven 3.9+
- MSYS2 from `https://www.msys2.org/`

Open the MSYS2 UCRT64 shell and install the C++ build tools:

```bash
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

Add the UCRT64 tools to your Windows PATH so PowerShell, Maven, and Spring can
find the C++ runtime:

```powershell
$env:Path = $env:Path + ';C:\msys64\ucrt64\bin'
```

For a permanent PATH update, add this folder in Windows environment variables:

```text
C:\msys64\ucrt64\bin
```

Check the tools from PowerShell:

```powershell
java -version
mvn -version
cmake --version
g++ --version
```

## Clone

```bash
git clone https://github.com/peprick/hedgeSt.git
cd hedgeSt
```

## Build The C++ Engine

On macOS:

```bash
cmake -S cpp-engine -B cpp-engine/build
cmake --build cpp-engine/build
./cpp-engine/build/stochedge_engine --json
```

On Windows PowerShell:

```powershell
cmake -S cpp-engine -B cpp-engine/build -G Ninja
cmake --build cpp-engine/build
.\cpp-engine\build\stochedge_engine.exe --json
```

The engine can also run with custom experiment inputs:

```bash
./cpp-engine/build/stochedge_engine --spot 120 --strike 110 --volatility 0.30 --mc-paths 50000 --rl-train 20000
```

On Windows, use the `.exe` path:

```powershell
.\cpp-engine\build\stochedge_engine.exe --spot 120 --strike 110 --volatility 0.30 --mc-paths 50000 --rl-train 20000
```

## Build And Run The Spring App

Build and test the backend:

```bash
cd backend-spring
mvn test
mvn package
```

Run the app:

```bash
java -jar target/stochedge-backend-0.1.0-SNAPSHOT.jar
```

Open the dashboard:

```text
http://localhost:8080
```

The backend looks for the C++ engine at this default path:

```properties
quant.engine.executable=../cpp-engine/build/stochedge_engine
```

On Windows, the backend automatically tries the same path with `.exe` appended
if the extensionless path does not exist.

To override the engine path without editing files:

```bash
java -Dquant.engine.executable=/absolute/path/to/stochedge_engine -jar target/stochedge-backend-0.1.0-SNAPSHOT.jar
```

On Windows:

```powershell
java -Dquant.engine.executable=C:\path\to\stochedge_engine.exe -jar target\stochedge-backend-0.1.0-SNAPSHOT.jar
```

Engine arguments and timeout live in:

```text
backend-spring/src/main/resources/application.properties
```

```properties
quant.engine.arguments=--json
quant.engine.timeout-seconds=90
```

## API Checks

Health check with curl:

```bash
curl http://localhost:8080/api/health
```

Health check with PowerShell:

```powershell
Invoke-RestMethod -Uri http://localhost:8080/api/health
```

Run the quant experiment with curl:

```bash
curl -X POST http://localhost:8080/api/experiments/run
```

Run the quant experiment with PowerShell:

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

## Market Data

The dashboard can request historical market data through:

```text
GET /api/market/history?symbol=AAPL&days=252
```

The backend tries Yahoo Finance, Stooq, and Twelve Data. If those providers are
unavailable, it falls back to generated demo data so the dashboard still works.
The default Twelve Data key is `demo`; set a real key in
`backend-spring/src/main/resources/application.properties` if you need more
reliable provider access.

## Frontend Dashboard

The dashboard is served directly by Spring from:

```text
backend-spring/src/main/resources/static/
```

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

## Generated Files

These files and folders are local build outputs or caches. They are ignored by
Git and can be deleted safely:

```text
cpp-engine/build/
backend-spring/target/
.m2/
.tools/
cmake-build-*/
```

## Troubleshooting

If Spring says the C++ engine was not found, build the C++ engine first and
check `quant.engine.executable`.

If Windows can build the engine but Spring cannot start it, make sure
`C:\msys64\ucrt64\bin` is on PATH.

If port `8080` is already in use, run with another port:

```bash
java -Dserver.port=8081 -jar target/stochedge-backend-0.1.0-SNAPSHOT.jar
```

If Maven cannot find Java, set `JAVA_HOME` to a Java 17 JDK and make sure
`java -version` reports version 17.

## Next Improvements

- Split the C++ engine into pricing, simulation, hedging, and learning modules.
- Add configurable experiment inputs through the Spring API.
- Add Heston stochastic-volatility simulation.
- Store experiment runs in a database.
- Add downloadable experiment reports.
