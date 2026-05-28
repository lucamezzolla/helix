# Helix v0.3.0-rc1 - Guarded Sell Reconciliation

## Stato

Questa è una release candidate tecnica per la futura produzione controllata.

## Incluso

- BUY reale micro già validato
- Reconciliation BUY reale funzionante
- Slot/lotti reali tramite position_slots
- SELL preview slot-based
- BEST_PROFIT diagnostico
- PAPER/SIM isolato
- Paper BEST_PROFIT funzionante
- Gate dedicato HELIX_ALLOW_REAL_SLOT_SELL
- Lookup slot reale per SELL reconciliation
- Chiusura slot reale preparata dopo SELL reconciliation OK
- Documentazione pre-produzione

## Non incluso

- SELL reale autonoma
- Bot 24/7
- Uso automatico della riserva
- Vendita wallet totale
- Più ordini reali consecutivi

## Regola di sicurezza

La SELL reale resta disabilitata finché non viene eseguito un test micro-live supervisionato con:

- HELIX_ALLOW_REAL_SLOT_SELL=true
- gate globali real trading true
- STOP_AFTER_REAL_ORDER=1
- max_orders_per_day=1
- importo micro
- acknowledge manuale dopo ordine
