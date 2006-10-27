/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vfs_vnops.c	8.2 (Berkeley) 1/21/94
 * $FreeBSD: src/sys/kern/vfs_vnops.c,v 1.87.2.13 2002/12/29 18:19:53 dillon Exp $
 * $DragonFly: src/sys/kern/vfs_vnops.c,v 1.49 2006/10/27 04:56:31 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/nlookup.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/filio.h>
#include <sys/ttycom.h>
#include <sys/conf.h>
#include <sys/syslog.h>

static int vn_closefile (struct file *fp);
static int vn_ioctl (struct file *fp, u_long com, caddr_t data,
		struct ucred *cred);
static int vn_read (struct file *fp, struct uio *uio, 
		struct ucred *cred, int flags);
static int svn_read (struct file *fp, struct uio *uio, 
		struct ucred *cred, int flags);
static int vn_poll (struct file *fp, int events, struct ucred *cred);
static int vn_kqfilter (struct file *fp, struct knote *kn);
static int vn_statfile (struct file *fp, struct stat *sb, struct ucred *cred);
static int vn_write (struct file *fp, struct uio *uio, 
		struct ucred *cred, int flags);
static int svn_write (struct file *fp, struct uio *uio, 
		struct ucred *cred, int flags);

struct fileops vnode_fileops = {
	.fo_read = vn_read,
	.fo_write = vn_write,
	.fo_ioctl = vn_ioctl,
	.fo_poll = vn_poll,
	.fo_kqfilter = vn_kqfilter,
	.fo_stat = vn_statfile,
	.fo_close = vn_closefile,
	.fo_shutdown = nofo_shutdown
};

struct fileops specvnode_fileops = {
	.fo_read = svn_read,
	.fo_write = svn_write,
	.fo_ioctl = vn_ioctl,
	.fo_poll = vn_poll,
	.fo_kqfilter = vn_kqfilter,
	.fo_stat = vn_statfile,
	.fo_close = vn_closefile,
	.fo_shutdown = nofo_shutdown
};

/*
 * Shortcut the device read/write.  This avoids a lot of vnode junk.
 * Basically the specfs vnops for read and write take the locked vnode,
 * unlock it (because we can't hold the vnode locked while reading or writing
 * a device which may block indefinitely), issues the device operation, then
 * relock the vnode before returning, plus other junk.  This bypasses all
 * of that and just does the device operation.
 */
void
vn_setspecops(struct file *fp)
{
	if (vfs_fastdev && fp->f_ops == &vnode_fileops) {
		fp->f_ops = &specvnode_fileops;
	}
}

/*
 * Common code for vnode open operations.  Check permissions, and call
 * the VOP_NOPEN or VOP_NCREATE routine.
 *
 * The caller is responsible for setting up nd with nlookup_init() and
 * for cleaning it up with nlookup_done(), whether we return an error
 * or not.
 *
 * On success nd->nl_open_vp will hold a referenced and, if requested,
 * locked vnode.  A locked vnode is requested via NLC_LOCKVP.  If fp
 * is non-NULL the vnode will be installed in the file pointer.
 *
 * NOTE: The vnode is referenced just once on return whether or not it
 * is also installed in the file pointer.
 */
