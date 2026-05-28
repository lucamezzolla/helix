# Helix

Helix è un'applicazione desktop scritta in **C** con interfaccia **GTK4**. Il progetto nasce come motore prudente di trading su **BTC-EUR** collegato a Coinbase, sviluppato con una regola fondamentale: prima la sicurezza, poi l'automazione.

Helix non è pensato per operare in modo aggressivo o autonomo senza controllo umano. L'obiettivo della versione attuale è arrivare a una **produzione controllata**, con micro-importi, uno slot alla volta, stop automatico dopo ogni ordine reale e riconciliazione obbligatoria.

> ⚠️ Helix può interagire con denaro reale. Prima di qualunque test live bisogna verificare codice, database, configurazione `.env`, log, journal e stato Coinbase.

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

Ultime milestone rilevanti:

```text
v0.2.2-paper-best-profit
v0.2.3-guarded-slot-sell-groundwork
v0.2.4-sell-path-readiness
v0.2.5-preprod-checklist
```

Stato tecnico attuale:

```text
BUY reale micro validato                         ✅
Post-order reconciliation BUY reale OK           ✅
position_slots reale attiva                      ✅
Primo slot reale OPEN                            ✅
SELL preview per slot                            ✅
BEST_PROFIT diagnostico su slot reali            ✅
PAPER/SIM isolato                                ✅
paper BEST_PROFIT funzionante                    ✅
Acknowledge ultimo ordine reale                  ✅
Gate HELIX_ALLOW_REAL_SLOT_SELL                  ✅
Lookup slot reale per futura SELL reconciliation ✅
SELL reconciliation con chiusura slot preparata  ✅
SELL reale ancora disabilitata                   ✅
```

Helix è quindi in stato **pre-produzione controllata**, non ancora in produzione autonoma.

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
- valutare SELL preview per slot;
- scegliere lo slot migliore con BEST_PROFIT;
- simulare BUY/SELL paper;
- chiudere slot paper;
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
├── .env.example
├── docs/
│   ├── PRE_PROD_CHECKLIST.md
│   ├── checklist_prelive.md
│   ├── release_notes_v0.2.0-prelive.md
│   └── cleanup_live_candidate_noise.sql
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
│   │   ├── order_journal.c
│   │   ├── order_journal.h
│   │   ├── post_order_reconciliation.c
│   │   ├── post_order_reconciliation.h
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

Contiene gli slot reali derivati da BUY reali Coinbase.

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

Uno slot reale può essere chiuso solo dopo futura SELL reale riconciliata correttamente.

### `paper_position_slots`

Contiene slot fittizi usati solo per PAPER/SIM.

Non deve mai essere mischiata con `position_slots` reale.

---

## Flusso BUY reale micro

Flusso validato:

```text
1. BUY preview Coinbase
2. exchange safety
3. runtime safety
4. final live gate
5. live execution lock
6. create-order Coinbase
7. order_journal REAL_SENT
8. post-order reconciliation
9. POST_ORDER_RECON_OK
10. creazione position_slots OPEN
11. STOP_AFTER_REAL_ORDER
12. acknowledge manuale
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
micro_live_stop_after_real_order
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
git log --oneline -8
git tag --list "v0.2.*"
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
       avg_buy_price,status,opened_at,closed_at,sell_order_id,
       sell_net_eur,realized_profit_eur
FROM position_slots
ORDER BY id;
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

---

## Creazione ZIP sicuro

Non includere mai `.env`, `data/`, `.git/` o binari.

Comando consigliato:

```bash
zip -r "../helix_state_$(date +%Y%m%d_%H%M%S).zip" . \
  -x ".git/*" \
  -x "data/*" \
  -x ".env" \
  -x "helix" \
  -x "helix-live" \
  -x "*.o" \
  -x "*~"
```

---

## Roadmap verso produzione controllata

### v0.2.5

```text
pre-produzione documentata
SELL reale ancora disabilitata
checklist pronta
```

### v0.3.0-rc1

Obiettivo:

```text
SELL reale slot-based riconciliata in modo controllato
```

Requisiti minimi:

```text
SELL solo su slot OPEN
Coinbase preview immediata valida
HELIX_ALLOW_REAL_SLOT_SELL=true solo per test controllato
ordine micro
POST_ORDER_RECON_OK
slot CLOSED solo dopo FILLED
STOP_AFTER_REAL_ORDER
acknowledge manuale
```

### v0.3.0

Prima produzione controllata:

```text
un ordine reale alla volta
micro importi
supervisione umana
nessun ciclo autonomo continuo
nessuna vendita wallet totale
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
