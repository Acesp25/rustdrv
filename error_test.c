#include <sys/linker.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
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

ATF_TC_WITH_CLEANUP(driver_null_input);
ATF_TC_HEAD(driver_null_input, tc) {}
ATF_TC_BODY(driver_null_input, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open MODULE_PATH: %s", strerror(errno));

    ssize_t rd = read(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (rd < 0));

    ssize_t wrt = write(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (wrt < 0));

    close(fd);

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));
}
ATF_TC_CLEANUP(driver_null_input, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

ATF_TC_WITH_CLEANUP(driver_open_unload);
ATF_TC_HEAD(driver_open_unload, tc) {}
ATF_TC_BODY(driver_open_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open MODULE_PATH: %s", strerror(errno));

    int unload = kldunload(kld_id);
    ATF_REQUIRE_MSG(unload < 0, "KLDUNLOAD returned %d", unload);

    close(fd);

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));
}
ATF_TC_CLEANUP(driver_open_unload, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

static void* writer(void* __unused arg) {
    int fd = open(MODULE_PATH, O_RDWR);
    char buff; 	
    while(1) {
        ATF_REQUIRE(write(fd, "A", 1) != -1);
        ATF_REQUIRE(read(fd, &buff, 1) != -1); 
    }
    close(fd);
    return NULL;
}
static void* unloader(void* __unused arg) {
    ATF_REQUIRE_ERRNO(EFAULT, kldunload(kldfind(DRIVER_NAME)) == -1);
    return NULL;  
}
ATF_TC_WITH_CLEANUP(driver_hot_unload);
ATF_TC_HEAD(driver_hot_unload, tc) {}
ATF_TC_BODY(driver_hot_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    pthread_t t1, t2;
    
    pthread_create(&t1, NULL, writer, NULL);
    sleep(2);
    pthread_create(&t2, NULL, unloader, NULL);
    
    pthread_join(t2, NULL);
    pthread_cancel(t1);
    pthread_join(t1, NULL);

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));
}
ATF_TC_CLEANUP(driver_hot_unload, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

ATF_TC_WITH_CLEANUP(driver_permission);
ATF_TC_HEAD(driver_permission, tc) {}
ATF_TC_BODY(driver_permission, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    ATF_REQUIRE_MSG(chmod(MODULE_PATH, S_IRUSR|S_IWUSR) == 0, "chmod(1) failed: %s", strerror(errno));

    // 65534 is UID of nobody
    ATF_REQUIRE_MSG(seteuid(65534) == 0, "seteuid(nobody) failed: %s", strerror(errno));

    ATF_REQUIRE_ERRNO(EACCES, open(MODULE_PATH, O_RDWR) == -1);

    ATF_REQUIRE_MSG(seteuid(0) == 0, "seteuid() root failed: %s", strerror(errno));

    int unloaded = kldunload(kld_id);
    ATF_REQUIRE_MSG(unloaded == 0, "kldunload(2) failed: %s", strerror(errno));
}
ATF_TC_CLEANUP(driver_permission, tc)
{
    (void)seteuid(0);
    (void)chmod(MODULE_PATH, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_null_input);
    ATF_TP_ADD_TC(tp, driver_open_unload);
    ATF_TP_ADD_TC(tp, driver_hot_unload);
    ATF_TP_ADD_TC(tp, driver_permission);
    
    return atf_no_error();
}
