// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

// Possbilement à modifier par la suite
#define OUICHEFS_HOLE_BLOCK 0xFFFFFFFFU

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>

#include "ouichefs.h"
#include "bitmap.h"
#include "ioctl.h"

static uint32_t reservation_size = 8;
module_param(reservation_size, uint, 0644);
MODULE_PARM_DESC(reservation_size, "Block reservation window size");

/*
 * Map the buffer_head passed in argument with the iblock-th block of the file
 * represented by inode. If the requested block is not allocated and create is
 * true, allocate a new block on disk and map it.
 */
static int ouichefs_file_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	int ret = 0, bno;

	/* If block number exceeds filesize, fail */
	// if (iblock >= OUICHEFS_BLOCK_SIZE >> 2)
	if (iblock >= OUICHEFS_MAX_EXTENTS)
		return -EFBIG;

	/* Read index block from disk */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;

	/*
	 * Check if iblock is already allocated. If not and create is true,
	 * allocate it. Else, get the physical block number.
	 */
	// if (index->blocks[iblock] == 0) {
	// 	if (!create) {
	// 		ret = 0;
	// 		goto brelse_index;
	// 	}
	// 	bno = get_free_block(sbi);
	// 	if (!bno) {
	// 		ret = -ENOSPC;
	// 		goto brelse_index;
	// 	}
	// 	index->blocks[iblock] = cpu_to_le32(bno);
	// 	mark_buffer_dirty(bh_index);
	// } else {
	// 	bno = le32_to_cpu(index->blocks[iblock]);
	// }
	if (index->extents[iblock].count == 0) {
	    if (!create) {
	        ret = 0;
	        goto brelse_index;
	    }
	    bno = get_free_block(sbi);
	    if (!bno) {
	        ret = -ENOSPC;
	        goto brelse_index;
	    }
	    index->extents[iblock].start = bno;
	    index->extents[iblock].count = 1;
	    mark_buffer_dirty(bh_index);
	} else {
	    bno = index->extents[iblock].start;
	}

	/* Map the physical block to the given buffer_head */
	map_bh(bh_result, sb, bno);

brelse_index:
	brelse(bh_index);

	return ret;
}

/*
 * Called by the page cache to read a page from the physical disk and map it in
 * memory.
 */
static void ouichefs_readahead(struct readahead_control *rac)
{
	mpage_readahead(rac, ouichefs_file_get_block);
}

/*
 * Called by the page cache to write a dirty page to the physical disk (when
 * sync is called or when memory is needed).
 */
static int ouichefs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, ouichefs_file_get_block, wbc);
}

/*
 * Called by the VFS when a write() syscall occurs on file before writing the
 * data in the page cache. This functions checks if the write will be able to
 * complete and allocates the necessary blocks through block_write_begin().
 */
static int ouichefs_write_begin(struct file *file,
				struct address_space *mapping, loff_t pos,
				unsigned int len, struct page **pagep,
				void **fsdata)
{
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);
	int err;
	uint32_t nr_allocs = 0;

	/* Check if the write can be completed (enough space?) */
	if (pos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;
	nr_allocs = max(pos + len, file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	/* prepare the write */
	err = block_write_begin(mapping, pos, len, pagep,
				ouichefs_file_get_block);
	/* if this failed, reclaim newly allocated blocks */
	if (err < 0) {
		pr_err("%s:%d: newly allocated blocks reclaim not implemented yet\n",
		       __func__, __LINE__);
	}
	return err;
}

/*
 * Called by the VFS after writing data from a write() syscall to the page
 * cache. This functions updates inode metadata and truncates the file if
 * necessary.
 */
static int ouichefs_write_end(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned int len, unsigned int copied,
			      struct page *page, void *fsdata)
{
	int ret;
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;

	/* Complete the write() */
	ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
	if (ret < len) {
		pr_err("%s:%d: wrote less than asked... what do I do? nothing for now...\n",
		       __func__, __LINE__);
	} else {
		uint32_t nr_blocks_old = inode->i_blocks;

		/* Update inode metadata */
		inode->i_blocks = (roundup(inode->i_size, OUICHEFS_BLOCK_SIZE) /
				   OUICHEFS_BLOCK_SIZE) +
				  1;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);

		/* If file is smaller than before, free unused blocks */
		if (nr_blocks_old > inode->i_blocks) {
			int i;
			struct buffer_head *bh_index;
			struct ouichefs_file_index_block *index;

			/* Free unused blocks from page cache */
			truncate_pagecache(inode, inode->i_size);

			/* Read index block to remove unused blocks */
			bh_index = sb_bread(sb, ci->index_block);
			if (!bh_index) {
				pr_err("failed truncating '%s'. we just lost %llu blocks\n",
				       file->f_path.dentry->d_name.name,
				       nr_blocks_old - inode->i_blocks);
				goto end;
			}
			index = (struct ouichefs_file_index_block *)
					bh_index->b_data;

			for (i = inode->i_blocks - 1; i < nr_blocks_old - 1;
			     i++) {
				// Comment out pour que ça compile au lieu de remplacer le code car cette fonction ne devrait normalement jamais être appelée.
				// put_block(OUICHEFS_SB(sb), le32_to_cpu(index->blocks[i]));
				// index->blocks[i] = 0;
			}
			mark_buffer_dirty(bh_index);
			brelse(bh_index);
		}
	}
end:
	return ret;
}

