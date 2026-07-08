/**
 * runtests.c  --  assertion-based test harness for the v6 filesystem reader.
 * Verifies inode_iget, inode_indexlookup (small AND large/indirect files),
 * file_getblock byte counts, directory_findname, and pathname_lookup against
 * the known contents baked into the mkfs_v6 image. Exits non-zero on failure.
 */
#include "unixfilesystem.h"
#include "diskimg.h"
#include "inode.h"
#include "file.h"
#include "directory.h"
#include "pathname.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
  if (cond) { g_pass++; printf("  ok   %s\n", msg); } \
  else      { g_fail++; printf("  FAIL %s\n", msg); } } while (0)

/* Read an entire file into a heap buffer; returns bytes read (-1 on error). */
static int read_whole(struct unixfilesystem *fs, int inumber, char **out) {
  struct inode in;
  if (inode_iget(fs, inumber, &in) < 0) return -1;
  int size = inode_getsize(&in);
  int nblocks = (size + DISKIMG_SECTOR_SIZE - 1) / DISKIMG_SECTOR_SIZE;
  char *buf = malloc(size ? size : 1);
  int off = 0;
  char blk[DISKIMG_SECTOR_SIZE];
  for (int b = 0; b < nblocks; b++) {
    int valid = file_getblock(fs, inumber, b, blk);
    if (valid < 0) { free(buf); return -1; }
    memcpy(buf + off, blk, valid);
    off += valid;
  }
  *out = buf;
  return off;
}

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "usage: %s <image>\n", argv[0]); return 2; }
  struct unixfilesystem fs;
  fs.fd = diskimg_open(argv[1], 1);
  if (fs.fd < 0) { perror("open"); return 2; }
  diskimg_readsector(fs.fd, 1, &fs.superblock);

  printf("== inode layer ==\n");
  struct inode root;
  CHECK(inode_iget(&fs, ROOTINO, &root) == 0, "inode_iget(root) succeeds");
  CHECK((root.i_mode & IALLOC) != 0, "root inode is allocated");
  CHECK((root.i_mode & IFMT) == IFDIR, "root inode is a directory");

  printf("== pathname layer ==\n");
  int hello = pathname_lookup(&fs, "/hello.txt");
  CHECK(hello > 0, "lookup /hello.txt resolves");
  int readme = pathname_lookup(&fs, "/README");
  CHECK(readme > 0, "lookup /README resolves");
  int deep = pathname_lookup(&fs, "/a/b/c/deep.txt");
  CHECK(deep > 0, "lookup /a/b/c/deep.txt resolves (nested)");
  CHECK(pathname_lookup(&fs, "/a/b/c") > 0, "lookup /a/b/c (dir) resolves");
  CHECK(pathname_lookup(&fs, "/nope") < 0, "lookup /nope fails as expected");
  CHECK(pathname_lookup(&fs, "/a/x/y") < 0, "lookup missing nested path fails");
  CHECK(pathname_lookup(&fs, "relative") < 0, "non-absolute path rejected");
  CHECK(pathname_lookup(&fs, "/") == ROOTINO, "lookup / is root");

  printf("== file content layer ==\n");
  char *data; int n;
  n = read_whole(&fs, hello, &data);
  const char *expectHello = "Hello from the Unix v6 filesystem!";
  CHECK(n > 0 && memcmp(data, expectHello, strlen(expectHello)) == 0,
        "/hello.txt content matches");
  if (n > 0) free(data);

  printf("== directory layer ==\n");
  struct direntv6 ent;
  CHECK(directory_findname(&fs, "hello.txt", ROOTINO, &ent) == 0 &&
        ent.d_inumber == hello, "directory_findname finds hello.txt");
  CHECK(directory_findname(&fs, "ghost", ROOTINO, &ent) < 0,
        "directory_findname returns -1 for missing name");

  printf("== large-file / indirect addressing ==\n");
  int big = pathname_lookup(&fs, "/big.dat");
  CHECK(big > 0, "lookup /big.dat resolves");
  struct inode bin;
  inode_iget(&fs, big, &bin);
  CHECK((bin.i_mode & ILARG) != 0, "big.dat uses ILARG (indirect) addressing");
  int bigSize = inode_getsize(&bin);
  CHECK(bigSize == 20 * DISKIMG_SECTOR_SIZE + 137, "big.dat size is 20.x blocks");
  n = read_whole(&fs, big, &data);
  CHECK(n == bigSize, "big.dat fully read via indirect blocks");
  int ok = (n == bigSize);
  for (int i = 0; ok && i < n; i++)
    if (data[i] != (char)('A' + (i % 26))) ok = 0;
  CHECK(ok, "big.dat byte pattern intact across indirect blocks");
  /* Verify last block returns the partial byte count 137. */
  char blk[DISKIMG_SECTOR_SIZE];
  int lastValid = file_getblock(&fs, big, 20, blk);
  CHECK(lastValid == 137, "file_getblock returns partial last-block size (137)");
  if (n > 0) free(data);

  diskimg_close(fs.fd);
  printf("\n== summary ==\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
