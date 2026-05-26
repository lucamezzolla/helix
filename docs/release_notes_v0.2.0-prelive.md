# Helix v0.2.0-prelive

This milestone closes the pre-live architecture phase.

## Summary

Helix can now run as a cautious read-only/dry-run trading engine with:

- Coinbase read-only integration
- Coinbase order preview
- internal fee-aware trade preview
- liquidity reserve management
- runtime safety checks
- final live gate
- live execution lock
- dry-run executor
- order journal
- pre-live report
- status snapshot export
- audit retention and hard cap

## Real trading status

Real order transport code exists for future controlled builds, but the normal Makefile does not enable it.

This release must be considered:

```text
NOT REAL-TRADING ENABLED
```

## Recommended next phase

Run extended validation in:

```text
LIVE_READONLY + dry-run executor
```

Then evaluate a separate `live-candidate` branch for micro-live testing.
