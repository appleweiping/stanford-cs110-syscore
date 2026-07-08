#!/usr/bin/env python3
"""
Interactive job-control test for stsh, driven over a real pseudo-terminal so the
terminal-control path (tcsetpgrp) and keyboard signals are genuinely exercised:

  Ctrl-Z (0x1a)  -> SIGTSTP to the foreground process group  -> job stops
  bg <n>         -> SIGCONT, job runs in the background
  slay %<n>      -> SIGKILL the whole group
  Ctrl-C (0x03)  -> SIGINT to the foreground group; at an idle prompt the shell
                    must survive (nothing to forward to)

Exits 0 iff every assertion holds.
"""
import os, pty, select, time, sys

STSH = "./stsh"
transcript = ""
results = []


def spawn():
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(STSH, [STSH])
        os._exit(127)
    return pid, fd


def drain(fd, seconds):
    global transcript
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                chunk = os.read(fd, 4096)
            except OSError:
                break
            if not chunk:
                break
            transcript += chunk.decode(errors="replace")


def send(fd, data):
    os.write(fd, data)


def check(name, condition):
    results.append((name, condition))
    print(f"  {'ok  ' if condition else 'FAIL'}{name}")


def main():
    global transcript
    pid, fd = spawn()
    drain(fd, 0.5)  # startup + first prompt

    # 1. Foreground job, then Ctrl-Z stops it.
    before = len(transcript)
    send(fd, b"sleep 100\n")
    drain(fd, 0.5)
    send(fd, b"\x1a")            # Ctrl-Z
    drain(fd, 1.0)
    stopped_seg = transcript[before:]
    check("Ctrl-Z stops fg job (reports Stopped)", "Stopped" in stopped_seg and "sleep 100" in stopped_seg)

    # 2. jobs shows it as Stopped.
    before = len(transcript)
    send(fd, b"jobs\n")
    drain(fd, 0.5)
    check("jobs lists the stopped job", "Stopped" in transcript[before:])

    # 3. bg resumes it; jobs now shows Running.
    send(fd, b"bg 1\n")
    drain(fd, 0.5)
    before = len(transcript)
    send(fd, b"jobs\n")
    drain(fd, 0.5)
    check("bg resumes job (jobs shows Running)", "Running" in transcript[before:])

    # 4. slay the whole group; job disappears.
    send(fd, b"slay %1\n")
    drain(fd, 0.8)
    before = len(transcript)
    send(fd, b"jobs\n")
    drain(fd, 0.5)
    seg = transcript[before:]
    check("slay %1 removes the job (jobs empty)", "Running" not in seg and "Stopped" not in seg)

    # 5. Ctrl-C at an idle prompt must NOT kill the shell.
    send(fd, b"\x03")
    drain(fd, 0.3)
    before = len(transcript)
    send(fd, b"echo alive\n")
    drain(fd, 0.5)
    check("shell survives Ctrl-C at idle prompt", "alive" in transcript[before:])

    # 6. Foreground Ctrl-C terminates the job and returns control.
    send(fd, b"sleep 100\n")
    drain(fd, 0.4)
    send(fd, b"\x03")           # Ctrl-C -> SIGINT to fg group
    drain(fd, 0.6)
    before = len(transcript)
    send(fd, b"echo back\n")
    drain(fd, 0.6)
    check("Ctrl-C kills fg job, prompt returns", "back" in transcript[before:])

    send(fd, b"quit\n")
    drain(fd, 0.4)
    try:
        os.close(fd)
    except OSError:
        pass

    passed = sum(1 for _, c in results if c)
    print(f"\n== summary ==\n{passed}/{len(results)} passed")
    if passed != len(results):
        print("\n--- transcript ---")
        print(transcript)
    sys.exit(0 if passed == len(results) else 1)


if __name__ == "__main__":
    main()
