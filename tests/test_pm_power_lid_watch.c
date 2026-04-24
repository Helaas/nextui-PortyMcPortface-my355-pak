#define main pm_power_lid_watch_main
#include "../tools/pm-power-lid-watch.c"
#undef main

#include <assert.h>
#include <sys/stat.h>

static void test_write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "w");

    assert(file != NULL);
    assert(fputs(content, file) >= 0);
    assert(fclose(file) == 0);
}

static void test_read_file(char *buffer, size_t buffer_size, const char *path) {
    FILE *file;
    size_t len;

    assert(buffer != NULL);
    assert(buffer_size > 0);

    file = fopen(path, "r");
    assert(file != NULL);
    len = fread(buffer, 1, buffer_size - 1, file);
    assert(ferror(file) == 0);
    buffer[len] = '\0';
    assert(fclose(file) == 0);
}

int main(void) {
    char root[PATH_MAX];
    char bin_dir[PATH_MAX];
    char amixer_path[PATH_MAX];
    char log_path[PATH_MAX];
    char old_path[PATH_MAX * 2];
    char path_env[PATH_MAX * 3];
    char playback_path_value[32];
    char log_content[1024];
    const char *current_path = getenv("PATH");
    int attempt;
    int created_root = 0;

    for (attempt = 0; attempt < 100; attempt++) {
        snprintf(root, sizeof(root), "/tmp/pm-power-lid-watch-test-%ld-%d", (long)getpid(), attempt);
        if (mkdir(root, 0755) == 0) {
            created_root = 1;
            break;
        }
    }
    assert(created_root);
    snprintf(bin_dir, sizeof(bin_dir), "%s/bin", root);
    snprintf(amixer_path, sizeof(amixer_path), "%s/amixer", bin_dir);
    snprintf(log_path, sizeof(log_path), "%s/amixer.log", root);
    assert(mkdir(bin_dir, 0755) == 0);

    test_write_file(amixer_path,
        "#!/bin/sh\n"
        "printf '%s\\n' \"$*\" >>\"$PMI_TEST_AMIXER_LOG\"\n"
        "if [ \"${1:-}\" = \"cget\" ] && [ \"${2:-}\" = \"numid=2\" ]; then\n"
        "    printf \"numid=2,iface=MIXER,name='Playback Path'\\n\"\n"
        "    printf '  ; type=ENUMERATED,access=rw------,values=1,items=3\\n'\n"
        "    printf '  : values=2\\n'\n"
        "fi\n"
        "exit 0\n");
    assert(chmod(amixer_path, 0755) == 0);

    snprintf(old_path, sizeof(old_path), "%s", current_path != NULL ? current_path : "");
    snprintf(path_env, sizeof(path_env), "%s:%s", bin_dir, old_path);
    assert(setenv("PATH", path_env, 1) == 0);
    assert(setenv("PMI_TEST_AMIXER_LOG", log_path, 1) == 0);

    assert(read_playback_path_control(playback_path_value, sizeof(playback_path_value)) == 0);
    assert(strcmp(playback_path_value, "2") == 0);

    assert(unlink(log_path) == 0);
    restore_resume_audio();

    test_read_file(log_content, sizeof(log_content), log_path);
    assert(strcmp(log_content,
        "cget numid=2\n"
        "cset numid=2 0\n"
        "cset numid=2 2\n") == 0);

    assert(unlink(log_path) == 0);
    assert(unlink(amixer_path) == 0);
    assert(rmdir(bin_dir) == 0);
    assert(rmdir(root) == 0);

    return 0;
}
