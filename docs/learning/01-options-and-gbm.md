# 01 - Options And Geometric Brownian Motion

This note explains the first tiny C++ program.

## The Finance Object: European Call Option

A European call option gives its holder the right to buy a stock at a fixed strike
price `K` on a fixed maturity date `T`.

At maturity, its payoff is:

```text
max(S_T - K, 0)
```

Where:

```text
S_T = stock price at maturity
K   = strike price
```

Example:

```text
K = 100
S_T = 125
payoff = max(125 - 100, 0) = 25
```

If the stock finishes below the strike:

```text
K = 100
S_T = 80
payoff = max(80 - 100, 0) = 0
```

## The Math Object: Random Stock Path

We model the stock price as a stochastic process. A stochastic process is just a
quantity that evolves over time with randomness.

The first model is Geometric Brownian Motion:

```text
dS_t = mu S_t dt + sigma S_t dW_t
```

Read it like this:

```text
S_t   = stock price at time t
mu    = drift, the average growth rate
sigma = volatility, the size of random movement
dt    = tiny time step
dW_t  = Brownian motion shock
```

Plain English:

```text
The stock has an average trend, but every small time step receives a random shock.
```

## The Discrete Formula Used In Code

Computers cannot simulate continuous time directly, so we split one year into
`252` trading-day steps.

The exact GBM step is:

```text
S_next = S_now * exp((mu - 0.5 sigma^2) dt + sigma sqrt(dt) Z)
```

Where:

```text
Z = random number from a standard normal distribution
```

The term `sigma sqrt(dt) Z` is the random shock.

## Why This Is Step One

Hedging needs many possible future stock paths. Before we can test a delta hedge
or an RL hedge, we need a market simulator.

This first program answers:

```text
If the stock follows GBM, what could one possible future path look like?
At the end of that path, what would a call option pay?
```

Later we will run thousands of paths, not just one.
