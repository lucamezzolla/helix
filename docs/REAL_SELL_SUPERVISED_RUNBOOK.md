# Helix - Runbook SELL reale supervisionato

Versione documento: v0.3.0-rc9 draft  
Branch previsto: `real-sell-supervised`  
Scopo: eseguire **un solo SELL reale supervisionato**, solo quando Helix segnala che uno slot reale è profittevole.

---

## 1. Regola principale

Il SELL reale non deve essere usato per “provare se funziona”.

Il SELL reale si valuta solo se Helix produce una diagnosi coerente e profittevole su uno slot reale aperto.

Condizioni minime richieste:

- Helix è in build live: `./helix-live`.
- Modalità operativa controllata.
- `micro_live_stop_after_real_order=1` o equivalente attivo.
- Nessuna condizione di linea/API degradata.
- Coinbase wallet raggiungibile.
- Reconciliation OK recente.
- Esiste almeno uno slot `OPEN` in `position_slots`.
- Helix ha scelto uno slot con `BEST_PROFIT_PROFITABLE_PREVIEW_ONLY`.
- È presente un evento `REAL_SLOT_SELL_PLAN`.
- Il piano è coerente con importo, fee, netto, costo e profitto.
- Si accetta manualmente di eseguire **un solo ordine reale**.

Se una di queste condizioni manca, non procedere.

---

## 2. Quando NON vendere

Non attivare SELL reale se trovi una di queste condizioni:

- `BEST_PROFIT_NOT_PROFITABLE`
- `SELL_SLOT_NOT_PROFITABLE`
- `SELL_SLOT_PREVIEW_UNAVAILABLE`
- `LIVE_READONLY ERROR`
- `wallet remoto non connesso`
- `errore curl durante order preview`
- `RECONCILIATION` non recente o non OK
- `position_slots` incoerenti
- `order_journal` mostra ordini reali pendenti
- linea/API rossa o offline nella UI
- BTC balance non coerente con gli slot aperti
- dubbi sull’importo che Helix sta per vendere

In particolare: se lo slot migliore è negativo, anche di poco, non si vende.

---

## 3. Preparazione ambiente

Prima di toccare `.env`, salvare una copia locale non versionata:

```bash
cd ~/Documenti/C/helix
cp .env .env.before-real-sell-$(date +%Y%m%d_%H%M%S)
```

Verificare che `.env` non sia tracciato da Git:

```bash
git status --ignored | grep -E '(^|/)\.env' || true
```

Controllare il branch:

```bash
git branch --show-current
git status
```

Branch previsto:

```text
real-sell-supervised
```

---

## 4. Build live pulita

```bash
cd ~/Documenti/C/helix
rm -f helix helix-live
./build_all.sh
```

La build deve completare senza errori.

---

## 5. Query pre-check

Generare un file di controllo prima di attivare qualunque gate SELL reale:

```bash
cd ~/Documenti/C/helix

{
  echo "===== HELIX REAL SELL PRECHECK ====="
  date
  echo

  echo "===== GIT ====="
  git branch --show-current
  git status
  git log --oneline -8
  echo

  echo "===== VERSION ====="
  cat VERSION
  echo

  echo "===== BOT STATE ====="
  sqlite3 -header -column data/helix.db "
  SELECT id,running,mode,eur_balance,btc_balance,current_price,avg_buy_price,last_trade,used_slots,max_slots
  FROM bot_state;
  "
  echo

  echo "===== OPEN POSITION SLOTS ====="
  sqlite3 -header -column data/helix.db "
  SELECT id,buy_order_id,buy_client_order_id,base_size_btc,cost_eur,buy_fee_eur,
         cost_eur + buy_fee_eur AS allocated_cost,
         avg_buy_price,status,opened_at,closed_at,sell_order_id,sell_net_eur,realized_profit_eur
  FROM position_slots
  WHERE status='OPEN'
  ORDER BY opened_at ASC, id ASC;
  "
  echo

  echo "===== LATEST RECONCILIATION ====="
  sqlite3 -header -column data/helix.db "
  SELECT created_at,event_type,decision,reason
  FROM engine_audit
  WHERE event_type='RECONCILIATION'
  ORDER BY id DESC
  LIMIT 10;
  "
  echo

  echo "===== LINE / LIVE READONLY AUDIT ====="
  sqlite3 -header -column data/helix.db "
  SELECT created_at,event_type,decision,reason
  FROM engine_audit
  WHERE event_type='LIVE_READONLY'
     OR reason LIKE '%wallet remoto non connesso%'
     OR reason LIKE '%curl%'
     OR reason LIKE '%conn%'
  ORDER BY id DESC
  LIMIT 30;
  "
  echo

  echo "===== SELL SLOT AUDIT ====="
  sqlite3 -header -column data/helix.db "
  SELECT created_at,event_type,decision,reason,eur_amount,estimated_fee,net_profit
  FROM engine_audit
  WHERE event_type IN ('SELL_SLOT_SELECTION','REAL_SLOT_SELL_PLAN','EXCHANGE_SAFETY_SELL','SLOT_CLOSE_RECONCILIATION')
  ORDER BY id DESC
  LIMIT 50;
  "
  echo

  echo "===== REAL ORDERS ====="
  sqlite3 -header -column data/helix.db "
  SELECT id,client_order_id,side,dry_run,status,phase,decision,
         coinbase_order_id,execution_decision,execution_reason,created_at
  FROM order_journal
  WHERE dry_run=0
  ORDER BY id DESC
  LIMIT 50;
  "
} > helix_real_sell_precheck_$(date +%Y%m%d_%H%M%S).txt

ls -lh helix_real_sell_precheck_*.txt | tail -3
```

