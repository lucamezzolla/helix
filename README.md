# Helix

Helix è un'applicazione desktop scritta in **C** con interfaccia **GTK4**. Il progetto nasce come percorso didattico e tecnico per costruire, capire e controllare un motore di trading prudente su **BTC-EUR** collegato a **Coinbase**.

La filosofia del progetto è semplice ma molto rigida:

- Helix non deve mai comprare usando tutto il saldo disponibile.
- Helix non deve mai vendere tutto il wallet BTC.
- Helix deve lavorare a **slot/lotti**.
- Ogni BUY reale apre uno slot.
- Ogni SELL deve valutare solo slot aperti.
- Una SELL può chiudere solo lo slot scelto, non il saldo globale.
- La riserva EUR non deve essere usata dai BUY normali.
- Qualsiasi operazione reale deve essere protetta, tracciata, riconciliata e fermare il bot subito dopo.

> ⚠️ **Attenzione**  
> Helix può interagire con denaro reale. Il codice deve essere trattato come software critico: prima sicurezza, poi studio, poi automazione.

---

## Stato attuale del progetto

Branch di lavoro corrente:

```bash
live-candidate
```

Milestone precedente importante:

```text
v0.2.0-rc1
```

Stato attuale dopo la patch slot/lotti:

```text
Primo BUY reale BTC-EUR completato e riconciliato     ✅
Tabella position_slots creata                         ✅
Primo BUY reale importato come slot OPEN              ✅
SELL preview per slot funzionante                     ✅
Scelta BEST_PROFIT diagnostica funzionante            ✅
SELL reale ancora disabilitata                        ✅
Vendita dell'intero wallet bloccata                   ✅
Rebuild position_slots da BUY reali migliorata        ✅
Acknowledge post real order impostato lato settings   ⚠️ da completare in UI
Modalità paper/sim                                    ⚠️ da progettare
SELL reale slot-based                                 ❌ non ancora abilitata
```

Helix ha già eseguito un primo micro-BUY reale su Coinbase:

```text
Prodotto:        BTC-EUR
Tipo:            Market BUY
Importo:         circa 10 EUR
Quantità BTC:    circa 0.00015346 BTC
Stato Coinbase:  FILLED
Reconciliation:  POST_ORDER_RECON_OK
```

Dopo il primo ordine reale, Helix ha eseguito lo stop automatico:

```text
MICRO_LIVE STOP_AFTER_REAL_ORDER
```

Questo comportamento è corretto: dopo un ordine reale, il bot deve fermarsi e richiedere revisione manuale.

---

## Obiettivo tecnico

L'obiettivo di Helix non è “fare trading aggressivo”. L'obiettivo è costruire un sistema controllabile, auditabile e prudente che permetta di studiare:

- programmazione C reale;
- GTK4;
- SQLite;
- chiamate API Coinbase;
- autenticazione;
- logging;
- gestione del rischio;
- progettazione di stato persistente;
- trading a slot/lotti;
- riconciliazione post-ordine;
- modalità live, readonly, paper e simulazione.

Helix deve procedere per milestone piccole, testabili e reversibili.

---

## Regola fondamentale: slot/lotti

Helix non ragiona così:

```text
BUY  -> compro tutto quello che posso
SELL -> vendo tutto quello che ho
```

Helix deve ragionare così:

```text
BUY  -> compro un piccolo slot configurato in EUR
SELL -> vendo solo lo slot BTC scelto
```

Esempio:

```text
Saldo EUR disponibile: 50 EUR
Slot configurato:      10 EUR

BUY:
  usa circa 10 EUR

SELL:
  vende solo la quantità BTC dello slot aperto
```

Questo evita che una SELL venda accidentalmente tutto il wallet Coinbase.

---

## Concetto di slot

Uno slot rappresenta un lotto acquistato con un BUY reale o simulato.

Esempio di slot:

```text
slot id:          1
buy_order_id:     a1015632-8cb0-4f7f-abfd-3f0fd0068954
base_size_btc:    0.0001534613110309
cost_eur:         10.0
buy_fee_eur:      0.118577075098814
avg_buy_price:    64390.3200000131
status:           OPEN
opened_at:        2026-05-27 17:35:08
```

