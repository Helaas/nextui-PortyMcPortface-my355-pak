#ifndef PM_TEST_LINUX_INPUT_H
#define PM_TEST_LINUX_INPUT_H

#include <sys/time.h>

#define EV_KEY 0x01
#define KEY_POWER 116
#define KEY_MAX 0x2ff
#define EVIOCGBIT(ev, len) 0

struct input_event {
    struct timeval time;
    unsigned short type;
    unsigned short code;
    int value;
};

#endif
