TESTSDIR=	${TESTSBASE}/rustdrv 

# Edit this path to select the driver
DRIVER_PATH ?= PATH/TO/YOUR/DRIVER/freebsd-kernel-module-rust/hello.ko

MODULE_PATH ?= /dev/rustmodule
DRIVER_NAME ?= hello.ko

CFLAGS+= \
    -DDRIVER_PATH=\"${DRIVER_PATH}\" \
    -DMODULE_PATH=\"${MODULE_PATH}\" \
    -DDRIVER_NAME=\"${DRIVER_NAME}\"

ATF_TESTS_C= core_test \
	error_test \
	perf_test

LIBADD+= pthread

.include <bsd.own.mk>
.include <bsd.test.mk>

KYUAFILE=	yes

.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 180000
CWARNFLAGS.printf_test.c+=	-Wno-format-truncation
.endif
