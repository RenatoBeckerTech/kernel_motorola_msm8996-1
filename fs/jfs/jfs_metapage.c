/*
 *   Copyright (C) International Business Machines Corp., 2000-2005
 *   Portions Copyright (C) Christoph Hellwig, 2001-2002
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/bio.h>
#include <linux/init.h>
#include <linux/buffer_head.h>
#include <linux/mempool.h>
#include "jfs_incore.h"
#include "jfs_superblock.h"
#include "jfs_filsys.h"
#include "jfs_metapage.h"
#include "jfs_txnmgr.h"
#include "jfs_debug.h"

#ifdef CONFIG_JFS_STATISTICS
static struct {
	uint	pagealloc;	/* # of page allocations */
	uint	pagefree;	/* # of page frees */
	uint	lockwait;	/* # of sleeping lock_metapage() calls */
} mpStat;
#endif

#define metapage_locked(mp) test_bit(META_locked, &(mp)->flag)
#define trylock_metapage(mp) test_and_set_bit(META_locked, &(mp)->flag)

static inline void unlock_metapage(struct metapage *mp)
{
	clear_bit(META_locked, &mp->flag);
	wake_up(&mp->wait);
}

static inline void __lock_metapage(struct metapage *mp)
{
	DECLARE_WAITQUEUE(wait, current);
	INCREMENT(mpStat.lockwait);
	add_wait_queue_exclusive(&mp->wait, &wait);
	do {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (metapage_locked(mp)) {
			unlock_page(mp->page);
			schedule();
			lock_page(mp->page);
		}
	} while (trylock_metapage(mp));
	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&mp->wait, &wait);
}

/*
 * Must have mp->page locked
 */
static inline void lock_metapage(struct metapage *mp)
{
	if (trylock_metapage(mp))
		__lock_metapage(mp);
}

#define METAPOOL_MIN_PAGES 32
static kmem_cache_t *metapage_cache;
static mempool_t *metapage_mempool;

#define MPS_PER_PAGE (PAGE_CACHE_SIZE >> L2PSIZE)

#if MPS_PER_PAGE > 1

struct meta_anchor {
	int mp_count;
	atomic_t io_count;
	struct metapage *mp[MPS_PER_PAGE];
};
#define mp_anchor(page) ((struct meta_anchor *)page->private)

static inline struct metapage *page_to_mp(struct page *page, uint offset)
{
	if (!PagePrivate(page))
		return NULL;
	return mp_anchor(page)->mp[offset >> L2PSIZE];
}

static inline int insert_metapage(struct page *page, struct metapage *mp)
{
	struct meta_anchor *a;
	int index;
	int l2mp_blocks;	/* log2 blocks per metapage */

	if (PagePrivate(page))
		a = mp_anchor(page);
	else {
		a = kmalloc(sizeof(struct meta_anchor), GFP_NOFS);
		if (!a)
			return -ENOMEM;
		memset(a, 0, sizeof(struct meta_anchor));
		page->private = (unsigned long)a;
		SetPagePrivate(page);
		kmap(page);
	}

	if (mp) {
		l2mp_blocks = L2PSIZE - page->mapping->host->i_blkbits;
		index = (mp->index >> l2mp_blocks) & (MPS_PER_PAGE - 1);
		a->mp_count++;
		a->mp[index] = mp;
	}

	return 0;
}

static inline void remove_metapage(struct page *page, struct metapage *mp)
{
	struct meta_anchor *a = mp_anchor(page);
	int l2mp_blocks = L2PSIZE - page->mapping->host->i_blkbits;
	int index;

	index = (mp->index >> l2mp_blocks) & (MPS_PER_PAGE - 1);

	BUG_ON(a->mp[index] != mp);

	a->mp[index] = NULL;
	if (--a->mp_count == 0) {
		kfree(a);
		page->private = 0;
		ClearPagePrivate(page);
		kunmap(page);
	}
}

static inline void inc_io(struct page *page)
{
	atomic_inc(&mp_anchor(page)->io_count);
}

static inline void dec_io(struct page *page, void (*handler) (struct page *))
{
	if (atomic_dec_and_test(&mp_anchor(page)->io_count))
		handler(page);
}

