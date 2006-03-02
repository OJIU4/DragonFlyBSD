/*	$NetBSD: vswprintf.c,v 1.1 2005/05/14 23:51:02 christos Exp $	*/
/*	$DragonFly: src/lib/libc/stdio/vswprintf.c,v 1.1.2.1 2006/03/02 18:09:09 joerg Exp $ */

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "local.h"
#include "priv_stdio.h"

int
vswprintf(wchar_t * __restrict s, size_t n, const wchar_t * __restrict fmt,
    va_list ap)
{
	static const mbstate_t initial;
	mbstate_t mbs;
	FILE f;
	char *mbp;
	int ret, sverrno;
	size_t nwc;

	if (n == 0) {
		errno = EINVAL;
		return (-1);
	}

	f.pub._fileno = -1;
	f.pub._flags = __SWR | __SSTR | __SALC;
	f._bf._base = f.pub._p = (unsigned char *)malloc(128);
	if (f._bf._base == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	f._bf._size = f.pub._w = 127;		/* Leave room for the NUL */
	f._up = NULL;
	f.fl_mutex = PTHREAD_MUTEX_INITIALIZER;
	f.fl_owner = NULL;
	f.fl_count = 0;
	memset(WCIO_GET(&f), 0, sizeof(struct wchar_io_data));
	ret = __vfwprintf_unlocked(&f, fmt, ap);
	if (ret < 0) {
		sverrno = errno;
		free(f._bf._base);
		errno = sverrno;
		return (-1);
	}
	*f.pub._p = '\0';
	mbp = (char *)f._bf._base;
	/*
	 * XXX Undo the conversion from wide characters to multibyte that
	 * fputwc() did in __vfwprintf().
	 */
	mbs = initial;
	nwc = mbsrtowcs(s, (const char **)&mbp, n, &mbs);
	free(f._bf._base);
	if (nwc == (size_t)-1) {
		errno = EILSEQ;
		return (-1);
	}
	if (nwc == n) {
		s[n - 1] = L'\0';
		errno = EOVERFLOW;
		return (-1);
	}

	return (ret);
}