const struct address_space_operations ouichefs_aops = {
	.readahead = ouichefs_readahead,
	.writepage = ouichefs_writepage,
	.write_begin = ouichefs_write_begin,
	.write_end = ouichefs_write_end
};

static int ouichefs_open(struct inode *inode, struct file *file)
{
	bool wronly = (file->f_flags & O_WRONLY) != 0;
	bool rdwr = (file->f_flags & O_RDWR) != 0;
	bool trunc = (file->f_flags & O_TRUNC) != 0;

	if ((wronly || rdwr) && trunc && (inode->i_size != 0)) {
		struct super_block *sb = inode->i_sb;
		struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
		struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
		struct ouichefs_file_index_block *index;
		struct buffer_head *bh_index;
		// sector_t iblock;
		int i;
		uint32_t j;

		/* Read index block from disk */
		bh_index = sb_bread(sb, ci->index_block);
		if (!bh_index)
			return -EIO;
		index = (struct ouichefs_file_index_block *)bh_index->b_data;

		// for (iblock = 0; index->blocks[iblock] != 0; iblock++) {
		// 	put_block(sbi, le32_to_cpu(index->blocks[iblock]));
		// 	index->blocks[iblock] = 0;
		// }

		// Loop modifié pour gérer les extents
		for (i = 0; i < OUICHEFS_MAX_EXTENTS; i++) {
			uint32_t start = index->extents[i].start;
			uint32_t count = index->extents[i].count;

			if (count == 0)
				break;
			if (start == 0) /* trou, rien à libérer */
				continue;
			for (j = 0; j < count; j++)
				put_block(sbi, start + j);
		}

		memset(index->extents, 0, OUICHEFS_BLOCK_SIZE);
		inode->i_size = 0;
		inode->i_blocks = 1;
		mark_buffer_dirty(bh_index);
		brelse(bh_index);
	}

	return 0;
}

static uint32_t ouichefs_extent_get_block(struct ouichefs_extent *extents,
					  uint32_t logical_block)
{
	uint32_t i, offset = 0;

	for (i = 0; i < OUICHEFS_MAX_EXTENTS; i++) {
		uint32_t start = extents[i].start;
		uint32_t count = extents[i].count;

		if (count == 0)
			break;

		/* Si le bloc logique est dans cet extent */
		if (logical_block < offset + count) {
			if (start == 0)
				return OUICHEFS_HOLE_BLOCK;
			return start + (logical_block - offset);
		}

		// Sinon on itère jusqu'à trouver le bon extent
		offset += count;
	}

	/* au-delà de EOF */
	return 0; 
}

static ssize_t ouichefs_read(struct file *file, char __user *buf,
                              size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh_index, *bh_data;
    struct ouichefs_file_index_block *index;
    ssize_t total = 0;
    uint32_t logical_block, block_offset, to_copy;
    uint32_t phys_block;

    /* Rien à lire */
    if (*ppos >= inode->i_size)
        return 0;

    /* Capper la longueur de ce qu'on lira */
    len = min_t(size_t, len, inode->i_size - *ppos);

    bh_index = sb_bread(sb, ci->index_block);
    if (!bh_index)
        return -EIO;
    index = (struct ouichefs_file_index_block *)bh_index->b_data;

    while (len > 0) {
        logical_block = *ppos / OUICHEFS_BLOCK_SIZE;
        block_offset  = *ppos % OUICHEFS_BLOCK_SIZE;
        to_copy = min_t(size_t, len, OUICHEFS_BLOCK_SIZE - block_offset);

        // phys_block = le32_to_cpu(index->blocks[logical_block]);
        // if (!phys_block)
        //     break;

		phys_block = ouichefs_extent_get_block(index->extents, logical_block);
		if (phys_block == 0)
			/* EOF ou bloc non alloué */
		    break; 
		if (phys_block == OUICHEFS_HOLE_BLOCK) {
		    /* Trou. A gérer par la suite */
		    break;
		}

        bh_data = sb_bread(sb, phys_block);
        if (!bh_data) {
            total = total ? total : -EIO;
            break;
        }

        if (copy_to_user(buf + total, bh_data->b_data + block_offset,
                         to_copy)) {
            brelse(bh_data);
            total = total ? total : -EFAULT;
            break;
        }
        brelse(bh_data);

        *ppos += to_copy;
        total += to_copy;
        len   -= to_copy;
    }

    brelse(bh_index);
    return total;
}