#else
static inline struct metapage *page_to_mp(struct page *page, uint offset)
{
	return PagePrivate(page) ? (struct metapage *)page->private : NULL;
}

static inline int insert_metapage(struct page *page, struct metapage *mp)
{
	if (mp) {
		page->private = (unsigned long)mp;
		SetPagePrivate(page);
		kmap(page);
	}
	return 0;
}

static inline void remove_metapage(struct page *page, struct metapage *mp)
{
	page->private = 0;
	ClearPagePrivate(page);
	kunmap(page);
}

#define inc_io(page) do {} while(0)
#define dec_io(page, handler) handler(page)

#endif

static void init_once(void *foo, kmem_cache_t *cachep, unsigned long flags)
{
	struct metapage *mp = (struct metapage *)foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR) {
		mp->lid = 0;
		mp->lsn = 0;
		mp->flag = 0;
		mp->data = NULL;
		mp->clsn = 0;
		mp->log = NULL;
		set_bit(META_free, &mp->flag);
		init_waitqueue_head(&mp->wait);
	}
}

static inline struct metapage *alloc_metapage(unsigned int gfp_mask)
{
	return mempool_alloc(metapage_mempool, gfp_mask);
}

static inline void free_metapage(struct metapage *mp)
{
	mp->flag = 0;
	set_bit(META_free, &mp->flag);

	mempool_free(mp, metapage_mempool);
}

int __init metapage_init(void)
{
	/*
	 * Allocate the metapage structures
	 */
	metapage_cache = kmem_cache_create("jfs_mp", sizeof(struct metapage),
					   0, 0, init_once, NULL);
	if (metapage_cache == NULL)
		return -ENOMEM;

	metapage_mempool = mempool_create(METAPOOL_MIN_PAGES, mempool_alloc_slab,
					  mempool_free_slab, metapage_cache);

	if (metapage_mempool == NULL) {
		kmem_cache_destroy(metapage_cache);
		return -ENOMEM;
	}

	return 0;
}

void metapage_exit(void)
{
	mempool_destroy(metapage_mempool);
	kmem_cache_destroy(metapage_cache);
}

static inline void drop_metapage(struct page *page, struct metapage *mp)
{
	if (mp->count || mp->nohomeok || test_bit(META_dirty, &mp->flag) ||
	    test_bit(META_io, &mp->flag))
		return;
	remove_metapage(page, mp);
	INCREMENT(mpStat.pagefree);
	free_metapage(mp);
}

/*
 * Metapage address space operations
 */

static sector_t metapage_get_blocks(struct inode *inode, sector_t lblock,
				    unsigned int *len)
{
	int rc = 0;
	int xflag;
	s64 xaddr;
	sector_t file_blocks = (inode->i_size + inode->i_blksize - 1) >>
			       inode->i_blkbits;

	if (lblock >= file_blocks)
		return 0;
	if (lblock + *len > file_blocks)
		*len = file_blocks - lblock;

	if (inode->i_ino) {
		rc = xtLookup(inode, (s64)lblock, *len, &xflag, &xaddr, len, 0);
		if ((rc == 0) && *len)
			lblock = (sector_t)xaddr;
		else
			lblock = 0;
	} /* else no mapping */

	return lblock;
}

static void last_read_complete(struct page *page)
{
	if (!PageError(page))
		SetPageUptodate(page);
	unlock_page(page);
}

static int metapage_read_end_io(struct bio *bio, unsigned int bytes_done,
				int err)
{
	struct page *page = bio->bi_private;

	if (bio->bi_size)
		return 1;

	if (!test_bit(BIO_UPTODATE, &bio->bi_flags)) {
		printk(KERN_ERR "metapage_read_end_io: I/O error\n");
		SetPageError(page);
	}

	dec_io(page, last_read_complete);
	bio_put(bio);

	return 0;
}

static void remove_from_logsync(struct metapage *mp)
{
	struct jfs_log *log = mp->log;
	unsigned long flags;
/*
 * This can race.  Recheck that log hasn't been set to null, and after
 * acquiring logsync lock, recheck lsn
 */
	if (!log)
		return;

	LOGSYNC_LOCK(log, flags);
	if (mp->lsn) {
		mp->log = NULL;
		mp->lsn = 0;
		mp->clsn = 0;
		log->count--;
		list_del(&mp->synclist);
	}
	LOGSYNC_UNLOCK(log, flags);
}

