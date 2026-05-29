# Helix

Helix è un'applicazione desktop scritta in **C** con interfaccia **GTK4**. Il progetto nasce come motore prudente di trading su **BTC-EUR** collegato a Coinbase, sviluppato con una regola fondamentale: prima la sicurezza, poi l'automazione.

Helix non è pensato per operare in modo aggressivo o autonomo senza controllo umano. L'obiettivo della versione attuale è arrivare a una **produzione controllata**, con micro-importi, uno slot alla volta, stop automatico dopo ogni ordine reale e riconciliazione obbligatoria.

> ⚠️ Helix può interagire con denaro reale. Prima di qualunque test live bisogna verificare codice, database, configurazione `.env`, log, journal, slot aperti e stato Coinbase.

---

## Principi del progetto

Helix segue alcune regole rigide:

- non compra mai usando tutto il saldo disponibile;
- non vende mai l'intero wallet BTC;
- compra e vende a **slot/lotti**;
- ogni BUY reale deve aprire uno slot reale;
- ogni SELL deve valutare solo slot aperti;
- una SELL deve chiudere solo lo slot scelto, non il saldo globale;
- la riserva EUR resta protetta;
- gli slot devono essere calcolati sul **capitale operativo**, non sul 100% del capitale totale;
- lo storico legacy deve essere importato o trattato esplicitamente come lotti legacy;
- ogni ordine reale deve essere tracciato in `order_journal`;
- ogni ordine reale deve essere riconciliato dopo Coinbase;
- dopo ogni ordine reale Helix deve fermarsi;
- l'utente deve fare acknowledge manuale prima di procedere.

La regola d'oro è:

```text
Meglio non fare nulla che fare un ordine ambiguo.
```

---

## Stato attuale

Branch principale di sviluppo controllato:

```bash
live-candidate
```

Versione corrente:

```text
0.3.0-rc3
```

Ultime milestone rilevanti:

```text
v0.3.0-rc1-guarded-sell-reconciliation
v0.3.0-rc2-readonly-polling
v0.3.0-rc3-operational-slot-buy-guard
```

Stato tecnico attuale:

```text
BUY reale micro validato                         ✅
Post-order reconciliation BUY reale OK           ✅
position_slots reale attiva                      ✅
Import legacy Excel in position_slots            ✅
Import legacy Excel in trades                    ✅
used_slots derivato dagli slot OPEN              ✅
SELL preview per slot                            ✅
BEST_PROFIT diagnostico su slot reali            ✅
PAPER/SIM isolato                                ✅
paper BEST_PROFIT funzionante                    ✅
Acknowledge ultimo ordine reale                  ✅
Polling readonly ridotto                         ✅
Gate HELIX_ALLOW_REAL_SLOT_SELL                  ✅
Lookup slot reale per futura SELL reconciliation ✅
SELL reconciliation con chiusura slot preparata  ✅
Operational slot BUY guard                       ✅
SELL reale ancora disabilitata                   ✅
```

Helix è quindi in stato **release candidate per produzione controllata readonly**, non ancora in produzione autonoma.

---

## Cosa Helix può fare ora

Modalità attualmente considerate accettabili:

```text
SIMULATION              ✅
LIVE_READONLY           ✅
PAPER/SIM               ✅
Micro-live BUY vigilato ✅ / ⚠️
```

Funzionalità validate:

- leggere stato e impostazioni;
- recuperare saldo/wallet in readonly;
- fare preview Coinbase;
- eseguire un micro BUY reale controllato;
- riconciliare il BUY reale;
- registrare il BUY reale come slot;
- importare lotti legacy da storico esterno;
- valutare SELL preview per slot;
- scegliere lo slot migliore con BEST_PROFIT;
- simulare BUY/SELL paper;
- chiudere slot paper;
- ridurre il polling in LIVE_READONLY;
- bloccare nuovi BUY quando il capitale operativo risulta già consumato dagli slot aperti;
- preparare il percorso di chiusura slot reale dopo futura SELL riconciliata.

---

## Cosa Helix NON deve ancora fare

La versione attuale **non deve**:

```text
eseguire SELL reale autonomamente       ❌
vendere tutto il wallet BTC             ❌
fare più ordini reali consecutivi       ❌
operare 24/7 senza supervisione         ❌
usare automaticamente la riserva EUR    ❌
chiudere slot reali senza FILLED        ❌
trattare il wallet BTC aggregato come un lotto unico ❌
```

La SELL reale slot-based è ancora intenzionalmente bloccata.

---

## Struttura del progetto

Struttura principale:

```text
helix/
├── Makefile
├── Makefile.live
├── build_all.sh
├── README.md
├── VERSION
├── .env.example
├── docs/
│   ├── LEGACY_IMPORT_NOTES.md
│   ├── OVERNIGHT_LIVE_READONLY_RUN.md
│   ├── PRE_PROD_CHECKLIST.md
│   ├── RELEASE_v0.3.0-rc1.md
│   ├── checklist_prelive.md
│   └── release_notes_v0.2.0-prelive.md
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
│   │   ├── bot_state.c
│   │   ├── bot_state.h
│   │   ├── order_journal.c
│   │   ├── order_journal.h
│   │   ├── post_order_reconciliation.c
│   │   ├── post_order_reconciliation.h
│   │   ├── live_execution_lock.c
│   │   ├── live_execution_lock.h
│   │   ├── final_live_gate.c
│   │   ├── final_live_gate.h
│   │   ├── runtime_safety.c
│   │   ├── runtime_safety.h
│   │   ├── exchange_safety.c
│   │   ├── exchange_safety.h
│   │   ├── operational_limits.c
│   │   ├── operational_limits.h
│   │   ├── risk_guard.c
│   │   ├── risk_guard.h
│   │   ├── liquidity_reserve.c
│   │   ├── liquidity_reserve.h
│   │   ├── volatility_protection.c
│   │   ├── volatility_protection.h
│   │   ├── reconciliation.c
│   │   ├── reconciliation.h
│   │   ├── anti_duplicate_order.c
│   │   ├── anti_duplicate_order.h
│   │   ├── emergency_stop.c
│   │   ├── emergency_stop.h
│   │   ├── api_health.c
│   │   ├── api_health.h
│   │   ├── live_readiness.c
│   │   ├── live_readiness.h
│   │   ├── prelive_validation.c
│   │   ├── prelive_validation.h
│   │   ├── trade_preview.c
│   │   ├── trade_preview.h
│   │   ├── order_state_recovery.c
│   │   ├── order_state_recovery.h
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
│   │   └── market_data.c
│   ├── ui/
│   │   ├── window.c
│   │   └── window.h
│   └── wallet/
│       ├── wallet_info.c
│       └── wallet_info.h
└── data/
    └── helix.db       # locale, non committare
```

---

## Installazione pacchetti

Su Debian/Ubuntu installare gli strumenti di compilazione e le librerie richieste:

```bash
sudo apt update

sudo apt install -y \
  build-essential \
  pkg-config \
  libgtk-4-dev \
  libsqlite3-dev \
  libcurl4-openssl-dev \
  libcjson-dev \
  libjwt-dev \
  sqlite3 \
  zip \
  unzip
```

Per verificare GTK4:

```bash
pkg-config --cflags gtk4
pkg-config --libs gtk4
```

Se `pkg-config` non trova `gtk4`, il pacchetto `libgtk-4-dev` non è installato correttamente.

---

## Build

Build normale:

```bash
make clean
make
```

Build live candidate:

```bash
make -f Makefile.live clean
make -f Makefile.live
```

Build completa consigliata:

```bash
./build_all.sh
```

`build_all.sh` esegue:

```bash
make clean
make
make -f Makefile.live clean
make -f Makefile.live
```

I binari generati (`helix`, `helix-live`) non devono essere committati.

---

## Avvio

Build normale:

```bash
./helix
```

Build live candidate:

```bash
./helix-live
```

Prima di qualunque run reale, verificare sempre dalla UI:

```text
Runtime mode
Emergency stop
Live trading armed
Coinbase credentials
Pre-live status
API health
```

Per la fase attuale è consigliato usare:

```text
LIVE_READONLY
```

---

## Configurazione ambiente

Copiare l'esempio:

```bash
cp .env.example .env
```

Il file `.env` è locale e non deve essere committato.

