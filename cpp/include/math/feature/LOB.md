# Snapshot Extensions (State at time $t$)

## Convexity-weighted multi-level imbalance
$$
I_t(\text{cvx}) = \frac{\sum_{i=1}^{N} w_i V_{i,t}^{\text{bid}} - \sum_{i=1}^{N} w_i V_{i,t}^{\text{ask}}}{\sum_{i=1}^{N} w_i (V_{i,t}^{\text{bid}} + V_{i,t}^{\text{ask}})}, \quad w_i = \frac{1}{(i+\epsilon)^\gamma}, \quad \gamma>0
$$
Gives more weight to near-touch depth while still using the whole book. Choose $\gamma$ by CV per symbol.

## Distance-discounted imbalance
$$
I_t(\lambda) = \frac{\sum_{i=1}^{N} e^{-\lambda \Delta p_{i,t}} (V_{i,t}^{\text{bid}} - V_{i,t}^{\text{ask}})}{\sum_{i=1}^{N} e^{-\lambda \Delta p_{i,t}} (V_{i,t}^{\text{bid}} + V_{i,t}^{\text{ask}})}, \quad \Delta p_{i,t} = i \cdot \text{tick}
$$
Encodes urgency: nearer quotes matter more. $\lambda$ controls decay.

## Spread-aware imbalance
$$
I_t(\text{spr}) = 
\begin{cases} 
I_t(N) \cdot 1\{\text{spread}_t \le s_0\} \\[2mm]
\frac{I_t(N)}{1 + \text{spread}_t / \text{tick}}
\end{cases}
$$
Signal is more actionable in tight-spread states.

## Hidden-liquidity–adjusted imbalance (via execution probability $\pi$)
$$
\tilde{V}_{i,t}^{\text{side}} = V_{i,t}^{\text{side}} \cdot \pi_{i,t}^{\text{side}}, \quad
I_t(\text{hid}) = \frac{\sum_i (\tilde{V}_{i,t}^{\text{bid}} - \tilde{V}_{i,t}^{\text{ask}})}{\sum_i (\tilde{V}_{i,t}^{\text{bid}} + \tilde{V}_{i,t}^{\text{ask}})}
$$
Estimate $\pi$ from recent fill/cancel rates at each level.

## Adverse-selection–penalized imbalance
$$
I_t(\text{as}) = I_t(N) - \beta \hat{AS}_t, \quad \hat{AS}_t = \mathbb{E}[\Delta \text{mid}_{t \to t+\tau} \mid \text{provide}]
$$
Down-weights imbalance when providing side is historically picked off.

## Microprice gap z-score
$$
Z_t(\text{mp}) = \frac{MP_t - \text{Mid}_t}{\hat{\sigma}(MP-\text{Mid})}, \quad MP_t = \frac{P_1^{\text{ask}} V_1^{\text{bid}} + P_1^{\text{bid}} V_1^{\text{ask}}}{V_1^{\text{bid}} + V_1^{\text{ask}}}
$$
A standardized push direction; robust across symbols after ToD normalization.

# Dynamic / Temporal Variants

## Imbalance momentum (event-time)
$$
M_t(I) = \sum_{k=1}^{K} \alpha_k \Delta I_{t-k}(N), \quad \Delta I_t(N) = I_t(N) - I_{t-1}(N), \quad \alpha \in (0,1)
$$
Detects building pressure rather than one-off blips.

## Imbalance volatility (uncertainty gate)
$$
\sigma_t(I) = \sqrt{\sum_{k=1}^{K} \omega_k (I_{t-k}(N) - \bar{I})^2}, \quad I_t(\text{stab}) = I_t(N) \cdot 1\{\sigma_t(I) \le q_\eta\}
$$
Trade only when the signal is stable enough.

## Imbalance surprise (innovation)
$$
S_t(I) = I_t(N) - \hat{\mathbb{E}}[I_t(N) \mid F_{t-1}]
$$
Forecast $I$ with a light online model (AR, Kalman, Hawkes); act on residuals.

## Imbalance reversion pressure
$$
R_t(I) = -\nabla I_t(N), \quad \nabla I_t(N) = I_t(N) - I_{t-\Delta}(N)
$$
Captures snapping back after extreme imbalance.

