# Helix - Legacy Import Notes

## Data import

Imported previous BTC purchases from the external Excel history into the local Helix database.

## Imported records

- 5 legacy BUY records inserted into `position_slots`
- 5 legacy BUY records inserted into `trades`
- Source marker: `LEGACY_EXCEL`
- Legacy slot IDs use `buy_order_id` prefix: `LEGACY-`

## Current slot model after import

- Total configured slots: 7
- Open slots in `position_slots`: 6
  - 5 legacy slots from Excel
  - 1 real Helix micro-live BUY
- Paper slots cleared

## Important distinction

`position_slots` is the source of truth for slot/l lot management.

`trades` is used as readable trade history / compatibility history.

`order_journal` remains the source of truth for Helix-generated order previews, real order attempts and reconciliations.

`engine_audit` remains the source of truth for engine decisions.

## Next strategy rule

Helix must reason by slots both in BUY and SELL.

Slot sizing must use operational capital, not total capital:

```text
reserve_eur = total_capital_eur * reserve_percent / 100
operational_capital_eur = total_capital_eur - reserve_eur
slot_size_eur = operational_capital_eur / max_slots

