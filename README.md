# Helix

**Helix** is a C/GTK4 desktop application for studying and building a cautious BTC-EUR trading engine.

It currently supports simulation, persistent local state, configurable strategy parameters, Coinbase read-only market data, and Coinbase authenticated read-only wallet synchronization.

Real trading is intentionally disabled.

---

## Current status

Implemented:

- GTK4 desktop dashboard
- expandable UI sections for strategy and Coinbase API settings
- SQLite persistence and state recovery
- automatic engine loop
- simulated BUY/SELL mode
- trade history journal
- configurable strategy settings
- minimum liquidity guard
- slot-based strategy model
- Coinbase BTC-EUR public spot price
- Coinbase authenticated read-only wallet sync
- JWT authentication for Coinbase Advanced Trade / CDP API
- runtime modes:
  - `SIMULATION`
  - `LIVE_READONLY`
  - `LIVE_TRADING` placeholder, intentionally blocked
- internal trade preview engine
- fee-aware BUY/SELL preview calculations
- estimated profit and break-even calculations
- Coinbase fills/order reconstruction (read-only)
- engine audit journal
- audit throttling and retention cleanup
- configurable audit retention days

Not implemented yet:

- real Coinbase BUY/SELL orders
- Coinbase fills/order history reconstruction
- real average buy price reconstruction
- fee/slippage-aware sell target
- emergency stop layer
- anti double-order protection
- background daemon/systemd mode

---

## Safety note

Helix is **not financial advice**.

The current code can read Coinbase wallet balances in read-only mode, but it must not be used for real trading yet.

`LIVE_TRADING` exists in the UI only as a future runtime mode and is blocked by the engine.

Before real trading, Helix must support:

- strict safety layer
- order preview
- maximum order size
- cooldown between orders
- fee-aware calculations
- persistent order journal
- reconciliation between Coinbase and SQLite
- emergency stop
- strong error handling

---

## Project structure

```text
helix/
├── Makefile
├── README.md
├── .gitignore
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
│   ├── exchange/
│   │   ├── coinbase_auth.c
│   │   ├── coinbase_auth.h
│   │   ├── coinbase_client.c
│   │   └── coinbase_client.h
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
| `LIVE_READONLY` | Reads real Coinbase price and real Coinbase EUR/BTC balances without trading |
| `LIVE_TRADING` | Present but intentionally blocked |

---

## Strategy settings

The strategy can be configured from the GTK4 interface:

- slot amount in EUR
- buy drop percentage
- sell profit percentage
- minimum liquidity percentage
- max slots
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

## Coinbase read-only mode

In `LIVE_READONLY`, Helix uses Coinbase only to read:

- BTC-EUR spot price
- EUR balance
- BTC balance

It does **not** place orders.

When Helix detects BTC and almost no EUR, it models the state as a fully allocated BTC position:

```text
Slot used: max / max
Mode: WAITING_SELL
```

This does not mean Helix knows the historical entry price yet. That will require reading fills/order history in a future step.

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

Do **not** enable trading, transfer, or withdrawal permissions yet.

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

---

## Git hygiene

Do not commit:

- `.env`
- local SQLite databases
- Coinbase debug JSON/log files
- compiled binary
- local archives

Useful cleanup before push:

```bash
rm -f data/*.json data/*.log
rm -f helix
git status
```

If something sensitive was already tracked:

```bash
git rm --cached .env
git rm --cached data/coinbase_accounts_raw.json
git rm --cached data/coinbase_accounts_summary.log
```

---

## Suggested commit message

```text
feat: add Coinbase authenticated read-only wallet sync

- Add Coinbase JWT authentication
- Add read-only wallet balance synchronization
- Add EUR/BTC remote wallet model
- Add Coinbase API settings in GTK UI
- Add expandable settings sections
- Keep LIVE_TRADING blocked by safety guard
- Improve decimal parsing for Coinbase balances
- Update gitignore for local debug files
```

---

## Roadmap

Next steps:

1. read Coinbase fills/order history
2. reconstruct real average buy price
3. compute real P/L and target sell price
4. add fee-aware profit calculation
5. add safety layer module
6. add order preview mode
7. add emergency stop
8. add optional paper trading with live data
9. split GTK UI into smaller files
10. add systemd/background runtime mode

---

## License

Not decided yet.
