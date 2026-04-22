#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static const char ARMHF_EXEC_WRAPPER_MARKER[] = "PMI_ARMHF_EXEC_COMPAT_BINARY=1";

static volatile sig_atomic_t g_child_pid = -1;

static void forward_signal_to_child(int signum) {
    pid_t child = (pid_t)g_child_pid;

    if (child > 0)
        kill(child, signum);
}

static int path_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0;
}

static int is_dir(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_executable_file(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode) && access(path, X_OK) == 0;
}

static int ensure_dir(const char *path) {
    if (path == NULL || *path == '\0')
        return -1;
    if (mkdir(path, 0777) == 0 || errno == EEXIST)
        return 0;
    return -1;
}

static int is_mounted_on(const char *mountpoint) {
    FILE *fp;
    char line[PATH_MAX * 2];
    size_t mount_len;

    if (mountpoint == NULL)
        return 0;

    fp = fopen("/proc/mounts", "r");
    if (fp == NULL)
        return 0;

    mount_len = strlen(mountpoint);
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *first_space = strchr(line, ' ');
        char *second_space;

        if (first_space == NULL)
            continue;
        second_space = strchr(first_space + 1, ' ');
        if (second_space == NULL)
            continue;
        if ((size_t)(second_space - (first_space + 1)) == mount_len &&
            strncmp(first_space + 1, mountpoint, mount_len) == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    int in_fd = -1;
    int out_fd = -1;
    char buffer[65536];
    ssize_t read_count;

    in_fd = open(src, O_RDONLY);
    if (in_fd < 0)
        goto error;
    out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0)
        goto error;

    while ((read_count = read(in_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t offset = 0;
        while (offset < read_count) {
            ssize_t written = write(out_fd, buffer + offset, (size_t)(read_count - offset));
            if (written < 0)
                goto error;
            offset += written;
        }
    }
    if (read_count < 0)
        goto error;

    close(in_fd);
    close(out_fd);
    return 0;

error:
    if (in_fd >= 0)
        close(in_fd);
    if (out_fd >= 0)
        close(out_fd);
    return -1;
}

static int append_file(const char *src, const char *dst) {
    int in_fd = -1;
    int out_fd = -1;
    char buffer[65536];
    ssize_t read_count;

    in_fd = open(src, O_RDONLY);
    if (in_fd < 0)
        goto error;
    out_fd = open(dst, O_WRONLY | O_APPEND);
    if (out_fd < 0)
        goto error;

    while ((read_count = read(in_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t offset = 0;
        while (offset < read_count) {
            ssize_t written = write(out_fd, buffer + offset, (size_t)(read_count - offset));
            if (written < 0)
                goto error;
            offset += written;
        }
    }
    if (read_count < 0)
        goto error;

    close(in_fd);
    close(out_fd);
    return 0;

error:
    if (in_fd >= 0)
        close(in_fd);
    if (out_fd >= 0)
        close(out_fd);
    return -1;
}

static int append_path(char *dst, size_t dst_size, const char *value) {
    size_t used;

    if (dst == NULL || value == NULL)
        return -1;
    used = strlen(dst);
    if (*value == '\0')
        return 0;
    if (used != 0) {
        if (used + 1 >= dst_size)
            return -1;
        dst[used++] = ':';
        dst[used] = '\0';
    }
    if (used + strlen(value) >= dst_size)
        return -1;
    strcat(dst, value);
    return 0;
}

static int resolve_self_path(const char *argv0, char *path, size_t path_size) {
    char resolved[PATH_MAX];

    if (argv0 == NULL || *argv0 == '\0')
        goto proc_self_fallback;
    if (argv0[0] == '/') {
        if (snprintf(path, path_size, "%s", argv0) >= (int)path_size)
            return -1;
        goto canonicalize;
    }
    if (strchr(argv0, '/') != NULL) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            return -1;
        if (snprintf(path, path_size, "%s/%s", cwd, argv0) >= (int)path_size)
            return -1;
        goto canonicalize;
    }

proc_self_fallback:
    {
        ssize_t len = readlink("/proc/self/exe", path, path_size - 1);
        if (len >= 0) {
            path[len] = '\0';
            return 0;
        }
    }

    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            return -1;
        if (snprintf(path, path_size, "%s/%s", cwd, argv0) >= (int)path_size)
            return -1;
        goto canonicalize;
    }

canonicalize:
    if (realpath(path, resolved) != NULL) {
        if (snprintf(path, path_size, "%s", resolved) >= (int)path_size)
            return -1;
    }
    return 0;
}

static int ensure_rootfs_image(const char *image_path,
                               const char *part_aa,
                               const char *part_ab,
                               const char *part_ac) {
    char tmp_path[PATH_MAX];

    if (path_exists(image_path))
        return 0;
    if (!path_exists(part_aa) || !path_exists(part_ab) || !path_exists(part_ac))
        return -1;
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", image_path) >= (int)sizeof(tmp_path))
        return -1;

    if (copy_file(part_aa, tmp_path) != 0)
        return -1;
    if (append_file(part_ab, tmp_path) != 0) {
        unlink(tmp_path);
        return -1;
    }
    if (append_file(part_ac, tmp_path) != 0) {
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, image_path) != 0) {
        unlink(tmp_path);
        return -1;
    }
    return 0;
}

static int mount_squashfs_image(const char *image_path, const char *mountpoint) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execlp("mount", "mount", "-t", "squashfs", image_path, mountpoint, (char *)NULL);
        _exit(127);
    }

    for (;;) {
        if (waitpid(pid, &status, 0) >= 0)
            break;
        if (errno != EINTR)
            return -1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

static int append_matching_self_paths(char *dst,
                                      size_t dst_size,
                                      const char *self_dir,
                                      const char *ld_library_path) {
    const char *cursor;
    size_t self_dir_len;

    if (dst == NULL || self_dir == NULL)
        return -1;
    if (ld_library_path == NULL || *ld_library_path == '\0')
        return 0;

    self_dir_len = strlen(self_dir);
    cursor = ld_library_path;
    while (*cursor != '\0') {
        const char *next = strchr(cursor, ':');
        size_t len = next == NULL ? strlen(cursor) : (size_t)(next - cursor);

        if (len != 0) {
            char entry[PATH_MAX];

            if (len >= sizeof(entry))
                return -1;
            memcpy(entry, cursor, len);
            entry[len] = '\0';

            if (strcasecmp(entry, self_dir) == 0 ||
                (strncasecmp(entry, self_dir, self_dir_len) == 0 &&
                 entry[self_dir_len] == '/')) {
                if (append_path(dst, dst_size, entry) != 0)
                    return -1;
            }
        }

        if (next == NULL)
            break;
        cursor = next + 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    char self_path[PATH_MAX];
    char self_dir_buf[PATH_MAX];
    char self_name_buf[PATH_MAX];
    char rootfs_mount[PATH_MAX];
    char rootfs_image[PATH_MAX];
    char rootfs_partaa[PATH_MAX];
    char rootfs_partab[PATH_MAX];
    char rootfs_partac[PATH_MAX];
    char real_binary[PATH_MAX];
    char armhf_loader[PATH_MAX];
    char armhf_lib_path[PATH_MAX];
    char armhf_extra_lib_path[PATH_MAX];
    char effective_ld_library_path[PATH_MAX * 2];
    char *args[argc + 6];
    const char *pak_dir;
    const char *temp_data_dir;
    const char *debug_wrapper;
    const char *self_dir;
    const char *self_name;
    pid_t child_pid;
    int status;
    struct sigaction action;

    if (ARMHF_EXEC_WRAPPER_MARKER[0] == '\0')
        return 1;

    pak_dir = getenv("PAK_DIR");
    temp_data_dir = getenv("TEMP_DATA_DIR");
    debug_wrapper = getenv("PMI_DEBUG_ARMHF_WRAPPER");
    if (temp_data_dir == NULL || *temp_data_dir == '\0')
        temp_data_dir = "/tmp";

    if (resolve_self_path((argc > 0) ? argv[0] : NULL, self_path, sizeof(self_path)) != 0) {
        fputs("FATAL: unable to resolve wrapper path\n", stderr);
        return 1;
    }

    if (snprintf(self_dir_buf, sizeof(self_dir_buf), "%s", self_path) >= (int)sizeof(self_dir_buf) ||
        snprintf(self_name_buf, sizeof(self_name_buf), "%s", self_path) >= (int)sizeof(self_name_buf)) {
        fputs("FATAL: wrapper path too long\n", stderr);
        return 1;
    }
    self_dir = dirname(self_dir_buf);
    self_name = basename(self_name_buf);

    if (snprintf(real_binary, sizeof(real_binary), "%s/%s.original", self_dir, self_name) >= (int)sizeof(real_binary)) {
        fputs("FATAL: unable to resolve wrapped binary\n", stderr);
        return 1;
    }

    if (snprintf(rootfs_mount, sizeof(rootfs_mount), "%s/spruceflip-rootfs", temp_data_dir) >= (int)sizeof(rootfs_mount) ||
        snprintf(rootfs_image, sizeof(rootfs_image), "%s/runtime/armhf/miyoo355_rootfs_32.img", pak_dir ? pak_dir : "") >= (int)sizeof(rootfs_image) ||
        snprintf(rootfs_partaa, sizeof(rootfs_partaa), "%s/runtime/armhf/miyoo355_rootfs_32.img_partaa", pak_dir ? pak_dir : "") >= (int)sizeof(rootfs_partaa) ||
        snprintf(rootfs_partab, sizeof(rootfs_partab), "%s/runtime/armhf/miyoo355_rootfs_32.img_partab", pak_dir ? pak_dir : "") >= (int)sizeof(rootfs_partab) ||
        snprintf(rootfs_partac, sizeof(rootfs_partac), "%s/runtime/armhf/miyoo355_rootfs_32.img_partac", pak_dir ? pak_dir : "") >= (int)sizeof(rootfs_partac)) {
        fputs("FATAL: runtime path too long\n", stderr);
        return 1;
    }

    if (pak_dir == NULL || *pak_dir == '\0' || !is_executable_file(real_binary)) {
        fputs("FATAL: missing armhf compatibility runtime\n", stderr);
        return 1;
    }

    if ((path_exists(rootfs_image) || path_exists(rootfs_partaa)) && ensure_dir(rootfs_mount) == 0) {
        if (!path_exists(rootfs_image) &&
            ensure_rootfs_image(rootfs_image, rootfs_partaa, rootfs_partab, rootfs_partac) != 0) {
            fputs("FATAL: failed to materialize armhf runtime image\n", stderr);
            return 1;
        }
        if (path_exists(rootfs_image) && !is_mounted_on(rootfs_mount))
            mount_squashfs_image(rootfs_image, rootfs_mount);
    }

    if (snprintf(armhf_loader, sizeof(armhf_loader), "%s/usr/lib/ld-linux-armhf.so.3", rootfs_mount) >= (int)sizeof(armhf_loader) ||
        snprintf(armhf_lib_path, sizeof(armhf_lib_path), "%s/usr/lib", rootfs_mount) >= (int)sizeof(armhf_lib_path) ||
        snprintf(armhf_extra_lib_path, sizeof(armhf_extra_lib_path), "%s/runtime/armhf/lib", pak_dir) >= (int)sizeof(armhf_extra_lib_path)) {
        fputs("FATAL: runtime path too long\n", stderr);
        return 1;
    }

    if (!is_executable_file(armhf_loader) || !is_dir(armhf_lib_path)) {
        fputs("FATAL: missing armhf compatibility runtime\n", stderr);
        return 1;
    }

    effective_ld_library_path[0] = '\0';
    if (append_path(effective_ld_library_path, sizeof(effective_ld_library_path), armhf_lib_path) != 0) {
        fputs("FATAL: armhf library path too long\n", stderr);
        return 1;
    }
    if (is_dir(armhf_extra_lib_path) &&
        append_path(effective_ld_library_path, sizeof(effective_ld_library_path), armhf_extra_lib_path) != 0) {
        fputs("FATAL: armhf library path too long\n", stderr);
        return 1;
    }
    if (append_matching_self_paths(effective_ld_library_path,
                                   sizeof(effective_ld_library_path),
                                   self_dir,
                                   getenv("LD_LIBRARY_PATH")) != 0) {
        fputs("FATAL: armhf library path too long\n", stderr);
        return 1;
    }

    if (debug_wrapper != NULL && *debug_wrapper != '\0') {
        fprintf(stderr, "PMI_DEBUG armhf_self_path=%s\n", self_path);
        fprintf(stderr, "PMI_DEBUG armhf_real_binary=%s\n", real_binary);
        fprintf(stderr, "PMI_DEBUG armhf_loader=%s\n", armhf_loader);
        fprintf(stderr, "PMI_DEBUG armhf_ld_library_path=%s\n", effective_ld_library_path);
    }

    setenv("LD_LIBRARY_PATH", effective_ld_library_path, 1);
    prctl(PR_SET_NAME, (unsigned long)self_name, 0UL, 0UL, 0UL);

    child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        return 1;
    }

    if (child_pid == 0) {
        prctl(PR_SET_PDEATHSIG, (unsigned long)SIGTERM, 0UL, 0UL, 0UL);
        if (getppid() == 1)
            _exit(1);

        args[0] = armhf_loader;
        args[1] = "--argv0";
        args[2] = (char *)self_name;
        args[3] = "--library-path";
        args[4] = effective_ld_library_path;
        args[5] = real_binary;
        for (int i = 1; i < argc; i++)
            args[i + 5] = argv[i];
        args[argc + 5] = NULL;

        execv(armhf_loader, args);
        perror("execv");
        _exit(127);
    }

    g_child_pid = (sig_atomic_t)child_pid;

    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = forward_signal_to_child;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);

    for (;;) {
        pid_t result = waitpid(child_pid, &status, 0);
        if (result < 0) {
            if (errno == EINTR)
                continue;
            perror("waitpid");
            return 1;
        }
        break;
    }

    g_child_pid = -1;

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}
