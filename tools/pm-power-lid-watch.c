#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DEFAULT_SYSTEM_PATH "/mnt/SDCARD/.system/my355"
#define SYSTEM_SUSPEND_FLAG "/tmp/system_suspend"
#define POWER_GUARD_MS 1000ULL
#define POWER_LONG_PRESS_MS 1000ULL
#define POWER_SCAN_RETRY_MS 1000ULL
#define LOOP_TIMEOUT_MS 100
#define MAX_EVENT_DEVICES 32
#define LID_SENSOR_PATH "/sys/devices/platform/hall-mh248/hallvalue"
#define LED_BRIGHTNESS_PATH "/sys/class/leds/work/brightness"

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signo) {
    (void)signo;
    keep_running = 0;
}

static void log_message(const char *level, const char *fmt, ...) {
    va_list args;

    fprintf(stdout, "%s ", level);
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fputc('\n', stdout);
    fflush(stdout);
}

static uint64_t monotonic_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int read_small_file(const char *path, char *buffer, size_t buffer_size) {
    int fd;
    ssize_t read_now;

    if (buffer == NULL || buffer_size == 0)
        return -1;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    read_now = read(fd, buffer, buffer_size - 1);
    close(fd);
    if (read_now <= 0)
        return -1;

    buffer[read_now] = '\0';
    return 0;
}

static int read_lid_state(int *state) {
    char buffer[32];
    char value;

    if (state == NULL)
        return -1;
    if (read_small_file(LID_SENSOR_PATH, buffer, sizeof(buffer)) != 0)
        return -1;

    value = buffer[0];
    if (value == '0') {
        *state = 0;
        return 0;
    }
    if (value == '1') {
        *state = 1;
        return 0;
    }
    return -1;
}

static int write_text_file(const char *path, const char *text) {
    int fd;
    size_t remaining;
    const char *cursor;

    fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    remaining = strlen(text);
    cursor = text;
    while (remaining > 0) {
        ssize_t written = write(fd, cursor, remaining);

        if (written < 0) {
            int saved_errno = errno;

            close(fd);
            errno = saved_errno;
            return -1;
        }
        remaining -= (size_t)written;
        cursor += written;
    }

    close(fd);
    return 0;
}

static int touch_file(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_CLOEXEC, 0644);

    if (fd < 0)
        return -1;
    close(fd);
    return 0;
}

static int wait_for_child(pid_t pid, const char *label) {
    int status = 0;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            if (!keep_running)
                return -1;
            continue;
        }
        log_message("PMI_WARN", "%s_wait_failed errno=%d (%s)", label, errno, strerror(errno));
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) {
        log_message("PMI_WARN", "%s_signaled=%d", label, WTERMSIG(status));
        return -1;
    }
    return -1;
}

static int run_program(const char *label, const char *path) {
    pid_t pid;
    int rc;

    pid = fork();
    if (pid < 0) {
        log_message("PMI_WARN", "%s_fork_failed errno=%d (%s)", label, errno, strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execl(path, path, (char *)NULL);
        _exit(127);
    }

    rc = wait_for_child(pid, label);
    log_message("PMI_DIAG", "%s_exit=%d path=%s", label, rc, path);
    return rc;
}

static int resolve_system_script(char *buffer, size_t buffer_size, const char *name) {
    const char *system_path = getenv("SYSTEM_PATH");

    if (buffer == NULL || buffer_size == 0 || name == NULL)
        return -1;

    if (system_path != NULL && system_path[0] != '\0') {
        if (snprintf(buffer, buffer_size, "%s/bin/%s", system_path, name) < (int)buffer_size &&
            access(buffer, F_OK) == 0) {
            return 0;
        }
    }

    if (snprintf(buffer, buffer_size, "%s/bin/%s", DEFAULT_SYSTEM_PATH, name) >= (int)buffer_size)
        return -1;
    return access(buffer, F_OK) == 0 ? 0 : -1;
}

static int run_suspend_fallback(void) {
    int rc = 0;

    if (touch_file(SYSTEM_SUSPEND_FLAG) != 0) {
        log_message("PMI_WARN", "suspend_flag_touch_failed path=%s errno=%d (%s)",
                    SYSTEM_SUSPEND_FLAG, errno, strerror(errno));
    }

    sync();

    if (write_text_file("/sys/power/mem_sleep", "deep\n") != 0) {
        log_message("PMI_WARN", "suspend_mem_sleep_write_failed errno=%d (%s)", errno, strerror(errno));
    }
    if (write_text_file("/sys/power/state", "mem\n") != 0) {
        log_message("PMI_WARN", "suspend_state_write_failed errno=%d (%s)", errno, strerror(errno));
        rc = -1;
    }

    if (write_text_file(LED_BRIGHTNESS_PATH, "100\n") != 0) {
        log_message("PMI_WARN", "resume_led_restore_failed path=%s errno=%d (%s)",
                    LED_BRIGHTNESS_PATH, errno, strerror(errno));
    }
    unlink(SYSTEM_SUSPEND_FLAG);
    return rc;
}

static int trigger_suspend(const char *reason) {
    char script_path[PATH_MAX];
    int rc;

    log_message("PMI_DIAG", "power_lid_suspend reason=%s", reason);
    if (resolve_system_script(script_path, sizeof(script_path), "suspend") == 0) {
        rc = run_program("power_lid_suspend", script_path);
        return rc == 0 ? 0 : -1;
    }

    log_message("PMI_WARN", "power_lid_suspend_script_missing fallback=kernel");
    return run_suspend_fallback();
}

static int trigger_shutdown(const char *reason) {
    char script_path[PATH_MAX];
    int rc;

    log_message("PMI_DIAG", "power_lid_shutdown reason=%s", reason);
    if (resolve_system_script(script_path, sizeof(script_path), "shutdown") == 0) {
        rc = run_program("power_lid_shutdown", script_path);
        return rc == 0 ? 0 : -1;
    }

    log_message("PMI_WARN", "power_lid_shutdown_script_missing fallback=poweroff");
    rc = system("poweroff");
    if (rc == -1) {
        log_message("PMI_WARN", "power_lid_shutdown_fallback_failed errno=%d (%s)", errno, strerror(errno));
        return -1;
    }
    return 0;
}

static int power_device_has_key_power(int fd) {
    unsigned char key_bits[(KEY_MAX + 8) / 8];

    memset(key_bits, 0, sizeof(key_bits));
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0)
        return 0;
    return (key_bits[KEY_POWER / 8] & (1u << (KEY_POWER % 8))) != 0;
}

