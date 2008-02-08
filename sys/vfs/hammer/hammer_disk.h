/*
 * Copyright (c) 2007 The DragonFly Project.  All rights reserved.
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
 * 
 * $DragonFly: src/sys/vfs/hammer/hammer_disk.h,v 1.22 2008/02/08 08:30:59 dillon Exp $
 */

#ifndef VFS_HAMMER_DISK_H_
#define VFS_HAMMER_DISK_H_

#ifndef _SYS_UUID_H_
#include <sys/uuid.h>
#endif

/*
 * The structures below represent the on-disk format for a HAMMER
 * filesystem.  Note that all fields for on-disk structures are naturally
 * aligned.  The host endian format is used - compatibility is possible
 * if the implementation detects reversed endian and adjusts data accordingly.
 *
 * Most of HAMMER revolves around the concept of an object identifier.  An
 * obj_id is a 64 bit quantity which uniquely identifies a filesystem object
 * FOR THE ENTIRE LIFE OF THE FILESYSTEM.  This uniqueness allows backups
 * and mirrors to retain varying amounts of filesystem history by removing
 * any possibility of conflict through identifier reuse.
 *
 * A HAMMER filesystem may spam multiple volumes.
 *
 * A HAMMER filesystem uses a 16K filesystem buffer size.  All filesystem
 * I/O is done in multiples of 16K.  Most buffer-sized headers such as those
 * used by volumes, super-clusters, clusters, and basic filesystem buffers
 * use fixed-sized A-lists which are heavily dependant on HAMMER_BUFSIZE.
 */
#define HAMMER_BUFSIZE	16384
#define HAMMER_BUFMASK	(HAMMER_BUFSIZE - 1)
#define HAMMER_MAXDATA	(256*1024)

#define HAMMER_BUFSIZE64	((u_int64_t)HAMMER_BUFSIZE)
#define HAMMER_BUFMASK64	((u_int64_t)HAMMER_BUFMASK)

#define HAMMER_OFF_ZONE_MASK	0xF000000000000000ULL /* zone portion */
#define HAMMER_OFF_VOL_MASK	0x0FF0000000000000ULL /* volume portion */
#define HAMMER_OFF_SHORT_MASK	0x000FFFFFFFFFFFFFULL /* offset portion */
#define HAMMER_OFF_LONG_MASK	0x0FFFFFFFFFFFFFFFULL /* offset portion */
#define HAMMER_OFF_SHORT_REC_MASK 0x000FFFFFFF000000ULL /* recovery boundary */
#define HAMMER_OFF_LONG_REC_MASK 0x0FFFFFFFFF000000ULL /* recovery boundary */
#define HAMMER_RECOVERY_BND	0x0000000001000000ULL

/*
 * Hammer transction ids are 64 bit unsigned integers and are usually
 * synchronized with the time of day in nanoseconds.
 *
 * Hammer offsets are used for FIFO indexing and embed a cycle counter
 * and volume number in addition to the offset.  Most offsets are required
 * to be 64-byte aligned.
 */
typedef u_int64_t hammer_tid_t;
typedef u_int64_t hammer_off_t;

#define HAMMER_MIN_TID		0ULL			/* unsigned */
#define HAMMER_MAX_TID		0xFFFFFFFFFFFFFFFFULL	/* unsigned */
#define HAMMER_MIN_KEY		-0x8000000000000000LL	/* signed */
#define HAMMER_MAX_KEY		0x7FFFFFFFFFFFFFFFLL	/* signed */
#define HAMMER_MIN_OBJID	HAMMER_MIN_KEY		/* signed */
#define HAMMER_MAX_OBJID	HAMMER_MAX_KEY		/* signed */
#define HAMMER_MIN_RECTYPE	0x0U			/* unsigned */
#define HAMMER_MAX_RECTYPE	0xFFFFU			/* unsigned */
#define HAMMER_MIN_OFFSET	0ULL			/* unsigned */
#define HAMMER_MAX_OFFSET	0xFFFFFFFFFFFFFFFFULL	/* unsigned */

/*
 * hammer_off_t has several different encodings.  Note that not all zones
 * encode a vol_no.
 *
 * zone 0 (z,v,o):	reserved (for sanity)
 * zone 1 (z,v,o):	raw volume relative (offset 0 is the volume header)
 * zone 2 (z,v,o):	raw buffer relative (offset 0 is the first buffer)
 * zone 3-15     :	reserved
 */