static void last_write_complete(struct page *page)
{
	struct metapage *mp;
	unsigned int offset;

	for (offset = 0; offset < PAGE_CACHE_SIZE; offset += PSIZE) {
		mp = page_to_mp(page, offset);
		if (mp && test_bit(META_io, &mp->flag)) {
			if (mp->lsn)
				remove_from_logsync(mp);
			clear_bit(META_io, &mp->flag);
		}
		/*
		 * I'd like to call drop_metapage here, but I don't think it's
		 * safe unless I have the page locked
		 */
	}
	end_page_writeback(page);
}

static int metapage_write_end_io(struct bio *bio, unsigned int bytes_done,
				 int err)
{
	struct page *page = bio->bi_private;

	BUG_ON(!PagePrivate(page));

	if (bio->bi_size)
		return 1;

	if (! test_bit(BIO_UPTODATE, &bio->bi_flags)) {
		printk(KERN_ERR "metapage_write_end_io: I/O error\n");
		SetPageError(page);
	}
	dec_io(page, last_write_complete);
	bio_put(bio);
	return 0;
}

static int metapage_writepage(struct page *page, struct writeback_control *wbc)
{
	struct bio *bio = NULL;
	unsigned int block_offset;	/* block offset of mp within page */
	struct inode *inode = page->mapping->host;
	unsigned int blocks_per_mp = JFS_SBI(inode->i_sb)->nbperpage;
	unsigned int len;
	unsigned int xlen;
	struct metapage *mp;
	int redirty = 0;
	sector_t lblock;
	sector_t pblock;
	sector_t next_block = 0;
	sector_t page_start;
	unsigned long bio_bytes = 0;
	unsigned long bio_offset = 0;
	unsigned int offset;

	page_start = (sector_t)page->index <<
		     (PAGE_CACHE_SHIFT - inode->i_blkbits);
	BUG_ON(!PageLocked(page));
	BUG_ON(PageWriteback(page));

	for (offset = 0; offset < PAGE_CACHE_SIZE; offset += PSIZE) {
		mp = page_to_mp(page, offset);

		if (!mp || !test_bit(META_dirty, &mp->flag))
			continue;

		if (mp->nohomeok && !test_bit(META_forcewrite, &mp->flag)) {
			redirty = 1;
			continue;
		}

		clear_bit(META_dirty, &mp->flag);
		block_offset = offset >> inode->i_blkbits;
		lblock = page_start + block_offset;
		if (bio) {
			if (xlen && lblock == next_block) {
				/* Contiguous, in memory & on disk */
				len = min(xlen, blocks_per_mp);
				xlen -= len;
				bio_bytes += len << inode->i_blkbits;
				set_bit(META_io, &mp->flag);
				continue;
			}
			/* Not contiguous */
			if (bio_add_page(bio, page, bio_bytes, bio_offset) <
			    bio_bytes)
				goto add_failed;
			/*
			 * Increment counter before submitting i/o to keep
			 * count from hitting zero before we're through
			 */
			inc_io(page);
			if (!bio->bi_size)
				goto dump_bio;
			submit_bio(WRITE, bio);
			bio = NULL;
		} else {
			set_page_writeback(page);
			inc_io(page);
		}
		xlen = (PAGE_CACHE_SIZE - offset) >> inode->i_blkbits;
		pblock = metapage_get_blocks(inode, lblock, &xlen);
		if (!pblock) {
			/* Need better error handling */
			printk(KERN_ERR "JFS: metapage_get_blocks failed\n");
			dec_io(page, last_write_complete);
			continue;
		}
		set_bit(META_io, &mp->flag);
		len = min(xlen, (uint) JFS_SBI(inode->i_sb)->nbperpage);

		bio = bio_alloc(GFP_NOFS, 1);
		bio->bi_bdev = inode->i_sb->s_bdev;
		bio->bi_sector = pblock << (inode->i_blkbits - 9);
		bio->bi_end_io = metapage_write_end_io;
		bio->bi_private = page;

		/* Don't call bio_add_page yet, we may add to this vec */
		bio_offset = offset;
		bio_bytes = len << inode->i_blkbits;

		xlen -= len;
		next_block = lblock + len;
	}
	if (bio) {
		if (bio_add_page(bio, page, bio_bytes, bio_offset) < bio_bytes)
				goto add_failed;
		if (!bio->bi_size)
			goto dump_bio;
		
		submit_bio(WRITE, bio);
	}
	if (redirty)
		redirty_page_for_writepage(wbc, page);

	unlock_page(page);

	return 0;
add_failed:
	/* We should never reach here, since we're only adding one vec */
	printk(KERN_ERR "JFS: bio_add_page failed unexpectedly\n");
	goto skip;
dump_bio:
	dump_mem("bio", bio, sizeof(*bio));
skip:
	bio_put(bio);
	unlock_page(page);
	dec_io(page, last_write_complete);

	return -EIO;
}

