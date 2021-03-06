# $FreeBSD: src/sys/boot/i386/pxeldr/Makefile,v 1.9 2003/09/03 08:12:20 phk Exp $

ORG=		0x7c00

LDR=		pxeldr
BOOT?=		pxeboot
LOADER?=	loader
PROG=		${BOOT}
MAN=		${BOOT}.8
STRIP=
BINDIR?=	/boot

.if defined(BOOT_PXELDR_PROBE_KEYBOARD)
CFLAGS+=	-DPROBE_KEYBOARD
.endif

.if defined(BOOT_PXELDR_ALWAYS_SERIAL)
CFLAGS+=	-DALWAYS_SERIAL
.endif


.if exists(${.OBJDIR}/../loader)
LOADERBIN?=	${.OBJDIR}/../loader/loader.bin
.else
LOADERBIN?=	${.CURDIR}/../loader/loader.bin
.endif

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o

${BOOT}: ${LDR} ${LOADER}
	cat ${LDR} ${LOADER} > ${.TARGET}.tmp
	dd if=${.TARGET}.tmp of=${.TARGET} obs=2k conv=osync
	rm ${.TARGET}.tmp

${LOADER}: ${LOADERBIN} ${BTXLDR} ${BTXKERN} ${BTXCRT}
	${.OBJDIR}/../btxld/btxld.nx -v -f aout -e ${LOADER_ADDRESS} \
	    -o ${.TARGET} -l ${BTXLDR} -b ${BTXKERN} ${LOADERBIN}

${LDR}: ${LDR}.o
	${LD} ${LDFLAGS:N-Wl,*} -N -e start -Ttext ${ORG} -o ${LDR}.out ${LDR}.o
	objcopy -S -O binary ${LDR}.out ${.TARGET}

${LDR}.o: ${LDR}.S
	${CC} ${CFLAGS} ${.IMPSRC} -o ${LDR}.o -c

CLEANFILES+= ${LDR} ${LDR}.out ${LDR}.o ${BOOT}.tmp ${LOADER}

.include <bsd.prog.mk>
