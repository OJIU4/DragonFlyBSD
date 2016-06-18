/*
 * Copyright (c) 2016 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "nvme.h"

int
nvme_getlog_ioctl(nvme_softc_t *sc, nvme_getlog_ioctl_t *ioc)
{
	int error;
	nvme_request_t *req;

	if ((ioc->ret_size & 3) || ioc->ret_size > sizeof(ioc->info))
		return EINVAL;
	if (ioc->ret_size < 4)
		return EINVAL;

	lockmgr(&sc->ioctl_lk, LK_EXCLUSIVE);
	req = nvme_get_admin_request(sc, NVME_OP_GET_LOG_PAGE);
	req->cmd.head.nsid = -1;
	req->cmd.getlog.lid = ioc->lid;
	req->cmd.getlog.numdl = ioc->ret_size / 4 - 1;
	/* leave numdh 0 (NVMe 1.2.1 allows > 65535 dwords) */
	/* leave lpol and lpou 0 for now */
	bzero(req->info, sizeof(*req->info));
	nvme_submit_request(req);
	ioc->status = nvme_wait_request(req, hz);
	kprintf("LID %02x STATUS %02x\n", ioc->lid, ioc->status);
	bcopy(req->info, &ioc->info, ioc->ret_size);
	nvme_put_request(req);
	lockmgr(&sc->ioctl_lk, LK_RELEASE);

	error = 0;

	return error;
}
