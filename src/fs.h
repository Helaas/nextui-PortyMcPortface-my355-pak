#ifndef FS_H
#define FS_H

#include <stddef.h>

int fs_path_exists(const char *path);
int fs_ensure_dir(const char *path);
int write_text_file(const char *path, const char *content);
int read_text_file_alloc(const char *path, char **out_text, size_t *out_len);
int replace_tree(const char *src, const char *dst);
int copy_tree(const char *src, const char *dst);
int copy_file(const char *src, const char *dst);
int fs_remove_path(const char *path);
int patch_text_file(const char *path, const char *old_text, const char *new_text);
int patch_text_file_all(const char *path, const char *old_text, const char *new_text);

#endif