Gate globali per ordini reali:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
```

Gate dedicato per la futura SELL reale slot-based:

```env
HELIX_ALLOW_REAL_SLOT_SELL=false
```

Durante sviluppo, diagnostica, paper e live-readonly i gate devono restare `false`.

---

## Gate dedicato per SELL reale slot-based

Helix introduce un gate ambiente separato:

```env
HELIX_ALLOW_REAL_SLOT_SELL=false
```

Questo gate è separato dai tre gate globali di real trading:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
```

Motivo: BUY reale e SELL reale hanno rischi operativi diversi. Una futura abilitazione della BUY reale non deve rendere automaticamente disponibile anche la SELL reale.

Comportamento previsto:

```text
HELIX_ALLOW_REAL_SLOT_SELL=false
    -> SELL reale slot-based bloccata dal gate ambiente

HELIX_ALLOW_REAL_SLOT_SELL=true
    -> percorso SELL può diventare pronto, ma resta soggetto a tutti gli altri gate,
       alla preview valida, alla reconciliation e allo stop automatico
```

Audit attesi quando lo slot sarà profittevole ma la SELL reale resterà bloccata:

```text
REAL_SLOT_SELL_PLAN
REAL_SLOT_SELL_BLOCKED_BY_ENV_GATE
```

Oppure, se il gate è true ma l'esecuzione resta non abilitata:

```text
REAL_SLOT_SELL_PLAN
REAL_SLOT_SELL_READY_BUT_NOT_EXECUTED
```

---

## Database SQLite

Database locale:

```text
data/helix.db
```

Il database non deve essere committato.

Tabelle principali:

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

Contiene gli slot/lotti reali conosciuti da Helix.

Possono derivare da:

```text
BUY reali eseguiti da Helix
import legacy da storico esterno
```

Campi principali:

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

Stati principali:

```text
OPEN
CLOSED
```

`position_slots` è la fonte primaria per la gestione a lotti. Uno slot reale può essere chiuso solo dopo futura SELL reale riconciliata correttamente.

### `trades`

Contiene lo storico leggibile delle operazioni, inclusi import legacy.

Esempio legacy:

```text
type = BUY
source = LEGACY_EXCEL
reference = LEGACY-...
```

`trades` non deve guidare la strategia. La strategia usa `position_slots`.

### `order_journal`

Contiene preview, piani, invii reali, blocchi, reconciliation e stati di esecuzione.

Per ordini reali è la fonte operativa principale.

### `engine_audit`

Contiene le decisioni del motore.

Esempi:

```text
SELL_SLOT_SELECTION
EXCHANGE_SAFETY_SELL
REAL_SLOT_SELL_PLAN
PORTFOLIO_SLOT_BUY
PORTFOLIO_SLOT_BUY_PREVIEW
SLOT_CLOSE_RECONCILIATION
```

### `paper_position_slots`

Contiene slot fittizi usati solo per PAPER/SIM.

Non deve mai essere mischiata con `position_slots` reale.

---

## Import legacy

Lo storico precedente a Helix può essere importato come lotti legacy.

Stato attuale locale dopo import:

```text
5 BUY legacy inseriti in position_slots
5 BUY legacy inseriti in trades
1 BUY reale Helix in position_slots
6 slot OPEN totali
```

Marcatori usati:

```text
source = LEGACY_EXCEL
buy_order_id prefix = LEGACY-
buy_client_order_id prefix = legacy-excel-
```

Dopo import, `used_slots` deve essere coerente con gli slot `OPEN` in `position_slots`.

Documento dedicato:

```text
docs/LEGACY_IMPORT_NOTES.md
```

---

## Logica slot e riserva

Helix deve ragionare a slot sia in acquisto sia in vendita.

Formula strategica:

```text
total_capital_eur = eur_balance + btc_balance * current_price
reserve_eur = total_capital_eur * reserve_percent / 100
operational_capital_eur = total_capital_eur - reserve_eur
slot_size_eur = operational_capital_eur / max_slots
```

Regola:

```text
gli slot normali consumano capitale operativo
la riserva crisi non viene usata automaticamente
gli slot riserva possono essere sbloccati solo manualmente
```

### Operational slot BUY guard

Da `v0.3.0-rc3`, Helix controlla anche il capitale già allocato negli slot aperti:

