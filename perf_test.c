#include <sys/linker.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <vm/vm_param.h>
#include <signal.h>
#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifndef DRIVER_PATH
#error DRIVER_PATH not defined
#endif
#ifndef MODULE_PATH
#error MODULE_PATH not defined
#endif
#ifndef DRIVER_NAME
#error DRIVER_NAME not defined
#endif

static int fd = -1;
static pid_t child = -1;

ATF_TC_WITHOUT_HEAD(driver_stress_load);
ATF_TC_BODY(driver_stress_load, tc)
{
    int kld_id;
    for (int i = 0; i < 1000; ++i) {
        kld_id = kldload(DRIVER_PATH);
        ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

        ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));
    }
}

ATF_TC_WITH_CLEANUP(driver_concurrency);
ATF_TC_HEAD(driver_concurrency, tc) {}
ATF_TC_BODY(driver_concurrency, tc) {
    int status;

    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    int sync[2];
    ATF_REQUIRE(pipe(sync) == 0);

    child = fork();
    ATF_REQUIRE_MSG(child >= 0, "fork() failed: %s", strerror(errno));

    if (child == 0) {
        close(sync[0]);
        int child_fd = open(MODULE_PATH, O_RDWR);
        if (child_fd < 0) _exit(1);

        write(sync[1], "1", 1);

        ssize_t child_wrt, child_rd;
        char child_buff;

        for (int i = 0; i < 1000; i++) {
            child_wrt = write(child_fd, "A", 1);
            if (child_wrt < 0) _exit(1);
            child_rd = read(child_fd, &child_buff, 1);
            if (child_rd != 1) _exit(1);

            lseek(child_fd, 0, SEEK_SET);
        }
        close(child_fd);
        _exit(0);
    }

    char syncb;
    close(sync[1]);
    ATF_REQUIRE(read(sync[0], &syncb, 1) == 1);

    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "parent open failed: %s", strerror(errno));

    ssize_t wrt, rd;
    char buff;

    for (int i = 0; i < 1000; i++) {
        wrt = write(fd, "A", 1);
        ATF_REQUIRE_MSG(wrt >= 0, "Unable to write to MODULE_PATH: %s", strerror(errno));
        rd = read(fd, &buff, 1);
        ATF_REQUIRE_MSG(rd == 1, "Unable to read from MODULE_PATH: %s", strerror(errno));

        lseek(fd, 0, SEEK_SET);
    }

    close(fd);
    ATF_REQUIRE(waitpid(child, &status, 0) == child);
    ATF_REQUIRE(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload failed: %s", strerror(errno));

    kld_id = -1;
    child  = -1;
}
ATF_TC_CLEANUP(driver_concurrency, tc) {
    if (child > 0) {
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
    }

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) {
        (void)kldunload(loaded);
    }
}

static void read_vmtotal(struct vmtotal *vt)
{
    int mib[2] = {CTL_VM, VM_TOTAL};
    size_t len = sizeof(*vt);
    ATF_REQUIRE_MSG(sysctl(mib, 2, vt, &len, NULL, 0) == 0, "sysctl failed: %s", strerror(errno));
    ATF_REQUIRE_MSG(len == sizeof(*vt), "sysctl returned unexpected size");
}
ATF_TC_WITH_CLEANUP(driver_leakage);
ATF_TC_HEAD(driver_leakage, tc) {}
ATF_TC_BODY(driver_leakage, tc)
{
    struct vmtotal before, after;
    read_vmtotal(&before);

    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));
    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open from MODULE_PATH: %s", strerror(errno));

    ssize_t wrt, rd;
    char buff[21] = {0};
    for (int i = 0; i < 1000; i++) {
        wrt = write(fd, "Better not leak this!", 21);
        ATF_REQUIRE_MSG(wrt >= 0, "Unable to write to MODULE_PATH: %s", strerror(errno));
        rd = read(fd, buff, 21);
        ATF_REQUIRE_MSG(rd == 21, "Unable to read from MODULE_PATH: %s", strerror(errno));

        lseek(fd, 0, SEEK_SET);
    }

    ATF_REQUIRE_MSG(close(fd) == 0, "close failed: %s", strerror(errno));
    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));

    kld_id = -1;
    fd = -1;

    read_vmtotal(&after);

    ATF_CHECK_MSG(after.t_free >= before.t_free - 10, "free pages dropped from %jd to %jd", 
            (intmax_t)before.t_free, (intmax_t)after.t_free);
    ATF_CHECK_MSG(after.t_vm   <= before.t_vm   + 10, "total VM pages grew from %jd to %jd",
            (intmax_t)before.t_vm, (intmax_t)after.t_vm);
}
ATF_TC_CLEANUP(driver_leakage, tc)
{
    if (fd > 0) close(fd);

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_stress_load);
    ATF_TP_ADD_TC(tp, driver_concurrency);
    ATF_TP_ADD_TC(tp, driver_leakage);

    return atf_no_error();
}