Quando lo slot viene venduto in futuro, non deve sparire: deve diventare `CLOSED` e conservare i dati di vendita.

---

## Tabella `position_slots`

La tabella introdotta per gestire i lotti è:

```sql
CREATE TABLE IF NOT EXISTS position_slots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    buy_order_id TEXT NOT NULL UNIQUE,
    buy_client_order_id TEXT NOT NULL,
    base_size_btc REAL NOT NULL,
    cost_eur REAL NOT NULL,
    buy_fee_eur REAL DEFAULT 0,
    avg_buy_price REAL DEFAULT 0,
    status TEXT NOT NULL DEFAULT 'OPEN',
    opened_at TEXT DEFAULT CURRENT_TIMESTAMP,
    closed_at TEXT DEFAULT '',
    sell_order_id TEXT DEFAULT '',
    sell_net_eur REAL DEFAULT 0,
    realized_profit_eur REAL DEFAULT 0
);
```

Indice:

```sql
CREATE INDEX IF NOT EXISTS idx_position_slots_status
ON position_slots(status);
```

Significato dei campi principali:

| Campo | Significato |
|---|---|
| `id` | Identificativo locale dello slot |
| `buy_order_id` | ID ordine Coinbase del BUY reale |
| `buy_client_order_id` | Client order id generato da Helix |
| `base_size_btc` | Quantità BTC acquistata nello slot |
| `cost_eur` | Costo EUR dello slot |
| `buy_fee_eur` | Fee stimata/preview del BUY |
| `avg_buy_price` | Prezzo medio di acquisto |
| `status` | `OPEN` oppure `CLOSED` |
| `opened_at` | Data apertura slot |
| `closed_at` | Data chiusura slot futura |
| `sell_order_id` | ID ordine Coinbase della SELL futura |
| `sell_net_eur` | Netto EUR ottenuto dalla SELL |
| `realized_profit_eur` | Profitto/perdita realizzato |

---

## Primo slot reale importato

Il primo BUY reale è stato importato come slot `OPEN`:

```text
buy_order_id:    a1015632-8cb0-4f7f-abfd-3f0fd0068954
base_size_btc:   0.0001534613110309
cost_eur:        10.0
buy_fee_eur:     0.118577075098814
avg_buy_price:   64390.3200000131
status:          OPEN
opened_at:       2026-05-27 17:35:08
```

Query di controllo:

```bash
sqlite3 -header -column data/helix.db \
"SELECT id,buy_order_id,base_size_btc,cost_eur,buy_fee_eur,avg_buy_price,status,opened_at FROM position_slots;"
```

Output atteso:

```text
id  buy_order_id                          base_size_btc       cost_eur  buy_fee_eur        avg_buy_price     status  opened_at
--  ------------------------------------  ------------------  --------  -----------------  ----------------  ------  -------------------
1   a1015632-8cb0-4f7f-abfd-3f0fd0068954  0.0001534613110309  10.0      0.118577075098814  64390.3200000131  OPEN    2026-05-27 17:35:08
```

---

## BUY reale: regola `quote_size`

Per un BUY Coinbase deve ricevere una quantità in EUR, cioè `quote_size`.

Payload corretto:

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

Errore corretto in precedenza:

```text
PREVIEW_INVALID_BASE_SIZE_TOO_SMALL
```

Motivo: Helix stava interpretando/inviando male la quantità. Per un BUY bisogna usare EUR (`quote_size`), non BTC (`base_size`).

---

## SELL per slot: regola `base_size`

Per una SELL Coinbase deve ricevere una quantità in BTC, cioè `base_size`.

Payload corretto:

```json
{
  "product_id": "BTC-EUR",
  "side": "SELL",
  "order_configuration": {
    "market_market_ioc": {
      "base_size": "0.00015346",
      "rfq_disabled": true
    }
  }
}
```

La quantità BTC deve arrivare dallo slot:

```text
position_slots.base_size_btc
```

Non deve arrivare dal wallet totale Coinbase.

---

## SELL preview diagnostica

