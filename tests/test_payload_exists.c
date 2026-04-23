#include <assert.h>
#include <sys/stat.h>

int main(void) {
    struct stat st = {0};

    assert(stat("payload/PORTS.pak/launch.sh", &st) == 0);
    assert(stat("payload/PORTS.pak/files/PortMaster.txt", &st) == 0);
    assert(stat("payload/PORTS.pak/files/config.py", &st) == 0);
    assert(stat("payload/PORTS.pak/files/gamecontrollerdb.txt", &st) == 0);
    assert(stat("payload/PORTS.pak/files/gamecontrollerdb_nintendo.txt", &st) == 0);
    assert(stat("payload/PORTS.pak/files/libmali-g2p0.so.1.9.0", &st) == 0);
    assert(stat("third_party/rsync/my355/rsync", &st) == 0);
    assert(stat("third_party/rsync/my355/rsync.txt", &st) == 0);
    return 0;
}