int
vn_open(struct nlookupdata *nd, struct file *fp, int fmode, int cmode)
{
	struct vnode *vp;
	struct ucred *cred = nd->nl_cred;
	struct vattr vat;
	struct vattr *vap = &vat;
	int mode, error;

	/*
	 * Lookup the path and create or obtain the vnode.  After a
	 * successful lookup a locked nd->nl_nch will be returned.
	 *
	 * The result of this section should be a locked vnode.
	 *
	 * XXX with only a little work we should be able to avoid locking
	 * the vnode if FWRITE, O_CREAT, and O_TRUNC are *not* set.
	 */
	if (fmode & O_CREAT) {
		/*
		 * CONDITIONAL CREATE FILE CASE
		 *
		 * Setting NLC_CREATE causes a negative hit to store
		 * the negative hit ncp and not return an error.  Then
		 * nc_error or nc_vp may be checked to see if the ncp 
		 * represents a negative hit.  NLC_CREATE also requires
		 * write permission on the governing directory or EPERM
		 * is returned.
		 */
		if ((fmode & O_EXCL) == 0 && (fmode & O_NOFOLLOW) == 0)
			nd->nl_flags |= NLC_FOLLOW;
		nd->nl_flags |= NLC_CREATE;
		bwillwrite();
		error = nlookup(nd);
	} else {
		/*
		 * NORMAL OPEN FILE CASE
		 */
		error = nlookup(nd);
	}

	if (error)
		return (error);

	/*
	 * split case to allow us to re-resolve and retry the ncp in case
	 * we get ESTALE.
	 */
again:
	if (fmode & O_CREAT) {
		if (nd->nl_nch.ncp->nc_vp == NULL) {
			if ((error = ncp_writechk(&nd->nl_nch)) != 0)
				return (error);
			VATTR_NULL(vap);
			vap->va_type = VREG;
			vap->va_mode = cmode;
			if (fmode & O_EXCL)
				vap->va_vaflags |= VA_EXCLUSIVE;
			error = VOP_NCREATE(&nd->nl_nch, &vp, nd->nl_cred, vap);
			if (error)
				return (error);
			fmode &= ~O_TRUNC;
			/* locked vnode is returned */
		} else {
			if (fmode & O_EXCL) {
				error = EEXIST;
			} else {
				error = cache_vget(&nd->nl_nch, cred, 
						    LK_EXCLUSIVE, &vp);
			}
			if (error)
				return (error);
			fmode &= ~O_CREAT;
		}
	} else {
		error = cache_vget(&nd->nl_nch, cred, LK_EXCLUSIVE, &vp);
		if (error)
			return (error);
	}

	/*
	 * We have a locked vnode and ncp now.  Note that the ncp will
	 * be cleaned up by the caller if nd->nl_nch is left intact.
	 */
	if (vp->v_type == VLNK) {
		error = EMLINK;
		goto bad;
	}
	if (vp->v_type == VSOCK) {
		error = EOPNOTSUPP;
		goto bad;
	}
	if ((fmode & O_CREAT) == 0) {
		mode = 0;
		if (fmode & (FWRITE | O_TRUNC)) {
			if (vp->v_type == VDIR) {
				error = EISDIR;
				goto bad;
			}
			error = vn_writechk(vp, &nd->nl_nch);
			if (error) {
				/*
				 * Special stale handling, re-resolve the
				 * vnode.
				 */
				if (error == ESTALE) {
					vput(vp);
					vp = NULL;
					cache_setunresolved(&nd->nl_nch);
					error = cache_resolve(&nd->nl_nch, cred);
					if (error == 0)
						goto again;
				}
				goto bad;
			}
			mode |= VWRITE;
		}
		if (fmode & FREAD)
			mode |= VREAD;
		if (mode) {
		        error = VOP_ACCESS(vp, mode, cred);
			if (error) {
				/*
				 * Special stale handling, re-resolve the
				 * vnode.
				 */
				if (error == ESTALE) {
					vput(vp);
					vp = NULL;
					cache_setunresolved(&nd->nl_nch);
					error = cache_resolve(&nd->nl_nch, cred);
					if (error == 0)
						goto again;
				}
				goto bad;
			}
		}
	}
	if (fmode & O_TRUNC) {
		vn_unlock(vp);				/* XXX */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);	/* XXX */
		VATTR_NULL(vap);
		vap->va_size = 0;
		error = VOP_SETATTR(vp, vap, cred);
		if (error)
			goto bad;
	}

	/*
	 * Setup the fp so VOP_OPEN can override it.  No descriptor has been
	 * associated with the fp yet so we own it clean.  
	 *
	 * f_nchandle inherits nl_nch.  This used to be necessary only for
	 * directories but now we do it unconditionally so f*() ops
	 * such as fchmod() can access the actual namespace that was
	 * used to open the file.
	 */
	if (fp) {
		fp->f_nchandle = nd->nl_nch;
		cache_zero(&nd->nl_nch);
		cache_unlock(&fp->f_nchandle);
	}

	/*
	 * Get rid of nl_nch.  vn_open does not return it (it returns the
	 * vnode or the file pointer).  Note: we can't leave nl_nch locked
	 * through the VOP_OPEN anyway since the VOP_OPEN may block, e.g.
	 * on /dev/ttyd0
	 */
	if (nd->nl_nch.ncp)
		cache_put(&nd->nl_nch);

	error = VOP_OPEN(vp, fmode, cred, fp);
	if (error) {
		/*
		 * setting f_ops to &badfileops will prevent the descriptor
		 * code from trying to close and release the vnode, since
		 * the open failed we do not want to call close.
		 */
		if (fp) {
			fp->f_data = NULL;
			fp->f_ops = &badfileops;
		}
		goto bad;
	}