La SELL reale è ancora disabilitata. Attualmente Helix fa solo preview diagnostica.

Flusso attuale:

```text
1. carica gli slot OPEN
2. per ogni slot fa SELL preview Coinbase con base_size_btc
3. calcola gross
4. calcola fee
5. calcola net
6. confronta net con cost_eur
7. calcola profit EUR
8. calcola profit %
9. sceglie lo slot migliore con BEST_PROFIT
10. se lo slot è in perdita, blocca la SELL
11. registra nel journal una DRY_RUN_BLOCKED
```

Ultimo test diagnostico valido:

```text
id journal:          269
side:                SELL
dry_run:             1
status:              DRY_RUN_BLOCKED
phase:               PREVIEW
decision:            SELL_SLOT_NOT_PROFITABLE
requested_base_size: 0.0001534613110309
preview_total_eur:   9.5850231333792
preview_fee_eur:     0.1164172850208
preview_base_size:   0.00015346
preview_avg_price:   63218.04
```

Reason:

```text
BEST_PROFIT slot #1 scelto | gross 9,59 | fee 0,12 | net 9,47 | cost 10,00 | profit -0,53 EUR -5,31% | SELL reale disabilitata
```

Risultato corretto:

```text
Helix non vende perché lo slot è in perdita.
```

---

## Strategia BEST_PROFIT

La prima strategia implementata è `BEST_PROFIT`.

Algoritmo:

```text
per ogni slot OPEN:
    fai SELL preview Coinbase usando base_size_btc dello slot
    calcola gross
    calcola fee
    calcola net = gross - fee
    calcola profit = net - cost_eur
    calcola profit_percent = profit / cost_eur * 100

scegli lo slot con profitto netto più alto

se:
    profit >= min_profit_eur
    profit_percent >= min_profit_percent

allora:
    slot candidato alla vendita

altrimenti:
    blocca la SELL
```

Esempio:

```text
Slot A: cost 10, net 9.45  -> -0.55  -> bloccato
Slot B: cost 10, net 10.18 -> +0.18  -> bloccato se min profit è 1 EUR
Slot C: cost 10, net 11.20 -> +1.20  -> candidato
```

In futuro, se la SELL reale verrà abilitata, Helix dovrà vendere solo lo Slot C.

---

## SELL reale: stato e regole future

La SELL reale è volutamente ancora disabilitata.

Motivi:

```text
position_slots è appena stata introdotta
BEST_PROFIT è ancora diagnostico
serve testare più slot OPEN
serve modalità paper/sim
serve UI acknowledge post ordine reale
serve chiusura slot dopo reconciliation reale
serve validare bene il journal SELL reale
```

La SELL reale futura dovrà rispettare questo flusso:

```text
1. bot in stato sicuro
2. git pulito
3. DB con backup
4. slot OPEN caricato
5. SELL preview Coinbase fresca
6. preview senza errs
7. profit >= soglia minima
8. final live gate OK
9. live execution lock OK
10. create-order SELL con base_size dello slot
11. journal REAL_SENT
12. post-order reconciliation
13. se FILLED/reconciliation OK, chiusura slot
14. STOP_AFTER_REAL_ORDER
15. LIVE_TRADING disarmato
```

La chiusura slot deve avvenire solo dopo conferma reale, non al momento della preview.

---

## Riserva

La riserva serve a proteggere capitale.

La riserva non deve essere usata dai BUY normali.

Comportamento desiderato:

```text
BUY ordinario:
    usa solo slot ordinari
    non tocca la riserva

forte calo / crisi:
    conserva la riserva

recovery dopo forte calo:
    la riserva può essere usata solo se:
        - la crisi è stata rilevata
        - la ripresa è confermata
        - esiste sblocco manuale
        - esistono limiti espliciti
        - esiste audit obbligatorio
```

Possibili campi futuri:

```text
slot_type = NORMAL / RESERVE
reserve_unlock_required = true/false
reserve_unlock_acknowledged_at
reserve_reason
```

---

## Modalità operative

Helix deve distinguere chiaramente più modalità.

### SAFE / normale

Build standard:

```bash
make clean
make
make run
```

