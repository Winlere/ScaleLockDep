#!/bin/bash
# run_tests.sh — Build lockdep + all tests, run under LD_PRELOAD, print summary & confusion matrix

set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
LOCKDEP_LIB="$REPO/lockdep/liblockdep.so"
TEST_DIR="$REPO/test"
TIMEOUT=5   # seconds; real deadlocks hang; lockdep may detect early (exit 66)

# ---------------------------------------------------------------------------
# Ground truth: 1 = deadlock expected, 0 = no deadlock expected.
# Categories match the "Expected:" line in each test_*.c header:
#   NON-DEADLOCK         expected outcome: no deadlock detected
#   DEADLOCK             expected outcome: deadlock detected (actual circular wait)
#   POTENTIAL-DEADLOCK   expected outcome: potential deadlock detected (graph cycle,
#                                          may not hang depending on scheduling)
# ---------------------------------------------------------------------------
declare -A GROUND_TRUTH=(
  [test_01]=0  [test_02]=0  [test_03]=0  [test_04]=0  [test_05]=0  [test_06]=0
  [test_07]=1  [test_08]=1  [test_09]=1  [test_10]=1
  [test_11]=1  [test_12]=0  [test_13]=1
  [test_14]=1  [test_15]=1  [test_16]=1
  [test_17]=0  [test_18]=0  [test_19]=0  [test_20]=0  [test_21]=0  [test_22]=0
  [test_23]=1  [test_24]=1  [test_25]=1  [test_26]=1
  [test_27]=1  [test_28]=1
  [test_29]=1  [test_30]=1
)

declare -A CATEGORY=(
  [test_01]="NON-DEADLOCK"        [test_02]="NON-DEADLOCK"        [test_03]="NON-DEADLOCK"
  [test_04]="NON-DEADLOCK"        [test_05]="NON-DEADLOCK"        [test_06]="NON-DEADLOCK"
  [test_07]="DEADLOCK"            [test_08]="DEADLOCK"            [test_09]="DEADLOCK"
  [test_10]="DEADLOCK"
  [test_11]="POTENTIAL-DEADLOCK"  [test_12]="NON-DEADLOCK"        [test_13]="POTENTIAL-DEADLOCK"
  [test_14]="DEADLOCK"            [test_15]="DEADLOCK"            [test_16]="DEADLOCK"
  [test_17]="NON-DEADLOCK"        [test_18]="NON-DEADLOCK"        [test_19]="NON-DEADLOCK"
  [test_20]="NON-DEADLOCK"        [test_21]="NON-DEADLOCK"        [test_22]="NON-DEADLOCK"
  [test_23]="DEADLOCK"            [test_24]="DEADLOCK"            [test_25]="DEADLOCK"
  [test_26]="DEADLOCK"
  [test_27]="POTENTIAL-DEADLOCK"  [test_28]="POTENTIAL-DEADLOCK"
  [test_29]="DEADLOCK"            [test_30]="DEADLOCK"
)

TESTS=(test_01 test_02 test_03 test_04 test_05 test_06
       test_07 test_08 test_09 test_10
       test_11 test_12 test_13
       test_14 test_15 test_16
       test_17 test_18 test_19 test_20 test_21 test_22
       test_23 test_24 test_25 test_26
       test_27 test_28
       test_29 test_30)

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
echo "=== Building lockdep library ==="
make -C "$REPO/lockdep" -s
echo "    $LOCKDEP_LIB"

echo ""
echo "=== Building tests ==="
make -C "$TEST_DIR" -s
echo "    All tests built."

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
echo ""
echo "=== Running tests (timeout=${TIMEOUT}s) ==="
printf "%-12s %-22s %-12s %-12s %-8s\n" "TEST" "CATEGORY" "EXPECTED" "DETECTED" "RESULT"
printf '%s\n' "----------------------------------------------------------------------"

declare -A DETECTED   # 1=deadlock, 0=no deadlock

TP=0; TN=0; FP=0; FN=0

for t in "${TESTS[@]}"; do
  bin="$TEST_DIR/$t"

  # Determine category label from explicit map
  cat="${CATEGORY[$t]:-UNKNOWN}"

  gt="${GROUND_TRUTH[$t]}"
  gt_label="$([ "$gt" = 1 ] && echo DEADLOCK || echo NO-DEADLOCK)"

  # Run under lockdep with timeout
  set +e
  output=$(timeout "$TIMEOUT" env LD_PRELOAD="$LOCKDEP_LIB" "$bin" 2>&1)
  rc=$?
  set -e

  # Classify result
  # rc=66  → lockdep detected a deadlock cycle
  # rc=124 → timeout (test hung = actual deadlock)
  # rc=0   → completed normally
  # other  → treat as deadlock (abnormal exit)
  if   [ "$rc" -eq 66 ] || [ "$rc" -eq 124 ]; then
    detected=1
    det_label="DEADLOCK"
  elif [ "$rc" -eq 0 ]; then
    detected=0
    det_label="NO-DEADLOCK"
  else
    detected=1
    det_label="DEADLOCK(rc=$rc)"
  fi

  DETECTED[$t]=$detected

  # Confusion matrix tally
  if   [ "$gt" -eq 1 ] && [ "$detected" -eq 1 ]; then verdict="TP"; TP=$(( TP + 1 ))
  elif [ "$gt" -eq 0 ] && [ "$detected" -eq 0 ]; then verdict="TN"; TN=$(( TN + 1 ))
  elif [ "$gt" -eq 0 ] && [ "$detected" -eq 1 ]; then verdict="FP"; FP=$(( FP + 1 ))
  else                                                  verdict="FN"; FN=$(( FN + 1 ))
  fi

  printf "%-12s %-22s %-12s %-22s %-8s\n" "$t" "$cat" "$gt_label" "$det_label" "$verdict"
done

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
TOTAL=$(( TP + TN + FP + FN ))
CORRECT=$(( TP + TN ))
ACC=$(awk "BEGIN { printf \"%.1f\", 100.0 * $CORRECT / $TOTAL }")
PREC_DENOM=$(( TP + FP ))
REC_DENOM=$(( TP + FN ))
PREC=$([ "$PREC_DENOM" -gt 0 ] && awk "BEGIN { printf \"%.1f\", 100.0 * $TP / $PREC_DENOM }" || echo "N/A")
REC=$([ "$REC_DENOM"  -gt 0 ] && awk "BEGIN { printf \"%.1f\", 100.0 * $TP / $REC_DENOM  }" || echo "N/A")

echo ""
echo "=== Confusion Matrix (Positive = DEADLOCK) ==="
echo ""
printf "                     Predicted\n"
printf "                  DEADLOCK   NO-DEADLOCK\n"
printf "Actual DEADLOCK   %6d     %6d\n"    "$TP" "$FN"
printf "       NO-DEADLOCK%6d     %6d\n"    "$FP" "$TN"
echo ""
echo "=== Summary ==="
printf "  Total tests : %d\n"   "$TOTAL"
printf "  Correct     : %d\n"   "$CORRECT"
printf "  Accuracy    : %s%%\n" "$ACC"
printf "  Precision   : %s%%\n" "$PREC"
printf "  Recall      : %s%%\n" "$REC"
printf "  TP=%d  TN=%d  FP=%d  FN=%d\n" "$TP" "$TN" "$FP" "$FN"
