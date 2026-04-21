#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EI_NIDENT 16
#define EI_CLASS 4
#define EI_DATA 5
#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define SHT_STRTAB 3
#define SHT_DYNSYM 11
#define SHN_UNDEF 0

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

_Static_assert(sizeof(elf64_ehdr) == 64, "unexpected elf64_ehdr size");
_Static_assert(sizeof(elf64_shdr) == 64, "unexpected elf64_shdr size");
_Static_assert(sizeof(elf64_sym) == 24, "unexpected elf64_sym size");

static const char *TARGET_SYMBOL = "SDL_GetDefaultAudioInfo";

static int read_file(const char *path, unsigned char **out_data, size_t *out_size);
static int is_supported_elf64(const unsigned char *data, size_t size);
static int range_within_file(size_t file_size, uint64_t offset, uint64_t length);
static int file_contains_bytes(const unsigned char *data, size_t size, const char *needle);
static int dynsym_contains_undefined_symbol(const unsigned char *data, size_t size, const char *symbol);

int main(int argc, char **argv) {
    unsigned char *data = NULL;
    size_t size = 0;
    int result = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <binary>\n", argc > 0 ? argv[0] : "pm-sdl-compat-check");
        return 2;
    }

    if (read_file(argv[1], &data, &size) != 0)
        return 1;

    if (is_supported_elf64(data, size)) {
        int dynsym_result = dynsym_contains_undefined_symbol(data, size, TARGET_SYMBOL);

        if (dynsym_result > 0 || (dynsym_result < 0 && file_contains_bytes(data, size, TARGET_SYMBOL)))
            result = 0;
    }

    free(data);
    return result;
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