La build standard non deve inviare ordini reali.

### LIVE_READONLY / diagnostico

Serve per:

```text
leggere prezzo
leggere saldi
fare preview
verificare Coinbase API
controllare journal
diagnosticare slot
```

Non deve inviare ordini reali.

### LIVE_TRADING / reale

È la modalità più pericolosa.

Può inviare ordini reali solo se passano tutti i gate:

```text
build live
runtime mode corretto
manual arm
flag .env espliciti
risk guard
operational limits
anti-duplicate
final live gate
live execution lock
post-order reconciliation
```

### PAPER / SIM futura

Da progettare.

La modalità paper/sim dovrà:

```text
non chiamare mai Coinbase create-order
simulare BUY
simulare SELL
creare slot paper
chiudere slot paper
calcolare P/L
permettere test lunghi senza rischio reale
separare dati reali da dati simulati
```

Possibile estensione futura:

```text
position_slots.mode = REAL / PAPER
```

oppure tabelle separate:

```text
position_slots
paper_position_slots
```

---

## `.env`

Il file `.env` non deve essere committato.

Flag principali per ordini reali:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
```

Per diagnostica sicura devono restare tutti `false`.

Permessi Coinbase consigliati:

```text
View
Trade
```

Permessi da non abilitare:

```text
Transfer
Receive
```

Motivo:

```text
View serve per saldo, wallet, preview e stato.
Trade serve per BUY/SELL.
Transfer e Receive non servono a Helix e aumentano il rischio.
```

---

## Build

Build normale sicura:

```bash
make clean
make
make run
```

Build live-candidate:

```bash
make -f Makefile.live clean
make -f Makefile.live
make -f Makefile.live run
```

`Makefile.live` compila con:

```c
-DHELIX_ENABLE_REAL_COINBASE_ORDERS
```

Questa macro abilita il codice live, ma non basta per inviare ordini reali: servono anche i flag `.env`, runtime mode, arm manuale e gate.

---

## Struttura progetto

Struttura attuale sintetica:

```text
helix/
├── .env.example
├── .gitignore
├── Makefile
├── Makefile.live
├── README.md
├── VERSION
├── docs/
│   ├── checklist_prelive.md
│   ├── cleanup_live_candidate_noise.sql
│   └── release_notes_v0.2.0-prelive.md
└── src/
    ├── main.c
    ├── config/
    │   ├── env_loader.c
    │   └── env_loader.h
    ├── db/
    │   ├── database.c
    │   └── database.h
    ├── engine/
    │   ├── anti_duplicate_order.c
    │   ├── anti_duplicate_order.h
    │   ├── api_health.c
    │   ├── api_health.h
    │   ├── bot_state.c
    │   ├── bot_state.h
    │   ├── emergency_stop.c
    │   ├── emergency_stop.h
    │   ├── engine.c
    │   ├── engine.h
    │   ├── exchange_safety.c
    │   ├── exchange_safety.h
    │   ├── final_live_gate.c
    │   ├── final_live_gate.h
    │   ├── liquidity_reserve.c
    │   ├── liquidity_reserve.h
    │   ├── live_execution_lock.c
    │   ├── live_execution_lock.h
    │   ├── live_readiness.c
    │   ├── live_readiness.h
    │   ├── operational_limits.c
    │   ├── operational_limits.h
    │   ├── order_journal.c
    │   ├── order_journal.h
    │   ├── order_state_recovery.c
    │   ├── order_state_recovery.h
    │   ├── post_order_reconciliation.c
    │   ├── post_order_reconciliation.h
    │   ├── prelive_validation.c
    │   ├── prelive_validation.h
    │   ├── reconciliation.c
    │   ├── reconciliation.h
    │   ├── risk_guard.c
    │   ├── risk_guard.h
    │   ├── runtime_safety.c
    │   ├── runtime_safety.h
    │   ├── settings.c
    │   ├── settings.h
    │   ├── trade_preview.c
    │   ├── trade_preview.h
    │   ├── volatility_protection.c
    │   ├── volatility_protection.h
    │   ├── wallet.c
    │   └── wallet.h
    ├── exchange/
    │   ├── coinbase_auth.c
    │   ├── coinbase_auth.h
    │   ├── coinbase_client.c
    │   ├── coinbase_client.h
    │   ├── order_executor.c
    │   ├── order_executor.h
    │   ├── order_preview.c
    │   └── order_preview.h
    ├── market/
    │   ├── market_data.c
    │   └── market_data.h
    ├── ui/
    │   ├── window.c
    │   └── window.h
    └── wallet/
        ├── wallet_info.c
        └── wallet_info.h
