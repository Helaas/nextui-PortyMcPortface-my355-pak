#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "../third_party/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *read_file(const char *path, size_t *size_out) {
    FILE *file;
    long size;
    unsigned char *buffer;

    file = fopen(path, "rb");
    if (file == NULL)
        return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = malloc((size_t)size);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }
    if (size > 0 && fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size_out = (size_t)size;
    return buffer;
}

static int write_png(const char *path, const unsigned char *pixels, int width, int height) {
    return stbi_write_png(path, width, height, 4, pixels, width * 4);
}

int main(int argc, char **argv) {
    unsigned char *input_data = NULL;
    unsigned char *pixels = NULL;
    size_t input_size = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    int ok;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.jpg> <output.png>\n", argv[0]);
        return 1;
    }

    input_data = read_file(argv[1], &input_size);
    if (input_data == NULL) {
        fprintf(stderr, "failed to read %s\n", argv[1]);
        return 1;
    }

    pixels = stbi_load_from_memory(input_data, (int)input_size, &width, &height, &channels, 4);
    free(input_data);
    if (pixels == NULL) {
        fprintf(stderr, "failed to decode %s: %s\n", argv[1], stbi_failure_reason());
        return 1;
    }

    ok = write_png(argv[2], pixels, width, height);
    stbi_image_free(pixels);
    if (!ok) {
        fprintf(stderr, "failed to write %s\n", argv[2]);
        return 1;
    }

    return 0;
}