#define HAMMER_ZONE_RAW_VOLUME		0x1000000000000000ULL
#define HAMMER_ZONE_RAW_BUFFER		0x2000000000000000ULL

#define HAMMER_VOL_ENCODE(vol_no)			\
	((hammer_off_t)((vol_no) & 255) << 52)
#define HAMMER_VOL_DECODE(ham_off)			\
	(int32_t)(((hammer_off_t)(ham_off) >> 52) & 255)
#define HAMMER_SHORT_OFF_ENCODE(offset)			\
	((hammer_off_t)(offset) & HAMMER_OFF_SHORT_MASK)
#define HAMMER_LONG_OFF_ENCODE(offset)			\
	((hammer_off_t)(offset) & HAMMER_OFF_LONG_MASK)

#define HAMMER_ENCODE_RAW_VOLUME(vol_no, offset)	\
	(HAMMER_ZONE_RAW_VOLUME |			\
	HAMMER_VOL_ENCODE(vol_no) |			\
	HAMMER_SHORT_OFF_ENCODE(offset))

#define HAMMER_ENCODE_RAW_BUFFER(vol_no, offset)	\
	(HAMMER_ZONE_RAW_BUFFER |			\
	HAMMER_VOL_ENCODE(vol_no) |			\
	HAMMER_SHORT_OFF_ENCODE(offset))


/*
 * All on-disk HAMMER structures which make up elements of the FIFO contain
 * a hammer_fifo_head structure.  This structure contains all the information
 * required to validate the fifo element and to scan the fifo in either
 * direction.
 *
 * Nearly all such structures are guaranteed to not cross a 16K filesystem
 * buffer boundary.  The one exception is a record, whos related data may
 * cross a buffer boundary.
 *
 * HAMMER guarantees alignment with a fifo head structure at 16MB intervals
 * (i.e. the base of the buffer will not be in the middle of a data record).
 * This is used to allow the recovery code to re-sync after hitting corrupted
 * data.
 */
#define HAMMER_HEAD_ONDISK_SIZE		32
#define HAMMER_HEAD_RECOVERY_ALIGNMENT  (16 * 1024 * 1024)
#define HAMMER_HEAD_ALIGN		32
#define HAMMER_HEAD_ALIGN_MASK		(HAMMER_HEAD_ALIGN - 1)

struct hammer_fifo_head {
	u_int16_t hdr_signature;
	u_int16_t hdr_type;
	u_int32_t hdr_fwd_link;
	u_int32_t hdr_rev_link;
	u_int32_t hdr_crc;
	hammer_tid_t hdr_seq;
	hammer_tid_t hdr_tid;
};

typedef struct hammer_fifo_head *hammer_fifo_head_t;

/*
 * Fifo header types.
 */
#define HAMMER_HEAD_TYPE_PAD	0xF000U		/* FIFO pad (also FREED) */
#define HAMMER_HEAD_TYPE_VOL	0x7001U		/* Volume (dummy header) */
#define HAMMER_HEAD_TYPE_BTREE	0x7002U		/* B-Tree node */
#define HAMMER_HEAD_TYPE_UNDO	0x7003U		/* random UNDO information */
#define HAMMER_HEAD_TYPE_DELETE	0x7004U		/* record deletion */
#define HAMMER_HEAD_TYPE_RECORD	0x7005U		/* Filesystem record */
#define HAMMER_HEAD_TYPE_TERM	0x7009U		/* Dummy Terminator */

#define HAMMER_HEAD_TYPEF_FREED	0x8000U		/* Indicates object freed */

#define HAMMER_HEAD_SIGNATURE	0xC84EU

/*
 * Misc FIFO structures (except for the B-Tree node and hammer record)
 */
struct hammer_fifo_undo {
	struct hammer_fifo_head	head;
	hammer_off_t		undo_offset;
	/* followed by data */
};

typedef struct hammer_fifo_undo *hammer_fifo_undo_t;

/*
 * Volume header types
 */
#define HAMMER_FSBUF_VOLUME	0xC8414D4DC5523031ULL	/* HAMMER01 */
#define HAMMER_FSBUF_VOLUME_REV	0x313052C54D4D41C8ULL	/* (reverse endian) */