/* Retourne le nombre de blocs libres consécutifs qu'on a trouvés (possiblement 0) et met l'index du premier bloc consécutif dans block */
static uint32_t ouichefs_alloc_contiguous(struct super_block *sb,
                                           uint32_t requested,
                                           uint32_t *block)
{
    struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
    unsigned long *bitmap = sbi->bfree_bitmap;
    uint32_t nr_blocks = sbi->nr_blocks;
    uint32_t best_start = 0, best_len = 0;
    uint32_t cur_start = 0, cur_len = 0;
    uint32_t i, alloc;

    for (i = 0; i < nr_blocks; i++) {
		/* bit à 1 (bloc libre) */
        if (test_bit(i, bitmap)) {
            if (cur_len == 0)
                cur_start = i;
            cur_len++;

            if (cur_len >= requested) {
                best_start = cur_start;
                best_len = cur_len;
                break;
            }
        /* bit à 0 (bloc utilisé) */
        } else {
            if (cur_len > best_len) {
                best_len = cur_len;
                best_start = cur_start;
            }
            cur_len = 0;
        }
    }

    /* Cas où le meilleur run va jusqu'à la fin du bitmap */
    if (cur_len > best_len) {
        best_len = cur_len;
        best_start = cur_start;
    }

	/* partition pleine */
    if (best_len == 0)
        return 0;

    alloc = min(best_len, requested);
    bitmap_clear(bitmap, best_start, alloc);
    sbi->nr_free_blocks -= alloc;
    *block = best_start;
    return alloc;
}

/* Retourne l'index du dernier extent valide ou -1 s'il n'y en a aucun */
static int ouichefs_last_extent(struct ouichefs_extent *extents)
{
    int i, last = -1;

    for (i = 0; i < OUICHEFS_MAX_EXTENTS; i++) {
        if (extents[i].count == 0)
            break;
        last = i;
    }
    return last;
}

/* On alloue un bloc avec get_free_block (équivalent de ouichefs_alloc_block de l'énoncé) et met à jour la liste d'extents.
 * Retourne le numéro de bloc physique alloué, ou 0 en cas d'échec. */
// static uint32_t ouichefs_append_block(struct super_block *sb,
//                                        struct ouichefs_extent *extents,
// 									   uint32_t blocks_needed)
// {
//     struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
//     int last_idx;
//     uint32_t start, got;

//     got = ouichefs_alloc_contiguous(sb, blocks_needed, &start);
//     if (!got)
//         return 0;

//     last_idx = ouichefs_last_extent(extents);

//     /* Merge si contigu avec le dernier extent (et pas un trou) */
//     if (last_idx >= 0 &&
//         extents[last_idx].start != 0 &&
// 		// Dans cette ligne, le 1er start est le champ de la struct, le 2ème start est la valeur retournée par ouichefs_alloc_contiguous
// 		extents[last_idx].start + extents[last_idx].count == start) {
//         extents[last_idx].count += got;
// 	} else {
// 		// Si ce n'est pas contigu, on alloue un nouvel extent

// 		// Si on n'a pas assez d'extents pour l'écriture entière, on libère ceux qu'on vient d'allouer
// 		if (last_idx + 1 >= OUICHEFS_MAX_EXTENTS) {
//             uint32_t j;

//             for (j = 0; j < got; j++)
//                 put_block(sbi, start + j);
//             return 0;
//         }

//         extents[last_idx + 1].start = start;
//         extents[last_idx + 1].count = got;
// 	}

//     return start;
// }

static void ouichefs_gc(struct super_block *sb)
{
    struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
    struct inode *inode;

    spin_lock(&sb->s_inode_list_lock);
    list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
        struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
        uint32_t i;

        if (ci->i_reserved_count == 0)
            continue;
        for (i = 0; i < ci->i_reserved_count; i++)
            put_block(sbi, ci->i_reserved_start + i);
        ci->i_reserved_start = 0;
        ci->i_reserved_count = 0;
    }
    spin_unlock(&sb->s_inode_list_lock);

    /* TODO 1.8 : sbi->gc_runs++ */
}

