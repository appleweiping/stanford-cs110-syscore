#include "diskimg.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int diskimg_open(const char *pathname, int readOnly) {
  return open(pathname, readOnly ? O_RDONLY : O_RDWR);
}

int diskimg_readsector(int fd, int sectorNum, void *buf) {
  off_t off = (off_t)sectorNum * DISKIMG_SECTOR_SIZE;
  if (lseek(fd, off, SEEK_SET) != off) return -1;
  size_t total = 0;
  char *p = (char *)buf;
  while (total < DISKIMG_SECTOR_SIZE) {
    ssize_t n = read(fd, p + total, DISKIMG_SECTOR_SIZE - total);
    if (n < 0) { if (errno == EINTR) continue; return -1; }
    if (n == 0) break; /* short image: zero-fill remainder */
    total += (size_t)n;
  }
  if (total < DISKIMG_SECTOR_SIZE)
    memset(p + total, 0, DISKIMG_SECTOR_SIZE - total);
  return (int)DISKIMG_SECTOR_SIZE;
}

int diskimg_writesector(int fd, int sectorNum, const void *buf) {
  off_t off = (off_t)sectorNum * DISKIMG_SECTOR_SIZE;
  if (lseek(fd, off, SEEK_SET) != off) return -1;
  size_t total = 0;
  const char *p = (const char *)buf;
  while (total < DISKIMG_SECTOR_SIZE) {
    ssize_t n = write(fd, p + total, DISKIMG_SECTOR_SIZE - total);
    if (n < 0) { if (errno == EINTR) continue; return -1; }
    total += (size_t)n;
  }
  return (int)total;
}

int diskimg_close(int fd) {
  return close(fd);
}
