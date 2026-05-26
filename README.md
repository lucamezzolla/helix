# Helix

**Helix** is a C/GTK4 desktop application for studying and building a cautious BTC-EUR trading engine.

It currently supports simulation, persistent local state, configurable strategy parameters, Coinbase read-only market data, Coinbase authenticated read-only wallet synchronization, Coinbase order preview, dry-run execution, and multiple safety layers.

Real trading is still intentionally blocked.

---

## Current status

Implemented:

- GTK4 desktop dashboard
- global vertical scrolling UI
- expandable UI sections for strategy and Coinbase API settings
- SQLite persistence and state recovery
- automatic engine loop
- simulated BUY/SELL mode
- trade history journal
- configurable strategy settings
- minimum liquidity guard
- liquidity reserve protection
- manual reserve slot release/lock
- slot-based strategy model
- Coinbase BTC-EUR public spot price
- Coinbase authenticated read-only wallet sync
- JWT authentication for Coinbase Advanced Trade / CDP API
- runtime modes:
  - `SIMULATION`
  - `LIVE_READONLY`
  - `LIVE_TRADING` placeholder, still intentionally blocked
- internal trade preview engine
- fee-aware BUY/SELL preview calculations
- estimated profit and break-even calculations
- Coinbase fills reconstruction in read-only mode
- Coinbase order preview integration
- exchange-side safety gate
- runtime safety layer
- reconciliation engine
- order state recovery
- anti duplicate-order engine
- persistent order journal
- final live gate
- pre-live validation
- API health checks
- operational limits:
  - max orders per day
  - cooldown between order attempts
- risk guard:
  - max daily loss guard
  - drawdown guard
- volatility protection
- emergency stop / kill-switch
- manual LIVE_TRADING arm/disarm flag
- dry-run order executor
- real Coinbase create-order transport implemented behind hard safety locks
- post-order result journal support
- post-order reconciliation placeholder
- engine audit journal
- audit throttling and retention cleanup
- engine audit hard cap at 5000 records
- configurable audit retention days
- GTK lifecycle/timer safety fixes

Not implemented yet:

- enabling real Coinbase BUY/SELL execution in normal builds
- final production-grade post-order reconciliation with live fills confirmation
- long pre-live dry-run validation period
- real-money rollout procedure
- background daemon/systemd mode
- advanced backtesting/paper-trading analytics

---

## Safety note

Helix is **not financial advice**.

The current code can read Coinbase wallet balances in read-only mode and can perform Coinbase order previews.

Real order creation code exists, but it is intentionally protected by multiple locks and is **not enabled by the normal Makefile build**.

`LIVE_TRADING` is present as a future runtime mode, but real execution remains blocked unless all of the following are explicitly satisfied in a future controlled build:

- runtime mode is `LIVE_TRADING`
- manual LIVE_TRADING arm flag is enabled from the UI
- emergency stop is disabled
- reconciliation checks pass
- order recovery checks pass
- anti duplicate-order checks pass
- risk guard checks pass
- operational limits pass
- volatility protection passes
- Coinbase order preview succeeds
- final live gate passes
- pre-live validation passes
- required `.env` safety flags are enabled
- binary is compiled with the explicit real-order compile flag

Before using Helix with real money, it must run for an extended period in:

```text
LIVE_READONLY + dry-run executor
```

and its audit/journal decisions must be reviewed carefully.

---

## Project structure

```text
helix/
├── Makefile
├── README.md
├── .gitignore
├── .env.example
├── src/
│   ├── main.c
│   ├── ui/
│   │   ├── window.c
│   │   └── window.h
│   ├── engine/
│   │   ├── anti_duplicate_order.c
│   │   ├── anti_duplicate_order.h
│   │   ├── api_health.c
│   │   ├── api_health.h
│   │   ├── bot_state.c
│   │   ├── bot_state.h
│   │   ├── emergency_stop.c
│   │   ├── emergency_stop.h
│   │   ├── engine.c
│   │   ├── engine.h
│   │   ├── exchange_safety.c
│   │   ├── exchange_safety.h
│   │   ├── final_live_gate.c
│   │   ├── final_live_gate.h
│   │   ├── liquidity_reserve.c
│   │   ├── liquidity_reserve.h
│   │   ├── live_execution_lock.c
│   │   ├── live_execution_lock.h
│   │   ├── live_readiness.c
│   │   ├── live_readiness.h
│   │   ├── operational_limits.c
│   │   ├── operational_limits.h
│   │   ├── order_journal.c
│   │   ├── order_journal.h
│   │   ├── order_state_recovery.c
│   │   ├── order_state_recovery.h
│   │   ├── post_order_reconciliation.c
│   │   ├── post_order_reconciliation.h
│   │   ├── prelive_validation.c
│   │   ├── prelive_validation.h
│   │   ├── reconciliation.c
│   │   ├── reconciliation.h
│   │   ├── risk_guard.c
│   │   ├── risk_guard.h
│   │   ├── runtime_safety.c
│   │   ├── runtime_safety.h
│   │   ├── settings.c
│   │   ├── settings.h
│   │   ├── trade_preview.c
│   │   ├── trade_preview.h
│   │   ├── volatility_protection.c
│   │   ├── volatility_protection.h
│   │   ├── wallet.c
│   │   └── wallet.h
│   ├── db/
│   │   ├── database.c
│   │   └── database.h
│   ├── market/
│   │   ├── market_data.c
│   │   └── market_data.h
│   ├── exchange/
│   │   ├── coinbase_auth.c
│   │   ├── coinbase_auth.h
│   │   ├── coinbase_client.c
│   │   ├── coinbase_client.h
│   │   ├── order_executor.c
│   │   ├── order_executor.h
│   │   ├── order_preview.c
│   │   └── order_preview.h
│   ├── config/
│   │   ├── env_loader.c
│   │   └── env_loader.h
│   └── wallet/
│       ├── wallet_info.c
│       └── wallet_info.h
└── data/
```

