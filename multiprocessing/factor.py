#!/usr/bin/env python3
"""
factor.py — a self-halting integer-factorization worker for farm.cc.

In --self-halting mode the worker loops:
  1. SIGSTOP itself (tells the manager "I'm ready for a number").
  2. On SIGCONT, read one integer from stdin.
  3. Factor it, print "<n> = p1 * p2 * ..." to stdout.
Reaching EOF on stdin ends the loop.

Without --self-halting it behaves as a plain filter: factor every line of stdin.
"""

import sys
import os
import signal


def factorize(n):
    n = int(n)
    if n < 2:
        return [n] if n >= 0 else []
    factors = []
    d = 2
    m = n
    while d * d <= m:
        while m % d == 0:
            factors.append(d)
            m //= d
        d += 1 if d == 2 else 2
    if m > 1:
        factors.append(m)
    return factors


def emit(n):
    factors = factorize(n)
    body = " * ".join(str(f) for f in factors) if factors else str(n)
    sys.stdout.write("{} = {}\n".format(n, body))
    sys.stdout.flush()


def main():
    self_halting = "--self-halting" in sys.argv
    if not self_halting:
        for line in sys.stdin:
            line = line.strip()
            if line:
                emit(line)
        return

    # Self-halting protocol: stop, get resumed, read one number, repeat.
    while True:
        os.kill(os.getpid(), signal.SIGSTOP)
        line = sys.stdin.readline()
        if line == "":          # EOF -> manager is done with us
            break
        line = line.strip()
        if line:
            emit(line)


if __name__ == "__main__":
    main()
