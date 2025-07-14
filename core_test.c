#include <sys/linker.h>
#include <sys/jail.h>
#include <sys/wait.h>
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

ATF_TC_WITH_CLEANUP(driver_load_unload);
ATF_TC_HEAD(driver_load_unload, tc) {}
ATF_TC_BODY(driver_load_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE(kld_id >= 0);

    ATF_REQUIRE_MSG(kldfind(DRIVER_NAME) >= 0, "kld not found after loading: %s", strerror(errno));

    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_load_unload, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TC_WITH_CLEANUP(driver_open_close);
ATF_TC_HEAD(driver_open_close, tc) {}
ATF_TC_BODY(driver_open_close, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE(kld_id >= 0);

    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE(fd >= 0);

    close(fd);
    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_open_close, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TC_WITH_CLEANUP(driver_read_write);
ATF_TC_HEAD(driver_read_write, tc) {}
ATF_TC_BODY(driver_read_write, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE(kld_id >= 0);

    int fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE(fd >= 0);

    ATF_REQUIRE(write(fd, "Hello :D", 8) != -1);
    char buff[8] = {0};
    ATF_REQUIRE(read(fd, &buff, 8) != -1);

    close(fd);
    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_read_write, tc) 
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TC_WITH_CLEANUP(driver_jail);
ATF_TC_HEAD(driver_jail, tc) {}
ATF_TC_BODY(driver_jail, tc)
{
    struct jail j;
    int jail_id;

    char path[] = "/";
    char hostname[] = "echojail";
    char jailname[] = "echojail";

    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE(kld_id >= 0);

    pid_t child = fork();
    ATF_REQUIRE(child >= 0);

    if (child == 0) { 
        memset(&j, 0, sizeof(j));
        j.version = JAIL_API_VERSION;
        j.path = path;
        j.hostname = hostname;
        j.jailname = jailname;

        jail_id = jail(&j);
        if (jail_id < 0) _exit(1);

        int fd = open(MODULE_PATH, O_RDWR);
        if (fd < 0) _exit(1);

        if (write(fd, "Hello :D", 8) == -1) _exit(1);
        char buff[8] = {0};
        if (read(fd, &buff, 8) == -1) _exit(1);

        close(fd);
        _exit(0);
    }

    int status;
    ATF_REQUIRE(waitpid(child, &status, 0) == child);
    ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0, 
            "child failed with status %d", WEXITSTATUS(status));

    ATF_REQUIRE_EQ(0, kldunload(kld_id));
}
ATF_TC_CLEANUP(driver_jail, tc)
{
    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) kldunloadf(loaded, LINKER_UNLOAD_FORCE);
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_load_unload);
    ATF_TP_ADD_TC(tp, driver_open_close);
    ATF_TP_ADD_TC(tp, driver_read_write);
    ATF_TP_ADD_TC(tp, driver_jail);
    return atf_no_error();
}
