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

// Global variables to help with cleanup
static int fd = -1;
static pid_t child = -1;

/*
 * Driver Load/Unload Test
 *
 * Loads the driver given the path in the Makefile
 * Checks to see if driver has been loaded
 * Unloads the driver with the load id
 */
ATF_TC_WITHOUT_HEAD(driver_load_unload);
ATF_TC_BODY(driver_load_unload, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    ATF_REQUIRE_MSG(kldfind(DRIVER_NAME) >= 0, "kld not found after loading: %s", strerror(errno));

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));
}

/*
 * Driver Open Close test
 *
 * Tests the drivers ability to open and close the module file.
 * Loads the driver, opens the /dev/rustmodule file
 * Closes file, the unloads the driver
 */
ATF_TC_WITHOUT_HEAD(driver_open_close);
ATF_TC_BODY(driver_open_close, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open /dev/rustmodule: %s", strerror(errno));

    close(fd);

    int unloaded = kldunload(kld_id);
    ATF_REQUIRE_MSG(unloaded == 0, "kldunload(2) failed: %s", strerror(errno));
}

/* 
 * Driver Read and Write test
 *
 * Tests the ability to read and write to the module path/
 * Loads the driver, opens the file, writes a small string, 
 * reads the passed string, then safely closes and unloads the driver
 */
ATF_TC_WITHOUT_HEAD(driver_read_write);
ATF_TC_BODY(driver_read_write, tc)
{
    int kld_id = kldload(DRIVER_PATH);
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    fd = open(MODULE_PATH, O_RDWR);
    ATF_REQUIRE_MSG(fd >= 0, "Unable to open /dev/rustmodule: %s", strerror(errno));

    ssize_t wrt, rd;
    char buff[8] = {0};

    wrt = write(fd, "Hello :D", 8);
    ATF_REQUIRE_MSG(wrt >= 0, "Unable to write to /dev/rustmodule: %s", strerror(errno));
    rd = read(fd, buff, 8);
    ATF_REQUIRE_MSG(rd == 8, "Unable to read from /dev/rustmodule: %s", strerror(errno));

    close(fd);

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));
}

/*
 * Driver Jail test
 *
 * Tests the drivers core functions (open, close, write, read)
 * in a jail enviroment
 */
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
    ATF_REQUIRE_MSG(kld_id >= 0, "kldload(2) failed: %s", strerror(errno));

    child = fork();
    ATF_REQUIRE_MSG(child >= 0, "fork failed: %s", strerror(errno));
    if (child == 0) { 
        memset(&j, 0, sizeof(j));
        j.version = JAIL_API_VERSION;
        j.path = path;
        j.hostname = hostname;
        j.jailname = jailname;

        jail_id = jail(&j);
        ATF_REQUIRE_MSG(jail_id >= 0, "jail() failed: %s", strerror(errno));

        fd = open(MODULE_PATH, O_RDWR);
        if (fd < 0) _exit(1);

        ssize_t wrt, rd;
        char buff[8] = {0};

        wrt = write(fd, "Hello :D", 8);
        if (wrt < 0) _exit(1);
        rd = read(fd, buff, 8);
        if (rd != 8) _exit(1);

        close(fd);
        _exit(0);
    }

    int status;
    ATF_REQUIRE_MSG(waitpid(child, &status, 0) == child, "waitpid failed: %s", strerror(errno));
    ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0, 
            "child failed with status %d", WEXITSTATUS(status));

    ATF_REQUIRE_MSG(kldunload(kld_id) == 0, "kldunload(2) failed: %s", strerror(errno));

    // Successful, ignore cleanup
    child = -1;
    fd = -1;
}
ATF_TC_CLEANUP(driver_jail, tc)
{
    if (fd >= 0) close(fd);

    if (child > 0) { // jail will close if child is killed
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
    }

    int loaded = kldfind(DRIVER_NAME);
    if (loaded >= 0) (void)kldunload(loaded);
}

ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, driver_load_unload);
    ATF_TP_ADD_TC(tp, driver_open_close);
    ATF_TP_ADD_TC(tp, driver_read_write);
    ATF_TP_ADD_TC(tp, driver_jail);
    return atf_no_error();
}
