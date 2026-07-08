/**
 * File: pipeline.h
 * ----------------
 * Exports pipeline(): the moral equivalent of the shell's  argv1 | argv2.
 * argv1's stdout is wired to argv2's stdin via an anonymous pipe; argv1 inherits
 * this process's stdin and argv2 inherits this process's stdout, so the caller
 * controls the two open ends by redirecting its own descriptors if desired.
 */

#pragma once
#include <unistd.h>  // for pid_t

/**
 * Launches argv1 and argv2 as two processes joined by a pipe. The two child
 * pids are written into pids[0] (argv1) and pids[1] (argv2). The caller is
 * responsible for waitpid()-ing on both.
 */
void pipeline(char *argv1[], char *argv2[], pid_t pids[]);