/*
 * The B-Tree structures need hammer_fsbuf_head.
 */
#include "hammer_btree.h"

/*
 * HAMMER Volume header
 *
 * A HAMMER filesystem is built from any number of block devices,  Each block
 * device contains a volume header followed by however many buffers fit
 * into the volume.
 *
 * One of the volumes making up a HAMMER filesystem is the master, the
 * rest are slaves.  It does not have to be volume #0.
 *
 * The volume header takes up an entire 16K filesystem buffer and may
 * represent up to 64KTB (65536 TB) of space.
 *
 * Special field notes:
 *
 *	vol_bot_beg - offset of boot area (mem_beg - bot_beg bytes)
 *	vol_mem_beg - offset of memory log (clu_beg - mem_beg bytes)
 *	vol_buf_beg - offset of the first buffer.
 *
 *	The memory log area allows a kernel to cache new records and data
 *	in memory without allocating space in the actual filesystem to hold
 *	the records and data.  In the event that a filesystem becomes full,
 *	any records remaining in memory can be flushed to the memory log
 *	area.  This allows the kernel to immediately return success.
 */
#define HAMMER_VOL_MAXCLUSTERS		32768	/* 1-layer */
#define HAMMER_VOL_MAXSUPERCLUSTERS	4096	/* 2-layer */
#define HAMMER_VOL_SUPERCLUSTER_GROUP	16
#define HAMMER_VOL_METAELMS_1LYR	HAMMER_ALIST_METAELMS_32K_1LYR
#define HAMMER_VOL_METAELMS_2LYR	HAMMER_ALIST_METAELMS_16K_2LYR

#define HAMMER_BOOT_MINBYTES		(32*1024)
#define HAMMER_BOOT_NOMBYTES		(64LL*1024*1024)
#define HAMMER_BOOT_MAXBYTES		(256LL*1024*1024)

#define HAMMER_MEM_MINBYTES		(256*1024)
#define HAMMER_MEM_NOMBYTES		(1LL*1024*1024*1024)
#define HAMMER_MEM_MAXBYTES		(64LL*1024*1024*1024)

struct hammer_volume_ondisk {
	struct hammer_fifo_head head;
	int64_t vol_bot_beg;	/* byte offset of boot area or 0 */
	int64_t vol_mem_beg;	/* byte offset of memory log or 0 */
	int64_t vol_buf_beg;	/* byte offset of first buffer in volume */
	int64_t vol_buf_end;	/* byte offset of volume EOF (on buf bndry) */
	int64_t vol_locked;	/* reserved clusters are >= this offset */

	uuid_t    vol_fsid;	/* identify filesystem */
	uuid_t    vol_fstype;	/* identify filesystem type */
	char	  vol_name[64];	/* Name of volume */

	u_int64_t vol_signature;/* Signature #2 */
	int32_t vol_no;		/* volume number within filesystem */
	int32_t vol_count;	/* number of volumes making up FS */

	u_int32_t vol_version;	/* version control information */
	u_int32_t vol_reserved01;
	u_int32_t vol_flags;	/* volume flags */
	u_int32_t vol_rootvol;	/* which volume is the root volume? */

	int32_t vol_reserved04;	/* cluster size (same for all volumes) */
	int32_t vol_reserved05;
	u_int32_t vol_reserved06;
	u_int32_t vol_reserved07;

	int32_t vol_blocksize;		/* for statfs only */
	int32_t vol_reserved08;
	int64_t vol_nblocks;		/* total allocatable hammer bufs */

	/*
	 * These fields are initialized and space is reserved in every
	 * volume making up a HAMMER filesytem, but only the master volume
	 * contains valid data.
	 */
	int64_t	vol0_stat_bytes;	/* for statfs only */
	int64_t vol0_stat_inodes;	/* for statfs only */
	int64_t vol0_stat_records;	/* total records in filesystem */
	hammer_off_t vol0_fifo_beg;	/* CIRCULAR FIFO START */
	hammer_off_t vol0_fifo_end;	/* CIRCULAR FIFO END */
	hammer_off_t vol0_btree_root;	/* B-Tree root */
	hammer_tid_t vol0_next_tid;	/* highest synchronized TID */
	hammer_tid_t vol0_next_seq;	/* next SEQ no */
};

typedef struct hammer_volume_ondisk *hammer_volume_ondisk_t;

