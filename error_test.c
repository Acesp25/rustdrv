#include <sys/linker.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
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

ATF_TC_WITH_CLEANUP(driver_null_input);
ATF_TC_HEAD(driver_null_input, tc) {}
ATF_TC_BODY(driver_null_input, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open MODULE_PATH: %s", strerror(errno));

    ssize_t rd = read(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (rd < 0));

    ssize_t wrt = write(fd, NULL, 10);
    ATF_REQUIRE_ERRNO(EFAULT, (wrt < 0));

    close(fd);

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));

    fd = -1;
}
ATF_TC_CLEANUP(driver_null_input, tc)
{
    if (fd >= 0) close(fd);

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

ATF_TC_WITH_CLEANUP(driver_open_unload);
ATF_TC_HEAD(driver_open_unload, tc) {}
ATF_TC_BODY(driver_open_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open MODULE_PATH: %s", strerror(errno));

    ATF_REQUIRE_ERRNO(EBUSY, kldunload(kld_id) == -1);

    close(fd);

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));

    fd = -1;
}
ATF_TC_CLEANUP(driver_open_unload, tc)
{
    if (fd >= 0) close(fd);

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

ATF_TC_WITH_CLEANUP(driver_hot_unload);
ATF_TC_HEAD(driver_hot_unload, tc) {}
ATF_TC_BODY(driver_hot_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    int sync[2];
    ATF_REQUIRE(pipe(sync) == 0);

    child = fork();
    ATF_REQUIRE_MSG(child >= 0, "fork() failed: %s", strerror(errno));

    if (child == 0) {
        close(sync[0]);

        fd = open(MODULE_PATH, O_RDWR);
        if (fd < 0) _exit(1);

        char mark = '!';
        char c;
        if (write(sync[1], &mark, 1) != 1) {
            _exit(2);
        }
        close(sync[1]);

        for (int i = 0; i < 300; ++i) {
            ssize_t wrt = write(fd, &mark, 1);
            if (wrt != 1) _exit(1);
            ssize_t rd = read(fd, &c, 1);
            if (rd != 1) _exit(1);
            lseek(fd, 0, SEEK_SET);
        }
        close(fd);
        _exit(0);
    }

    close(sync[1]);
    char mark;
    ATF_REQUIRE_MSG(read(sync[0], &mark, 1) == 1, "sync read failed: %s", strerror(errno));
    close(sync[0]);

    ATF_REQUIRE_ERRNO(EBUSY, kldunload(kld_id) == -1);

    int status;
    ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child, "waitpid() failed: %s", strerror(errno));
    ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0, "I/O child exited abnormally");

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));

    fd = -1;
    child = -1;
}
ATF_TC_CLEANUP(driver_hot_unload, tc)
{
    if (fd >= 0) close(fd);

    if (child > 0) {
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
    }

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
