# Helix

**Helix** is a C/GTK4 desktop application that started as a learning project and is evolving into a small automated BTC-EUR trading engine.

The current version is intentionally conservative: it supports simulation, persistent state, configurable strategy parameters, trade history, and Coinbase read-only market data. Real trading is not enabled yet.

---

## Project status

Helix is currently in an early development phase.

Implemented:

- GTK4 desktop dashboard
- SQLite persistence
- automatic engine loop
- simulated trading mode
- Coinbase BTC-EUR read-only price fetch
- strategy settings saved in SQLite
- wallet state recovery after restart
- trade history journal
- runtime modes:
  - `SIMULATION`
  - `LIVE_READONLY`
  - `LIVE_TRADING` placeholder, intentionally blocked
- minimum liquidity guard
- slot-based buying strategy
- average buy price tracking
- automatic sell condition in simulation mode

Not implemented yet:

- Coinbase authenticated wallet sync
- Coinbase real orders
- real trading execution
- fee/slippage-aware order preview
- reconciliation between local state and Coinbase
- background daemon/systemd service
- multi-threaded engine

---

## Safety note

Helix is not financial advice and should not be used with real funds in its current state.

The `LIVE_TRADING` mode exists only as a UI/runtime placeholder and is blocked by the engine until proper safety checks are implemented.

Before any real trading, Helix must support:

- API key security
- read-only wallet synchronization
- local/remote state reconciliation
- order preview
- fee calculation
- minimum liquidity enforcement
- emergency stop
- persistent order journal
- error handling and retry logic

---

## Current architecture

```text
helix/
├── Makefile
├── README.md
├── src/
│   ├── main.c
│   ├── ui/
│   │   ├── window.c
│   │   └── window.h
│   ├── engine/
│   │   ├── bot_state.c
│   │   ├── bot_state.h
│   │   ├── engine.c
│   │   ├── engine.h
│   │   ├── settings.c
│   │   ├── settings.h
│   │   ├── wallet.c
│   │   └── wallet.h
│   ├── db/
│   │   ├── database.c
│   │   └── database.h
│   ├── market/
│   │   ├── market_data.c
│   │   └── market_data.h
│   └── exchange/
│       ├── coinbase_client.c
│       └── coinbase_client.h
└── data/
    └── helix.db
```

---

## Main concepts

### Bot state

Helix keeps its runtime state in memory using `BotState`, then persists it to SQLite.

The state includes:

- running/stopped flag
- engine mode
- EUR balance
- BTC balance
- current BTC-EUR price
- last buy price
- average buy price
- last trade description
- used slots
- max slots

This allows Helix to recover after closing or restarting the application.

---

### Runtime modes

Helix supports three runtime modes:

| Mode | Description |
|---|---|
| `SIMULATION` | Uses simulated price movement and virtual BUY/SELL operations |
| `LIVE_READONLY` | Reads BTC-EUR spot price from Coinbase without trading |
| `LIVE_TRADING` | Present but intentionally blocked |

---

### Strategy settings

Strategy parameters are saved in SQLite and can be edited from the GTK4 interface:

- slot amount in EUR
- buy drop percentage
- sell profit percentage
- minimum liquidity percentage
- maximum number of slots
- runtime mode

Default values:

```text
slot amount EUR:       100.00
buy drop percent:      2.00%
sell profit percent:   1.50%
minimum liquidity:     25.00%
max slots:             6
runtime mode:          SIMULATION
```

---

### Minimum liquidity guard

Helix does not buy if the operation would leave EUR liquidity below the configured minimum percentage of the estimated wallet value.

Example:

```text
Estimated wallet value: 1000 EUR
Minimum liquidity:      25%
Required EUR reserve:   250 EUR
```

If a BUY would reduce EUR below the reserve, Helix skips the operation.

---

### Trade history

Every simulated BUY/SELL is saved in SQLite in the `trades` table.

The GTK4 dashboard shows the most recent trades first.

---

## Dependencies

On Debian/Ubuntu/Trisquel-like systems:

```bash
sudo apt install build-essential libgtk-4-dev pkg-config libsqlite3-dev libcurl4-openssl-dev libcjson-dev
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

---

## Coinbase read-only mode

Helix currently uses Coinbase only for public BTC-EUR spot price data.

No API key is needed for this step.

To test:

1. start Helix
2. set runtime mode to `LIVE_READONLY`
3. save settings
4. press `Start bot`

Helix will fetch the BTC-EUR spot price from Coinbase and will not execute trades.

---

## Git ignore recommendation

The repository should not include compiled binaries, local databases, archives or secrets.

Recommended `.gitignore`:

```gitignore
# Binary
helix

# SQLite local database
data/*.db
data/*.db-shm
data/*.db-wal

# Object/build files
*.o
*.out

# Archives
*.zip
*.7z
*.tar.gz

# Environment/secrets
.env
*.secret
config/*.secret

# Editor files
.vscode/
.idea/

# System files
.DS_Store
```

---

## Important Git cleanup

If the binary or database were already committed, remove them from Git tracking while keeping local files:

```bash
git rm --cached helix
git rm --cached data/helix.db
```

Then commit the `.gitignore`.

---

## Suggested commit message

```text
feat: add persistent GTK trading simulator with Coinbase read-only mode

- Add GTK4 dashboard with live bot state
- Add SQLite persistence and recovery
- Add automatic simulation engine
- Add configurable strategy settings
- Add trade history journal
- Add runtime modes for simulation/read-only/live trading
- Add Coinbase BTC-EUR read-only market data
- Add minimum liquidity guard
- Block LIVE_TRADING until safety checks are implemented
```

---

## Roadmap

Next steps:

1. improve logging and error reporting
2. add structured application events table
3. add Coinbase authenticated read-only wallet sync
4. add reconciliation between SQLite and Coinbase balances
5. add fee-aware profit calculation
6. add order preview mode
7. add emergency stop
8. add real trading only after safety checks
9. split GTK UI into smaller modules
10. add systemd service mode

---

## License

Not decided yet.