```

---

## Ruolo dei file principali

### `src/main.c`

Entry point dell'applicazione. Inizializza il contesto principale e avvia la UI GTK4.

### `src/ui/window.c`

Gestisce la finestra GTK4, i controlli utente e la visualizzazione dello stato del bot.

Qui dovrà essere aggiunto in futuro il pulsante:

```text
Acknowledge last real order
```

### `src/engine/engine.c`

Cuore del ciclo operativo.

Responsabilità:

```text
leggere stato runtime
leggere saldi/prezzi
valutare BUY/SELL
rispettare cooldown e limiti
chiamare preview Coinbase
valutare slot OPEN
scegliere BEST_PROFIT
bloccare SELL non profittevoli
non inviare SELL reali in questa fase
```

### `src/db/database.c` / `database.h`

Gestione SQLite.

Contiene:

```text
bot_state
settings
engine_audit
order_journal
order_state
trades
position_slots
```

Funzioni slot principali:

```c
int db_create_position_slot_from_buy(...);
int db_rebuild_position_slots_from_real_buys(void);
int db_get_open_position_slots(...);
int db_close_position_slot(...);
```

### `src/engine/order_journal.c` / `order_journal.h`

Journal degli ordini.

Registra:

```text
dry-run
preview
pre-execution
real execution
post-order reconciliation
rejected/blocked states
```

Dopo un BUY reale accettato, il sistema deve poter creare o ricostruire lo slot OPEN.

### `src/exchange/order_preview.c`

Gestisce preview Coinbase.

Regole fondamentali:

```text
BUY  -> quote_size EUR
SELL -> base_size BTC
```

### `src/exchange/order_executor.c`

Gestisce create-order Coinbase.

È codice pericoloso: deve essere invocato solo dopo tutti i gate.

### `src/engine/risk_guard.c`

Controlli di rischio.

### `src/engine/runtime_safety.c`

Sicurezza runtime, modalità operative, blocchi di protezione.

### `src/engine/operational_limits.c`

Limiti giornalieri, cooldown, prevenzione operatività eccessiva.

### `src/engine/anti_duplicate_order.c`

Previene duplicati ravvicinati.

### `src/engine/final_live_gate.c`

Gate finale prima di consentire un ordine reale.

### `src/engine/live_execution_lock.c`

Ultimo lock prima della chiamata create-order.

### `src/engine/post_order_reconciliation.c`

Riconciliazione dopo ordine reale.

Deve verificare che l'ordine accettato da Coinbase sia effettivamente nello stato previsto.

---

## Database locale

Database locale:

```text
data/helix.db
```

Non va committato.

File da non committare:

```text
.env
data/
helix
helix-live
*.db
*.db-wal
*.db-shm
coinbase_*_last.json
log personali
backup locali
```

Controllare `.gitignore` prima di ogni commit.

---

## Query utili SQLite

Tabelle:

```bash
sqlite3 data/helix.db ".tables"
```

Schema slot:

```bash
sqlite3 data/helix.db ".schema position_slots"
```

Slot aperti:

```bash
sqlite3 -header -column data/helix.db \
"SELECT id,buy_order_id,base_size_btc,cost_eur,buy_fee_eur,avg_buy_price,status,opened_at FROM position_slots WHERE status='OPEN';"
```

Ultimi ordini:

```bash
sqlite3 -header -column data/helix.db \
"SELECT id,client_order_id,side,dry_run,status,phase,decision,requested_base_size,preview_total_eur,preview_fee_eur,preview_base_size,preview_avg_price,reason,created_at FROM order_journal ORDER BY id DESC LIMIT 10;"
```

Audit SELL/slot:

```bash
sqlite3 -header -column data/helix.db \
"SELECT created_at,event_type,decision,reason,btc_amount,eur_amount,estimated_fee,net_profit FROM engine_audit WHERE event_type LIKE '%SELL%' OR event_type LIKE '%POSITION%' ORDER BY id DESC LIMIT 30;"
```

BUY reali importabili come slot:

```bash
sqlite3 -header -column data/helix.db \
"SELECT coinbase_order_id, client_order_id, preview_base_size, CASE WHEN requested_quote_size > 0 THEN requested_quote_size ELSE preview_total_eur END AS cost_eur, preview_fee_eur, preview_avg_price FROM order_journal WHERE side = 'BUY' AND dry_run = 0 AND status = 'REAL_SENT' AND phase = 'REAL_EXECUTION' AND decision = 'SENT' AND coinbase_order_id <> '' AND preview_base_size > 0 ORDER BY id ASC;"
```

---

## Journal ordini

La tabella `order_journal` registra le decisioni di Helix.

Campi importanti:

```text
client_order_id
side
product_id
dry_run
status
phase
decision
requested_quote_size
requested_base_size
preview_total_eur
preview_fee_eur
preview_base_size
preview_avg_price
reason
http_code
coinbase_order_id
execution_decision
execution_reason
executed_at
```

Esempi di stati:

```text
DRY_RUN_READY
DRY_RUN_BLOCKED
REAL_PLAN_READY
REAL_SENT
REAL_REJECTED
POST_ORDER_RECON_OK
```

Esempi di decisioni:

```text
FINAL_GATE_OK
ANTI_DUPLICATE_BLOCKED
SELL_SLOT_NOT_PROFITABLE
SELL_CANDIDATE_BLOCKED
POST_ORDER_RECON_OK
```

---

## Audit engine

La tabella `engine_audit` usa colonne:

```text
event_type
decision
reason
price
btc_amount
eur_amount
estimated_fee
net_profit
created_at
```

Nota: la colonna si chiama `event_type`, non `event`.

Query corretta:

```bash
sqlite3 -header -column data/helix.db \
"SELECT created_at,event_type,decision,reason,btc_amount,eur_amount,estimated_fee,net_profit FROM engine_audit ORDER BY id DESC LIMIT 30;"
```

---

## Stop after real order

Dopo un ordine reale, Helix deve:

```text
fermare il bot
disarmare LIVE_TRADING
impedire nuovi ordini reali
richiedere revisione manuale
```

È stato introdotto il setting:

```text
micro_live.last_real_order_acknowledged
```

Obiettivo:

```text
se ultimo ordine reale non è acknowledged:
    bloccare LIVE_TRADING

