#include "pathname.h"
#include "directory.h"
#include <string.h>
#include <stdio.h>

int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {
  if (pathname == NULL || pathname[0] != '/') return -1;

  int curinumber = ROOTINO;
  const char *p = pathname;

  while (*p != '\0') {
    while (*p == '/') p++;            /* skip leading/duplicate slashes */
    if (*p == '\0') break;            /* trailing slash -> we're done   */

    /* Extract the next component into a bounded buffer. */
    char component[MAX_COMPONENT_LENGTH + 1];
    int len = 0;
    while (*p != '/' && *p != '\0') {
      if (len < MAX_COMPONENT_LENGTH) component[len++] = *p;
      p++;
    }
    component[len] = '\0';

    struct direntv6 ent;
    if (directory_findname(fs, component, curinumber, &ent) < 0)
      return -1;
    curinumber = ent.d_inumber;
  }
  return curinumber;
}