#if 0
	/*
	 * Assert that VREG files have been setup for vmio.
	 */
	KASSERT(vp->v_type != VREG || vp->v_object != NULL,
		("vn_open: regular file was not VMIO enabled!"));
#endif

	/*
	 * Return the vnode.  XXX needs some cleaning up.  The vnode is
	 * only returned in the fp == NULL case.
	 */
	if (fp == NULL) {
		nd->nl_open_vp = vp;
		nd->nl_vp_fmode = fmode;
		if ((nd->nl_flags & NLC_LOCKVP) == 0)
			vn_unlock(vp);
	} else {
		vput(vp);
	}
	return (0);
bad:
	if (vp)
		vput(vp);
	return (error);
}

/*
 * Check for write permissions on the specified vnode.  nch may be NULL.
 */
int
vn_writechk(struct vnode *vp, struct nchandle *nch)
{
	/*
	 * If there's shared text associated with
	 * the vnode, try to free it up once.  If
	 * we fail, we can't allow writing.
	 */
	if (vp->v_flag & VTEXT)
		return (ETXTBSY);

	/*
	 * If the vnode represents a regular file, check the mount
	 * point via the nch.  This may be a different mount point
	 * then the one embedded in the vnode (e.g. nullfs).
	 *
	 * We can still write to non-regular files (e.g. devices)
	 * via read-only mounts.
	 */
	if (nch && nch->ncp && vp->v_type == VREG)
		return (ncp_writechk(nch));
	return (0);
}

/*
 * Check whether the underlying mount is read-only.  The mount point 
 * referenced by the namecache may be different from the mount point
 * used by the underlying vnode in the case of NULLFS, so a separate
 * check is needed.
 */
