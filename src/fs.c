#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int ensure_parent_dir(const char *path);
static int remove_tree(const char *path);
static int copy_regular_file(const char *src, const char *dst, mode_t mode);
static int patch_text_file_with_mode(const char *path, const char *old_text, const char *new_text, int replace_all);

int fs_path_exists(const char *path) {
    struct stat st;

    return stat(path, &st) == 0;
}

int fs_ensure_dir(const char *path) {
    char temp[PATH_MAX];
    size_t len;
    char *cursor;

    if (path == NULL || path[0] == '\0')
        return -1;

    len = strlen(path);
    if (len >= sizeof(temp))
        return -1;

    memcpy(temp, path, len + 1);
    if (len > 1 && temp[len - 1] == '/')
        temp[len - 1] = '\0';

    for (cursor = temp + 1; *cursor != '\0'; cursor++) {
        if (*cursor != '/')
            continue;
        *cursor = '\0';
        if (mkdir(temp, 0755) != 0 && errno != EEXIST)
            return -1;
        *cursor = '/';
    }

    if (mkdir(temp, 0755) != 0 && errno != EEXIST)
        return -1;

    return 0;
}

int write_text_file(const char *path, const char *content) {
    FILE *file;
    const char *safe_content = content == NULL ? "" : content;

    if (ensure_parent_dir(path) != 0)
        return -1;

    file = fopen(path, "wb");
    if (file == NULL)
        return -1;

    if (safe_content[0] != '\0' && fwrite(safe_content, 1, strlen(safe_content), file) != strlen(safe_content)) {
        fclose(file);
        return -1;
    }

    if (fclose(file) != 0)
        return -1;

    return 0;
}

int read_text_file_alloc(const char *path, char **out_text, size_t *out_len) {
    FILE *file;
    long size;
    char *buffer;

    if (out_text == NULL || out_len == NULL)
        return -1;

    *out_text = NULL;
    *out_len = 0;

    file = fopen(path, "rb");
    if (file == NULL)
        return -1;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return -1;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    buffer = malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        return -1;
    }

    if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return -1;
    }
    buffer[size] = '\0';
    fclose(file);

    *out_text = buffer;
    *out_len = (size_t)size;
    return 0;
}

int copy_file(const char *src, const char *dst) {
    struct stat st;

    if (stat(src, &st) != 0)
        return -1;
    return copy_regular_file(src, dst, st.st_mode);
}

int copy_tree(const char *src, const char *dst) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    if (stat(src, &st) != 0)
        return -1;

    if (S_ISREG(st.st_mode))
        return copy_regular_file(src, dst, st.st_mode);

    if (!S_ISDIR(st.st_mode))
        return -1;

    if (fs_ensure_dir(dst) != 0)
        return -1;

    dir = opendir(src);
    if (dir == NULL)
        return -1;

    while ((entry = readdir(dir)) != NULL) {
        char src_child[PATH_MAX];
        char dst_child[PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(src_child, sizeof(src_child), "%s/%s", src, entry->d_name);
        snprintf(dst_child, sizeof(dst_child), "%s/%s", dst, entry->d_name);

        if (copy_tree(src_child, dst_child) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    chmod(dst, st.st_mode & 0777);
    return 0;
}

int replace_tree(const char *src, const char *dst) {
    if (fs_path_exists(dst) && remove_tree(dst) != 0)
        return -1;
    return copy_tree(src, dst);
}

int fs_remove_path(const char *path) {
    if (path == NULL || path[0] == '\0')
        return -1;
    return remove_tree(path);
}

int patch_text_file(const char *path, const char *old_text, const char *new_text) {
    return patch_text_file_with_mode(path, old_text, new_text, 0);
}

int patch_text_file_all(const char *path, const char *old_text, const char *new_text) {
    return patch_text_file_with_mode(path, old_text, new_text, 1);
}

static int patch_text_file_with_mode(const char *path, const char *old_text, const char *new_text, int replace_all) {
    char *content = NULL;
    char *original_content = NULL;
    size_t len = 0;
    const char *match;
    char *updated;
    size_t old_len;
    size_t new_len;
    size_t final_len;
    char *cursor;
    int rc;

    if (read_text_file_alloc(path, &content, &len) != 0)
        return -1;
    original_content = content;

    old_len = strlen(old_text);
    new_len = strlen(new_text);
    match = strstr(content, old_text);
    if (match == NULL) {
        free(content);
        return -1;
    }

    final_len = len;
    if (replace_all) {
        const char *scan = content;

        while ((scan = strstr(scan, old_text)) != NULL) {
            final_len = final_len - old_len + new_len;
            scan += old_len;
        }
    } else {
        final_len = final_len - old_len + new_len;
    }

    updated = malloc(final_len + 1);
    if (updated == NULL) {
        free(content);
        return -1;
    }

    cursor = updated;
    while ((match = strstr(content, old_text)) != NULL) {
        size_t prefix_len = (size_t)(match - content);

        memcpy(cursor, content, prefix_len);
        cursor += prefix_len;
        memcpy(cursor, new_text, new_len);
        cursor += new_len;
        content = (char *)(match + old_len);

        if (!replace_all)
            break;
    }

    strcpy(cursor, content);

    rc = write_text_file(path, updated);
    free(updated);
    free(original_content);
    return rc;
}

static int ensure_parent_dir(const char *path) {
    char parent[PATH_MAX];
    char *slash;

    if (path == NULL || strlen(path) >= sizeof(parent))
        return -1;

    strcpy(parent, path);
    slash = strrchr(parent, '/');
    if (slash == NULL)
        return 0;
    if (slash == parent)
        return 0;

    *slash = '\0';
    return fs_ensure_dir(parent);
}

static int remove_tree(const char *path) {
    struct stat st;
    DIR *dir;
    struct dirent *entry;

    if (lstat(path, &st) != 0)
        return errno == ENOENT ? 0 : -1;

    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
        return unlink(path);

    if (!S_ISDIR(st.st_mode))
        return -1;

    dir = opendir(path);
    if (dir == NULL)
        return -1;

    while ((entry = readdir(dir)) != NULL) {
        char child[PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (remove_tree(child) != 0) {
            closedir(dir);
            return -1;
        }
    }

    closedir(dir);
    return rmdir(path);
}

static int copy_regular_file(const char *src, const char *dst, mode_t mode) {
    FILE *in;
    FILE *out;
    char buffer[8192];
    size_t read_now;

    if (ensure_parent_dir(dst) != 0)
        return -1;

    in = fopen(src, "rb");
    if (in == NULL)
        return -1;

    out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }

    while ((read_now = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, read_now, out) != read_now) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    if (fclose(out) != 0)
        return -1;

    if (chmod(dst, mode & 0777) != 0)
        return -1;

    return 0;
}
