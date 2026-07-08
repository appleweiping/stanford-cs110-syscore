#!/usr/bin/env bash
# End-to-end tests for stproxy: start a local origin + the proxy, then drive
# real requests through the proxy with curl and assert forwarding, header
# injection, caching, the blocklist, and concurrency.
set -u
cd "$(dirname "$0")/.."
PROXY=./proxy
OPORT=$(( (RANDOM % 2000) + 20000 ))
PPORT=$(( (RANDOM % 2000) + 23000 ))
LOG=$(mktemp)
pass=0; fail=0

check() { if [ "$3" = "$2" ]; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1"; echo "         expected [$2] got [$3]"; fail=$((fail+1)); fi; }
checkContains() { if printf '%s' "$3" | grep -qF "$2"; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1 (missing '$2')"; echo "         in: $3"; fail=$((fail+1)); fi; }

python3 tests/origin.py "$OPORT" & OPID=$!
$PROXY "$PPORT" blocklist.txt 2>"$LOG" & PROXYPID=$!
trap 'kill $OPID $PROXYPID 2>/dev/null; rm -f "$LOG"' EXIT
sleep 1

C="curl -s -x http://127.0.0.1:$PPORT"
O="http://127.0.0.1:$OPORT"

echo "== stproxy end-to-end tests =="

b1=$($C $O/page)
checkContains "forward: GET returns origin content" "path=/page" "$b1"
checkContains "header: x-forwarded-for injected (127.0.0.1)" "xff=127.0.0.1" "$b1"

b2=$($C $O/page)
check "cache: 2nd GET byte-identical (served from cache)" "$b1" "$b2"

cnt=$($C $O/count)
checkContains "cache: origin hit only once for /page (count reads 2)" "origin-hits=2" "$cnt"

code=$(curl -s -o /dev/null -w '%{http_code}' -x http://127.0.0.1:$PPORT http://blocked.test/x)
check "blocklist: blocked host returns 403" "403" "$code"

# concurrency: 25 parallel distinct requests must all be served correctly
tmp=$(mktemp -d)
cpids=""
for i in $(seq 25); do
  ( $C $O/p$i | grep -q "path=/p$i" && : > "$tmp/$i" ) &
  cpids="$cpids $!"
done
wait $cpids
served=$(ls "$tmp" | wc -l); rm -rf "$tmp"
check "concurrency: 25 parallel requests all served" "25" "$served"

sleep 0.2
log=$(cat "$LOG")
checkContains "log: records a cache [HIT]" "[HIT]" "$log"
checkContains "log: records a [MISS] + cache TTL" "[MISS]" "$log"
checkContains "log: records a [BLOCK]" "[BLOCK]" "$log"

echo
echo "== summary =="
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
