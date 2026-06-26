# StochHedge Roadmap

## Milestone 1 - C++ Math Foundations

- Simulate one GBM path.
- Compute a European call payoff.
- Run many GBM paths.
- Estimate the expected discounted payoff with Monte Carlo.
- Implement Black-Scholes price.
- Implement Black-Scholes delta.

## Milestone 2 - Classical Hedging

- Simulate discrete delta hedging.
- Track stock position and cash account.
- Add transaction costs.
- Compute final hedging error.
- Generate P&L distribution.

## Milestone 3 - Risk Metrics

- Mean P&L.
- Standard deviation.
- Value at Risk.
- Expected Shortfall / CVaR.
- Hedge turnover.
- Total transaction cost.

## Milestone 4 - Java Spring Layer

- Create Spring Boot API.
- Send experiment configs to the C++ engine.
- Return pricing, hedging, and risk reports.
- Store experiment history.

## Milestone 5 - RL Hedging

- Define the hedging environment.
- Start with discrete hedge actions.
- Compare RL hedge against Black-Scholes delta hedge.
- Evaluate tail loss, CVaR, and transaction cost tradeoff.

## Milestone 6 - Stronger Stochastic Models

- Add Heston stochastic volatility.
- Add jump diffusion.
- Compare model behavior under the same hedging strategy.
