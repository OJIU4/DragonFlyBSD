/*	$KAME: advcap.h,v 1.3 2001/02/01 09:12:08 jinmei Exp $	*/

/*
 * Copyright (C) 1994,1995 by Andrey A. Chernov, Moscow, Russia.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/rtadvd/advcap.h,v 1.1.2.2 2001/07/03 11:02:14 ume Exp $
 * $DragonFly: src/usr.sbin/rtadvd/advcap.h,v 1.3 2003/11/03 19:31:42 eirikn Exp $
 */

/* Based on Id: termcap.h,v 1.8 1996/09/10 12:42:10 peter Exp */

#ifndef _ADVCAP_H_
#define _ADVCAP_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

extern int agetent(char *, const char *);
extern int agetflag(const char *);
extern long long agetnum(const char *);
extern char *agetstr(const char *, char **);

__END_DECLS

#endif /* _ADVCAP_H_ */
