#!/usr/bin/env bash
# End-to-end test for stnewsaggregator. Serves a small local site (a feed list,
# two feeds, and three articles — one shared between both feeds to exercise
# de-duplication), runs the aggregator against it, and queries the index.
set -u
cd "$(dirname "$0")/.."
PORT=$(( (RANDOM % 2000) + 26000 ))
WEB=$(mktemp -d)
ERR=$(mktemp)
ACCESS=$(mktemp)
BASE="http://127.0.0.1:$PORT"
pass=0; fail=0

check() { if [ "$3" = "$2" ]; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1"; echo "         expected [$2] got [$3]"; fail=$((fail+1)); fi; }
checkContains() { if printf '%s' "$3" | grep -qF "$2"; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1 (missing '$2')"; fail=$((fail+1)); fi; }

# Instantiate the templated site with the real base URL.
for f in tests/site/*; do
  sed "s#__BASE__#$BASE#g" "$f" > "$WEB/$(basename "$f")"
done

python3 tests/serve.py "$PORT" "$WEB" 2>"$ACCESS" & SPID=$!
trap 'kill $SPID 2>/dev/null; rm -rf "$WEB" "$ERR" "$ACCESS"' EXIT
sleep 1

echo "== stnewsaggregator end-to-end test =="
out=$(printf 'systems\nthreads\nnetworking\nzzzznope\n' | ./aggregate "$BASE/feeds.xml" 2>"$ERR")
err=$(cat "$ERR")

checkContains "dedup: 3 distinct articles from 2 feeds (a2 shared)" "indexed 3 articles from 2 feeds" "$err"
checkContains "query 'systems' matches all 3 articles" '3 article(s) contain "systems"' "$out"
# a1 must rank first for 'systems' with count 4 (title + 3 in <p>; <script> skipped)
top=$(printf '%s' "$out" | grep -A1 '3 article(s) contain "systems"' | tail -1)
checkContains "ranking: 'Operating Systems Intro' top hit for systems" "Operating Systems Intro" "$top"
checkContains "count: script-block 'systems' excluded (count = 4, not 8)" "4	Operating Systems Intro" "$out"
checkContains "query 'threads' matches exactly 1 article" '1 article(s) contain "threads"' "$out"
checkContains "query 'networking' matches exactly 1 article" '1 article(s) contain "networking"' "$out"
checkContains "missing word reports no matches" 'No matches for "zzzznope"' "$out"

echo
echo "== origin access log (note: a2.html fetched once, though in both feeds) =="
sort "$ACCESS" | sed -E 's/.*"(GET [^"]*)".*/  \1/' | sort | uniq -c

echo
echo "== summary =="
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
