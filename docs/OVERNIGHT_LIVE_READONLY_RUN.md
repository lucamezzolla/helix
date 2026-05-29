# Helix - Overnight LIVE_READONLY Run

## Scopo

Eseguire Helix in modalità osservativa fino alla mattina successiva per raccogliere dati reali di mercato, wallet, SELL preview e audit, senza inviare ordini reali.

## Modalità consentita

- LIVE_READONLY
- PAPER/SIM
- diagnostica SELL slot-based
- nessun ordine reale
- nessuna SELL reale
- nessuna vendita wallet totale

## Gate obbligatori durante il run

I gate reali devono restare disattivati:

```env
HELIX_REAL_TRADING_ENABLED=false
HELIX_ALLOW_COINBASE_ORDERS=false
HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false
HELIX_ALLOW_REAL_SLOT_SELL=false
```

## Obiettivo dati

Durante il run Helix deve produrre:

- SELL preview slot-based
- BEST_PROFIT diagnostics
- engine_audit
- order_journal dry-run/preview
- nessun nuovo REAL_SENT

## Segnale interessante

Il run diventa interessante solo se compare una di queste decisioni:

```text
BEST_PROFIT_PROFITABLE_PREVIEW_ONLY
REAL_SLOT_SELL_BLOCKED_BY_ENV_GATE
REAL_SLOT_SELL_READY_BUT_NOT_EXECUTED
```

Finché compare:

```text
BEST_PROFIT_NOT_PROFITABLE
SELL_SLOT_NOT_PROFITABLE
```

non si vende.

## Regola

Non modificare gate reali durante il run overnight.
Non attivare SELL reale.
Non lasciare Helix in LIVE_TRADING armato.

## Polling consigliato

Per ridurre CPU e rumore DB:

```text
UI/engine timer: 10 secondi
Coinbase SELL preview: 10 minuti
Wallet/reconciliation: 10-20 minuti, dove applicabile
```

## Report mattutino

Dopo un run overnight, produrre un report con:

- `VERSION`
- `git status`
- `position_slots`
- ultimi eventi SELL in `order_journal`
- ordini reali in `order_journal`
- audit SELL da `engine_audit`

Il report serve per controllare che non siano stati inviati ordini reali e che lo slot reale sia rimasto coerente.
