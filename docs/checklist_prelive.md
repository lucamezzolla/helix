# Helix pre-live checklist

Milestone:

```text
v0.2.0-prelive
```

This checklist is for validating Helix before any real-money build is considered.

Helix must remain in normal safe build mode during this checklist. The normal Makefile must not include:

```text
-DHELIX_ENABLE_REAL_COINBASE_ORDERS
```

---

## 1. Repository safety

Before running or pushing:

```bash
rm -f helix
rm -f data/*.json
rm -f data/*.log
rm -f data/*.txt
rm -f data/*.db-shm
rm -f data/*.db-wal
git status
```

Confirm that the following files are not staged:

- `.env`
- local database files
- generated reports
- personal financial history
- local archives
- compiled binaries

---

## 2. Build check

Run:

```bash
make clean
make
```

Expected result:

- build completes
- no compiler errors
- no unexpected warnings

Run:

```bash
make run
```

Expected result:

- Helix opens
- no GTK critical warnings
- menus work
- dashboard refreshes

---

## 3. SIMULATION mode test

Set runtime mode to:

```text
SIMULATION
```

Expected result:

- bot can start and stop
- dashboard updates
- no Coinbase trading call is made
- audit stays capped and readable
- reports open correctly

Check:

- `File -> Start bot`
- `File -> Stop bot`
- `Visualizza -> Report pre-live`
- `Visualizza -> Stato protezioni`
- `Visualizza -> Esporta report pre-live`
- `Visualizza -> Esporta snapshot stato`

---

## 4. LIVE_READONLY mode test

Set runtime mode to:

```text
LIVE_READONLY
```

Use a Coinbase/CDP key with View-only permissions.

Expected result:

- BTC-EUR price is read
- EUR balance is read
- BTC balance is read
- order preview may run when appropriate
- no real order is created
- real executor remains blocked

---

## 5. LIVE_TRADING safety test

Set runtime mode to:

```text
LIVE_TRADING
```

Do not compile with the real-order compile flag.

Expected result:

- real execution remains blocked
- pre-live report says not ready unless requirements are satisfied
- live execution lock blocks real orders
- final live gate blocks unsafe paths

Test:

- LIVE_TRADING not armed -> blocked
- LIVE_TRADING armed -> still blocked by normal build
- kill-switch active -> blocked
- missing `.env` future flags -> blocked
- pre-live validation incomplete -> blocked

---

## 6. Kill-switch test

Use:

```text
Attiva kill-switch
```

Expected result:

- bot stops
- BUY/SELL paths are blocked
- status updates
- audit records the event
- readiness shows a blocking condition

Then use:

```text
Reset kill-switch
```

Expected result:

- kill-switch resets
- bot remains stopped until started manually

---

## 7. Liquidity reserve test

Verify:

- protected reserve percentage is respected
- reserve slots are not used automatically
- manual release increases available reserve slots
- manual lock decreases available reserve slots
- BUY is blocked when operational liquidity is insufficient

---

## 8. Operational limits test

Configure conservative values:

```text
Max orders per day: 1
Cooldown seconds: 300
```

Expected result:

- repeated dry-run attempts are blocked by cooldown or daily limit
- report shows operational-limit blocks if triggered

---

## 9. Risk guard test

Configure conservative values:

```text
Max daily loss EUR
Max drawdown %
```

Expected result:

- risk guard blocks unsafe operation attempts
- report shows risk-guard status
- audit remains capped

---

## 10. Volatility protection test

Configure:

```text
Volatility window seconds
Volatility max movement %
```

Expected result:

- fast price movement blocks operation attempts
- report shows volatility-related blocks if triggered

---

## 11. Dry-run validation target

Before considering a real-money candidate branch, Helix should record at least:

```text
20 valid dry-runs in the last 7 days
```

Check:

```text
Visualizza -> Report pre-live
```

and export:

```text
data/prelive_report.txt
data/status_snapshot.txt
```

Review:

- dry-run count
- final gate OK count
- final gate blocked count
- safety blocks by category
- latest journal block
- readiness status
- API health status

---

## 12. Manual review before live-candidate branch

Before creating a live-candidate branch, confirm:

- no unexpected safety blocks
- no audit spam
- audit cap works
- wallet balances are coherent
- fills reconstruction is coherent when available
- Coinbase previews are coherent
- no duplicate-order attempts appear
- order journal is readable
- reports and snapshots are understandable

Only after this checklist passes should a separate branch be considered:

```bash
git checkout -b live-candidate
```

Do not enable real trading on the development branch.