#define HAMMER_VOLF_VALID		0x0001	/* valid entry */
#define HAMMER_VOLF_OPEN		0x0002	/* volume is open */

/*
 * All HAMMER records have a common 72-byte base and a variable-length
 * extension, plus a possible data reference.  The data portion of the
 * HAMMER record can cross a filesystem buffer boundary (but not the primary
 * record portion).
 *
 * Current only relative in-band data offsets are supported, but the field
 * is large enough for future out-of-band references.
 */
struct hammer_base_record {
	struct hammer_fifo_head head;	/* 16 byte fifo header */
	struct hammer_base_elm base;	/* 40 byte base element */
	hammer_off_t	data_off;	/* in-band or out-of-band */
	int32_t		data_len;	/* size of data in bytes */
	u_int32_t	reserved03;
};

/*
 * Record types are fairly straightforward.  The B-Tree includes the record
 * type in its index sort.
 *
 * In particular please note that it is possible to create a pseudo-
 * filesystem within a HAMMER filesystem by creating a special object
 * type within a directory.  Pseudo-filesystems are used as replication
 * targets and even though they are built within a HAMMER filesystem they
 * get their own obj_id space (and thus can serve as a replication target)
 * and look like a mount point to the system.
 *
 * Inter-cluster records are special-cased in the B-Tree.  These records
 * are referenced from a B-Tree INTERNAL node, NOT A LEAF.  This means
 * that the element in the B-Tree node is actually a boundary element whos
 * base element fields, including rec_type, reflect the boundary, NOT
 * the inter-cluster record type.
 *
 * HAMMER_RECTYPE_CLUSTER - only set in the actual inter-cluster record,
 * not set in the left or right boundary elements around the inter-cluster
 * reference of an internal node in the B-Tree (because doing so would
 * interfere with the boundary tests).
 *
 * NOTE: hammer_ip_delete_range_all() deletes all record types greater
 * then HAMMER_RECTYPE_INODE.
 */
#define HAMMER_RECTYPE_UNKNOWN		0
#define HAMMER_RECTYPE_LOWEST		1	/* lowest record type avail */
#define HAMMER_RECTYPE_INODE		1	/* inode in obj_id space */
#define HAMMER_RECTYPE_PSEUDO_INODE	2	/* pseudo filesysem */
#define HAMMER_RECTYPE_CLUSTER		3	/* inter-cluster reference */
#define HAMMER_RECTYPE_DATA		0x10
#define HAMMER_RECTYPE_DIRENTRY		0x11
#define HAMMER_RECTYPE_DB		0x12
#define HAMMER_RECTYPE_EXT		0x13	/* ext attributes */
#define HAMMER_RECTYPE_FIX		0x14	/* fixed attribute */

#define HAMMER_FIXKEY_SYMLINK		1

#define HAMMER_OBJTYPE_UNKNOWN		0	/* (never exists on-disk) */
#define HAMMER_OBJTYPE_DIRECTORY	1
#define HAMMER_OBJTYPE_REGFILE		2
#define HAMMER_OBJTYPE_DBFILE		3
#define HAMMER_OBJTYPE_FIFO		4
#define HAMMER_OBJTYPE_CDEV		5
#define HAMMER_OBJTYPE_BDEV		6
#define HAMMER_OBJTYPE_SOFTLINK		7
#define HAMMER_OBJTYPE_PSEUDOFS		8	/* pseudo filesystem obj */

/*
 * A HAMMER inode record.
 *
 * This forms the basis for a filesystem object.  obj_id is the inode number,
 * key1 represents the pseudo filesystem id for security partitioning
 * (preventing cross-links and/or restricting a NFS export and specifying the
 * security policy), and key2 represents the data retention policy id.
 *
 * Inode numbers are 64 bit quantities which uniquely identify a filesystem
 * object for the ENTIRE life of the filesystem, even after the object has
 * been deleted.  For all intents and purposes inode numbers are simply 
 * allocated by incrementing a sequence space.
 *
 * There is an important distinction between the data stored in the inode
 * record and the record's data reference.  The record references a
 * hammer_inode_data structure but the filesystem object size and hard link
 * count is stored in the inode record itself.  This allows multiple inodes
 * to share the same hammer_inode_data structure.  This is possible because
 * any modifications will lay out new data.  The HAMMER implementation need
 * not use the data-sharing ability when laying down new records.
 *
 * A HAMMER inode is subject to the same historical storage requirements
 * as any other record.  In particular any change in filesystem or hard link
 * count will lay down a new inode record when the filesystem is synced to
 * disk.  This can lead to a lot of junk records which get cleaned up by
 * the data retention policy.
 *
 * The ino_atime and ino_mtime fields are a special case.  Modifications to
 * these fields do NOT lay down a new record by default, though the values
 * are effectively frozen for snapshots which access historical versions
 * of the inode record due to other operations.  This means that atime will
 * not necessarily be accurate in snapshots, backups, or mirrors.  mtime
 * will be accurate in backups and mirrors since it can be regenerated from
 * the mirroring stream.
 *
 * Because nlinks is historically retained the hardlink count will be
 * accurate when accessing a HAMMER filesystem snapshot.
 */
