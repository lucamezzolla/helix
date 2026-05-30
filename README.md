# Helix

[![Donate with PayPal](https://img.shields.io/badge/Donate-PayPal-00457C?logo=paypal&logoColor=white)](https://www.paypal.com/paypalme/lucamezzolla82)

> If you find Helix useful or want to support its development, you can make a small donation through PayPal.  
> Your support helps improve documentation, testing, safety checks, UI polish and controlled production-readiness.

---

Helix is a desktop application written in **C** with a **GTK4** interface. It is designed as a cautious BTC-EUR trading engine connected to Coinbase, built around one core principle: safety first, automation second.

Helix is not intended to trade aggressively or autonomously without human supervision. The current goal is **controlled production**: micro-sized operations, one slot at a time, automatic stop after every real order, mandatory reconciliation, and manual acknowledgement before continuing.

> ⚠️ Helix can interact with real money. Before any live test, always verify the code, local database, `.env` configuration, logs, journal entries, Coinbase status, and current wallet balances.

---

## Project principles

Helix follows strict operating rules:

- it never buys using the entire available balance;
- it never sells the whole BTC wallet as a single undifferentiated position;
- it buys and sells by **slots/lots**;
- every real BUY must open a real slot;
- every SELL must evaluate only open slots;
- a SELL must close only the selected slot, not the global BTC balance;
- the EUR reserve remains protected;
- every real order must be tracked in `order_journal`;
- every real order must be reconciled after Coinbase execution;
- after every real order Helix must stop;
- the user must manually acknowledge the last real order before proceeding.

The golden rule is:

```text
It is better to do nothing than to place an ambiguous order.
```

---

## Current status

Main controlled development branches:

```text
live-candidate
real-sell-supervised
```

Relevant milestones:

```text
v0.2.2-paper-best-profit
v0.2.3-guarded-slot-sell-groundwork
v0.2.4-sell-path-readiness
v0.2.5-preprod-checklist
v0.3.0-rc1-guarded-sell-reconciliation
v0.3.0-rc2-readonly-polling
v0.3.0-rc3-operational-slot-buy-guard
v0.3.0-rc4-production-readonly-candidate
v0.3.0-rc5-email-settings-table-views
v0.3.0-rc6-daily-email-report
v0.3.0-rc7-line-status-indicator
v0.3.0-rc8-readable-daily-report
v0.3.0-rc9-real-sell-supervised-runbook
v0.3.0-rc10-real-sell-one-shot-gate
v0.3.0-rc12-simulated-price-override
```

Current technical status:

```text
Micro real BUY validated                              ✅
Post-order reconciliation for real BUY OK             ✅
Real position_slots active                            ✅
First real OPEN slot                                  ✅
Slot-based SELL preview                               ✅
BEST_PROFIT diagnostics on real slots                 ✅
Isolated PAPER/SIM mode                               ✅
Paper BEST_PROFIT working                             ✅
Manual acknowledgement for last real order            ✅
HELIX_ALLOW_REAL_SLOT_SELL gate                       ✅
Real slot lookup for future SELL reconciliation        ✅
SELL reconciliation and slot close groundwork          ✅
Operational portfolio BUY guard / reserve logic        ✅
Reduced readonly polling                              ✅
Email report settings and delivery test               ✅
Daily email report                                    ✅
Table views in the View menu                          ✅
Line/API status indicator                             ✅
Readable daily report mode names                      ✅
Supervised real SELL runbook                          ✅
One-shot real SELL gate                               ✅
Simulated market price override for safe SELL testing  ✅
Real SELL execution still disabled                    ✅
```

Helix is therefore in a **controlled pre-production** state. It is not an autonomous production trading bot.

---

## What Helix can do now

Currently acceptable modes:

```text
SIMULATION              ✅
LIVE_READONLY           ✅
PAPER/SIM               ✅
Supervised micro-live BUY ✅ / ⚠️
```

Validated features:

- read state and settings;
- retrieve wallet status in readonly mode;
- retrieve Coinbase BTC-EUR spot price;
- perform Coinbase previews;
- execute a controlled micro real BUY;
- reconcile the real BUY;
- register the real BUY as an OPEN position slot;
- evaluate slot-based SELL previews;
- choose the best candidate slot with BEST_PROFIT;
- simulate paper BUY/SELL slot behavior;
- close paper slots;
- generate daily email reports;
- display data through GTK table views;
- test a simulated BTC-EUR price override without allowing real orders;
- prepare the future real slot closing flow after a properly reconciled real SELL.

---

## What Helix must NOT do yet

The current version must not:

```text
execute autonomous real SELL orders       ❌
sell the whole BTC wallet                 ❌
place multiple real orders in sequence    ❌
run 24/7 without supervision              ❌
automatically consume the EUR reserve     ❌
close real slots without FILLED status    ❌
trade using simulated market prices       ❌
```

The slot-based real SELL path remains intentionally gated and supervised.

---

## Project structure

```text
helix/
├── Makefile
├── Makefile.live
├── build_all.sh
├── README.md
├── .env.example
├── assets/
│   └── helix_dracula.css
├── docs/
│   ├── PRE_PROD_CHECKLIST.md
│   ├── PRODUCTION_READONLY_RUNBOOK.md
│   ├── REAL_SELL_SUPERVISED_RUNBOOK.md
│   ├── LEGACY_IMPORT_NOTES.md
│   ├── OVERNIGHT_LIVE_READONLY_RUN.md
│   └── release notes / checklist files
├── src/
│   ├── main.c
│   ├── config/
│   │   ├── env_loader.c
│   │   └── env_loader.h
│   ├── db/
│   │   ├── database.c
│   │   └── database.h
│   ├── engine/
│   │   ├── engine.c
│   │   ├── settings.c
│   │   ├── settings.h
│   │   ├── daily_report.c
│   │   ├── daily_report.h
│   │   ├── email_delivery.c
│   │   ├── email_delivery.h
│   │   ├── order_journal.c
│   │   ├── order_journal.h
│   │   ├── post_order_reconciliation.c
│   │   ├── post_order_reconciliation.h
│   │   ├── real_sell_supervision.c
│   │   ├── real_sell_supervision.h
│   │   ├── live_execution_lock.c
│   │   ├── live_execution_lock.h
│   │   ├── final_live_gate.c
│   │   ├── final_live_gate.h
│   │   ├── runtime_safety.c
│   │   ├── exchange_safety.c
│   │   ├── operational_limits.c
│   │   ├── risk_guard.c
│   │   ├── liquidity_reserve.c
│   │   ├── volatility_protection.c
│   │   ├── reconciliation.c
│   │   ├── anti_duplicate_order.c
│   │   └── wallet.c
│   ├── exchange/
│   │   ├── coinbase_client.c
│   │   ├── coinbase_client.h
│   │   ├── coinbase_auth.c
│   │   ├── coinbase_auth.h
│   │   ├── order_preview.c
│   │   ├── order_preview.h
│   │   ├── order_executor.c
│   │   └── order_executor.h
│   ├── market/
│   │   ├── market_data.c
│   │   └── market_data.h
│   ├── ui/
│   │   ├── window.c
│   │   └── window.h
│   └── wallet/
│       ├── wallet_info.c
│       └── wallet_info.h
└── data/
    └── helix.db       # local only, never commit
```

---

## Debian/Ubuntu package installation

This section prepares a Debian/Ubuntu machine to build and run Helix.

### Minimum build and runtime packages

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  make \
  pkg-config \
  git \
  zip unzip \
  sqlite3 \
  libsqlite3-dev \
  libgtk-4-dev \
  libcurl4-openssl-dev \
  libcjson-dev \
  libjwt-dev \
  ca-certificates
```

Package purpose:

```text
build-essential / make     -> C compiler and build tools
pkg-config                 -> resolves GTK4/library compiler flags
git                        -> repository management
zip / unzip                 -> create and inspect safe project archives
sqlite3 / libsqlite3-dev    -> local Helix database
libgtk-4-dev                -> GTK4 graphical interface
libcurl4-openssl-dev        -> HTTP calls to Coinbase
libcjson-dev                -> JSON parsing
libjwt-dev                  -> JWT/Coinbase authentication
ca-certificates             -> TLS/HTTPS certificates
```

### Email report packages

To use `Preferences -> Email report` and the `Test email delivery` button:

```bash
sudo apt install -y msmtp msmtp-mta
```

`msmtp-mta` provides a `sendmail`-compatible command. Helix uses by default:

```text
sendmail -t
```

### Optional development tools

```bash
sudo apt install -y gdb valgrind
```

These tools are not required for normal use, but are useful for debugging and memory checks.

### Installation checks

```bash
pkg-config --modversion gtk4
sqlite3 --version
gcc --version
which sendmail
```

---

## Build

Normal build:

```bash
make clean
make
```

Live candidate build:

```bash
make -f Makefile.live clean
make -f Makefile.live
```

Recommended full build:

```bash
./build_all.sh
```

`build_all.sh` builds both local binaries:

```text
helix
helix-live
```

These binaries are local artifacts and must not be committed.

---

## Environment configuration

Copy the example file:

```bash
cp .env.example .env
```

The `.env` file is local and must never be committed.

Global gates for real orders:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
```

Dedicated gate for future slot-based real SELL:

```env
HELIX_ALLOW_REAL_SLOT_SELL=false
```

During development, diagnostics, paper simulation and live-readonly runs, all real-order gates must remain `false`.

### Coinbase credentials

Coinbase credentials must be stored only in the local `.env` file:

```env
COINBASE_API_KEY=...
COINBASE_API_SECRET=...
```

Never paste real private keys into issues, commits, README files, screenshots, logs, ZIP archives, or chat messages. If a private key is accidentally exposed, revoke it immediately and create a new one.

---

## Dedicated gate for slot-based real SELL

Helix has a separate environment gate:

```env
HELIX_ALLOW_REAL_SLOT_SELL=false
```

This gate is intentionally separate from the global real-trading gates:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
```

Reason: real BUY and real SELL have different operational risks. Enabling a future real BUY path must not automatically enable real SELL.

Expected behavior:

```text
HELIX_ALLOW_REAL_SLOT_SELL=false
    -> slot-based real SELL is blocked by the environment gate

HELIX_ALLOW_REAL_SLOT_SELL=true
    -> the SELL path may become eligible, but it is still subject to all other gates,
       valid preview, one-shot supervision, reconciliation, and automatic stop
```

Expected audit entries when a slot is profitable but real SELL remains blocked:

```text
REAL_SLOT_SELL_PLAN
BLOCKED_ENV_GATE
```

---

## Email configuration on Debian/Ubuntu

Helix can send a test email and a daily status report through a local `sendmail`-compatible command. The recommended approach is `msmtp`; this avoids storing SMTP passwords in the application database or source code.

### Install packages

```bash
sudo apt update
sudo apt install msmtp msmtp-mta ca-certificates
```

### Gmail configuration

For Gmail, do not use the normal account password. Use a Google **App Password**.

Indicative path:

```text
Google Account -> Security -> 2-Step Verification -> App passwords
```

Create a local file:

```bash
nano ~/.msmtprc
```

Example:

```text
defaults
auth           on
tls            on
tls_trust_file /etc/ssl/certs/ca-certificates.crt
logfile        ~/.msmtp.log

account gmail
host smtp.gmail.com
port 587
from your-email@gmail.com
user your-email@gmail.com
password GOOGLE_APP_PASSWORD

account default : gmail
```

Protect it:

```bash
chmod 600 ~/.msmtprc
```

> ⚠️ Never commit `~/.msmtprc`, never copy it into `docs/`, never include it in ZIP files, and never write the app password in README, database, or `.env`.

### Manual terminal test

```bash
printf "To: your-email@gmail.com\nSubject: Helix manual msmtp test\n\nManual email delivery test from Helix via msmtp.\n" | sendmail -v -t
echo "exit_code=$?"
```

Expected result:

```text
exit_code=0
```

If it fails:

```bash
tail -40 ~/.msmtp.log
```

Typical errors:

```text
account default not found
```

`~/.msmtprc` is missing or does not contain `account default`.

```text
Application-specific password required
```

A normal Gmail password was used instead of an app password.

### Helix UI configuration

In the UI:

```text
Preferences -> Email report
```

Suggested values:

```text
Daily email enabled: 1
Email recipient: your-email@gmail.com
Email report hour: 12
Email report minute: 0
Email send command: sendmail -t
```

Then press:

```text
Test email delivery
```

A successful test should show `smtpstatus=250` and `exitcode=EX_OK` in `~/.msmtp.log`.

---

## SQLite database

Local database:

```text
data/helix.db
```

The database must not be committed.

Main tables:

```text
settings
bot_state
trades
engine_audit
order_state
order_journal
position_slots
paper_position_slots
```

### `position_slots`

Contains real slots created from real Coinbase BUY orders.

Main fields:

```text
id
buy_order_id
buy_client_order_id
base_size_btc
cost_eur
buy_fee_eur
avg_buy_price
status
opened_at
closed_at
sell_order_id
sell_net_eur
realized_profit_eur
```

Main statuses:

```text
OPEN
CLOSED
```

A real slot may be closed only after a future real SELL has been correctly reconciled.

### `paper_position_slots`

Contains fake slots used only by PAPER/SIM. This table must never be mixed with real `position_slots`.

---

## Real micro BUY flow

Validated flow:

```text
1. Coinbase BUY preview
2. exchange safety
3. runtime safety
4. final live gate
5. live execution lock
6. Coinbase create-order
7. order_journal REAL_SENT
8. post-order reconciliation
9. POST_ORDER_RECON_OK
10. creation of OPEN position_slots
11. STOP_AFTER_REAL_ORDER
12. manual acknowledgement
```

The first real micro BUY was completed and reconciled successfully.

---

## Slot-based SELL preview flow

Helix no longer evaluates selling the whole BTC wallet. The current diagnostic flow is:

```text
1. read OPEN position_slots
2. calculate SELL preview for every slot
3. calculate gross, fee, net, cost, EUR profit and profit percentage
4. choose the best slot with BEST_PROFIT
5. block if the best slot is not profitable
6. if profitable, prepare a blocked/supervised real SELL plan
7. do not send a real SELL order
```

Typical decisions:

```text
SELL_SLOT_PREVIEW_EVALUATED
BEST_PROFIT_NOT_PROFITABLE
BEST_PROFIT_PROFITABLE_PREVIEW_ONLY
REAL_SLOT_SELL_ONE_SHOT_GATE
REAL_SLOT_SELL_PLAN
BLOCKED_ENV_GATE
```

---

## PAPER/SIM

PAPER/SIM is isolated from the real trading path.

UI buttons:

```text
Seed paper slots demo
Clear paper slots
Run paper BEST_PROFIT
```

`Seed paper slots demo` creates three demonstration paper slots:

```text
paper-slot-a
paper-slot-b
paper-slot-c
```

`Run paper BEST_PROFIT`:

```text
evaluates all OPEN paper slots
calculates gross, fee, net, EUR profit and profit percentage
chooses the best slot
closes the paper slot if thresholds are met
writes PAPER_SIM audit entries
```

---

## SELL reconciliation and real slot close

Groundwork for future reconciled real SELL is prepared.

Desired future flow:

```text
1. real SELL is sent only for the selected slot
2. Coinbase returns an order_id
3. order_journal records REAL_SENT
4. post_order_reconciliation reads the order status
5. only FILLED / SETTLED / DONE / 100% completion is accepted
6. Coinbase wallet is checked
7. the matching OPEN slot is found
8. sell_net_eur = preview_total_eur - preview_fee_eur
9. realized_profit_eur = sell_net_eur - cost_eur
10. db_close_position_slot(...) closes the slot
11. SLOT_CLOSE_RECONCILIATION audit is written
12. STOP_AFTER_REAL_ORDER
13. manual acknowledgement
```

Expected audit:

```text
SLOT_CLOSE_RECONCILIATION
SLOT_CLOSED_AFTER_SELL_RECON
```

This logic is preparatory. Real SELL remains gated until the controlled live path is completed.

---

## Manual acknowledgement for real orders

After a real order, Helix requires manual acknowledgement.

Setting used:

```text
micro_live.last_real_order_acknowledged
```

UI control:

```text
Acknowledge last real order
```

Acknowledgement prevents Helix from continuing after a real order without human review.

---

## Main safety gates

Helix uses multiple layers of protection:

```text
runtime_mode
emergency_stop
live_trading_armed
micro_live_enabled
micro_live_max_order_eur
max_orders_per_day
order_cooldown_seconds
liquidity_reserve_percent
micro_live_stop_after_real_order
HELIX_REAL_TRADING_ENABLED
HELIX_ALLOW_COINBASE_ORDERS
HELIX_I_UNDERSTAND_REAL_MONEY_RISK
HELIX_ALLOW_REAL_SLOT_SELL
HELIX_ENABLE_REAL_COINBASE_ORDERS
final_live_gate
live_execution_lock
post_order_reconciliation
real_sell_supervision one-shot gate
```

For controlled micro-live:

```text
max_orders_per_day = 1
micro_live_stop_after_real_order = 1
liquidity_reserve_percent >= 80
micro_live_max_order_eur <= 10 recommended
```

---

## Controlled production rc10+ status

The `real-sell-supervised` branch is dedicated to the first supervised real SELL test.

### Branches and tags

- `live-candidate`: stable observational-production candidate.
- `real-sell-supervised`: branch for the supervised real SELL path.
- `v0.3.0-rc6-daily-email-report`: automatic daily email report.
- `v0.3.0-rc7-line-status-indicator`: `Line/API` dashboard indicator.
- `v0.3.0-rc8-readable-daily-report`: readable engine mode in daily email.
- `v0.3.0-rc9-real-sell-supervised-runbook`: operational runbook for supervised real SELL.
- `v0.3.0-rc10-real-sell-one-shot-gate`: one-shot gate allowing at most one real SELL in the supervised test.
- `v0.3.0-rc12-simulated-price-override`: simulated BTC-EUR price override for safe SELL-path testing.

### Expected behavior in LIVE_READONLY

In `LIVE_READONLY`, Helix should:

1. read wallet and BTC-EUR price;
2. synchronize wallet status with the BTC position;
3. evaluate real OPEN slots in `position_slots`;
4. choose the best SELL candidate with BEST_PROFIT;
5. block BUY if operational capital is exhausted;
6. write diagnostic audit entries in `engine_audit`;
7. send daily email reports if configured;
8. show Line/API status in the UI;
9. never send real orders.

### Slot rule

Helix reasons in slots both when buying and when selling. Slots are based on operational capital, not the entire account value. Operational capital means total capital minus protected reserve. This prevents Helix from consuming all liquidity during accumulation.

Real SELL must be slot-based: Helix must not sell the whole BTC wallet if it cannot find coherent OPEN slots in `position_slots`.

### Minimum conditions for supervised real SELL consideration

A supervised real SELL may be considered only if all of the following are true:

- branch `real-sell-supervised` is clean and up to date;
- live build compiled successfully;
- `HELIX_SIMULATED_MARKET_PRICE_ENABLED=0`;
- `HELIX_ALLOW_REAL_SLOT_SELL=true`;
- runtime is `LIVE_TRADING`;
- `live_trading_armed=1`;
- `micro_live_stop_after_real_order=1`;
- `max_orders_per_day=1`;
- no recent Line/API error;
- Coinbase preview is valid;
- `SELL_SLOT_SELECTION = BEST_PROFIT_PROFITABLE_PREVIEW_ONLY`;
- `REAL_SLOT_SELL_ONE_SHOT_GATE = ALLOWED_PRE_EXECUTION`;
- no real SELL has already been sent today;
- no slot has already been closed today by a real SELL.

If one condition is missing, Helix must remain in observation mode or block the SELL.

### One-shot gate

The one-shot gate does not decide whether a price is profitable. Profitability is determined earlier by BEST_PROFIT. The gate checks whether a real SELL would be authorized once, under controlled conditions.

Expected gate decisions:

```text
ALLOWED_PRE_EXECUTION
BLOCKED_ENV_GATE
BLOCKED_NOT_LIVE_TRADING
BLOCKED_NOT_ARMED
BLOCKED_ALREADY_SENT_TODAY
BLOCKED_SLOT_ALREADY_CLOSED_TODAY
BLOCKED_AUDIT_UNAVAILABLE
BLOCKED_SLOT_AUDIT_UNAVAILABLE
```

---

## Simulated market price test

Helix can run in observational mode with a simulated BTC-EUR price to test the SELL decision chain without waiting for the real market to reach the target price.

This mode is only for controlled logic and audit testing. It must never be used for real trading.

Local `.env` configuration:

```env
HELIX_SIMULATED_MARKET_PRICE_ENABLED=1
HELIX_SIMULATED_MARKET_PRICE_EUR=73000
```

Safety rules:

- works only in `SIMULATION` or `LIVE_READONLY`;
- in `LIVE_TRADING`, Helix blocks the cycle and records `MARKET_PRICE_SIMULATION / BLOCKED_LIVE_TRADING`;
- slot-based SELL previews become simulated and do not rely on Coinbase for the simulated price;
- audit entries clearly say `SELL preview SIMULATA`;
- any `REAL_SLOT_SELL_PLAN` remains gated and not executed unless all real live gates are explicitly enabled in a future controlled test.

Useful test prices for the closest slot:

```text
66500  -> near break-even
70000  -> positive profit, thresholds to verify
73000  -> expected SELL plan / one-shot gate scenario
```

Always disable this after testing:

```env
HELIX_SIMULATED_MARKET_PRICE_ENABLED=0
HELIX_SIMULATED_MARKET_PRICE_EUR=0
```

---

## Useful commands

Full build:

```bash
./build_all.sh
```

Git status:

```bash
git status
git log --oneline -8
git tag --list "v0.3.*"
```

Database backup:

```bash
mkdir -p data/backups
cp data/helix.db "data/backups/helix_backup_$(date +%Y%m%d_%H%M%S).db"
```

Check real slots:

```bash
sqlite3 -header -column data/helix.db "
SELECT id,buy_order_id,buy_client_order_id,base_size_btc,cost_eur,buy_fee_eur,
       avg_buy_price,status,opened_at,closed_at,sell_order_id,
       sell_net_eur,realized_profit_eur
FROM position_slots
ORDER BY id;
"
```

Check latest orders:

```bash
sqlite3 -header -column data/helix.db "
SELECT id,client_order_id,side,dry_run,status,phase,decision,
       requested_base_size,preview_total_eur,preview_fee_eur,
       preview_base_size,preview_avg_price,reason,created_at
FROM order_journal
ORDER BY id DESC
LIMIT 20;
"
```

Check SELL audit:

```bash
sqlite3 -header -column data/helix.db "
SELECT created_at,event_type,decision,reason,eur_amount,estimated_fee,net_profit
FROM engine_audit
WHERE event_type IN (
  'SELL_SLOT_SELECTION',
  'REAL_SLOT_SELL_PLAN',
  'REAL_SLOT_SELL_ONE_SHOT_GATE',
  'EXCHANGE_SAFETY_SELL',
  'SLOT_CLOSE_RECONCILIATION'
)
ORDER BY id DESC
LIMIT 40;
"
```

Check simulated price audit:

```bash
sqlite3 -header -column data/helix.db "
SELECT created_at,event_type,decision,reason
FROM engine_audit
WHERE event_type='MARKET_PRICE_SIMULATION'
ORDER BY id DESC
LIMIT 10;
"
```

---

## Safe ZIP creation

Never include `.env`, `data/`, `.git/`, binaries, logs, or temporary reports.

Recommended command:

```bash
zip -r "../helix_state_$(date +%Y%m%d_%H%M%S).zip" . \
  -x ".git/*" \
  -x "data/*" \
  -x ".env" \
  -x ".env.*" \
  -x "helix" \
  -x "helix-live" \
  -x "*.o" \
  -x "*.log" \
  -x "*.txt" \
  -x "*~" \
  -x "*.zip"
```

---

## Dracula GTK4 theme

Helix includes a local GTK4 Dracula-inspired theme.

Theme file:

```text
assets/helix_dracula.css
```

The theme customizes:

- main window background;
- dashboard labels;
- Line/API online/offline colors;
- buttons;
- input fields;
- dropdowns;
- tables and lists;
- warning/error/success labels;
- action notification dialogs.

The theme is local to the project and does not require installing a system-wide GTK theme.

---

## Funding

Helix is a personal project developed in C/GTK4 with the goal of building a cautious, observable and controlled trading engine.

If you find the project useful or want to support its development, you can donate through PayPal:

```text
https://www.paypal.com/paypalme/lucamezzolla82
```

Your support helps maintain the project, improve documentation, add tests, strengthen safety checks, improve the UI and continue the controlled production-readiness work.

Thank you for every contribution.

---

## Final rule

Helix can become productive only when every real step is:

```text
explicit
small
tracked
reconciled
operationally reversible
stopped after execution
manually acknowledged by the user
```

Until then, Helix remains a controlled technical pre-production release.
