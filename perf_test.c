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
#include <pthread.h>

#ifndef DRIVER_PATH
#error DRIVER_PATH not defined
#endif
#ifndef MODULE_PATH
#error MODULE_PATH not defined
#endif
#ifndef DRIVER_NAME
#error DRIVER_NAME not defined
#endif

ATF_TC_WITH_CLEANUP(driver_stress_load);
ATF_TC_HEAD(driver_stress_load, tc) {}
ATF_TC_BODY(driver_stress_load, tc)
{
    int kld_id;
    for (int i = 0; i < 1000; ++i) {
        kld_id = kldload(DRIVER_PATH);
        ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

        ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));
    }
}
ATF_TC_CLEANUP(driver_stress_load, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

static void* writer(void* __unused arg)
{
    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE(fd >= 0);
    char buff;
    for (int i = 0; i < 1000; ++i) {
        ATF_REQUIRE(write(fd, "A", 1) != -1);
        ATF_REQUIRE(read(fd, &buff, 1) != -1);
        ATF_REQUIRE_EQ(0, lseek(fd, 0, SEEK_SET));
    }
    close(fd);
    return NULL;
}
ATF_TC_WITH_CLEANUP(driver_concurrency);
ATF_TC_HEAD(driver_concurrency, tc) {}
ATF_TC_BODY(driver_concurrency, tc) {
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    pthread_t t1, t2;
    
    pthread_create(&t1, NULL, writer, NULL);
    pthread_create(&t2, NULL, writer, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_concurrency, tc) {
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
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
    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open from MODULE_PATH: %s", strerror(errno));

    char buff[21] = {0};
    for (int i = 0; i < 1000; i++) {
        ATF_REQUIRE(write(fd, "Better not leak this!", 21) != -1);
        ATF_REQUIRE(read(fd, &buff, 21) != -1);

        lseek(fd, 0, SEEK_SET);
    }

    close(fd);
    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));

    read_vmtotal(&after);

    ATF_CHECK_MSG(after.t_free >= before.t_free - 10, "free pages dropped from %jd to %jd", 
            (intmax_t)before.t_free, (intmax_t)after.t_free);
    ATF_CHECK_MSG(after.t_vm <= before.t_vm + 10, "total VM pages grew from %jd to %jd",
            (intmax_t)before.t_vm, (intmax_t)after.t_vm);
}
ATF_TC_CLEANUP(driver_leakage, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_stress_load);
    ATF_TP_ADD_TC(tp, driver_concurrency);
    ATF_TP_ADD_TC(tp, driver_leakage);

    return atf_no_error();
}