int
ncp_writechk(struct nchandle *nch)
{
	if (nch->mount && (nch->mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	return(0);
}

/*
 * Vnode close call
 */
int
vn_close(struct vnode *vp, int flags)
{
	int error;

	if ((error = vn_lock(vp, LK_EXCLUSIVE | LK_RETRY)) == 0) {
		error = VOP_CLOSE(vp, flags);
		vn_unlock(vp);
	}
	vrele(vp);
	return (error);
}

static __inline
int
sequential_heuristic(struct uio *uio, struct file *fp)
{
	/*
	 * Sequential heuristic - detect sequential operation
	 */
	if ((uio->uio_offset == 0 && fp->f_seqcount > 0) ||
	    uio->uio_offset == fp->f_nextoff) {
		int tmpseq = fp->f_seqcount;
		/*
		 * XXX we assume that the filesystem block size is
		 * the default.  Not true, but still gives us a pretty
		 * good indicator of how sequential the read operations
		 * are.
		 */
		tmpseq += (uio->uio_resid + BKVASIZE - 1) / BKVASIZE;
		if (tmpseq > IO_SEQMAX)
			tmpseq = IO_SEQMAX;
		fp->f_seqcount = tmpseq;
		return(fp->f_seqcount << IO_SEQSHIFT);
	}

	/*
	 * Not sequential, quick draw-down of seqcount
	 */
	if (fp->f_seqcount > 1)
		fp->f_seqcount = 1;
	else
		fp->f_seqcount = 0;
	return(0);
}

/*
 * Package up an I/O request on a vnode into a uio and do it.
 */
int
vn_rdwr(enum uio_rw rw, struct vnode *vp, caddr_t base, int len,
	off_t offset, enum uio_seg segflg, int ioflg, 
	struct ucred *cred, int *aresid)
{
	struct uio auio;
	struct iovec aiov;
	struct ccms_lock ccms_lock;
	int error;

	if ((ioflg & IO_NODELOCKED) == 0)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = base;
	aiov.iov_len = len;
	auio.uio_resid = len;
	auio.uio_offset = offset;
	auio.uio_segflg = segflg;
	auio.uio_rw = rw;
	auio.uio_td = curthread;
	ccms_lock_get_uio(&vp->v_ccms, &ccms_lock, &auio);
	if (rw == UIO_READ) {
		error = VOP_READ(vp, &auio, ioflg, cred);
	} else {
		error = VOP_WRITE(vp, &auio, ioflg, cred);
	}
	ccms_lock_put(&vp->v_ccms, &ccms_lock);
	if (aresid)
		*aresid = auio.uio_resid;
	else
		if (auio.uio_resid && error == 0)
			error = EIO;
	if ((ioflg & IO_NODELOCKED) == 0)
		vn_unlock(vp);
	return (error);
}

/*
 * Package up an I/O request on a vnode into a uio and do it.  The I/O
 * request is split up into smaller chunks and we try to avoid saturating
 * the buffer cache while potentially holding a vnode locked, so we 
 * check bwillwrite() before calling vn_rdwr().  We also call uio_yield()
 * to give other processes a chance to lock the vnode (either other processes
 * core'ing the same binary, or unrelated processes scanning the directory).
 */
int
vn_rdwr_inchunks(enum uio_rw rw, struct vnode *vp, caddr_t base, int len,
		 off_t offset, enum uio_seg segflg, int ioflg,
		 struct ucred *cred, int *aresid)
{
	int error = 0;

	do {
		int chunk;

		/*
		 * Force `offset' to a multiple of MAXBSIZE except possibly
		 * for the first chunk, so that filesystems only need to
		 * write full blocks except possibly for the first and last
		 * chunks.
		 */
		chunk = MAXBSIZE - (uoff_t)offset % MAXBSIZE;

		if (chunk > len)
			chunk = len;
		if (rw != UIO_READ && vp->v_type == VREG)
			bwillwrite();
		error = vn_rdwr(rw, vp, base, chunk, offset, segflg,
			    ioflg, cred, aresid);
		len -= chunk;	/* aresid calc already includes length */
		if (error)
			break;
		offset += chunk;
		base += chunk;
		uio_yield();
	} while (len);
	if (aresid)
		*aresid += len;
	return (error);
}

/*
 * MPALMOSTSAFE - acquires mplock
 */
static int
vn_read(struct file *fp, struct uio *uio, struct ucred *cred, int flags)
{
	struct ccms_lock ccms_lock;
	struct vnode *vp;
	int error, ioflag;

	get_mplock();
	KASSERT(uio->uio_td == curthread,
		("uio_td %p is not td %p", uio->uio_td, curthread));
	vp = (struct vnode *)fp->f_data;

	ioflag = 0;
	if (flags & O_FBLOCKING) {
		/* ioflag &= ~IO_NDELAY; */
	} else if (flags & O_FNONBLOCKING) {
		ioflag |= IO_NDELAY;
	} else if (fp->f_flag & FNONBLOCK) {
		ioflag |= IO_NDELAY;
	}
	if (flags & O_FBUFFERED) {
		/* ioflag &= ~IO_DIRECT; */
	} else if (flags & O_FUNBUFFERED) {
		ioflag |= IO_DIRECT;
	} else if (fp->f_flag & O_DIRECT) {
		ioflag |= IO_DIRECT;
	}
	vn_lock(vp, LK_SHARED | LK_RETRY);
	if ((flags & O_FOFFSET) == 0)
		uio->uio_offset = fp->f_offset;
	ioflag |= sequential_heuristic(uio, fp);

	ccms_lock_get_uio(&vp->v_ccms, &ccms_lock, uio);
	error = VOP_READ(vp, uio, ioflag, cred);
	ccms_lock_put(&vp->v_ccms, &ccms_lock);
	if ((flags & O_FOFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	vn_unlock(vp);
	rel_mplock();
	return (error);
}

/*
 * Device-optimized file table vnode read routine.
 *
 * This bypasses the VOP table and talks directly to the device.  Most
 * filesystems just route to specfs and can make this optimization.
 *
 * MPALMOSTSAFE - acquires mplock
 */
static int
svn_read(struct file *fp, struct uio *uio, struct ucred *cred, int flags)
{
	struct vnode *vp;
	int ioflag;
	int error;
	cdev_t dev;

	get_mplock();
	KASSERT(uio->uio_td == curthread,
		("uio_td %p is not td %p", uio->uio_td, curthread));

	vp = (struct vnode *)fp->f_data;
	if (vp == NULL || vp->v_type == VBAD) {
		error = EBADF;
		goto done;
	}

	if ((dev = vp->v_rdev) == NULL) {
		error = EBADF;
		goto done;
	}
	reference_dev(dev);

	if (uio->uio_resid == 0) {
		error = 0;
		goto done;
	}
	if ((flags & O_FOFFSET) == 0)
		uio->uio_offset = fp->f_offset;

	ioflag = 0;
	if (flags & O_FBLOCKING) {
		/* ioflag &= ~IO_NDELAY; */
	} else if (flags & O_FNONBLOCKING) {
		ioflag |= IO_NDELAY;
	} else if (fp->f_flag & FNONBLOCK) {
		ioflag |= IO_NDELAY;
	}
	if (flags & O_FBUFFERED) {
		/* ioflag &= ~IO_DIRECT; */
	} else if (flags & O_FUNBUFFERED) {
		ioflag |= IO_DIRECT;
	} else if (fp->f_flag & O_DIRECT) {
		ioflag |= IO_DIRECT;
	}
	ioflag |= sequential_heuristic(uio, fp);

	error = dev_dread(dev, uio, ioflag);

	release_dev(dev);
	if ((flags & O_FOFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
done:
	rel_mplock();
	return (error);
}

/*
 * MPALMOSTSAFE - acquires mplock
 */
static int
vn_write(struct file *fp, struct uio *uio, struct ucred *cred, int flags)
{
	struct ccms_lock ccms_lock;
	struct vnode *vp;
	int error, ioflag;

	get_mplock();
	KASSERT(uio->uio_td == curthread,
		("uio_procp %p is not p %p", uio->uio_td, curthread));
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type == VREG)
		bwillwrite();
	vp = (struct vnode *)fp->f_data;	/* XXX needed? */

	ioflag = IO_UNIT;
	if (vp->v_type == VREG &&
	   ((fp->f_flag & O_APPEND) || (flags & O_FAPPEND))) {
		ioflag |= IO_APPEND;
	}

	if (flags & O_FBLOCKING) {
		/* ioflag &= ~IO_NDELAY; */
	} else if (flags & O_FNONBLOCKING) {
		ioflag |= IO_NDELAY;
	} else if (fp->f_flag & FNONBLOCK) {
		ioflag |= IO_NDELAY;
	}
	if (flags & O_FBUFFERED) {
		/* ioflag &= ~IO_DIRECT; */
	} else if (flags & O_FUNBUFFERED) {
		ioflag |= IO_DIRECT;
	} else if (fp->f_flag & O_DIRECT) {
		ioflag |= IO_DIRECT;
	}
	if (flags & O_FASYNCWRITE) {
		/* ioflag &= ~IO_SYNC; */
	} else if (flags & O_FSYNCWRITE) {
		ioflag |= IO_SYNC;
	} else if (fp->f_flag & O_FSYNC) {
		ioflag |= IO_SYNC;
	}

	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_SYNCHRONOUS))
		ioflag |= IO_SYNC;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if ((flags & O_FOFFSET) == 0)
		uio->uio_offset = fp->f_offset;
	ioflag |= sequential_heuristic(uio, fp);
	ccms_lock_get_uio(&vp->v_ccms, &ccms_lock, uio);
	error = VOP_WRITE(vp, uio, ioflag, cred);
	ccms_lock_put(&vp->v_ccms, &ccms_lock);
	if ((flags & O_FOFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	vn_unlock(vp);
	rel_mplock();
	return (error);
}

/*
 * Device-optimized file table vnode write routine.
 *
 * This bypasses the VOP table and talks directly to the device.  Most
 * filesystems just route to specfs and can make this optimization.
 *
 * MPALMOSTSAFE - acquires mplock
 */
static int
svn_write(struct file *fp, struct uio *uio, struct ucred *cred, int flags)
{
	struct vnode *vp;
	int ioflag;
	int error;
	cdev_t dev;

	get_mplock();
	KASSERT(uio->uio_td == curthread,
		("uio_procp %p is not p %p", uio->uio_td, curthread));

	vp = (struct vnode *)fp->f_data;
	if (vp == NULL || vp->v_type == VBAD) {
		error = EBADF;
		goto done;
	}
	if (vp->v_type == VREG)
		bwillwrite();
	vp = (struct vnode *)fp->f_data;	/* XXX needed? */

	if ((dev = vp->v_rdev) == NULL) {
		error = EBADF;
		goto done;
	}
	reference_dev(dev);

	if ((flags & O_FOFFSET) == 0)
		uio->uio_offset = fp->f_offset;

	ioflag = IO_UNIT;
	if (vp->v_type == VREG && 
	   ((fp->f_flag & O_APPEND) || (flags & O_FAPPEND))) {
		ioflag |= IO_APPEND;
	}

	if (flags & O_FBLOCKING) {
		/* ioflag &= ~IO_NDELAY; */
	} else if (flags & O_FNONBLOCKING) {
		ioflag |= IO_NDELAY;
	} else if (fp->f_flag & FNONBLOCK) {
		ioflag |= IO_NDELAY;
	}
	if (flags & O_FBUFFERED) {
		/* ioflag &= ~IO_DIRECT; */
	} else if (flags & O_FUNBUFFERED) {
		ioflag |= IO_DIRECT;
	} else if (fp->f_flag & O_DIRECT) {
		ioflag |= IO_DIRECT;
	}
	if (flags & O_FASYNCWRITE) {
		/* ioflag &= ~IO_SYNC; */
	} else if (flags & O_FSYNCWRITE) {
		ioflag |= IO_SYNC;
	} else if (fp->f_flag & O_FSYNC) {
		ioflag |= IO_SYNC;
	}

	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_SYNCHRONOUS))
		ioflag |= IO_SYNC;
	ioflag |= sequential_heuristic(uio, fp);

	error = dev_dwrite(dev, uio, ioflag);

	release_dev(dev);
	if ((flags & O_FOFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
done:
	rel_mplock();
	return (error);
}

/*
 * MPALMOSTSAFE - acquires mplock
 */
static int
vn_statfile(struct file *fp, struct stat *sb, struct ucred *cred)
{
	struct vnode *vp;
	int error;

	get_mplock();
	vp = (struct vnode *)fp->f_data;
	error = vn_stat(vp, sb, cred);
	rel_mplock();
	return (error);
}

int
vn_stat(struct vnode *vp, struct stat *sb, struct ucred *cred)
{
	struct vattr vattr;
	struct vattr *vap;
	int error;
	u_short mode;
	cdev_t dev;

	vap = &vattr;
	error = VOP_GETATTR(vp, vap);
	if (error)
		return (error);

	/*
	 * Zero the spare stat fields
	 */
	sb->st_lspare = 0;
	sb->st_qspare = 0;

	/*
	 * Copy from vattr table
	 */
	if (vap->va_fsid != VNOVAL)
		sb->st_dev = vap->va_fsid;
	else
		sb->st_dev = vp->v_mount->mnt_stat.f_fsid.val[0];
	sb->st_ino = vap->va_fileid;
	mode = vap->va_mode;
	switch (vap->va_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		/* This is a cosmetic change, symlinks do not have a mode. */
		if (vp->v_mount->mnt_flag & MNT_NOSYMFOLLOW)
			sb->st_mode &= ~ACCESSPERMS;	/* 0000 */
		else
			sb->st_mode |= ACCESSPERMS;	/* 0777 */
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		return (EBADF);
	};
	sb->st_mode = mode;
	sb->st_nlink = vap->va_nlink;
	sb->st_uid = vap->va_uid;
	sb->st_gid = vap->va_gid;
	sb->st_rdev = vap->va_rdev;
	sb->st_size = vap->va_size;
	sb->st_atimespec = vap->va_atime;
	sb->st_mtimespec = vap->va_mtime;
	sb->st_ctimespec = vap->va_ctime;

	/*
	 * A VCHR and VBLK device may track the last access and last modified
	 * time independantly of the filesystem.  This is particularly true
	 * because device read and write calls may bypass the filesystem.
	 */
	if (vp->v_type == VCHR || vp->v_type == VBLK) {
		if ((dev = vp->v_rdev) != NULL) {
			if (dev->si_lastread) {
				sb->st_atimespec.tv_sec = dev->si_lastread;
				sb->st_atimespec.tv_nsec = 0;
			}
			if (dev->si_lastwrite) {
				sb->st_atimespec.tv_sec = dev->si_lastwrite;
				sb->st_atimespec.tv_nsec = 0;
			}
		}
	}

        /*
	 * According to www.opengroup.org, the meaning of st_blksize is 
	 *   "a filesystem-specific preferred I/O block size for this 
	 *    object.  In some filesystem types, this may vary from file
	 *    to file"
	 * Default to PAGE_SIZE after much discussion.
	 */

	if (vap->va_type == VREG) {
		sb->st_blksize = vap->va_blocksize;
	} else if (vn_isdisk(vp, NULL)) {
		/*
		 * XXX this is broken.  If the device is not yet open (aka
		 * stat() call, aka v_rdev == NULL), how are we supposed
		 * to get a valid block size out of it?
		 */
		cdev_t dev;

		if ((dev = vp->v_rdev) == NULL)
			dev = udev2dev(vp->v_udev, vp->v_type == VBLK);
		sb->st_blksize = dev->si_bsize_best;
		if (sb->st_blksize < dev->si_bsize_phys)
			sb->st_blksize = dev->si_bsize_phys;
		if (sb->st_blksize < BLKDEV_IOSIZE)
			sb->st_blksize = BLKDEV_IOSIZE;
	} else {
		sb->st_blksize = PAGE_SIZE;
	}
	
	sb->st_flags = vap->va_flags;
	if (suser_cred(cred, 0))
		sb->st_gen = 0;
	else
		sb->st_gen = vap->va_gen;

#if (S_BLKSIZE == 512)
	/* Optimize this case */
	sb->st_blocks = vap->va_bytes >> 9;
#else
	sb->st_blocks = vap->va_bytes / S_BLKSIZE;
#endif
	sb->st_fsmid = vap->va_fsmid;
	return (0);
}

/*
 * MPALMOSTSAFE - acquires mplock
 */
static int
vn_ioctl(struct file *fp, u_long com, caddr_t data, struct ucred *ucred)
{
	struct vnode *vp = ((struct vnode *)fp->f_data);
	struct vnode *ovp;
	struct vattr vattr;
	int error;

	get_mplock();

	switch (vp->v_type) {
	case VREG:
	case VDIR:
		if (com == FIONREAD) {
			if ((error = VOP_GETATTR(vp, &vattr)) != 0)
				break;
			*(int *)data = vattr.va_size - fp->f_offset;
			error = 0;
			break;
		}
		if (com == FIOASYNC) {				/* XXX */
			error = 0;				/* XXX */
			break;
		}
		/* fall into ... */
	default:
#if 0
		return (ENOTTY);
#endif
	case VFIFO:
	case VCHR:
	case VBLK:
		if (com == FIODTYPE) {
			if (vp->v_type != VCHR && vp->v_type != VBLK) {
				error = ENOTTY;
				break;
			}
			*(int *)data = dev_dflags(vp->v_rdev) & D_TYPEMASK;
			error = 0;
			break;
		}
		error = VOP_IOCTL(vp, com, data, fp->f_flag, ucred);
		if (error == 0 && com == TIOCSCTTY) {
			struct proc *p = curthread->td_proc;
			struct session *sess;

			if (p == NULL) {
				error = ENOTTY;
				break;
			}

			sess = p->p_session;
			/* Do nothing if reassigning same control tty */
			if (sess->s_ttyvp == vp) {
				error = 0;
				break;
			}

			/* Get rid of reference to old control tty */
			ovp = sess->s_ttyvp;
			vref(vp);
			sess->s_ttyvp = vp;
			if (ovp)
				vrele(ovp);
		}
		break;
	}
	rel_mplock();
	return (error);
}

/*
 * MPALMOSTSAFE - acquires mplock
 */
static int
vn_poll(struct file *fp, int events, struct ucred *cred)
{
	int error;

	get_mplock();
	error = VOP_POLL(((struct vnode *)fp->f_data), events, cred);
	rel_mplock();
	return (error);
}

/*
 * Check that the vnode is still valid, and if so
 * acquire requested lock.
 */
int
#ifndef	DEBUG_LOCKS
vn_lock(struct vnode *vp, int flags)
#else
debug_vn_lock(struct vnode *vp, int flags, const char *filename, int line)
#endif
{
	int error;
	
	do {
#ifdef	DEBUG_LOCKS
		vp->filename = filename;
		vp->line = line;
		error = debuglockmgr(&vp->v_lock, flags,
				     "vn_lock", filename, line);
#else
		error = lockmgr(&vp->v_lock, flags);
#endif
		if (error == 0)
			break;
	} while (flags & LK_RETRY);

	/*
	 * Because we (had better!) have a ref on the vnode, once it
	 * goes to VRECLAIMED state it will not be recycled until all
	 * refs go away.  So we can just check the flag.
	 */
	if (error == 0 && (vp->v_flag & VRECLAIMED)) {
		lockmgr(&vp->v_lock, LK_RELEASE);
		error = ENOENT;
	}
	return (error);
}

void
vn_unlock(struct vnode *vp)
{
	lockmgr(&vp->v_lock, LK_RELEASE);
}

int
vn_islocked(struct vnode *vp)
{
	return (lockstatus(&vp->v_lock, curthread));
}

/*
 * MPALMOSTSAFE - acquires mplock
 */
static int
vn_closefile(struct file *fp)
{
	int error;

	get_mplock();
	fp->f_ops = &badfileops;
	error = vn_close(((struct vnode *)fp->f_data), fp->f_flag);
	rel_mplock();
	return(error);
}

/*
 * MPALMOSTSAFE - acquires mplock
 */
static int
vn_kqfilter(struct file *fp, struct knote *kn)
{
	int error;

	get_mplock();
	error = VOP_KQFILTER(((struct vnode *)fp->f_data), kn);
	rel_mplock();
	return (error);
}
