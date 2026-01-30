PROG=	xruns
SRCS=	xruns.c
LDADD=	-lmixer -lnv
MAN=

.include <bsd.prog.mk>