/*
 * Alloue le prochain bloc pour un write en utilisant la fenêtre de
 * réservation. Si la réservation est épuisée, en alloue une nouvelle
 * via ouichefs_alloc_contiguous. Déclenche le GC si nécessaire.
 *
 * Met à jour l'extent list et les champs de réservation de l'inode.
 * Retourne le numéro de bloc physique à écrire, ou 0 en cas d'échec.
 */
static uint32_t ouichefs_get_next_block(struct super_block *sb,
                                         struct ouichefs_inode_info *ci,
                                         struct ouichefs_extent *extents)
{
    struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
    uint32_t bno;
    int last_idx;

	/* Consomme depuis la réservation existante */
    if (ci->i_reserved_count > 0) {
        bno = ci->i_reserved_start;
        ci->i_reserved_start++;
        ci->i_reserved_count--;
    
	/* Alloue une nouvelle réserve */
    } else {
        uint32_t start, got;

        got = ouichefs_alloc_contiguous(sb, reservation_size, &start);
        if (got == 0) {
            ouichefs_gc(sb);
            got = ouichefs_alloc_contiguous(sb, reservation_size, &start);
            if (got == 0)
                return 0;
        }
        bno = start;
        /* Les blocs restants sont mis en réserve */
        ci->i_reserved_start = start + 1;
        ci->i_reserved_count = got - 1;
    }

    /* Met à jour la liste d'extents : merge ou nouveau slot */
    last_idx = ouichefs_last_extent(extents);

	/* Merge si contigu avec le dernier extent (et pas un trou) */
    if (last_idx >= 0 &&
        extents[last_idx].start != 0 &&
        extents[last_idx].start + extents[last_idx].count == bno) {
        extents[last_idx].count++;
    } else {
		// Si ce n'est pas contigu, on alloue un nouvel extent
        last_idx++;

		// Si on n'a pas assez d'extents, on libère celui qu'on vient d'allouer
        if (last_idx >= OUICHEFS_MAX_EXTENTS) {
            put_block(sbi, bno);
            return 0;
        }
        extents[last_idx].start = bno;
        extents[last_idx].count = 1;
    }

    return bno;
}