## Queue-position–aware imbalance drift
$$
I_t(qp) = I_t(N) \cdot \left(1 - \frac{\hat{T}_{\text{provide fill}}}{\tau_0}\right)
$$
Down-weights signal if queue position implies slow fills.

## Regime-switched imbalance
$$
I_t(\text{reg}) = \sum_r \Pr(R_t = r \mid \text{features}) \cdot I_t(N)
$$
Gating with latent regimes (spread, intensity, news, vol spikes).

# Shape / Geometry of Depth

## Depth-slope imbalance
$$
V_{i,t}^{\text{bid}} \approx a_b + b_b i, \quad V_{i,t}^{\text{ask}} \approx a_a + b_a i
$$
$$
I_t(\text{slope}) = \frac{b_b - b_a}{|b_b| + |b_a| + \epsilon}
$$
Upward pressure weaker if bids thin faster with level than asks.

## Depth-skew / convexity imbalance
$$
V_{i,t}^{\text{side}} \approx c_0 + c_1 i + c_2 i^2, \quad I_t(\text{cvx-shape}) = \text{sign}(c_{2,b} - c_{2,a}) \cdot \frac{\sum_i (V_{i,t}^{\text{bid}} + V_{i,t}^{\text{ask}})}{|c_{2,b} - c_{2,a}|}
$$
Detects “walls” sitting a few ticks away vs near-touch liquidity.

## Price-gap–weighted imbalance
$$
I_t(\Delta p) = \frac{\sum_i (\Delta p_{i,t})^\rho (V_{i,t}^{\text{bid}} - V_{i,t}^{\text{ask}})}{\sum_i (\Delta p_{i,t})^\rho (V_{i,t}^{\text{bid}} + V_{i,t}^{\text{ask}})}, \quad \rho \in [0,2]
$$
Gives influence to liquidity walls further from the touch (impact risk).

## Entropy of book and directional skew
$$
H_{t}^{\text{side}} = -\sum_{i=1}^{N} \pi_{i,t}^{\text{side}} \log \pi_{i,t}^{\text{side}}, \quad \pi_{i,t}^{\text{side}} = \frac{V_{i,t}^{\text{side}}}{\sum_{j=1}^{N} V_{j,t}^{\text{side}}}
$$
$$
I_t(\text{ent}) = \frac{H_t^{\text{ask}} - H_t^{\text{bid}}}{H_t^{\text{ask}} + H_t^{\text{bid}}}
$$
Concentrated (low-entropy) liquidity signals fragility.

# Flow / Venue Conditioned Imbalance

## Arrival-intensity–adjusted imbalance
$$
I_t(\lambda\text{-adj}) = I_t(N) \cdot \frac{\lambda_{t}^{MO,\text{buy}} - \lambda_{t}^{MO,\text{sell}}}{\lambda_{t}^{MO,\text{buy}} + \lambda_{t}^{MO,\text{sell}} + \epsilon}
$$
Combines standing depth with expected taking flow.

## Add–cancel flow imbalance (fake-liq filter)
$$
I_t(\text{netflow}) = \frac{\sum_i (A_{i,t}^{\text{bid}} - C_{i,t}^{\text{bid}}) - \sum_i (A_{i,t}^{\text{ask}} - C_{i,t}^{\text{ask}})}{\sum_i |A_{i,t}^{\text{bid}} - C_{i,t}^{\text{bid}}| + \sum_i |A_{i,t}^{\text{ask}} - C_{i,t}^{\text{ask}}| + \epsilon}
$$
High cancels reduce trust in raw OBI.

## Venue-weighted imbalance (fragmented markets)
$$
I_t(\text{venue}) = \sum_{v \in V} w_v I_t(N)(v), \quad w_v \propto \text{fill quality or toxicity score of venue } v
$$
Some venues lead or host more informed flow.

## Lead–lag cross-asset imbalance
$$
I_{A\to B,t}(\text{lead}) = \sum_{\ell=1}^{L} \theta_\ell I_{A,t-\ell}(N)
$$
Index future/implied ETF or large component → predict target microprice drift.

## Imbalance under sweep risk
$$
I_t(\text{sweep}) = I_t(N) \cdot 1\{\text{sweep risk low}\}, \quad I_t(\text{sweep+}) = I_t(N) + \gamma \cdot 1\{\text{sweep just occurred}\} \cdot \text{sign}
$$
Adjusts when book is fragile to one-shot takers.
