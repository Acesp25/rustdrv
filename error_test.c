#include <sys/linker.h>
#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DRIVER_PATH "/home/dev/freebsd-kernel-module-rust/hello.ko"
#define MODULE_PATH "/dev/rustmodule"
#define DRIVER_NAME "hello.ko"

static int fd = -1; // file i/o global variable for cleanup, need to improve

ATF_TC_WITH_CLEANUP(driver_null_input);
ATF_TC_HEAD(driver_null_input, tc) {}
ATF_TC_BODY(driver_null_input, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open /dev/rustmodule: %s", strerror(errno));

    ssize_t wrt = write(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (wrt < 0));

    ssize_t rd = read(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (rd < 0));

    close(fd);

    int unloaded = kldunload(kld_id);
    ATF_REQUIRE_MSG(unloaded == 0, "kldunload(2) failed: %s", strerror(errno));

    // Set global to -1 so cleanup can ignore
    fd = -1;
}
ATF_TC_CLEANUP(driver_null_input, tc)
{
    if (fd >= 0) {
        close(fd);
    }

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) {
        (void)kldunload(loaded);
    }
}

ATF_TC_WITH_CLEANUP(driver_open_unload);
ATF_TC_HEAD(driver_open_unload, tc) {}
ATF_TC_BODY(driver_open_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open /dev/rustmodule: %s", strerror(errno));

    ATF_REQUIRE_ERRNO(EBUSY, kldunload(kld_id) == -1);

    close(fd);

    int unloaded = kldunload(kld_id);
    ATF_REQUIRE_MSG(unloaded == 0, "kldunload(2) failed: %s", strerror(errno));

    fd = -1;
}
ATF_TC_CLEANUP(driver_open_unload, tc)
{
    if (fd >= 0) {
        close(fd);
    }

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) {
        (void)kldunload(loaded);
    }
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_null_input);
    ATF_TP_ADD_TC(tp, driver_open_unload);
    
    return atf_no_error();
}
