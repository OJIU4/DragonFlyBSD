#
# acpixtract - extract binary ACPI tables from acpidump text output
#

UTILDIR=${.CURDIR}/../../../sys/contrib/dev/acpica/source/tools/acpixtract

.PATH:	${UTILDIR}

PROG=	acpixtract
MAN=	acpixtract.8

SRCS=	\
	acpixtract.c \
	axmain.c \
	axutils.c

SRCS+=	\
	getopt.c \
	osunixxf.c \
	utascii.c \
	utdebug.c \
	utexcep.c \
	utglobal.c \
	uthex.c \
	utmath.c \
	utnonansi.c \
	utxferror.c

CFLAGS+= \
	-DACPI_XTRACT_APP \
	-I${UTILDIR}

.include <bsd.prog.mk>
