/**
 * File: subprocess-test.cc
 * ------------------------
 * Exercises subprocess() the way CS110's assign3 does: spawn `sort` with both
 * its stdin and stdout wired to the parent, publish a shuffled word list down
 * the child's stdin, then ingest the sorted result back out of its stdout.
 *
 * Verifies (with an in-process check) that what comes back is the input words
 * in ascending order — proving the pipe plumbing and descriptor discipline are
 * correct end-to-end.
 */

#include "subprocess.h"
#include "subprocess-exception.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

int main() {
  char sortArg[] = "sort";
  char *argv[] = {sortArg, nullptr};

  subprocess_t sp = subprocess(argv, /*supplyChildInput=*/true,
                                     /*supplyChildOutput=*/true);

  const std::vector<std::string> words = {
      "felicity", "umbrage", "susurrus", "halcyon", "pulchritude",
      "ablution", "somnambulist", "pettifogger", "quixotic", "ephemeral"};

  // Feed the child, then close the supply end so sort sees EOF and emits output.
  for (const std::string &w : words) {
    std::string line = w + "\n";
    if (write(sp.supplyfd, line.c_str(), line.size()) == -1) {
      perror("write");
      return 1;
    }
  }
  close(sp.supplyfd);

  // Ingest the sorted stream.
  std::string received;
  char buf[256];
  ssize_t n;
  while ((n = read(sp.ingestfd, buf, sizeof(buf))) > 0)
    received.append(buf, n);
  close(sp.ingestfd);

  int status;
  waitpid(sp.pid, &status, 0);

  std::cout << "sort said:\n" << received;

  // Independent correctness check.
  std::vector<std::string> expected = words;
  std::sort(expected.begin(), expected.end());
  std::string expectedStr;
  for (const std::string &w : expected) expectedStr += w + "\n";

  bool ok = (received == expectedStr) && WIFEXITED(status) && WEXITSTATUS(status) == 0;
  std::cout << (ok ? "PASS: output is correctly sorted\n"
                   : "FAIL: output mismatch\n");
  return ok ? 0 : 1;
}