```text
allocated_cost_eur = SUM(cost_eur + buy_fee_eur) degli slot OPEN
```

Un nuovo BUY viene bloccato se:

```text
allocated_cost_eur + buy_amount_eur > operational_capital_eur + reserve_released_eur
```

Questa regola impedisce a Helix di comprare solo perché vede EUR disponibili, quando il capitale operativo è già consumato dai lotti aperti.

Audit attesi:

```text
PORTFOLIO_SLOT_BUY
PORTFOLIO_SLOT_BUY_PREVIEW
```

---

## Flusso BUY reale micro

Flusso validato:

```text
1. BUY preview Coinbase
2. exchange safety
3. runtime safety
4. portfolio slot BUY guard
5. final live gate
6. live execution lock
7. create-order Coinbase
8. order_journal REAL_SENT
9. post-order reconciliation
10. POST_ORDER_RECON_OK
11. creazione position_slots OPEN
12. STOP_AFTER_REAL_ORDER
13. acknowledge manuale
```

Il primo BUY reale micro è stato completato e riconciliato con successo.

---

## Flusso SELL preview per slot

Helix non valuta più la vendita dell'intero wallet BTC.

Flusso diagnostico attuale:

```text
1. legge position_slots OPEN
2. per ogni slot calcola preview SELL Coinbase
3. calcola gross, fee, net, cost, profit EUR, profit %
4. sceglie lo slot migliore con BEST_PROFIT
5. se lo slot non è profittevole blocca
6. se lo slot è profittevole prepara audit/piano bloccato
7. non invia SELL reale
```

Decisioni tipiche:

```text
SELL_SLOT_PREVIEW_EVALUATED
BEST_PROFIT_NOT_PROFITABLE
SELL_SLOT_NOT_PROFITABLE
REAL_SLOT_SELL_BLOCKED_BY_ENV_GATE
REAL_SLOT_SELL_READY_BUT_NOT_EXECUTED
```

---

## PAPER/SIM

PAPER/SIM è isolato dalla parte reale.

Pulsanti UI:

```text
Seed paper slots demo
Clear paper slots
Run paper BEST_PROFIT
```

`Seed paper slots demo` crea tre slot paper dimostrativi:

```text
paper-slot-a
paper-slot-b
paper-slot-c
```

`Run paper BEST_PROFIT`:

```text
valuta tutti gli slot paper OPEN
calcola gross, fee, net, profit EUR, profit %
sceglie lo slot migliore
chiude lo slot paper se supera le soglie
scrive audit PAPER_SIM
```

Test validato:

```text
paper-slot-c scelto come migliore
paper-slot-c chiuso come CLOSED
realized_profit_eur valorizzato
```

---

## LIVE_READONLY e polling

In `v0.3.0-rc2` il polling è stato ridotto per rendere Helix più adatto a restare acceso in osservazione.

Obiettivo:

```text
UI/engine tick meno frequente
preview Coinbase ogni circa 10 minuti
meno CPU
meno rumore nel DB
```

La modalità consigliata per osservazione è:

```text
LIVE_READONLY
```

Questa modalità deve leggere mercato e wallet, ma non deve inviare ordini reali.

---

## Reconciliation SELL e chiusura slot

Il groundwork per la futura SELL reale riconciliata è stato preparato.

Flusso desiderato per una futura SELL reale:

```text
1. SELL reale inviata solo su slot scelto
2. Coinbase restituisce order_id
3. order_journal registra REAL_SENT
4. post_order_reconciliation legge stato ordine
5. accetta solo FILLED / SETTLED / DONE / completion 100%
6. legge wallet Coinbase
7. trova slot OPEN compatibile con requested_base_size
8. calcola sell_net_eur = preview_total_eur - preview_fee_eur
9. calcola realized_profit_eur = sell_net_eur - cost_eur
10. chiude position_slots con db_close_position_slot(...)
11. scrive audit SLOT_CLOSE_RECONCILIATION
12. STOP_AFTER_REAL_ORDER
13. acknowledge manuale
```

Audit previsto:

```text
SLOT_CLOSE_RECONCILIATION
SLOT_CLOSED_AFTER_SELL_RECON
```

Questa logica è preparatoria: la SELL reale resta ancora disabilitata finché non viene completato il percorso live controllato.

---

