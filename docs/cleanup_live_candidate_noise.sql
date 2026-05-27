-- Helix live-candidate cleanup
-- Mantiene lo storico reale: NON tocca trades, bot_state, settings
-- e NON cancella eventuali righe REAL_SENT o POST_ORDER_RECON* reali.
--
-- Uso:
--   cp data/helix.db data/helix_before_cleanup.db
--   sqlite3 data/helix.db < docs/cleanup_live_candidate_noise.sql

BEGIN TRANSACTION;

-- Rumore diagnostico generato dai test micro-live:
-- preview BUY/SELL bloccate o pronte, senza coinbase_order_id e senza ordine reale.
DELETE FROM order_journal
WHERE dry_run = 1
  AND status IN ('DRY_RUN_BLOCKED', 'DRY_RUN_READY')
  AND COALESCE(coinbase_order_id, '') = ''
  AND (
      client_order_id LIKE 'helix-dryrun-%'
      OR client_order_id LIKE 'helix-live-%'
  );

-- Blocchi locali dell'executor senza invio a Coinbase: non sono ordini reali.
DELETE FROM order_journal
WHERE dry_run = 0
  AND status = 'REAL_BLOCKED'
  AND COALESCE(coinbase_order_id, '') = '';

-- Stati ordine dry-run/recovered rimasti in order_state.
DELETE FROM order_state
WHERE dry_run = 1
   OR status IN ('DRY_RUN_RECOVERED', 'PLANNED');

-- Audit rumorosi dei test. Mantiene trades e stato reale.
DELETE FROM engine_audit
WHERE event_type IN (
    'OPERATIONAL_LIMITS',
    'RISK_GUARD',
    'EXCHANGE_SAFETY_BUY',
    'EXCHANGE_SAFETY_SELL',
    'RUNTIME_SAFETY_BUY',
    'RUNTIME_SAFETY_BUY_PREVIEW',
    'MICRO_LIVE_ACCUMULATION',
    'ORDER_EXECUTOR_BUY',
    'POST_ORDER_RECONCILIATION'
)
AND reason NOT LIKE '%REAL_SENT%'
AND reason NOT LIKE '%coinbase_order_id%';

COMMIT;

VACUUM;
