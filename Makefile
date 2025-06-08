TESTSDIR=	${TESTSBASE}/rustdrv 

ATF_TESTS_C= core_test \
			 error_test

.include <bsd.own.mk>
.include <bsd.test.mk>

KYUAFILE=	yes

.if ${COMPILER_TYPE} == "clang" && ${COMPILER_VERSION} >= 180000
CWARNFLAGS.printf_test.c+=	-Wno-format-truncation
.endif