se runtime è LIVE_READONLY:
    permettere diagnostica
```

Da fare:

```text
aggiungere pulsante UI: Acknowledge last real order
mostrare ultimo order id reale
salvare acknowledge in settings
loggare audit dell'acknowledge
```

---

## Sicurezza Coinbase

Prima di qualsiasi test reale:

```text
bot fermo
git status pulito
backup DB creato
.env controllato
LIVE_TRADING armato solo manualmente
importo minimo
saldo e wallet controllati
permessi Coinbase limitati a View/Trade
nessun Transfer/Receive
STOP_AFTER_REAL_ORDER attivo
journal controllato dopo ordine
reconciliation controllata
```

---

## Procedura diagnostica slot SELL

1. Compilare:

```bash
make clean
make
make -f Makefile.live clean
make -f Makefile.live
```

2. Verificare `.env`:

```bash
grep -E "HELIX_REAL_TRADING_ENABLED|HELIX_ALLOW_COINBASE_ORDERS|HELIX_I_UNDERSTAND_REAL_MONEY_RISK" .env
```

Valori attesi:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
```

3. Avviare:

```bash
make -f Makefile.live run
```

4. Fare Bot Start solo in diagnostica.

5. Non armare LIVE_TRADING.

6. Aspettare un ciclo.

7. Fermare il bot.

8. Controllare journal:

```bash
sqlite3 -header -column data/helix.db \
"SELECT id,client_order_id,side,dry_run,status,phase,decision,requested_base_size,preview_total_eur,preview_fee_eur,preview_base_size,preview_avg_price,reason,created_at FROM order_journal ORDER BY id DESC LIMIT 5;"
```

Risultato buono attuale:

```text
SELL_SLOT_NOT_PROFITABLE
```

Questo significa che Helix ha valutato lo slot e ha deciso correttamente di non vendere.

---

## Workflow Git consigliato

Controllo stato:

```bash
git status
git diff --stat
```

Aggiunta file sorgente:

```bash
git add \
  README.md \
  src/db/database.c \
  src/db/database.h \
  src/engine/engine.c \
  src/engine/order_journal.c \
  src/engine/order_journal.h \
  src/engine/settings.c \
  src/engine/settings.h
```

Controllo staged:

```bash
git diff --stat --cached
```

Commit consigliato:

```bash
git commit -m "feat: add slot-based position tracking and sell preview"
```

Push:

```bash
git push
```

Tag possibile:

```bash
git tag v0.2.1-slot-preview
git push origin v0.2.1-slot-preview
```

---

## Roadmap immediata

Priorità:

```text
1. committare patch slot/lotti diagnostica
2. aggiornare README
3. aggiungere UI acknowledge ultimo ordine reale
4. migliorare audit della rebuild position_slots
5. progettare modalità paper/sim
6. testare più slot OPEN
7. simulare BEST_PROFIT con più slot
8. definire soglie min_profit_eur e min_profit_percent configurabili
9. progettare chiusura slot dopo SELL reale riconciliata
10. solo molto più avanti: abilitare SELL reale slot-based
```

---

## Roadmap paper/sim

La modalità paper/sim dovrà essere progettata in modo da non poter inviare ordini reali neanche per errore.

Possibili regole:

```text
runtime PAPER ignora create-order reale
usa solo prezzo corrente/preview locale
crea slot PAPER
chiude slot PAPER
non mischia slot REAL e PAPER
journal separa PAPER da REAL
UI mostra chiaramente PAPER MODE
```

Possibile schema futuro:

```text
position_slots.mode = REAL / PAPER
position_slots.source = COINBASE / SIMULATED
```

Oppure:

```text
paper_slots
paper_order_journal
```

La soluzione va scelta prima di implementare.

---

## Roadmap UI acknowledge

Da aggiungere in `src/ui/window.c`:

```text
se esiste ultimo ordine reale non acknowledged:
    mostra warning
    disabilita LIVE_TRADING
    abilita bottone "Acknowledge last real order"

click bottone:
    mostra ultimo order id
    richiede conferma
    salva micro_live.last_real_order_acknowledged = order id
    scrive engine_audit
```

Questo evita di modificare SQLite a mano.

---

## Roadmap SELL reale slot-based

La SELL reale non va abilitata ora.

Quando sarà il momento, dovrà essere introdotta con una milestone dedicata.

Requisiti minimi:

```text
almeno 2-3 run diagnostici stabili
più slot OPEN simulati o paper
BEST_PROFIT validato
soglie configurabili
preview Coinbase fresca immediatamente prima della SELL
SELL reale con base_size dello slot
post-order reconciliation SELL
chiusura slot solo dopo FILLED
STOP_AFTER_REAL_ORDER dopo SELL
report finale
```

---

## Regola d'oro

Helix deve sempre preferire:

```text
non fare nulla
```

rispetto a:

```text
fare un ordine ambiguo
```

Una BUY o una SELL deve partire solo quando:

```text
il codice sa cosa sta facendo
il database lo rappresenta correttamente
Coinbase preview è valida
i gate sono tutti verdi
l'utente ha armato manualmente
il rischio è piccolo
il sistema può fermarsi e riconciliare
```

---

## Stato finale di questa milestone

La milestone slot preview è positiva perché:

```text
Helix ha uno slot reale OPEN
Helix valuta SELL sullo slot, non sul wallet intero
Helix calcola profitto netto dopo fee
Helix sceglie BEST_PROFIT
Helix blocca lo slot in perdita
Helix non manda ordini reali
```

Questo è il comportamento corretto.

