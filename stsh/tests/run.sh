#!/usr/bin/env bash
# Deterministic functional tests for stsh, driven through a piped (non-tty)
# stdin: pipelines, redirection, background jobs, and the fg/jobs built-ins.
# Job-control signal forwarding (Ctrl-C / Ctrl-Z) is covered by jobcontrol_pty.py.
set -u
cd "$(dirname "$0")/.."
SH=./stsh
DATA=tests/data.txt
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0

check() { # name expected actual
  if [ "$3" = "$2" ]; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1"; echo "         expected: [$2]"; echo "         actual:   [$3]"; fail=$((fail+1)); fi
}
checkContains() { # name needle haystack
  if printf '%s' "$3" | grep -qF "$2"; then echo "  ok   $1"; pass=$((pass+1))
  else echo "  FAIL $1"; echo "         missing: [$2]"; echo "         in:      [$3]"; fail=$((fail+1)); fi
}

echo "== stsh functional tests =="

out=$(printf 'echo hello world | tr a-z A-Z\nquit\n' | $SH)
check "pipeline: echo | tr (uppercase)" "HELLO WORLD" "$out"

out=$(printf 'sort < %s | uniq -c | wc -l\nquit\n' "$DATA" | $SH | tr -d ' ')
check "3-stage: sort<data | uniq -c | wc -l = distinct count" "3" "$out"

out=$(printf 'cat < %s | wc -l\nquit\n' "$DATA" | $SH | tr -d ' ')
check "input redirection: cat<data | wc -l" "6" "$out"

printf 'sort < %s > %s/sorted.txt\nquit\n' "$DATA" "$TMP" | $SH >/dev/null
out=$(head -1 "$TMP/sorted.txt")
check "output redirection: sort<data>file (first line)" "apple" "$out"

out=$(printf 'echo abc | rev\nquit\n' | $SH)
check "pipeline: echo | rev" "cba" "$out"

out=$(printf 'sleep 3 &\njobs\nquit\n' | $SH)
checkContains "background: sleep 3 & shows in jobs as Running" "Running    sleep 3 &" "$out"

# fg blocks until the job finishes, which removes it: `jobs` then prints nothing.
out=$(printf 'sleep 1 &\nfg 1\njobs\nquit\n' | $SH | grep -cE 'Running|Stopped')
check "fg: brings bg job foreground, waits, reaps (no job left)" "0" "$out"

out=$(printf '| oops\nquit\n' | $SH 2>&1)
checkContains "parse error: leading pipe is rejected" "parse error" "$out"

out=$(printf 'nonexistent-cmd-xyz\nquit\n' | $SH 2>&1)
checkContains "exec failure reported for unknown command" "nonexistent-cmd-xyz" "$out"

echo
echo "== summary =="
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