Leggere il file prima di procedere.

---

## 6. Stato atteso prima dell’abilitazione

Nel pre-check deve comparire uno scenario simile:

```text
SELL_SLOT_SELECTION | BEST_PROFIT_PROFITABLE_PREVIEW_ONLY
REAL_SLOT_SELL_PLAN | REAL_SLOT_SELL_BLOCKED_BY_ENV_GATE
```

oppure:

```text
REAL_SLOT_SELL_PLAN | REAL_SLOT_SELL_READY_BUT_NOT_EXECUTED
```

ma solo se il gate di ambiente è già attivo e il codice è comunque ancora in modalità di non esecuzione finale.

Non procedere se compare solo:

```text
BEST_PROFIT_NOT_PROFITABLE
SELL_SLOT_NOT_PROFITABLE
```

---

## 7. Attivazione gate SELL reale

Aprire `.env` e impostare solo per il test supervisionato:

```text
HELIX_ALLOW_REAL_SLOT_SELL=true
```

Tutti gli altri gate di sicurezza devono restare conservativi.

Non attivare BUY reale se lo scopo è testare SELL.

---

## 8. Avvio test supervisionato

Avviare la build live:

```bash
cd ~/Documenti/C/helix
./helix-live
```

Controllare dalla UI:

- Linea/API verde.
- Modalità corretta.
- Stato bot chiaro.
- Nessun warning nuovo.
- Slot aperti coerenti.

Avviare il bot solo quando si è pronti a guardare l’esecuzione.

Restare davanti alla UI durante il test.

---

## 9. Regola “un solo ordine”

Dopo il primo ordine reale:

- Helix deve fermarsi o essere fermato manualmente.
- Non lasciare il bot libero di eseguire altri ordini.
- Non cambiare impostazioni mentre l’ordine è in corso.
- Attendere reconciliation.

Se l’ordine viene accettato da Coinbase, verificare subito `order_journal`.

---

## 10. Verifica post SELL reale

Dopo il test, generare il report:

```bash
cd ~/Documenti/C/helix

{
  echo "===== HELIX REAL SELL POSTCHECK ====="
  date
  echo

  echo "===== BOT STATE ====="
  sqlite3 -header -column data/helix.db "
  SELECT id,running,mode,eur_balance,btc_balance,current_price,avg_buy_price,last_trade,used_slots,max_slots
  FROM bot_state;
  "
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

  echo "===== SELL AUDIT ====="
  sqlite3 -header -column data/helix.db "
  SELECT created_at,event_type,decision,reason,eur_amount,estimated_fee,net_profit
  FROM engine_audit
  WHERE event_type IN ('SELL_SLOT_SELECTION','REAL_SLOT_SELL_PLAN','EXCHANGE_SAFETY_SELL','SLOT_CLOSE_RECONCILIATION','POST_ORDER_RECONCILIATION')
  ORDER BY id DESC
  LIMIT 80;
  "
  echo

  echo "===== REAL SELL ORDERS ====="
  sqlite3 -header -column data/helix.db "
  SELECT id,client_order_id,side,dry_run,status,phase,decision,
         coinbase_order_id,execution_decision,execution_reason,created_at
  FROM order_journal
  WHERE dry_run=0 AND side='SELL'
  ORDER BY id DESC
  LIMIT 30;
  "
} > helix_real_sell_postcheck_$(date +%Y%m%d_%H%M%S).txt

ls -lh helix_real_sell_postcheck_*.txt | tail -3
```

Verifiche attese:

- Uno slot passa da `OPEN` a `CLOSED`.
- `sell_order_id` valorizzato.
- `sell_net_eur` valorizzato.
- `realized_profit_eur` coerente.
- `used_slots` diminuisce o viene derivato correttamente dagli slot aperti.
- Nessun secondo SELL reale parte automaticamente.

---

## 11. Spegnimento gate dopo test

Subito dopo il test:

```text
HELIX_ALLOW_REAL_SLOT_SELL=false
```

Poi rebuild o riavvio se necessario.

Verificare:

```bash
grep HELIX_ALLOW_REAL_SLOT_SELL .env
```

Deve tornare:

```text
HELIX_ALLOW_REAL_SLOT_SELL=false
```

---

## 12. Criteri di successo

Il test è riuscito se:

- è partito un solo SELL reale;
- Coinbase ha accettato l’ordine;
- reconciliation ha chiuso lo slot corretto;
- il profitto realizzato è coerente con la preview;
- Helix si è fermato o non ha mandato altri ordini;
- DB e wallet restano coerenti;
- il report email/visualizza mostrano dati comprensibili.

---

## 13. Criteri di stop immediato

Fermare tutto se:

- Linea/API diventa offline;
- compare errore Coinbase;
- l’ordine resta in stato ambiguo;
- gli slot non si aggiornano;
- compare un secondo piano SELL;
- il profitto calcolato non coincide con la preview;
- il wallet non torna coerente;
- Helix non si ferma dopo l’ordine.

---

## 14. Nota finale

Questo runbook non autorizza trading autonomo.

Serve solo per il primo SELL reale supervisionato, manualmente osservato, con capitale e rischio limitati.
