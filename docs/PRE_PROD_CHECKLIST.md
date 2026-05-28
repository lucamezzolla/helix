HELIX - Pre-produzione controllata v0.3.0

Stato corrente:
- Branch: live-candidate
- Ultimo tag: v0.2.4-sell-path-readiness
- SELL reale: NON attiva
- BUY reale micro: già validato
- Slot/lotti: attivi
- Paper BEST_PROFIT: validato
- Gate SELL reale slot-based: presente

Prima di qualunque test reale:
[ ] git status pulito
[ ] build_all.sh senza warning
[ ] backup data/helix.db
[ ] .env con gate reali controllati
[ ] HELIX_REAL_TRADING_ENABLED=false fino al test
[ ] HELIX_ALLOW_COINBASE_ORDERS=false fino al test
[ ] HELIX_I_UNDERSTAND_REAL_MONEY_RISK=false fino al test
[ ] HELIX_ALLOW_REAL_SLOT_SELL=false
[ ] emergency stop disattivabile/attivabile da UI
[ ] STOP_AFTER_REAL_ORDER=1
[ ] max_orders_per_day=1
[ ] micro_live_max_order_eur <= 10
[ ] liquidity_reserve_percent >= 80
[ ] acknowledge ultimo ordine reale già eseguito
[ ] position_slots controllata
[ ] order_journal controllato
[ ] engine_audit controllato

Modalità consentite ora:
[ ] SIMULATION
[ ] LIVE_READONLY
[ ] PAPER/SIM
[ ] micro-live BUY solo con supervisione

Modalità NON consentite ora:
[ ] SELL reale
[ ] bot autonomo continuo
[ ] più ordini reali consecutivi
[ ] vendita wallet totale
[ ] uso automatico riserva

Prossimo obiettivo:
v0.3.0-rc1 guarded live slot trading

Regola:
Non attivare SELL reale finché non esiste reconciliation SELL completa con chiusura slot CLOSED solo dopo FILLED.