static int metapage_readpage(struct file *fp, struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct bio *bio = NULL;
	unsigned int block_offset;
	unsigned int blocks_per_page = PAGE_CACHE_SIZE >> inode->i_blkbits;
	sector_t page_start;	/* address of page in fs blocks */
	sector_t pblock;
	unsigned int xlen;
	unsigned int len;
	unsigned int offset;

	BUG_ON(!PageLocked(page));
	page_start = (sector_t)page->index <<
		     (PAGE_CACHE_SHIFT - inode->i_blkbits);

	block_offset = 0;
	while (block_offset < blocks_per_page) {
		xlen = blocks_per_page - block_offset;
		pblock = metapage_get_blocks(inode, page_start + block_offset,
					     &xlen);
		if (pblock) {
			if (!PagePrivate(page))
				insert_metapage(page, NULL);
			inc_io(page);
			if (bio)
				submit_bio(READ, bio);

			bio = bio_alloc(GFP_NOFS, 1);
			bio->bi_bdev = inode->i_sb->s_bdev;
			bio->bi_sector = pblock << (inode->i_blkbits - 9);
			bio->bi_end_io = metapage_read_end_io;
			bio->bi_private = page;
			len = xlen << inode->i_blkbits;
			offset = block_offset << inode->i_blkbits;
			if (bio_add_page(bio, page, len, offset) < len)
				goto add_failed;
			block_offset += xlen;
		} else
			block_offset++;
	}
	if (bio)
		submit_bio(READ, bio);
	else
		unlock_page(page);

	return 0;

add_failed:
	printk(KERN_ERR "JFS: bio_add_page failed unexpectedly\n");
	bio_put(bio);
	dec_io(page, last_read_complete);
	return -EIO;
}

static int metapage_releasepage(struct page *page, int gfp_mask)
{
	struct metapage *mp;
	int busy = 0;
	unsigned int offset;

	for (offset = 0; offset < PAGE_CACHE_SIZE; offset += PSIZE) {
		mp = page_to_mp(page, offset);

		if (!mp)
			continue;

		jfs_info("metapage_releasepage: mp = 0x%p", mp);
		if (mp->count || mp->nohomeok) {
			jfs_info("count = %ld, nohomeok = %d", mp->count,
				 mp->nohomeok);
			busy = 1;
			continue;
		}
		wait_on_page_writeback(page);
		//WARN_ON(test_bit(META_dirty, &mp->flag));
		if (test_bit(META_dirty, &mp->flag)) {
			dump_mem("dirty mp in metapage_releasepage", mp,
				 sizeof(struct metapage));
			dump_mem("page", page, sizeof(struct page));
			dump_stack();
		}
		WARN_ON(mp->lsn);
		if (mp->lsn)
			remove_from_logsync(mp);
		remove_metapage(page, mp);
		INCREMENT(mpStat.pagefree);
		free_metapage(mp);
	}
	if (busy)
		return -1;

	return 0;
}

static int metapage_invalidatepage(struct page *page, unsigned long offset)
{
	BUG_ON(offset);

	if (PageWriteback(page))
		return 0;

	return metapage_releasepage(page, 0);
}

struct address_space_operations jfs_metapage_aops = {
	.readpage	= metapage_readpage,
	.writepage	= metapage_writepage,
	.sync_page	= block_sync_page,
	.releasepage	= metapage_releasepage,
	.invalidatepage	= metapage_invalidatepage,
	.set_page_dirty	= __set_page_dirty_nobuffers,
};

