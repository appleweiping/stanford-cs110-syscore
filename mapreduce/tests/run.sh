#!/usr/bin/env bash
# Runs the word-count MapReduce over tests/input.txt with M=4 mappers and R=3
# reducers, then verifies the merged output: exact per-word counts, the total
# token count, distinct-key count, and that no key was split across partitions.
set -u
cd "$(dirname "$0")/.."
pass=0; fail=0
check() { if [ "$3" = "$2" ]; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1"; echo "         expected [$2] got [$3]"; fail=$((fail+1)); fi; }
checkContains() { if printf '%s' "$3" | grep -qxF "$2"; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1 (missing line '$2')"; fail=$((fail+1)); fi; }

rm -rf tests/out
./mr tests/input.txt 4 3 ./mapper ./reducer tests/out

merged=$(cat tests/out/part-* 2>/dev/null | sort)
echo "== merged output =="
printf '%s\n' "$merged" | sed 's/^/  /'
echo "== checks =="

checkContains "alpha total = 16" "$(printf 'alpha\t16')" "$merged"
checkContains "beta total = 12"  "$(printf 'beta\t12')"  "$merged"
checkContains "gamma total = 16" "$(printf 'gamma\t16')" "$merged"
checkContains "delta total = 4"  "$(printf 'delta\t4')"  "$merged"

distinct=$(cat tests/out/part-* | wc -l | tr -d ' ')
check "4 distinct keys across all partitions" "4" "$distinct"

dup=$(cat tests/out/part-* | cut -f1 | sort | uniq -d | wc -l | tr -d ' ')
check "no key split across partitions (shuffle correct)" "0" "$dup"

sum=$(cat tests/out/part-* | cut -f2 | awk '{s+=$1} END{print s}')
check "sum of counts = 48 tokens (no data lost)" "48" "$sum"

echo
echo "== summary =="
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
