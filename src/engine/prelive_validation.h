#ifndef HELIX_PRELIVE_VALIDATION_H
#define HELIX_PRELIVE_VALIDATION_H

typedef struct {
    int allowed;
    int required_successful_dry_runs;
    int successful_dry_runs;
    char decision[32];
    char reason[256];
} PreLiveValidationCheck;

/*
 * Final pre-live validation counter.
 *
 * This does not execute orders. It only checks whether Helix has accumulated
 * enough successful dry-run order paths before any future real execution can
 * even be considered.
 */
PreLiveValidationCheck prelive_validation_check(void);

#endif
