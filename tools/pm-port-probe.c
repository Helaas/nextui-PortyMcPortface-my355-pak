#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define EI_NIDENT 16
#define EI_CLASS 4
#define EI_DATA 5
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EM_ARM 40
#define SHT_STRTAB 3
#define SHT_DYNSYM 11
#define SHN_UNDEF 0

#define SDL_WRAPPER_MARKER "# PMI_AARCH64_SDL_COMPAT_WRAPPER=1"
#define NATIVE_WRAPPER_MARKER "# PMI_AARCH64_NATIVE_COMPAT_WRAPPER=1"
#define ARMHF_WRAPPER_MARKER "# PMI_ARMHF_EXEC_COMPAT_WRAPPER=1"
#define ARMHF_BINARY_WRAPPER_MARKER "PMI_ARMHF_EXEC_COMPAT_BINARY=1"

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} elf64_shdr;

typedef struct {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} elf64_sym;

typedef struct {
    char *path;
    int needs_default_audio_info;
    int needs_pulse_simple;
    int uses_sdl_gl_windowing;
    int is_wrapper;
} probe_result;

typedef struct {
    probe_result *items;
    size_t count;
    size_t capacity;
    int has_bundled_native_gl_stack;
} probe_result_list;

static const char *DEFAULT_AUDIO_INFO_SYMBOLS[] = {
    "SDL_GetDefaultAudioInfo"
};

static const char *PULSE_SIMPLE_DEPENDENCY = "libpulse-simple.so.0";

static const char *SDL_GL_WINDOWING_SYMBOLS[] = {
    "SDL_GL_CreateContext",
    "SDL_CreateWindow"
};

static const char *BUNDLED_GL_PATTERNS[] = {
    "libEGL.so*",
    "libGLES*.so*",
    "libGL.so*",
    "libgbm.so*",
    "libdrm.so*",
    "libmali.so*"
};

_Static_assert(sizeof(elf64_ehdr) == 64, "unexpected elf64_ehdr size");
_Static_assert(sizeof(elf64_shdr) == 64, "unexpected elf64_shdr size");
_Static_assert(sizeof(elf64_sym) == 24, "unexpected elf64_sym size");

static const char *basename_ptr(const char *path);
static int path_has_suffix(const char *path, const char *suffix);
static int should_ignore_probe_artifact(const char *path);
static int read_file(const char *path, unsigned char **out_data, size_t *out_size);
static int is_supported_elf32_arm(const unsigned char *data, size_t size);
static int is_supported_elf64(const unsigned char *data, size_t size);
static int range_within_file(size_t file_size, uint64_t offset, uint64_t length);
static int file_contains_bytes(const unsigned char *data, size_t size, const char *needle);
static int dynsym_contains_undefined_symbol(const unsigned char *data, size_t size, const char *symbol);
static int binary_matches_symbols(const unsigned char *data, size_t size, const char *const *symbols, size_t symbol_count);
static int stat_regular_file(const char *path, struct stat *st);
static char *join_path(const char *left, const char *right);
static char *dup_string(const char *value);
static int append_probe_result(probe_result_list *results, const probe_result *item);
static int compare_probe_result(const void *lhs, const void *rhs);
static int file_has_aarch64_wrapper_marker(const char *path);
static int file_has_armhf_wrapper_marker(const char *path);
static int classify_candidate(const char *path, probe_result *out_result);
static int classify_armhf_candidate(const char *path, probe_result *out_result);
static int scan_launch_tree(const char *root, const char *dir_path, int depth, probe_result_list *results);
static int scan_armhf_launch_tree(const char *dir_path, int depth, probe_result_list *results);
static int scan_bundled_gl_tree(const char *dir_path, int depth, int *out_found);
static void free_probe_results(probe_result_list *results);
static int run_scan_aarch64_launch_port(const char *port_dir);
static int run_scan_armhf_launch_port(const char *port_dir);

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <scan-aarch64-launch-port|scan-armhf-launch-port> <port_dir>\n",
                (argc > 0 && argv[0] != NULL) ? argv[0] : "pm-port-probe");
        return 2;
    }

    if (strcmp(argv[1], "scan-aarch64-launch-port") == 0)
        return run_scan_aarch64_launch_port(argv[2]);
    if (strcmp(argv[1], "scan-armhf-launch-port") == 0)
        return run_scan_armhf_launch_port(argv[2]);

    fprintf(stderr, "usage: %s <scan-aarch64-launch-port|scan-armhf-launch-port> <port_dir>\n",
            (argc > 0 && argv[0] != NULL) ? argv[0] : "pm-port-probe");
    return 2;
}

