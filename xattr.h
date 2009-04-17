#ifndef __XATTR_H
#define __XATTR_H

extern char *safe_getxattr(char *path, char *attr_name);
extern int safe_setxattr(char *path, char *name, void *value, size_t len);

#endif