struct metapage *__get_metapage(struct inode *inode, unsigned long lblock,
				unsigned int size, int absolute,
				unsigned long new)
{
	int l2BlocksPerPage;
	int l2bsize;
	struct address_space *mapping;
	struct metapage *mp = NULL;
	struct page *page;
	unsigned long page_index;
	unsigned long page_offset;

	jfs_info("__get_metapage: ino = %ld, lblock = 0x%lx, abs=%d",
		 inode->i_ino, lblock, absolute);

	l2bsize = inode->i_blkbits;
	l2BlocksPerPage = PAGE_CACHE_SHIFT - l2bsize;
	page_index = lblock >> l2BlocksPerPage;
	page_offset = (lblock - (page_index << l2BlocksPerPage)) << l2bsize;
	if ((page_offset + size) > PAGE_CACHE_SIZE) {
		jfs_err("MetaData crosses page boundary!!");
		jfs_err("lblock = %lx, size  = %d", lblock, size);
		dump_stack();
		return NULL;
	}
	if (absolute)
		mapping = JFS_SBI(inode->i_sb)->direct_inode->i_mapping;
	else {
		/*
		 * If an nfs client tries to read an inode that is larger
		 * than any existing inodes, we may try to read past the
		 * end of the inode map
		 */
		if ((lblock << inode->i_blkbits) >= inode->i_size)
			return NULL;
		mapping = inode->i_mapping;
	}

	if (new && (PSIZE == PAGE_CACHE_SIZE)) {
		page = grab_cache_page(mapping, page_index);
		if (!page) {
			jfs_err("grab_cache_page failed!");
			return NULL;
		}
		SetPageUptodate(page);
	} else {
		page = read_cache_page(mapping, page_index,
			    (filler_t *)mapping->a_ops->readpage, NULL);
		if (IS_ERR(page)) {
			jfs_err("read_cache_page failed!");
			return NULL;
		}
		lock_page(page);
	}

	mp = page_to_mp(page, page_offset);
	if (mp) {
		if (mp->logical_size != size) {
			jfs_error(inode->i_sb,
				  "__get_metapage: mp->logical_size != size");
			jfs_err("logical_size = %d, size = %d",
				mp->logical_size, size);
			dump_stack();
			goto unlock; 
		}
		mp->count++;
		lock_metapage(mp);
		if (test_bit(META_discard, &mp->flag)) {
			if (!new) {
				jfs_error(inode->i_sb,
					  "__get_metapage: using a "
					  "discarded metapage");
				discard_metapage(mp);
				goto unlock; 
			}
			clear_bit(META_discard, &mp->flag);
		}
	} else {
		INCREMENT(mpStat.pagealloc);
		mp = alloc_metapage(GFP_NOFS);
		mp->page = page;
		mp->flag = 0;
		mp->xflag = COMMIT_PAGE;
		mp->count = 1;
		mp->nohomeok = 0;
		mp->logical_size = size;
		mp->data = page_address(page) + page_offset;
		mp->index = lblock;
		if (unlikely(insert_metapage(page, mp))) {
			free_metapage(mp);
			goto unlock;
		}
		lock_metapage(mp);
	}

	if (new) {
		jfs_info("zeroing mp = 0x%p", mp);
		memset(mp->data, 0, PSIZE);
	}

	unlock_page(page);
	jfs_info("__get_metapage: returning = 0x%p data = 0x%p", mp, mp->data);
	return mp;

unlock:
	unlock_page(page);
	return NULL;
}

void grab_metapage(struct metapage * mp)
{
	jfs_info("grab_metapage: mp = 0x%p", mp);
	page_cache_get(mp->page);
	lock_page(mp->page);
	mp->count++;
	lock_metapage(mp);
	unlock_page(mp->page);
}

void force_metapage(struct metapage *mp)
{
	struct page *page = mp->page;
	jfs_info("force_metapage: mp = 0x%p", mp);
	set_bit(META_forcewrite, &mp->flag);
	clear_bit(META_sync, &mp->flag);
	page_cache_get(page);
	lock_page(page);
	set_page_dirty(page);
	write_one_page(page, 1);
	clear_bit(META_forcewrite, &mp->flag);
	page_cache_release(page);
}