static int run_scan_aarch64_launch_port(const char *port_dir) {
    probe_result_list results = {0};
    char *lib_dir = NULL;
    struct stat st;
    size_t index;

    if (port_dir == NULL || stat(port_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return 1;

    if (scan_launch_tree(port_dir, port_dir, 0, &results) != 0) {
        free_probe_results(&results);
        return 1;
    }

    lib_dir = join_path(port_dir, "lib");
    if (lib_dir != NULL) {
        if (scan_bundled_gl_tree(lib_dir, 0, &results.has_bundled_native_gl_stack) != 0) {
            free(lib_dir);
            free_probe_results(&results);
            return 1;
        }
        free(lib_dir);
    }

    qsort(results.items, results.count, sizeof(results.items[0]), compare_probe_result);

    printf("PORT\t%s\thas_bundled_native_gl_stack=%d\n",
           port_dir,
           results.has_bundled_native_gl_stack ? 1 : 0);
    for (index = 0; index < results.count; index++) {
        printf("BIN\t%s\tneeds_default_audio_info=%d\tneeds_pulse_simple=%d\tuses_sdl_gl_windowing=%d\tis_wrapper=%d\n",
               results.items[index].path,
               results.items[index].needs_default_audio_info ? 1 : 0,
               results.items[index].needs_pulse_simple ? 1 : 0,
               results.items[index].uses_sdl_gl_windowing ? 1 : 0,
               results.items[index].is_wrapper ? 1 : 0);
    }

    free_probe_results(&results);
    return 0;
}

static int run_scan_armhf_launch_port(const char *port_dir) {
    probe_result_list results = {0};
    struct stat st;
    size_t index;

    if (port_dir == NULL || stat(port_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return 1;

    if (scan_armhf_launch_tree(port_dir, 0, &results) != 0) {
        free_probe_results(&results);
        return 1;
    }

    qsort(results.items, results.count, sizeof(results.items[0]), compare_probe_result);

    printf("PORT\t%s\n", port_dir);
    for (index = 0; index < results.count; index++) {
        printf("BIN\t%s\tis_wrapper=%d\n",
               results.items[index].path,
               results.items[index].is_wrapper ? 1 : 0);
    }

    free_probe_results(&results);
    return 0;
}

static const char *basename_ptr(const char *path) {
    const char *slash;

    if (path == NULL)
        return "";

    slash = strrchr(path, '/');
    return (slash == NULL) ? path : slash + 1;
}

static int path_has_suffix(const char *path, const char *suffix) {
    size_t path_len;
    size_t suffix_len;

    if (path == NULL || suffix == NULL)
        return 0;

    path_len = strlen(path);
    suffix_len = strlen(suffix);
    return path_len >= suffix_len && strcmp(path + path_len - suffix_len, suffix) == 0;
}

static int should_ignore_probe_artifact(const char *path) {
    const char *base = basename_ptr(path);

    if (strstr(base, ".pmi-") != NULL)
        return 1;
    if (path_has_suffix(base, ".sh") || path_has_suffix(base, ".bash"))
        return 1;
    if (path_has_suffix(base, ".so") || strstr(base, ".so.") != NULL)
        return 1;
    if (path_has_suffix(base, ".original"))
        return 1;
    return 0;
}

static int stat_regular_file(const char *path, struct stat *st) {
    if (path == NULL || st == NULL || stat(path, st) != 0)
        return 0;
    return S_ISREG(st->st_mode);
}

static char *dup_string(const char *value) {
    size_t length;
    char *copy;

    if (value == NULL)
        return NULL;

    length = strlen(value) + 1;
    copy = malloc(length);
    if (copy == NULL)
        return NULL;

    memcpy(copy, value, length);
    return copy;
}

static char *join_path(const char *left, const char *right) {
    size_t left_len;
    size_t right_len;
    char *result;

    if (left == NULL || right == NULL)
        return NULL;

    left_len = strlen(left);
    right_len = strlen(right);
    result = malloc(left_len + 1 + right_len + 1);
    if (result == NULL)
        return NULL;

    memcpy(result, left, left_len);
    result[left_len] = '/';
    memcpy(result + left_len + 1, right, right_len);
    result[left_len + 1 + right_len] = '\0';
    return result;
}

static int append_probe_result(probe_result_list *results, const probe_result *item) {
    probe_result *new_items;
    size_t new_capacity;

    if (results == NULL || item == NULL)
        return -1;

    if (results->count == results->capacity) {
        new_capacity = (results->capacity == 0) ? 8 : results->capacity * 2;
        new_items = realloc(results->items, new_capacity * sizeof(results->items[0]));
        if (new_items == NULL)
            return -1;
        results->items = new_items;
        results->capacity = new_capacity;
    }

    results->items[results->count++] = *item;
    return 0;
}

static int compare_probe_result(const void *lhs, const void *rhs) {
    const probe_result *left = lhs;
    const probe_result *right = rhs;

    return strcmp(left->path, right->path);
}

static int file_has_aarch64_wrapper_marker(const char *path) {
    FILE *file;
    char line[256];
    int count = 0;

    file = fopen(path, "r");
    if (file == NULL)
        return 0;

    while (count < 2 && fgets(line, sizeof(line), file) != NULL) {
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n')
            line[line_len - 1] = '\0';
        if (strcmp(line, SDL_WRAPPER_MARKER) == 0 || strcmp(line, NATIVE_WRAPPER_MARKER) == 0) {
            fclose(file);
            return 1;
        }
        count++;
    }

    fclose(file);
    return 0;
}

static int file_has_armhf_wrapper_marker(const char *path) {
    FILE *file;
    unsigned char *data = NULL;
    size_t size = 0;
    char line[256];
    int count = 0;

    file = fopen(path, "r");
    if (file == NULL)
        return 0;

    while (count < 2 && fgets(line, sizeof(line), file) != NULL) {
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\n')
            line[line_len - 1] = '\0';
        if (strcmp(line, ARMHF_WRAPPER_MARKER) == 0) {
            fclose(file);
            return 1;
        }
        count++;
    }

    fclose(file);
    if (read_file(path, &data, &size) != 0)
        return 0;
    if (is_supported_elf64(data, size) && file_contains_bytes(data, size, ARMHF_BINARY_WRAPPER_MARKER)) {
        free(data);
        return 1;
    }
    free(data);
    return 0;
}

static int classify_candidate(const char *path, probe_result *out_result) {
    char *inspect_path = NULL;
    unsigned char *data = NULL;
    size_t size = 0;
    struct stat st;
    int is_wrapper = 0;
    int ok = -1;
    probe_result result = {0};

    if (path == NULL || out_result == NULL || should_ignore_probe_artifact(path))
        return 1;

    if (!stat_regular_file(path, &st))
        return 1;

    if (file_has_aarch64_wrapper_marker(path)) {
        char *original_path = malloc(strlen(path) + strlen(".original") + 1);
        if (original_path == NULL)
            return -1;
        strcpy(original_path, path);
        strcat(original_path, ".original");
        if (stat_regular_file(original_path, &st)) {
            inspect_path = original_path;
            is_wrapper = 1;
        } else {
            free(original_path);
            return 1;
        }
    } else {
        inspect_path = dup_string(path);
        if (inspect_path == NULL)
            return -1;
    }

    if (read_file(inspect_path, &data, &size) != 0)
        goto cleanup;
    if (!is_supported_elf64(data, size)) {
        ok = 1;
        goto cleanup;
    }

    result.path = dup_string(path);
    if (result.path == NULL)
        goto cleanup;
    result.needs_default_audio_info = binary_matches_symbols(
        data, size,
        DEFAULT_AUDIO_INFO_SYMBOLS,
        sizeof(DEFAULT_AUDIO_INFO_SYMBOLS) / sizeof(DEFAULT_AUDIO_INFO_SYMBOLS[0]));
    result.needs_pulse_simple = file_contains_bytes(data, size, PULSE_SIMPLE_DEPENDENCY);
    result.uses_sdl_gl_windowing = binary_matches_symbols(
        data, size,
        SDL_GL_WINDOWING_SYMBOLS,
        sizeof(SDL_GL_WINDOWING_SYMBOLS) / sizeof(SDL_GL_WINDOWING_SYMBOLS[0]));
    result.is_wrapper = is_wrapper;

    *out_result = result;
    result.path = NULL;
    ok = 0;

cleanup:
    free(result.path);
    free(inspect_path);
    free(data);
    return ok;
}

static int classify_armhf_candidate(const char *path, probe_result *out_result) {
    char *inspect_path = NULL;
    unsigned char *data = NULL;
    size_t size = 0;
    struct stat st;
    int is_wrapper = 0;
    int ok = -1;
    probe_result result = {0};

    if (path == NULL || out_result == NULL || should_ignore_probe_artifact(path))
        return 1;

    if (!stat_regular_file(path, &st))
        return 1;

    if (file_has_armhf_wrapper_marker(path)) {
        char *original_path = malloc(strlen(path) + strlen(".original") + 1);
        if (original_path == NULL)
            return -1;
        strcpy(original_path, path);
        strcat(original_path, ".original");
        if (stat_regular_file(original_path, &st)) {
            inspect_path = original_path;
            is_wrapper = 1;
        } else {
            free(original_path);
            return 1;
        }
    } else {
        if ((st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0)
            return 1;
        inspect_path = dup_string(path);
        if (inspect_path == NULL)
            return -1;
    }

    if (read_file(inspect_path, &data, &size) != 0)
        goto cleanup;
    if (!is_supported_elf32_arm(data, size)) {
        ok = 1;
        goto cleanup;
    }

    result.path = dup_string(path);
    if (result.path == NULL)
        goto cleanup;
    result.is_wrapper = is_wrapper;

    *out_result = result;
    result.path = NULL;
    ok = 0;

cleanup:
    free(result.path);
    free(inspect_path);
    free(data);
    return ok;
}

static int scan_launch_tree(const char *root, const char *dir_path, int depth, probe_result_list *results) {
    DIR *dir;
    struct dirent *entry;

    (void)root;

    dir = opendir(dir_path);
    if (dir == NULL) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        struct stat st;
        char *entry_path;
        int entry_depth = depth + 1;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        entry_path = join_path(dir_path, entry->d_name);
        if (entry_path == NULL) {
            closedir(dir);
            return -1;
        }

        if (stat(entry_path, &st) != 0) {
            free(entry_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (entry_depth < 2 && scan_launch_tree(root, entry_path, entry_depth, results) != 0) {
                free(entry_path);
                closedir(dir);
                return -1;
            }
            free(entry_path);
            continue;
        }

        if (S_ISREG(st.st_mode) && entry_depth <= 2) {
            probe_result item;
            int classify_result = classify_candidate(entry_path, &item);

            if (classify_result < 0) {
                free(entry_path);
                closedir(dir);
                return -1;
            }
            if (classify_result == 0) {
                if (append_probe_result(results, &item) != 0) {
                    free(item.path);
                    free(entry_path);
                    closedir(dir);
                    return -1;
                }
            }
        }

        free(entry_path);
    }

    closedir(dir);
    return 0;
}

static int scan_armhf_launch_tree(const char *dir_path, int depth, probe_result_list *results) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(dir_path);
    if (dir == NULL) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        struct stat st;
        char *entry_path;
        int entry_depth = depth + 1;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        entry_path = join_path(dir_path, entry->d_name);
        if (entry_path == NULL) {
            closedir(dir);
            return -1;
        }

        if (stat(entry_path, &st) != 0) {
            free(entry_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (entry_depth < 2 && scan_armhf_launch_tree(entry_path, entry_depth, results) != 0) {
                free(entry_path);
                closedir(dir);
                return -1;
            }
            free(entry_path);
            continue;
        }

        if (S_ISREG(st.st_mode) && entry_depth <= 2) {
            probe_result item;
            int classify_result = classify_armhf_candidate(entry_path, &item);

            if (classify_result < 0) {
                free(entry_path);
                closedir(dir);
                return -1;
            }
            if (classify_result == 0) {
                if (append_probe_result(results, &item) != 0) {
                    free(item.path);
                    free(entry_path);
                    closedir(dir);
                    return -1;
                }
            }
        }

        free(entry_path);
    }

    closedir(dir);
    return 0;
}

static int scan_bundled_gl_tree(const char *dir_path, int depth, int *out_found) {
    DIR *dir;
    struct dirent *entry;
    size_t index;

    if (out_found == NULL)
        return -1;
    if (*out_found)
        return 0;

    dir = opendir(dir_path);
    if (dir == NULL) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        struct stat st;
        char *entry_path;
        int entry_depth = depth + 1;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        entry_path = join_path(dir_path, entry->d_name);
        if (entry_path == NULL) {
            closedir(dir);
            return -1;
        }

        if (stat(entry_path, &st) != 0) {
            free(entry_path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (entry_depth < 2 && scan_bundled_gl_tree(entry_path, entry_depth, out_found) != 0) {
                free(entry_path);
                closedir(dir);
                return -1;
            }
            free(entry_path);
            if (*out_found) {
                closedir(dir);
                return 0;
            }
            continue;
        }

        if (S_ISREG(st.st_mode) && entry_depth <= 2) {
            for (index = 0; index < sizeof(BUNDLED_GL_PATTERNS) / sizeof(BUNDLED_GL_PATTERNS[0]); index++) {
                if (fnmatch(BUNDLED_GL_PATTERNS[index], entry->d_name, 0) == 0) {
                    *out_found = 1;
                    free(entry_path);
                    closedir(dir);
                    return 0;
                }
            }
        }

        free(entry_path);
    }

    closedir(dir);
    return 0;
}

static void free_probe_results(probe_result_list *results) {
    size_t index;

    if (results == NULL)
        return;

    for (index = 0; index < results->count; index++)
        free(results->items[index].path);
    free(results->items);
    results->items = NULL;
    results->count = 0;
    results->capacity = 0;
    results->has_bundled_native_gl_stack = 0;
}

static int read_file(const char *path, unsigned char **out_data, size_t *out_size) {
    FILE *file = NULL;
    long file_size;
    unsigned char *buffer = NULL;

    if (out_data == NULL || out_size == NULL)
        return -1;

    *out_data = NULL;
    *out_size = 0;

    file = fopen(path, "rb");
    if (file == NULL)
        return -1;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return -1;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    buffer = malloc((size_t)file_size);
    if (buffer == NULL && file_size > 0) {
        fclose(file);
        return -1;
    }

    if (file_size > 0 && fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size) {
        free(buffer);
        fclose(file);
        return -1;
    }

    fclose(file);
    *out_data = buffer;
    *out_size = (size_t)file_size;
    return 0;
}

static int is_supported_elf64(const unsigned char *data, size_t size) {
    const elf64_ehdr *ehdr;

    if (data == NULL || size < sizeof(*ehdr))
        return 0;

    ehdr = (const elf64_ehdr *)data;
    return ehdr->e_ident[0] == ELFMAG0 &&
        ehdr->e_ident[1] == ELFMAG1 &&
        ehdr->e_ident[2] == ELFMAG2 &&
        ehdr->e_ident[3] == ELFMAG3 &&
        ehdr->e_ident[EI_CLASS] == ELFCLASS64 &&
        ehdr->e_ident[EI_DATA] == ELFDATA2LSB;
}

static int is_supported_elf32_arm(const unsigned char *data, size_t size) {
    if (data == NULL || size < 20)
        return 0;

    return data[0] == ELFMAG0 &&
        data[1] == ELFMAG1 &&
        data[2] == ELFMAG2 &&
        data[3] == ELFMAG3 &&
        data[EI_CLASS] == ELFCLASS32 &&
        data[EI_DATA] == ELFDATA2LSB &&
        data[18] == (unsigned char)(EM_ARM & 0xff) &&
        data[19] == (unsigned char)((EM_ARM >> 8) & 0xff);
}

static int range_within_file(size_t file_size, uint64_t offset, uint64_t length) {
    return offset <= file_size && length <= file_size - (size_t)offset;
}

static int file_contains_bytes(const unsigned char *data, size_t size, const char *needle) {
    size_t needle_len;
    size_t index;

    if (data == NULL || needle == NULL)
        return 0;

    needle_len = strlen(needle);
    if (needle_len == 0 || size < needle_len)
        return 0;

    for (index = 0; index + needle_len <= size; index++) {
        if (memcmp(data + index, needle, needle_len) == 0)
            return 1;
    }

    return 0;
}

static int dynsym_contains_undefined_symbol(const unsigned char *data, size_t size, const char *symbol) {
    const elf64_ehdr *ehdr;
    const elf64_shdr *sections;
    uint16_t section_index;

    if (!is_supported_elf64(data, size))
        return -1;

    ehdr = (const elf64_ehdr *)data;
    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0 || ehdr->e_shentsize != sizeof(elf64_shdr))
        return -1;
    if (!range_within_file(size, ehdr->e_shoff, (uint64_t)ehdr->e_shnum * sizeof(elf64_shdr)))
        return -1;

    sections = (const elf64_shdr *)(data + ehdr->e_shoff);
    for (section_index = 0; section_index < ehdr->e_shnum; section_index++) {
        const elf64_shdr *dynsym = &sections[section_index];
        const elf64_shdr *dynstr;
        size_t symbol_count;
        size_t symbol_index;

        if (dynsym->sh_type != SHT_DYNSYM || dynsym->sh_entsize != sizeof(elf64_sym))
            continue;
        if (dynsym->sh_link >= ehdr->e_shnum)
            continue;
        if (!range_within_file(size, dynsym->sh_offset, dynsym->sh_size))
            continue;

        dynstr = &sections[dynsym->sh_link];
        if (dynstr->sh_type != SHT_STRTAB)
            continue;
        if (!range_within_file(size, dynstr->sh_offset, dynstr->sh_size))
            continue;

        symbol_count = (size_t)(dynsym->sh_size / dynsym->sh_entsize);
        for (symbol_index = 0; symbol_index < symbol_count; symbol_index++) {
            const elf64_sym *sym = (const elf64_sym *)(data + dynsym->sh_offset + symbol_index * dynsym->sh_entsize);
            const char *name;
            size_t remaining;

            if (sym->st_shndx != SHN_UNDEF || sym->st_name >= dynstr->sh_size)
                continue;

            name = (const char *)(data + dynstr->sh_offset + sym->st_name);
            remaining = (size_t)(dynstr->sh_size - sym->st_name);
            if (memchr(name, '\0', remaining) == NULL)
                continue;
            if (strcmp(name, symbol) == 0)
                return 1;
        }
    }

    return 0;
}

static int binary_matches_symbols(const unsigned char *data, size_t size, const char *const *symbols, size_t symbol_count) {
    size_t index;

    if (!is_supported_elf64(data, size) || symbols == NULL || symbol_count == 0)
        return 0;

    for (index = 0; index < symbol_count; index++) {
        int dynsym_result = dynsym_contains_undefined_symbol(data, size, symbols[index]);

        if (dynsym_result > 0)
            continue;
        if (dynsym_result < 0 && file_contains_bytes(data, size, symbols[index]))
            continue;
        return 0;
    }

    return 1;
}
