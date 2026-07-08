#!/usr/bin/env bash
# Builds the sample database from tests/reviews.tsv, then exercises dbase_test
# and amazon_search: record access, single-word / phrase / multi-term (AND)
# queries, sorting, and (if available) a valgrind leak check.
set -u
cd "$(dirname "$0")/.."
pass=0; fail=0
check() { if [ "$3" = "$2" ]; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1"; echo "         expected [$2] got [$3]"; fail=$((fail+1)); fi; }
checkContains() { if printf '%s' "$3" | grep -qF "$2"; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1 (missing '$2')"; fail=$((fail+1)); fi; }

echo "== building sample database =="
./mkamazondb tests/reviews.tsv tests sample

echo "== amazon reviews search tests =="
D="-d tests -f sample"

total=$(printf '\n' | ./dbase_test $D)
checkContains "dbase_test reports 10 reviews" "Total number of reviews: 10" "$total"

r6=$(./dbase_test $D 6)
checkContains "dbase_test: review #6 body via mmap+offset table" "supercalifragilistic" "$r6"

hp=$(./amazon_search $D headphones)
checkContains "single-word 'headphones' -> 3 reviews" "Found 3 matching reviews out of 10" "$hp"

phrase=$(./amazon_search $D '"works great"')
checkContains "phrase '\"works great\"' -> 3 reviews (consecutive only)" "Found 3 matching reviews" "$phrase"

andq=$(./amazon_search $D works great)
checkContains "AND 'works great' -> 4 reviews (incl non-adjacent USB Cable)" "Found 4 matching reviews" "$andq"

uniq=$(./amazon_search $D supercalifragilistic)
checkContains "rare word -> exactly 1 review" "Found 1 matching reviews" "$uniq"

none=$(./amazon_search $D zxcvbnmqwerty)
checkContains "unknown word -> 0 reviews" "Found 0 matching reviews" "$none"

hi=$(./amazon_search $D -k stars -r -n 1 headphones)
checkContains "sort by stars, reversed, top hit is 5-star" "5 star(s)" "$hi"

lo=$(./amazon_search $D -k stars -n 1 headphones)
checkContains "sort by stars, ascending, first hit is 4-star" "4 star(s)" "$lo"

if command -v valgrind >/dev/null 2>&1; then
  valgrind --error-exitcode=42 --leak-check=full --errors-for-leak-kinds=definite \
    ./amazon_search $D works great >/dev/null 2>tests/vg.log
  check "valgrind: no definite leaks / errors" "0" "$?"
  rm -f tests/vg.log
else
  echo "  --   valgrind not installed, skipping leak check"
fi

echo
echo "== summary =="
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
