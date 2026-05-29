# HELIX - Pre-produzione controllata v0.3.0

## Stato corrente

- Branch: live-candidate
- Versione corrente: 0.3.0-rc2
- Ultimo tag tecnico rilevante: v0.3.0-rc2-readonly-polling
- SELL reale: NON attiva
- BUY reale micro: già validato
- Slot/lotti: attivi
- Legacy slot import: documentato
- Paper BEST_PROFIT: validato
- Gate SELL reale slot-based: presente
- SELL reconciliation: preparata con chiusura slot solo dopo reconciliation OK

## Prima di qualunque test reale

- [ ] git status pulito
- [ ] build_all.sh senza warning
- [ ] backup data/helix.db
- [ ] .env con gate reali controllati
- [ ] HELIX_REAL_TRADING_ENABLED=false fino al test
- [ ] HELIX_ALLOW_COINBASE_ORDERS=false fino al test
- [ ] HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false fino al test
- [ ] HELIX_ALLOW_REAL_SLOT_SELL=false
- [ ] STOP_AFTER_REAL_ORDER=1
- [ ] max_orders_per_day=1
- [ ] micro_live_max_order_eur <= 10
- [ ] liquidity_reserve_percent >= 80
- [ ] acknowledge ultimo ordine reale già eseguito
- [ ] position_slots controllata
- [ ] trades legacy controllata
- [ ] order_journal controllato
- [ ] engine_audit controllato

## Modalità consentite ora

- SIMULATION
- LIVE_READONLY
- PAPER/SIM
- micro-live BUY solo con supervisione

## Modalità NON consentite ora

- SELL reale autonoma
- bot autonomo continuo
- più ordini reali consecutivi
- vendita wallet totale
- uso automatico riserva
- uso della riserva crisi per BUY ordinari

## Regola slot

Helix deve ragionare a slot/lotti sia in BUY sia in SELL.

Gli slot vanno calcolati sul capitale operativo, non sul capitale totale:

```text
reserve_eur = total_capital_eur * reserve_percent / 100
operational_capital_eur = total_capital_eur - reserve_eur
slot_size_eur = operational_capital_eur / max_slots
```

La riserva crisi non deve essere consumata da acquisti ordinari.

## Prossimo obiettivo

v0.3.0-rc3 slot-aware buy sizing / capital model.

## Regola

Non attivare SELL reale finché non esiste un test micro-live supervisionato con slot profittevole, gate espliciti e reconciliation confermata.
