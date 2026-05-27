# Helix

**Helix** is a C/GTK4 desktop application for studying and building a cautious BTC-EUR trading engine.

It currently supports simulation, persistent local state, configurable strategy parameters, Coinbase read-only market data, Coinbase authenticated read-only wallet synchronization, Coinbase order preview, dry-run/live-candidate validation, and multiple safety gates.

Current milestone:

```text
v0.2.0-rc1
```

The normal build remains intentionally safe and does not execute real orders.

The separate live-candidate build has completed the first supervised micro-live milestone: one real BTC-EUR BUY was accepted by Coinbase, filled, reconciled, and followed by automatic stop after real order.

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
  - `LIVE_TRADING` live-candidate mode behind safety gates
- Coinbase order preview integration
- BUY preview/order sizing with `quote_size` in EUR
- SELL sizing model with `base_size` in BTC
- locale-safe decimal formatting for Coinbase JSON payloads
- dry-run order executor
- live-candidate real Coinbase create-order transport
- final live gate
- live execution lock
- persistent order journal
- risk guard
- runtime safety layer
- operational limits:
  - max orders per day
  - order cooldown
- anti duplicate-order engine
- post-order reconciliation for controlled live candidate tests
- micro-live settings:
  - micro-live enabled flag
  - micro-live max order in EUR
  - micro-live accumulation consent
  - stop after real order
- first supervised real BTC-EUR BUY completed and reconciled in `v0.2.0-rc1`

Not implemented yet:

- real Coinbase BUY/SELL orders in the normal build
- continuous unattended live trading
- full BUY -> SELL profit cycle validation
- production-grade live SELL execution validation
- long post-live dry-run validation period
- dedicated paper/simulation executable that can never trade
- advanced paper-trading analytics
- background daemon/systemd mode

---

## Safety note

Helix is **not financial advice**.

The normal build can read Coinbase wallet balances in read-only mode and must remain safe for normal development.

`LIVE_TRADING` is available only as a guarded live-candidate path in the dedicated `Makefile.live` build. It must not be used casually.

As of `v0.2.0-rc1`, Helix has completed one supervised micro-live BUY with real money. This does **not** make Helix production-ready and does **not** mean unattended trading is safe.

Before continuous real trading, Helix must support and validate:

- strict safety layer
- order preview
- maximum order size
- cooldown between orders
- fee-aware calculations
- persistent order journal
- reconciliation between Coinbase and SQLite
- emergency stop
- strong error handling

Implemented and validated in the `v0.2.0-rc1` micro-live milestone:

```text
Coinbase preview OK
Final live gate OK
Live execution lock OK
Real executor SENT
Coinbase HTTP 200
Order FILLED / Completato
Post-order reconciliation OK
Micro-live stop after real order
```

Recommended current posture:

```text
Use normal build for daily development.
Use Makefile.live only for explicit supervised micro-live tests.
Keep stop-after-real-order enabled.
Do not run continuous live trading yet.
```

---

## Project structure

```text
helix/
├── Makefile
├── Makefile.live
├── README.md
├── VERSION
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
│   │   ├── final_live_gate.c
│   │   ├── live_execution_lock.c
│   │   ├── operational_limits.c
│   │   ├── order_journal.c
│   │   ├── risk_guard.c
│   │   ├── runtime_safety.c
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
| `LIVE_READONLY` | Reads real Coinbase price and real Coinbase EUR/BTC balances without trading |
| `LIVE_TRADING` | Normal build stays blocked; live-candidate build can execute only if all gates pass |

---

## Strategy settings

The strategy can be configured from the GTK4 interface:

- slot amount in EUR
- buy drop percentage
- sell profit percentage
- minimum liquidity percentage
- max slots
- runtime mode
- emergency stop / kill-switch
- LIVE_TRADING arm/disarm
- micro-live enabled flag
- micro-live max order in EUR
- micro-live accumulation consent
- stop after real order
- max orders per day
- cooldown between order attempts
- max daily loss EUR
- max drawdown percentage
- protected liquidity reserve percentage

Default values:

```text
slot amount EUR:       100.00
buy drop percent:      2.00%
sell profit percent:   1.50%
minimum liquidity:     25.00%
max slots:             6
runtime mode:          SIMULATION

Micro-live values used for the first real BUY milestone:

slot amount EUR:       10.00
micro-live max order:  10.00
protected reserve:     80.00%
max orders per day:    1
cooldown:              900 seconds
stop after real order: enabled
```

---

## Coinbase read-only mode

In `LIVE_READONLY`, Helix uses Coinbase only to read:

- BTC-EUR spot price
- EUR balance
- BTC balance
- fills/order history when available
- official Coinbase order preview when needed

It does **not** place orders in the normal safe build.

When Helix detects BTC and almost no EUR, it models the state as a fully allocated BTC position:

```text
Slot used: max / max
Mode: WAITING_SELL
```

Helix can reconstruct and reconcile parts of the position using Coinbase fills, but this area still needs more long-term validation before unattended trading.

---

## Live-candidate execution status

The normal Makefile build remains safe.

```bash
make clean
make run
```

The live-candidate build is explicit:

```bash
make -f Makefile.live clean
make -f Makefile.live
make -f Makefile.live run
```

The live-candidate build creates:

```text
./helix-live
```

Real execution in `helix-live` requires all of the following:

```text
Runtime = LIVE_TRADING
LIVE_TRADING arm = enabled from UI
Kill-switch = disabled
Micro-live = enabled
Micro-live accumulation = explicitly enabled when buying with an open BTC position
Max order limit respected
Max orders/day respected
Cooldown respected
Liquidity reserve respected
Risk guard passed
Runtime safety passed
Exchange safety passed
Final live gate passed
Anti duplicate-order passed
Live execution lock passed
Coinbase preview valid
.env real-trading flags enabled
```

The live `.env` flags are intentionally strict:

```env
HELIX_REAL_TRADING_ENABLED=true
HELIX_ALLOW_COINBASE_ORDERS=true
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=true
```

Do not enable these unless you are intentionally running a supervised live-candidate test.

---

## First real BUY milestone

Milestone:

```text
v0.2.0-rc1
```

Helix completed one supervised real BUY:

```text
Market: BTC-EUR
Type: Market
Side: BUY
Total: about 10 EUR
Status: FILLED / Completato
```

Expected and observed flow:

```text
BUY uses quote_size in EUR
Coinbase preview returned errs: []
Final gate OK
Real executor sent order
Coinbase accepted order
Post-order reconciliation OK
Micro-live stop after real order
Bot stopped automatically
```

This validates the guarded micro-live BUY path. It does **not** validate continuous automatic trading yet.

---

## Coinbase order sizing

Helix must build Coinbase orders according to the side:

```text
BUY  -> quote_size in EUR
SELL -> base_size in BTC
```

Example BUY request:

```json
{
  "product_id": "BTC-EUR",
  "side": "BUY",
  "order_configuration": {
    "market_market_ioc": {
      "quote_size": "10.00",
      "rfq_disabled": true
    }
  }
}
```

Example meaning:

```text
BUY 10 EUR of BTC
Coinbase receives quote_size = "10.00"
Coinbase calculates base_size BTC
```

For SELL:

```text
Sell BTC quantity
Coinbase receives base_size
Coinbase calculates EUR received
```

Coinbase JSON payloads must use decimal dots:

```text
10.00
0.00015346
```

not Italian locale commas:

```text
10,00
0,00015346
```

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
Permissions for normal testing: View only
Permissions for supervised live-candidate: View + Trade
```

Recommended live-candidate key:

```text
[✓] View
[✓] Trade
[ ] Transfer
[ ] Receive
```

Do **not** enable transfer or withdrawal permissions.

Never commit `.env`.

---

## Dependencies

On Debian/Ubuntu/Trisquel-like systems:

```bash
sudo apt install build-essential libgtk-4-dev pkg-config libsqlite3-dev libcurl4-openssl-dev libcjson-dev libjwt-dev
```

---

## Build

### Normal safe build

```bash
make clean
make run
```

This builds and runs:

```text
./helix
```

Use this for normal development, simulation, read-only Coinbase checks and dry-run validation.

### Live-candidate build

```bash
make -f Makefile.live clean
make -f Makefile.live
make -f Makefile.live run
```

This builds and runs:

```text
./helix-live
```

Use this only for explicit supervised live-candidate tests.

### Quick reminder

```text
make run                    -> safe normal build, runs ./helix
make -f Makefile.live run   -> live-candidate build, runs ./helix-live
```

---

## Git hygiene

Do not commit:

- `.env`
- local SQLite databases
- SQLite WAL/SHM files
- Coinbase debug JSON/log files
- generated reports/snapshots
- compiled binary
- `helix-live`
- local archives
- files containing personal financial history

Useful cleanup before push:

```bash
rm -f helix
rm -f helix-live
rm -f data/*.json
rm -f data/*.log
rm -f data/*.txt
rm -f data/*.db-shm
rm -f data/*.db-wal
git status
```

If something sensitive was already tracked:

```bash
git rm --cached .env
git rm --cached data/coinbase_accounts_raw.json
git rm --cached data/coinbase_accounts_summary.log
```

---

## Local DB and cleanup

The active database is:

```text
data/helix.db
```

Do not delete it if you want to preserve Helix state, settings and journal.

Useful backup:

```bash
cp data/helix.db data/helix_after_first_real_buy.db
```

Do not commit `data/`.

If the local DB contains test noise, use a dedicated cleanup script only after making a backup:

```bash
cp data/helix.db data/helix_before_cleanup.db
sqlite3 data/helix.db < docs/cleanup_live_candidate_noise.sql
```

---

## Suggested commit message

```text
feat: complete guarded micro-live buy execution path

- Add guarded live-candidate BUY path
- Use quote_size for BUY and base_size for SELL
- Add locale-safe Coinbase order formatting
- Add micro-live accumulation consent
- Add live execution lock and final gate flow
- Fix operational limits to count real orders only
- Fix anti-duplicate behavior for live path
- Add post-order reconciliation for first live BUY
- Keep normal build safe
- Document first real BUY milestone
```

---

## Roadmap

Next steps:

1. verify state after restart following the first real order
2. review Coinbase order, local journal and reconciliation data
3. improve report clarity for real-order lifecycle
4. add a separate paper/simulation executable that can never trade
5. add automated tests around engine decisions and safety gates
6. study and document the engine flow module by module
7. validate the complete cycle BUY -> hold -> SELL profitably
8. improve post-order reconciliation with broader live-fill edge cases
9. introduce controlled `micro-live continuous` mode
10. only later evaluate disabling stop-after-real-order for supervised tests
11. add optional paper trading with live data
12. add backtesting support
13. split GTK UI into smaller files
14. add systemd/background runtime mode

---

## Versioning

Current milestone:

```text
v0.2.0-rc1
```

Suggested tag:

```bash
git tag -a v0.2.0-rc1 -m "Micro-live candidate: first real BUY executed and reconciled"
git push origin v0.2.0-rc1
```

`v0.2.0-rc1` means release candidate / milestone, not stable unattended trading.

---

## License

Not decided yet.
