#include <sys/param.h>
#include <sys/linker.h>
#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DRIVER_PATH "/home/dev/freebsd-kernel-module-rust/hello.ko"

ATF_TC_WITHOUT_HEAD(driver_load_unload);
ATF_TC_BODY(driver_load_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    int unloaded = kldunload(kld_id);
    ATF_REQUIRE_MSG(unloaded == 0, "kldunload(2) failed: %s", strerror(errno));
}

ATF_TC_WITHOUT_HEAD(driver_open_close);
ATF_TC_BODY(driver_open_close, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    int fd = open("/dev/rustmodule", O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open /dev/rustmodule: %s", strerror(errno));

    close(fd);

    int unloaded = kldunload(kld_id);
    ATF_REQUIRE_MSG(unloaded == 0, "kldunload(2) failed: %s", strerror(errno));
}

ATF_TC_WITHOUT_HEAD(driver_read_write);
ATF_TC_BODY(driver_read_write, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    int fd = open("/dev/rustmodule", O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open /dev/rustmodule: %s", strerror(errno));

    ssize_t wrt = write(fd, "Hello :D", 8);
    ATF_REQUIRE_MSG(wrt >= 0, "Unable to write to /dev/rustmodule: %s", strerror(errno));
    char buff[8] = {0};
    ssize_t rd = read(fd, buff, 8);
    ATF_REQUIRE_MSG(rd == 8, "Unable to read from /dev/rustmodule: %s", strerror(errno));

    close(fd);

    int unloaded = kldunload(kld_id);
    ATF_REQUIRE_MSG(unloaded == 0, "kldunload(2) failed: %s", strerror(errno));
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_load_unload);
    ATF_TP_ADD_TC(tp, driver_open_close);
    ATF_TP_ADD_TC(tp, driver_read_write);
    return atf_no_error();
}