static int open_power_device(void) {
    int index;

    for (index = 0; index < MAX_EVENT_DEVICES; index++) {
        char path[64];
        int fd;

        snprintf(path, sizeof(path), "/dev/input/event%d", index);
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        if (power_device_has_key_power(fd)) {
            log_message("PMI_DIAG", "power_input_selected=%s", path);
            return fd;
        }
        close(fd);
    }

    return -1;
}

static void drain_power_events(int fd) {
    struct input_event ev;

    if (fd < 0)
        return;
    while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
    }
}

int main(void) {
    int power_fd = -1;
    int has_lid = access(LID_SENSOR_PATH, R_OK) == 0;
    int last_lid_state = 1;
    bool power_pressed = false;
    uint64_t power_pressed_at = 0;
    uint64_t ignore_until = 0;
    uint64_t next_power_scan_at = 0;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGHUP, handle_signal);
    signal(SIGQUIT, handle_signal);

    if (has_lid && read_lid_state(&last_lid_state) == 0)
        log_message("PMI_DIAG", "power_lid_lid_sensor=ready state=%d", last_lid_state);
    else
        has_lid = 0;

    while (keep_running) {
        struct pollfd pollfd;
        uint64_t now = monotonic_ms();

        if (power_fd < 0 && now >= next_power_scan_at) {
            power_fd = open_power_device();
            next_power_scan_at = now + POWER_SCAN_RETRY_MS;
            if (power_fd < 0)
                log_message("PMI_WARN", "power_input_missing retry_ms=%u", (unsigned)POWER_SCAN_RETRY_MS);
        }

        if (power_pressed && now >= ignore_until && now - power_pressed_at >= POWER_LONG_PRESS_MS) {
            power_pressed = false;
            power_pressed_at = 0;
            if (trigger_shutdown("power_hold") == 0) {
                ignore_until = monotonic_ms() + POWER_GUARD_MS;
                drain_power_events(power_fd);
            }
        }

        if (has_lid) {
            int current_lid_state;

            if (read_lid_state(&current_lid_state) == 0) {
                if (now >= ignore_until && last_lid_state == 1 && current_lid_state == 0) {
                    if (trigger_suspend("lid_close") == 0) {
                        ignore_until = monotonic_ms() + POWER_GUARD_MS;
                        drain_power_events(power_fd);
                        if (read_lid_state(&current_lid_state) != 0)
                            current_lid_state = 0;
                    }
                }
                last_lid_state = current_lid_state;
            }
        }

        if (power_fd < 0) {
            poll(NULL, 0, LOOP_TIMEOUT_MS);
            continue;
        }

        pollfd.fd = power_fd;
        pollfd.events = POLLIN;
        pollfd.revents = 0;

        if (poll(&pollfd, 1, LOOP_TIMEOUT_MS) <= 0)
            continue;

        if ((pollfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            close(power_fd);
            power_fd = -1;
            power_pressed = false;
            power_pressed_at = 0;
            continue;
        }

        if ((pollfd.revents & POLLIN) != 0) {
            struct input_event ev;

            while (read(power_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                now = monotonic_ms();
                if (ev.type != EV_KEY || (ev.code != KEY_POWER && ev.code != 102))
                    continue;
                if (now < ignore_until)
                    continue;

                if (ev.value == 1) {
                    power_pressed = true;
                    power_pressed_at = now;
                } else if (ev.value == 0) {
                    if (power_pressed && now - power_pressed_at < POWER_LONG_PRESS_MS) {
                        if (trigger_suspend("power_short_press") == 0) {
                            ignore_until = monotonic_ms() + POWER_GUARD_MS;
                            drain_power_events(power_fd);
                            if (has_lid)
                                read_lid_state(&last_lid_state);
                        }
                    }
                    power_pressed = false;
                    power_pressed_at = 0;
                }
            }
        }
    }

    if (power_fd >= 0)
        close(power_fd);
    log_message("PMI_DIAG", "power_lid_helper_exit");
    return 0;
}
