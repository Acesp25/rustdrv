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
#include <semaphore.h>
#include <machine/atomic.h>

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
    ATF_REQUIRE(kld_id >= 0);

    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE(fd >= 0);

    ssize_t rd = read(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (rd < 0));

    ssize_t wrt = write(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (wrt < 0));

    close(fd);

    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_null_input, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TC_WITH_CLEANUP(driver_open_unload);
ATF_TC_HEAD(driver_open_unload, tc) {}
ATF_TC_BODY(driver_open_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE(kld_id >= 0);

    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE(fd >= 0);

    ATF_REQUIRE_ERRNO(EBUSY, kldunload(kld_id) == -1);

    close(fd);
    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_open_unload, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

static sem_t mutex;
static volatile int loop = 1;
static void* writer(void* __unused arg) {
    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE(fd >= 0);
    char buff; 	

    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    ATF_REQUIRE_EQ(0, sem_post(&mutex)); 

    while (loop) {
        (void)write(fd, "A", 1);
        (void)read(fd, &buff, 1); 
    }
    close(fd);
    return NULL;
}
static void* unloader(void* __unused arg) {
    ATF_REQUIRE_EQ(0, sem_wait(&mutex));

    sleep(1); // time for the while loop to start

    ATF_REQUIRE_ERRNO(EBUSY, kldunload(kldfind(DRIVER_NAME)) == -1);

    atomic_subtract_int(&loop, 1); // make writer loop stop

    return NULL;  
}
ATF_TC_WITH_CLEANUP(driver_hot_unload);
ATF_TC_HEAD(driver_hot_unload, tc) {}
ATF_TC_BODY(driver_hot_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE(kld_id >= 0);

    ATF_REQUIRE_EQ(0, sem_init(&mutex, 0, 0));

    pthread_t t1, t2;
    
    ATF_REQUIRE_EQ(0, pthread_create(&t1, NULL, writer, NULL));
    ATF_REQUIRE_EQ(0, pthread_create(&t2, NULL, unloader, NULL));
    
    ATF_REQUIRE_EQ(0, pthread_join(t1, NULL));
    ATF_REQUIRE_EQ(0, pthread_join(t2, NULL));

    ATF_REQUIRE_EQ(0, sem_destroy(&mutex));
    
    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_hot_unload, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TC_WITH_CLEANUP(driver_permission);
ATF_TC_HEAD(driver_permission, tc) {}
ATF_TC_BODY(driver_permission, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE(kld_id >= 0);

    ATF_REQUIRE_EQ(0, chmod(MODULE_PATH, S_IRUSR|S_IWUSR));

    // 65534 is UID of nobody
    ATF_REQUIRE_EQ(0, seteuid(65534));

    ATF_REQUIRE_ERRNO(EACCES, open(MODULE_PATH, O_RDWR) == -1);

    ATF_REQUIRE_EQ(0, seteuid(0));

    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_permission, tc)
{
    seteuid(0);
    chmod(MODULE_PATH, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_null_input);
    ATF_TP_ADD_TC(tp, driver_open_unload);
    ATF_TP_ADD_TC(tp, driver_hot_unload);
    ATF_TP_ADD_TC(tp, driver_permission);
    
    return atf_no_error();
}
