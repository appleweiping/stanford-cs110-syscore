#ifndef _PATHNAME_H
#define _PATHNAME_H

#include "unixfilesystem.h"

/**
 * pathname_lookup: resolve an absolute pathname (must begin with '/') to an
 * inode number, walking one component at a time from the root inode, exactly
 * as described in Saltzer & Kaashoek Section 2.5.6. Returns the inumber of the
 * named file/directory, or -1 if any component is missing or not a directory.
 */
int pathname_lookup(struct unixfilesystem *fs, const char *pathname);

#endif