---

## Runtime modes

| Mode | Description |
|---|---|
| `SIMULATION` | Uses simulated price movement and virtual BUY/SELL operations |
| `LIVE_READONLY` | Reads real Coinbase price, wallet balances, fills, and previews without trading |
| `LIVE_TRADING` | Present but intentionally blocked by safety gates and build flags |

---

## Strategy and safety settings

The strategy can be configured from the GTK4 interface:

- slot amount in EUR
- buy drop percentage
- sell profit percentage
- estimated fee percentage
- minimum profit in EUR
- minimum profit percentage
- minimum liquidity percentage
- protected liquidity reserve percentage
- manually released reserve slots
- max slots
- audit retention days
- volatility window seconds
- volatility maximum movement percentage
- max orders per day
- order cooldown seconds
- max daily loss EUR
- max drawdown percentage
- runtime mode
- emergency stop
- LIVE_TRADING arm/disarm

Default values may evolve during development. Review them before running Helix for long sessions.

---

## Coinbase read-only and preview mode

In `LIVE_READONLY`, Helix uses Coinbase to read:

- BTC-EUR spot price
- EUR balance
- BTC balance
- fills/order history when available
- official Coinbase order preview

It does **not** place orders in normal builds.

When Helix detects BTC and almost no EUR, it can model the state as a fully allocated BTC position:

```text
Slot used: max / max
Mode: WAITING_SELL
```

Sell decisions are expected to be fee-aware and should only be allowed when the estimated net sell value is above the position cost basis plus the configured minimum profit.

---

## Real execution status

The real Coinbase order transport is implemented for future use, but it is intentionally disabled by default.

The normal Makefile does **not** enable:

```text
HELIX_ENABLE_REAL_COINBASE_ORDERS
```

Even if future `.env` flags are enabled, real order execution remains blocked unless the application is intentionally compiled with the explicit real-order compile flag and all safety gates pass.

Future real-trading `.env` flags are intentionally strict:

```env
HELIX_REAL_TRADING_ENABLED=true
HELIX_ALLOW_COINBASE_ORDERS=true
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=true
```

Do not enable these unless you are intentionally testing a controlled real-trading build.

---

## Coinbase API credentials

Helix stores Coinbase credentials in a local `.env` file.

Example:

```env
COINBASE_API_KEY=organizations/.../apiKeys/...
COINBASE_API_SECRET=-----BEGIN EC PRIVATE KEY-----\n...\n-----END EC PRIVATE KEY-----\n
```

Use a Coinbase/CDP key with:

```text
Algorithm: ECDSA
Permissions: View only
```

Do **not** enable trading, transfer, or withdrawal permissions until the project is intentionally moved to a controlled real-trading branch.

Never commit `.env`.

---

## Dependencies

On Debian/Ubuntu/Trisquel-like systems:

```bash
sudo apt install build-essential libgtk-4-dev pkg-config libsqlite3-dev libcurl4-openssl-dev libcjson-dev libjwt-dev
```

---

## Build

```bash
make
```

Run:

```bash
make run
```

Clean:

```bash
make clean
```

The normal build is intentionally safe and does not enable real Coinbase order execution.

---

## Git hygiene

Do not commit:

- `.env`
- local SQLite databases
- SQLite WAL/SHM files
- Coinbase debug JSON/log files
- compiled binary
- local archives
- files containing personal financial history

Useful cleanup before push:

```bash
rm -f helix
rm -f data/*.json
rm -f data/*.log
rm -f data/*.db-shm
rm -f data/*.db-wal
git status
```

If something sensitive was accidentally staged:

```bash
git restore --staged <file>
```

If something sensitive was already tracked:

```bash
git rm --cached <file>
```

---

## Roadmap

Next steps:

1. run extended `LIVE_READONLY + dry-run` validation
2. review order journal and engine audit decisions
3. improve post-order reconciliation with real fills confirmation
4. add stronger reporting for readiness and risk state
5. add optional paper-trading analytics
6. add backtesting support
7. split GTK UI into smaller files
8. add daemon/systemd runtime mode
9. prepare a separate controlled branch for real-order testing
10. only then evaluate whether to enable a real trading build

---

## License

Not decided yet.