## Acknowledge ordine reale

Dopo un ordine reale Helix richiede acknowledgement manuale.

Impostazione usata:

```text
micro_live.last_real_order_acknowledged
```

La UI contiene il controllo:

```text
Acknowledge ultimo ordine reale
```

L'acknowledge serve a impedire che Helix prosegua dopo un ordine reale senza revisione umana.

---

## Safety gates principali

Helix usa più livelli di blocco:

```text
runtime_mode
emergency_stop
live_trading_armed
micro_live_enabled
micro_live_max_order_eur
max_orders_per_day
order_cooldown_seconds
liquidity_reserve_percent
reserve_released_slots
micro_live_stop_after_real_order
portfolio_operational_buy_guard
HELIX_REAL_TRADING_ENABLED
HELIX_ALLOW_COINBASE_ORDERS
HELIX_I_UNDERSTAND_REAL_MONEY_RISK
HELIX_ALLOW_REAL_SLOT_SELL
HELIX_ENABLE_REAL_COINBASE_ORDERS
final_live_gate
live_execution_lock
post_order_reconciliation
```

Per micro-live controllato:

```text
max_orders_per_day = 1
micro_live_stop_after_real_order = 1
liquidity_reserve_percent >= 80
micro_live_max_order_eur <= 10 consigliato
```

---

## Checklist pre-produzione

Documento dedicato:

```text
docs/PRE_PROD_CHECKLIST.md
```

Prima di qualunque test reale:

```text
git status pulito
build_all.sh senza warning
backup data/helix.db
.env verificato
HELIX_ALLOW_REAL_SLOT_SELL=false
STOP_AFTER_REAL_ORDER=1
max_orders_per_day=1
micro_live_max_order_eur <= 10
liquidity_reserve_percent >= 80
position_slots controllata
order_journal controllato
engine_audit controllato
```

---

## Comandi utili

Build completa:

```bash
./build_all.sh
```

Stato Git:

```bash
git status
git log --oneline -10
git tag --list "v0.3.*"
```

Backup DB:

```bash
mkdir -p data/backups
cp data/helix.db "data/backups/helix_backup_$(date +%Y%m%d_%H%M%S).db"
```

Controllo slot reali:

```bash
sqlite3 -header -column data/helix.db "
SELECT id,buy_order_id,buy_client_order_id,base_size_btc,cost_eur,buy_fee_eur,
       cost_eur + buy_fee_eur AS allocated_cost,
       avg_buy_price,status,opened_at,closed_at,sell_order_id,
       sell_net_eur,realized_profit_eur
FROM position_slots
ORDER BY opened_at ASC, id ASC;
"
```

Totale slot aperti:

```bash
sqlite3 -header -column data/helix.db "
SELECT
  COUNT(*) AS open_slots,
  SUM(base_size_btc) AS open_btc,
  SUM(cost_eur) AS open_cost_eur,
  SUM(buy_fee_eur) AS open_buy_fee_eur,
  SUM(cost_eur + buy_fee_eur) AS allocated_cost_eur
FROM position_slots
WHERE status='OPEN';
"
```

Controllo ultimi ordini:

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

Controllo audit SELL:

```bash
sqlite3 -header -column data/helix.db "
SELECT created_at,event_type,decision,reason,eur_amount,estimated_fee,net_profit
FROM engine_audit
WHERE event_type IN ('SELL_SLOT_SELECTION','REAL_SLOT_SELL_PLAN','EXCHANGE_SAFETY_SELL','SLOT_CLOSE_RECONCILIATION')
ORDER BY id DESC
LIMIT 30;
"
```

Controllo audit BUY slot guard:

```bash
sqlite3 -header -column data/helix.db "
SELECT created_at,event_type,decision,reason,eur_amount,net_profit
FROM engine_audit
WHERE event_type IN ('PORTFOLIO_SLOT_BUY','PORTFOLIO_SLOT_BUY_PREVIEW')
ORDER BY id DESC
LIMIT 30;
"
```

Report validazione rc3 readonly:

```bash
{
  echo "===== HELIX RC3 READONLY VALIDATION REPORT ====="
  date
  echo

  echo "===== VERSION ====="
  cat VERSION
  echo

  echo "===== GIT STATUS ====="
  git status
  echo

  echo "===== POSITION SLOTS ====="
  sqlite3 -header -column data/helix.db "
  SELECT id,buy_order_id,buy_client_order_id,base_size_btc,cost_eur,buy_fee_eur,
         cost_eur + buy_fee_eur AS allocated_cost,
         avg_buy_price,status,opened_at,closed_at,sell_order_id,sell_net_eur,realized_profit_eur
  FROM position_slots
  ORDER BY opened_at ASC, id ASC;
  "
  echo

  echo "===== OPEN POSITION TOTALS ====="
  sqlite3 -header -column data/helix.db "
  SELECT
    COUNT(*) AS open_slots,
    SUM(base_size_btc) AS open_btc,
    SUM(cost_eur) AS open_cost_eur,
    SUM(buy_fee_eur) AS open_buy_fee_eur,
    SUM(cost_eur + buy_fee_eur) AS allocated_cost_eur
  FROM position_slots
  WHERE status='OPEN';
  "
  echo

  echo "===== PORTFOLIO SLOT BUY AUDIT ====="
  sqlite3 -header -column data/helix.db "
  SELECT created_at,event_type,decision,reason,eur_amount,net_profit
  FROM engine_audit
  WHERE event_type IN ('PORTFOLIO_SLOT_BUY','PORTFOLIO_SLOT_BUY_PREVIEW')
  ORDER BY id DESC
  LIMIT 30;
  "
  echo

  echo "===== LATEST SELL AUDIT ====="
  sqlite3 -header -column data/helix.db "
  SELECT created_at,event_type,decision,reason,eur_amount,estimated_fee,net_profit
  FROM engine_audit
  WHERE event_type IN ('SELL_SLOT_SELECTION','REAL_SLOT_SELL_PLAN','EXCHANGE_SAFETY_SELL','SLOT_CLOSE_RECONCILIATION')
  ORDER BY id DESC
  LIMIT 30;
  "
} > helix_rc3_readonly_validation_$(date +%Y%m%d_%H%M%S).txt
```

---

## Creazione ZIP sicuro

Non includere mai `.env`, backup `.env`, `data/`, `.git/` o binari.

Comando consigliato:

```bash
zip -r "../helix_state_$(date +%Y%m%d_%H%M%S).zip" . \
  -x ".git/*" \
  -x "data/*" \
  -x ".env" \
  -x ".env.backup*" \
  -x "helix" \
  -x "helix-live" \
  -x "*.o" \
  -x "*~" \
  -x "*.zip"
```

Verifica che non ci siano file sensibili:

```bash
zipinfo -1 "$(ls -t ../helix_state_*.zip | head -1)" | grep -E '(^|/)(\.env|data/|helix-live|helix$|\.git/)' || echo "OK: nessun file sensibile trovato"
```

---

## Roadmap verso produzione controllata

### v0.3.0-rc1

Obiettivo:

```text
SELL reale slot-based riconciliata in modo controllato
```

Incluso:

```text
SELL preview slot-based
gate dedicato HELIX_ALLOW_REAL_SLOT_SELL
lookup slot reale per SELL reconciliation
chiusura slot reale preparata dopo SELL reconciliation OK
```

### v0.3.0-rc2

Obiettivo:

```text
rendere LIVE_READONLY più leggero
```

Incluso:

```text
polling ridotto
preview Coinbase meno frequenti
minore carico CPU
meno rumore nel DB
```

### v0.3.0-rc3

Obiettivo:

```text
ragionare sul BUY in base a capitale operativo e slot allocati
```

Incluso:

```text
used_slots derivato da position_slots OPEN
import legacy documentato
operational slot BUY guard
blocco BUY se capitale operativo esaurito
```

### v0.3.0

Prima produzione controllata:

```text
LIVE_READONLY stabile
un ordine reale alla volta
micro importi
supervisione umana
nessun ciclo autonomo continuo
nessuna vendita wallet totale
nessun uso automatico della riserva
```

---

## Regola finale

Helix può diventare produttivo solo quando ogni passaggio reale è:

```text
esplicito
piccolo
tracciato
riconciliato
reversibile operativamente
fermato dopo esecuzione
confermato manualmente dall'utente
```

Fino ad allora, Helix resta una release tecnica/pre-prod controllata.
