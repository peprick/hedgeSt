# 02 - Monte Carlo Option Pricing

The first program simulated one possible future stock path. One path is useful for
intuition, but pricing needs many possible futures.

## The Pricing Question

For a European call option, the maturity payoff is:

```text
max(S_T - K, 0)
```

But today we do not know `S_T`, so we ask:

```text
What is the average payoff across many possible futures?
```

That average is an expectation:

```text
E[max(S_T - K, 0)]
```

## Discounting

Money in the future is not worth the same as money today. If the risk-free rate is
`r` and maturity is `T`, the discount factor is:

```text
exp(-rT)
```

So a first pricing formula is:

```text
price = exp(-rT) * expected payoff
```

## Risk-Neutral Drift

In the one-path demo, the stock drift was called `mu`. That is the real-world
expected growth rate.

For option pricing under Black-Scholes assumptions, we do not use `mu`. We use the
risk-free rate `r` as the drift:

```text
dS_t = r S_t dt + sigma S_t dW_t
```

This is called risk-neutral pricing. The short version:

```text
Under the pricing measure, investors are already compensated through discounting,
so the stock grows on average at the risk-free rate.
```

That sentence is deep. It will make more sense after delta hedging and no-arbitrage.
For now, remember the implementation rule:

```text
Use mu for real-world simulation.
Use r for option pricing.
```

## Monte Carlo Estimate

Monte Carlo pricing repeats this many times:

```text
1. Simulate a terminal stock price S_T.
2. Compute payoff = max(S_T - K, 0).
3. Store the payoff.
```

Then:

```text
average payoff = sum(payoffs) / number of paths
price = exp(-rT) * average payoff
```

More paths usually means a more stable estimate, but it costs more computation.

## Standard Error

The program also reports a standard error. This estimates how noisy the Monte Carlo
price is.

Rough interpretation:

```text
price estimate = reported price plus/minus some simulation noise
```

If standard error is large, increase the number of paths.
