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