# Helix - Production READONLY Runbook

## Scopo

Questo documento descrive come eseguire Helix in produzione controllata READONLY.

La modalità READONLY serve a osservare il mercato reale, leggere wallet, produrre preview BUY/SELL, aggiornare audit e individuare segnali utili senza inviare ordini reali.

## Stato consentito

Modalità consentita:

```text
LIVE_READONLY
```

Modalità non consentite durante questo run:

```text
LIVE_TRADING
SELL reale
BUY reale non supervisionato
bot autonomo 24/7 non presidiato
uso automatico della riserva
```

## Gate ambiente obbligatori

Nel file `.env` i gate reali devono restare disattivati:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
HELIX_ALLOW_REAL_SLOT_SELL=false
```

`HELIX_ALLOW_REAL_SLOT_SELL` deve restare `false` finché Helix non segnala uno slot profittevole e finché non viene preparato un test reale supervisionato.

## Controlli prima dell'avvio

Prima di avviare Helix:

```bash
git status
git log --oneline -10
git tag --list "v0.3.*"
./build_all.sh
```

Il repository deve essere pulito. I binari `helix` e `helix-live` non devono essere committati.

## Avvio

Compilare e avviare:

```bash
./build_all.sh
./helix-live
```

Dalla UI verificare che la modalità sia:

```text
LIVE_READONLY
```

## Cosa deve fare Helix in READONLY

Helix deve:

- leggere prezzo BTC-EUR;
- leggere wallet EUR/BTC;
- mantenere `position_slots` coerente;
- valutare SELL preview per slot;
- scegliere il miglior slot con BEST_PROFIT;
- bloccare BUY se il capitale operativo è già consumato;
- proteggere la riserva;
- non inviare ordini reali;
- scrivere audit utili e non rumorosi.

## Segnali normali

Sono normali questi eventi:

```text
PORTFOLIO_SLOT_BUY_PREVIEW | BLOCKED
SELL_SLOT_PREVIEW_EVALUATED
BEST_PROFIT_NOT_PROFITABLE
SELL_SLOT_NOT_PROFITABLE
```

`PORTFOLIO_SLOT_BUY_PREVIEW | BLOCKED` è corretto quando il capitale operativo è esaurito.

## Segnali interessanti

Questi segnali richiedono revisione umana:

```text
BEST_PROFIT_PROFITABLE_PREVIEW_ONLY
REAL_SLOT_SELL_BLOCKED_BY_ENV_GATE
REAL_SLOT_SELL_READY_BUT_NOT_EXECUTED
```

Se compaiono, non attivare subito il trading reale. Fermare Helix, produrre report, controllare DB, controllare `.env`, poi decidere il test reale supervisionato.

## Segnali critici

Questi segnali richiedono stop immediato e analisi:

```text
REAL_SENT non previsto
POST_ORDER_RECON_ERROR
SLOT_CLOSE_RECONCILIATION error
used_slots diverso da position_slots OPEN
SELL reale partita senza test supervisionato
```

## Report produzione readonly

Dopo un run, produrre un report locale con dati su:

- versione;
- stato Git;
- slot reali;
- bot_state;
- audit BUY portfolio guard;
- audit SELL;
- ordini reali.

Il report deve essere controllato prima di qualunque passaggio a trading reale.

## Regola SELL reale

La SELL reale può essere considerata solo se:

- uno slot OPEN è profittevole netto;
- Helix ha prodotto un segnale profittevole;
- il gate `HELIX_ALLOW_REAL_SLOT_SELL` è ancora false al momento della diagnosi;
- il DB è stato salvato con backup;
- il test è supervisionato;
- `STOP_AFTER_REAL_ORDER=1`;
- `max_orders_per_day=1`;
- la reconciliation può chiudere solo lo slot venduto.

## Regola BUY reale

Il BUY reale può essere considerato solo se:

- c'è capitale operativo disponibile;
- gli slot non sono pieni;
- la riserva resta protetta;
- `micro_live.max_order_eur` limita l'importo;
- `STOP_AFTER_REAL_ORDER=1`;
- la reconciliation apre un nuovo slot reale.

## Produzione ammessa

Stato ammesso per questa release:

```text
Produzione READONLY / osservativa: SI
Produzione trading autonoma: NO
SELL reale automatica: NO
BUY reale automatico continuo: NO
```