static ssize_t ouichefs_write(struct file *file, const char __user *buf,
                               size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(file);
    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
    // struct ouichefs_sb_info *sbi = OUICHEFS_SB(inode->i_sb);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh_index, *bh_data;
    struct ouichefs_file_index_block *index;
    ssize_t total = 0;
    uint32_t logical_block, block_offset, to_copy, bno;
	// uint32_t nr_allocs = 0;

	/* Curseur à la fin du fichier si on est en mode APPEND */
    if (file->f_flags & O_APPEND) {
    	*ppos = inode->i_size;
	}

	/* Check if the write can be completed (enough space?) */
	// NB: J'ai choisi d'utiliser ppos et pas ppos + len car on tronque l'écriture si jamais on ne peut pas tout écrire
	// Il serait probablement pertinent d'enlever entièrement ce check maintenant qu'on a des extents complets (je regarde ça tout à l'heure si j'y pense)
	// if (*ppos > OUICHEFS_MAX_FILESIZE)
	// 	return -ENOSPC;

	loff_t end_pos;
	end_pos = *ppos + len;

	// Comment out de nr_allocs pour permettre un write partiel au lieu d'un refus en cas de manque de place (plus cohérent avec la spec)
	// nr_allocs = max(end_pos, file->f_inode->i_size) / OUICHEFS_BLOCK_SIZE;
	// // On soustrait 1 car le premier bloc est l'index bloc
	// if (nr_allocs > file->f_inode->i_blocks - 1)
	// 	nr_allocs -= file->f_inode->i_blocks - 1;
	// else
	// 	nr_allocs = 0;
	// if (nr_allocs > sbi->nr_free_blocks)
	// 	return -ENOSPC;

    bh_index = sb_bread(sb, ci->index_block);
    if (!bh_index)
        return -EIO;
    index = (struct ouichefs_file_index_block *)bh_index->b_data;

    while (len > 0) {
        logical_block = *ppos / OUICHEFS_BLOCK_SIZE;
        block_offset  = *ppos % OUICHEFS_BLOCK_SIZE;
        to_copy = min_t(size_t, len, OUICHEFS_BLOCK_SIZE - block_offset);

		// Check par sécurité, normalement jamais utilisé car 
		// le premier check l'aura catché avant
        // if (logical_block >= OUICHEFS_BLOCK_SIZE >> 2) {
		// Ce check devra être supprimé/modifié quand on introduira de "vrais" extents
		// if (logical_block >= OUICHEFS_MAX_EXTENTS) {  /* = 512 */
        //     total = total ? total : -EFBIG;
        //     break;
        // }

        // bno = le32_to_cpu(index->blocks[logical_block]);
		// bno = index->extents[logical_block].start;
		bno = ouichefs_extent_get_block(index->extents, logical_block);

		if (bno == OUICHEFS_HOLE_BLOCK) {
            /* Trou (géré plus tard) */
            total = total ? total : -EIO;
            break;
        }

        if (!bno) {
			// Nombre de blocs nécessaires pour tout stocker - nombre de blocs déjà alloués
    		// uint32_t blocks_needed = DIV_ROUND_UP(*ppos + len, OUICHEFS_BLOCK_SIZE) - logical_block;
    		bno = ouichefs_get_next_block(sb, ci, index->extents);
            // bno = get_free_block(sbi);
			// bno = ouichefs_append_block(sb, index->extents);
            if (!bno) {
                total = total ? total : -ENOSPC;
                break;
            }
            // index->blocks[logical_block] = cpu_to_le32(bno);
			// index->extents[logical_block].start = bno;
			// index->extents[logical_block].count = 1;

            mark_buffer_dirty(bh_index);
        }

        bh_data = sb_bread(sb, bno);
        if (!bh_data) {
            total = total ? total : -EIO;
            break;
        }

		// On ne remplit pas les blocs "skippés" actuellement, mais il faudra sûrement le faire ensuite.
		/* Zero-fill if we're writing past the current block end (hole) */
        // if (block_offset > 0 && *ppos > inode->i_size) {
        //     uint32_t gap = min_t(uint32_t, block_offset, inode->i_size % OUICHEFS_BLOCK_SIZE);
        //     /* memset the gap to zero — simplified, you may need to be more careful */
        // }

        if (copy_from_user(bh_data->b_data + block_offset, buf + total, to_copy)) {
            brelse(bh_data);
            total = total ? total : -EFAULT;
            break;
        }
        mark_buffer_dirty(bh_data);
        sync_dirty_buffer(bh_data);
        brelse(bh_data);

        *ppos += to_copy;
        total += to_copy;
        len   -= to_copy;

        if (*ppos > inode->i_size) {
            inode->i_size = *ppos;
            /* i_blocks = nb data blocks + 1 (index block) */
            inode->i_blocks = (roundup(inode->i_size, OUICHEFS_BLOCK_SIZE) / OUICHEFS_BLOCK_SIZE) + 1;
        }
    }

    sync_dirty_buffer(bh_index);
    brelse(bh_index);

    if (total > 0) {
        inode->i_mtime = inode->i_ctime = current_time(inode);
        mark_inode_dirty(inode);
    }
    return total;
}

static long ouichefs_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	struct inode *inode = file_inode(file);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	struct ouichefs_file_index_block *index;
	int i, n = 0;

	switch (cmd) {
	case OUICHEFS_IOC_GET_EXTENTS:
		bh = sb_bread(sb, ci->index_block);
		if (!bh)
			return -EIO;
		index = (struct ouichefs_file_index_block *)bh->b_data;

		/* Nombre d'extents valides */
		for (i = 0; i < OUICHEFS_MAX_EXTENTS; i++) {
			if (index->extents[i].count == 0)
				break;
			n++;
		}

		pr_info("extents for inode %lu: %d extent(s)\n",
			inode->i_ino, n);
		for (i = 0; i < n; i++) {
			uint32_t s = index->extents[i].start;
			uint32_t c = index->extents[i].count;

			pr_info("  [%d] start=%u count=%u (blocks %u-%u)\n",
				i, s, c, s, s + c - 1);
		}

		brelse(bh);
		return 0;

	default:
		return -ENOTTY;
	}
}

static int ouichefs_release(struct inode *inode, struct file *file)
{
    struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
    struct ouichefs_sb_info *sbi = OUICHEFS_SB(inode->i_sb);
    uint32_t i;

    if (ci->i_reserved_count > 0) {
        for (i = 0; i < ci->i_reserved_count; i++)
            put_block(sbi, ci->i_reserved_start + i);
        ci->i_reserved_start = 0;
        ci->i_reserved_count = 0;
    }
    return 0;
}

const struct file_operations ouichefs_file_ops = {
	.owner = THIS_MODULE,
	.open = ouichefs_open,
	.llseek = generic_file_llseek,
	.read = ouichefs_read,
	.write = ouichefs_write,
	.fsync = generic_file_fsync,
	.unlocked_ioctl = ouichefs_ioctl,
	.release = ouichefs_release,
};