struct hammer_inode_record {
	struct hammer_base_record base;
	u_int64_t ino_atime;	/* last access time (not historical) */
	u_int64_t ino_mtime;	/* last modified time (not historical) */
	u_int64_t ino_size;	/* filesystem object size */
	u_int64_t ino_nlinks;	/* hard links */
};

/*
 * Data records specify the entire contents of a regular file object,
 * including attributes.  Small amounts of data can theoretically be
 * embedded in the record itself but the use of this ability verses using
 * an out-of-band data reference depends on the implementation.
 */
struct hammer_data_record {
	struct hammer_base_record base;
};

/*
 * A directory entry specifies the HAMMER filesystem object id, a copy of
 * the file type, and file name (either embedded or as out-of-band data).
 * If the file name is short enough to fit into den_name[] (including a
 * terminating nul) then it will be embedded in the record, otherwise it
 * is stored out-of-band.  The base record's data reference always points
 * to the nul-terminated filename regardless.
 *
 * Directory entries are indexed with a 128 bit namekey rather then an
 * offset.  A portion of the namekey is an iterator or randomizer to deal
 * with collisions.
 *
 * NOTE: base.base.obj_type holds the filesystem object type of obj_id,
 *	 e.g. a den_type equivalent.
 *
 * NOTE: den_name / the filename data reference is NOT terminated with \0.
 *
 */
struct hammer_entry_record {
	struct hammer_base_record base;
	u_int64_t obj_id;		/* object being referenced */
	u_int64_t reserved01;
};

/*
 * Hammer rollup record
 */
union hammer_record_ondisk {
	struct hammer_base_record	base;
	struct hammer_inode_record	inode;
	struct hammer_data_record	data;
	struct hammer_entry_record	entry;
};

typedef union hammer_record_ondisk *hammer_record_ondisk_t;

/*
 * HAMMER UNIX Attribute data
 *
 * The data reference in a HAMMER inode record points to this structure.  Any
 * modifications to the contents of this structure will result in a record
 * replacement operation.
 *
 * short_data_off allows a small amount of data to be embedded in the
 * hammer_inode_data structure.  HAMMER typically uses this to represent
 * up to 64 bytes of data, or to hold symlinks.  Remember that allocations
 * are in powers of 2 so 64, 192, 448, or 960 bytes of embedded data is
 * support (64+64, 64+192, 64+448 64+960).
 *
 * parent_obj_id is only valid for directories (which cannot be hard-linked),
 * and specifies the parent directory obj_id.  This field will also be set
 * for non-directory inodes as a recovery aid, but can wind up specifying
 * stale information.  However, since object id's are not reused, the worse
 * that happens is that the recovery code is unable to use it.
 */
struct hammer_inode_data {
	u_int16_t version;	/* inode data version */
	u_int16_t mode;		/* basic unix permissions */
	u_int32_t uflags;	/* chflags */
	u_int32_t rmajor;	/* used by device nodes */
	u_int32_t rminor;	/* used by device nodes */
	u_int64_t ctime;
	u_int64_t parent_obj_id;/* parent directory obj_id */
	uuid_t	uid;
	uuid_t	gid;
	/* XXX device, softlink extension */
};

#define HAMMER_INODE_DATA_VERSION	1

#define HAMMER_OBJID_ROOT		1

/*
 * Rollup various structures embedded as record data
 */
union hammer_data_ondisk {
	struct hammer_inode_data inode;
};

#endif