void hold_metapage(struct metapage *mp)
{
	lock_page(mp->page);
}

void put_metapage(struct metapage *mp)
{
	if (mp->count || mp->nohomeok) {
		/* Someone else will release this */
		unlock_page(mp->page);
		return;
	}
	page_cache_get(mp->page);
	mp->count++;
	lock_metapage(mp);
	unlock_page(mp->page);
	release_metapage(mp);
}

void release_metapage(struct metapage * mp)
{
	struct page *page = mp->page;
	jfs_info("release_metapage: mp = 0x%p, flag = 0x%lx", mp, mp->flag);

	BUG_ON(!page);

	lock_page(page);
	unlock_metapage(mp);

	assert(mp->count);
	if (--mp->count || mp->nohomeok) {
		unlock_page(page);
		page_cache_release(page);
		return;
	}

	if (test_bit(META_dirty, &mp->flag)) {
		set_page_dirty(page);
		if (test_bit(META_sync, &mp->flag)) {
			clear_bit(META_sync, &mp->flag);
			write_one_page(page, 1);
			lock_page(page); /* write_one_page unlocks the page */
		}
	} else if (mp->lsn)	/* discard_metapage doesn't remove it */
		remove_from_logsync(mp);

#if MPS_PER_PAGE == 1
	/*
	 * If we know this is the only thing in the page, we can throw
	 * the page out of the page cache.  If pages are larger, we
	 * don't want to do this.
	 */

	/* Retest mp->count since we may have released page lock */
	if (test_bit(META_discard, &mp->flag) && !mp->count) {
		clear_page_dirty(page);
		ClearPageUptodate(page);
#ifdef _NOT_YET
		if (page->mapping) {
		/* Remove from page cache and page cache reference */
			remove_from_page_cache(page);
			page_cache_release(page);
			metapage_releasepage(page, 0);
		}
#endif
	}
#else
	/* Try to keep metapages from using up too much memory */
	drop_metapage(page, mp);
#endif
	unlock_page(page);
	page_cache_release(page);
}

void __invalidate_metapages(struct inode *ip, s64 addr, int len)
{
	sector_t lblock;
	int l2BlocksPerPage = PAGE_CACHE_SHIFT - ip->i_blkbits;
	int BlocksPerPage = 1 << l2BlocksPerPage;
	/* All callers are interested in block device's mapping */
	struct address_space *mapping =
		JFS_SBI(ip->i_sb)->direct_inode->i_mapping;
	struct metapage *mp;
	struct page *page;
	unsigned int offset;

	/*
	 * Mark metapages to discard.  They will eventually be
	 * released, but should not be written.
	 */
	for (lblock = addr & ~(BlocksPerPage - 1); lblock < addr + len;
	     lblock += BlocksPerPage) {
		page = find_lock_page(mapping, lblock >> l2BlocksPerPage);
		if (!page)
			continue;
		for (offset = 0; offset < PAGE_CACHE_SIZE; offset += PSIZE) {
			mp = page_to_mp(page, offset);
			if (!mp)
				continue;
			if (mp->index < addr)
				continue;
			if (mp->index >= addr + len)
				break;

			clear_bit(META_dirty, &mp->flag);
			set_bit(META_discard, &mp->flag);
			if (mp->lsn)
				remove_from_logsync(mp);
		}
		unlock_page(page);
		page_cache_release(page);
	}
}

#ifdef CONFIG_JFS_STATISTICS
int jfs_mpstat_read(char *buffer, char **start, off_t offset, int length,
		    int *eof, void *data)
{
	int len = 0;
	off_t begin;

	len += sprintf(buffer,
		       "JFS Metapage statistics\n"
		       "=======================\n"
		       "page allocations = %d\n"
		       "page frees = %d\n"
		       "lock waits = %d\n",
		       mpStat.pagealloc,
		       mpStat.pagefree,
		       mpStat.lockwait);

	begin = offset;
	*start = buffer + begin;
	len -= begin;

	if (len > length)
		len = length;
	else
		*eof = 1;

	if (len < 0)
		len = 0;

	return len;
}
#endif
