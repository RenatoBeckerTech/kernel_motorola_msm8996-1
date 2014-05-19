#ifndef INT_BLK_MQ_TAG_H
#define INT_BLK_MQ_TAG_H

#include "blk-mq.h"

enum {
	BT_WAIT_QUEUES	= 8,
	BT_WAIT_BATCH	= 8,
};

struct bt_wait_state {
	atomic_t wait_cnt;
	wait_queue_head_t wait;
} ____cacheline_aligned_in_smp;

#define TAG_TO_INDEX(bt, tag)	((tag) >> (bt)->bits_per_word)
#define TAG_TO_BIT(bt, tag)	((tag) & ((1 << (bt)->bits_per_word) - 1))

struct blk_mq_bitmap_tags {
	unsigned int depth;
	unsigned int wake_cnt;
	unsigned int bits_per_word;

	unsigned int map_nr;
	struct blk_align_bitmap *map;

	unsigned int wake_index;
	struct bt_wait_state *bs;
};

/*
 * Tag address space map.
 */
struct blk_mq_tags {
	unsigned int nr_tags;
	unsigned int nr_reserved_tags;

	struct blk_mq_bitmap_tags bitmap_tags;
	struct blk_mq_bitmap_tags breserved_tags;

	struct request **rqs;
	struct list_head page_list;
};


extern struct blk_mq_tags *blk_mq_init_tags(unsigned int nr_tags, unsigned int reserved_tags, int node);
extern void blk_mq_free_tags(struct blk_mq_tags *tags);

extern unsigned int blk_mq_get_tag(struct blk_mq_tags *tags, struct blk_mq_hw_ctx *hctx, unsigned int *last_tag, gfp_t gfp, bool reserved);
extern void blk_mq_wait_for_tags(struct blk_mq_tags *tags, struct blk_mq_hw_ctx *hctx, bool reserved);
extern void blk_mq_put_tag(struct blk_mq_tags *tags, unsigned int tag, unsigned int *last_tag);
extern void blk_mq_tag_busy_iter(struct blk_mq_tags *tags, void (*fn)(void *data, unsigned long *), void *data);
extern bool blk_mq_has_free_tags(struct blk_mq_tags *tags);
extern ssize_t blk_mq_tag_sysfs_show(struct blk_mq_tags *tags, char *page);
extern void blk_mq_tag_init_last_tag(struct blk_mq_tags *tags, unsigned int *last_tag);

enum {
	BLK_MQ_TAG_CACHE_MIN	= 1,
	BLK_MQ_TAG_CACHE_MAX	= 64,
};

enum {
	BLK_MQ_TAG_FAIL		= -1U,
	BLK_MQ_TAG_MIN		= BLK_MQ_TAG_CACHE_MIN,
	BLK_MQ_TAG_MAX		= BLK_MQ_TAG_FAIL - 1,
};

#endif
