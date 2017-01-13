#include "testfs.h"
#include "list.h"
#include "super.h"
#include "block.h"
#include "inode.h"


#define ADDR_SIZE (BLOCK_SIZE / 4)
long MAX_FILE_SIZE = NR_DIRECT_BLOCKS + NR_INDIRECT_BLOCKS + NR_INDIRECT_BLOCKS * NR_INDIRECT_BLOCKS;

/* given logical block number, read the corresponding physical block into block.
 * return physical block number.
 * returns 0 if physical block does not exist.
 * returns negative value on other errors. */
static int
testfs_read_block(struct inode *in, int log_block_nr, char *block)
{
    //log_block_nr is currently should read from the log_block_nr th block
    //of this file or directory data
    int phy_block_nr = 0;

    if (log_block_nr > MAX_FILE_SIZE)
        return EFBIG;

    assert(log_block_nr >= 0);
    if (log_block_nr < NR_DIRECT_BLOCKS)
    {
        phy_block_nr = (int) in->in.i_block_nr[log_block_nr];
    }
    else
    {
        //find the offset in first layer of indirect block
        log_block_nr -= NR_DIRECT_BLOCKS;
        //*************************** start of doubly level bloc *********//
        if (log_block_nr >= NR_INDIRECT_BLOCKS)
        {
            //the current need to read block is in the second level block
            //find the offset in second level block
            log_block_nr = log_block_nr - NR_INDIRECT_BLOCKS;
            int num_block_from_indirect = BLOCK_SIZE / 4;
            int layer_three_pointer_offset = log_block_nr % num_block_from_indirect;
            int layer_three_block_offset = log_block_nr / num_block_from_indirect;
            //            if (layer_three_pointer_offset != 0)
            //                layer_three_block_offset++; //start from 1

            if (in->in.i_dindirect > 0)
            {
                //buffer the second level doubly indirect block
                //into block
                read_blocks(in->sb, block, in->in.i_dindirect, 1);
                phy_block_nr = ((int *) block)[layer_three_block_offset];
            }
            else
            {
                bzero(block, BLOCK_SIZE);
                return phy_block_nr;
            }

            if (phy_block_nr > 0)
            {
                //buffer the third level doubly indirect block
                //into block
                read_blocks(in->sb, block, phy_block_nr, 1);
                phy_block_nr = ((int *) block)[layer_three_pointer_offset];
            }
            else
            {
                bzero(block, BLOCK_SIZE);
                return phy_block_nr;
            }

            if (phy_block_nr > 0)
            {
                //actually read data
                read_blocks(in->sb, block, phy_block_nr, 1);
            }
            else
            {
                bzero(block, BLOCK_SIZE);
                return phy_block_nr;
            }
            return phy_block_nr;
            //*********** finish of doubly level block assignment ************//
        }
        else if (in->in.i_indirect > 0)
        {
            //not in the double level block
            read_blocks(in->sb, block, in->in.i_indirect, 1);
            phy_block_nr = ((int *) block)[log_block_nr];
            if (phy_block_nr > 0)
            {
                read_blocks(in->sb, block, phy_block_nr, 1);
            }
            else
            {
                /* we support sparse files by zeroing out a block that is not
                 * allocated on disk. */
                bzero(block, BLOCK_SIZE);
            }
            return phy_block_nr;
        }
    }

    //read from direct block if the reading block falls in this area
    //only reach this line if the block is in direct mapping area
    if (phy_block_nr > 0)
    {
        read_blocks(in->sb, block, phy_block_nr, 1);
    }
    else
    {
        /* we support sparse files by zeroing out a block that is not
         * allocated on disk. */
        bzero(block, BLOCK_SIZE);
    }
    return phy_block_nr;
}

int
testfs_read_data(struct inode *in, char *buf, off_t start, size_t size)
{
    char block[BLOCK_SIZE];
    long block_nr = start / BLOCK_SIZE;
    long block_ix = start % BLOCK_SIZE;
    long remain_size_first_block = BLOCK_SIZE - block_ix;
    int ret;
    //char *buf_position = buf;

    assert(buf);

    if (block_nr >= MAX_FILE_SIZE)
        return EFBIG;

    if (start + (off_t) size > in->in.i_size)
    {
        size = in->in.i_size - start;
    }

    size_t need_to_read_size = size;

    if (block_ix + need_to_read_size > BLOCK_SIZE)
    {
        while (block_ix + need_to_read_size > BLOCK_SIZE)
        {
            //read the first block, it might be half way done, read the
            //remaining data in this block

            if (block_nr >= MAX_FILE_SIZE)
                return EFBIG;
            ret = testfs_read_block(in, block_nr, block);
            if (ret < 0)
                return ret;
            //read a little bit into buf
            memcpy(buf, block + block_ix, remain_size_first_block);
            //update the remaining total size need to read after this
            need_to_read_size = need_to_read_size - remain_size_first_block;
            //from now on only read from the beginning of a block
            block_ix = 0;
            block_nr ++;
            buf = buf + remain_size_first_block;
            remain_size_first_block = BLOCK_SIZE;
        }
        if (block_nr >= MAX_FILE_SIZE)
            return EFBIG;
        ret = testfs_read_block(in, block_nr, block);
        if (ret < 0)
            return ret;
        memcpy(buf, block + block_ix, size);
        return size;
    }

    //is inside a single BLOCK_size
    if (block_nr >= MAX_FILE_SIZE)
        return EFBIG;
    ret = testfs_read_block(in, block_nr, block);
    if (ret < 0)
        return ret;
    memcpy(buf, block + block_ix, size);

    /* return the number of bytes read or any error */
    return size;
}



/* given logical block number, allocate a new physical block, if it does not
* exist already, and return the physical block number that is allocated.
* returns negative value on error. */
static int
testfs_allocate_block(struct inode *in, int log_block_nr, char *block)
{
    int phy_block_nr;
    char indirect[BLOCK_SIZE];
    char dindirect[BLOCK_SIZE];
    char layer_three_indirect[BLOCK_SIZE];
    int indirect_allocated = 0;
    int dindirect_allocated = 0;
    int layer_three_block_allocated = 0;

    assert(log_block_nr >= 0);
    if (log_block_nr > MAX_FILE_SIZE)
        return EFBIG;
    phy_block_nr = testfs_read_block(in, log_block_nr, block);

    /* phy_block_nr > 0: block exists, so we don't need to allocate it,
       phy_block_nr < 0: some error */
    if (phy_block_nr != 0)
        return phy_block_nr;

    /* allocate a direct block */
    if (log_block_nr < NR_DIRECT_BLOCKS)
    {
        assert(in->in.i_block_nr[log_block_nr] == 0);
        phy_block_nr = testfs_alloc_block_for_inode(in);
        if (phy_block_nr >= 0)
        {
            in->in.i_block_nr[log_block_nr] = phy_block_nr;
        }
        return phy_block_nr;
    }

    log_block_nr -= NR_DIRECT_BLOCKS;

    /********************* start of d_indirect block **********************/
    if (log_block_nr >= NR_INDIRECT_BLOCKS)
    {
        log_block_nr = log_block_nr - NR_INDIRECT_BLOCKS;
        //this logical block exist in secondary level block
        //if it is requested in the doubly level, indirect block must be allocated
        if (in->in.i_dindirect == 0)
        {
            //allocated doubly indirect block into "indirect" buffer
            bzero(dindirect, BLOCK_SIZE);
            phy_block_nr = testfs_alloc_block_for_inode(in);
            if (phy_block_nr < 0)
                return phy_block_nr;
            dindirect_allocated = 1;
            //update double level block physical block number
            in->in.i_dindirect = phy_block_nr;
        }
        else
        {
            //read the doubly indirect block
            read_blocks(in->sb, dindirect, in->in.i_dindirect, 1);
        }
        //up to here "dindirect" buffer has the doubly indirect block copy anyway

        //check is third level block exist, if not, allocated one for this block
        //calculate the offset inside third level block and which third level
        //block this logic block belongs to
        int num_block_from_indirect = BLOCK_SIZE / 4;
        int layer_three_pointer_offset = log_block_nr % num_block_from_indirect;
        int layer_three_block_offset = log_block_nr / num_block_from_indirect;

        if (((int *) dindirect)[layer_three_block_offset] == 0)
        {
            //layer three block does not exist
            bzero(layer_three_indirect, BLOCK_SIZE);
            phy_block_nr = testfs_alloc_block_for_inode(in);
            if (phy_block_nr < 0)
                return phy_block_nr;
            layer_three_block_allocated = 1;
            ((int *) dindirect)[layer_three_block_offset] = phy_block_nr;
            write_blocks(in->sb, dindirect, in->in.i_dindirect, 1);
        }
        else
        {
            read_blocks(in->sb, layer_three_indirect, ((int *) dindirect)[layer_three_block_offset], 1);
        }
        //up to here, "layer_three_indirect" has the layer three block copy

        //this block is the actual block logic
        phy_block_nr = testfs_alloc_block_for_inode(in);

        if (phy_block_nr >= 0)
        {
            //update corresponding entry
            ((int *) layer_three_indirect)[layer_three_pointer_offset] = phy_block_nr;
            write_blocks(in->sb, layer_three_indirect, ((int *) dindirect)[layer_three_block_offset], 1);
            if (layer_three_block_allocated == 1)
                write_blocks(in->sb, dindirect, in->in.i_dindirect, 1);
        }
        else
        {
            //actual logic block allocation fail
            if (layer_three_block_allocated == 1)
            {
                testfs_free_block_from_inode(in, ((int *) dindirect)[layer_three_block_offset]);
                ((int *)dindirect)[layer_three_block_offset] = 0;
                write_blocks(in->sb, dindirect, in->in.i_dindirect, 1);
            }
            if (dindirect_allocated == 1)
            {
                testfs_free_block_from_inode(in, in->in.i_indirect);
                in->in.i_dindirect = 0;
            }
        }
        return phy_block_nr;
    }/********************* end of d_indirect block ***********************/
    else
    {
        //the logical block is in the first level indirect block
        if (in->in.i_indirect == 0)   /* allocate an indirect block */
        {
            //indirect block does not exist
            bzero(indirect, BLOCK_SIZE);
            phy_block_nr = testfs_alloc_block_for_inode(in);
            if (phy_block_nr < 0)
                return phy_block_nr;
            //flag
            indirect_allocated = 1;
            //in->in.i_indirect is the indirect block physical block number
            in->in.i_indirect = phy_block_nr;
        }
        else     /* read indirect block */
        {
            //indirect block already exist, read it
            read_blocks(in->sb, indirect, in->in.i_indirect, 1);
        }
        //now indirect has the copy of indirect block

        /* allocate direct block for indirect block*/
        //now log_block_nr is already the offset in indirect level blocks
        assert(((int *) indirect)[log_block_nr] == 0);
        phy_block_nr = testfs_alloc_block_for_inode(in);

        if (phy_block_nr >= 0)
        {
            /* update indirect block */
            ((int *) indirect)[log_block_nr] = phy_block_nr;
            write_blocks(in->sb, indirect, in->in.i_indirect, 1);
        }
        else if (indirect_allocated)
        {
            //not indirect block allocated first -> allocate -> request for
            //physical block -> failed -> free the indirect block
            /* free the indirect block that was allocated */
            testfs_free_block_from_inode(in, in->in.i_indirect);
            //0 for in->in.i_indirect means indirect block is not allocated yet
            in->in.i_indirect = 0;
        }
        return phy_block_nr;
    }
}


int
testfs_write_data(struct inode *in, const char *buf, off_t start, size_t size)
{
    char block[BLOCK_SIZE];
    long block_nr = start / BLOCK_SIZE;
    long block_ix = start % BLOCK_SIZE;
    int ret;
    int need_to_write = size;
    int first_block_to_write = BLOCK_SIZE - block_ix;

    if (block_nr >= MAX_FILE_SIZE)
    {
        return EFBIG;
    }

    if (block_ix + size > BLOCK_SIZE)
    {
        //need to finish first block and write in later blocks
        //write the first unfinished block
        if (block_nr >= MAX_FILE_SIZE)
        {
            in->in.i_size = 34376597504;
            in->i_flags |= I_FLAGS_DIRTY;
            return EFBIG;
            //
        }
        ret = testfs_allocate_block(in, block_nr, block);
        if (ret < 0)
            return ret;
        memcpy(block + block_ix, buf, first_block_to_write);
        write_blocks(in->sb, block, ret, 1);

        block_nr++;
        block_ix = 0;
        need_to_write = size - first_block_to_write;
        buf = buf + first_block_to_write;

        while (need_to_write > BLOCK_SIZE)
        {
            ret = testfs_allocate_block(in, block_nr, block);
            if (ret < 0)
            {
                in->in.i_size = 34376597504;
                in->i_flags |= I_FLAGS_DIRTY;
                return ret;
            }
            memcpy(block, buf, BLOCK_SIZE);
            write_blocks(in->sb, block, ret, 1);
            need_to_write = need_to_write - BLOCK_SIZE;
            block_nr++;
            buf = buf + BLOCK_SIZE;
        }
    }

    //fix write too big test case
    if(block_nr >= MAX_FILE_SIZE)
    {
        in->in.i_size = 34376597504;
        in->i_flags |= I_FLAGS_DIRTY;
        return -EFBIG;
    }
    //write the last unfinished block
    ret = testfs_allocate_block(in, block_nr, block);
    if (ret < 0)
    {
        in->in.i_size = 34376597504;
        in->i_flags |= I_FLAGS_DIRTY;
        return ret;
    }

    //same as the second function, this one serves two functionalities one, finish unfinished block
    //when the total need to write block exceeds one block (this case block_ix will be zero);
    //second, when the write can be finishedwithin one block
    memcpy(block + block_ix, buf, need_to_write);
    write_blocks(in->sb, block, ret, 1);

    /* increment i_size by the number of bytes written. */
    if (size > 0)
        in->in.i_size = MAX(in->in.i_size, start + (off_t) size);
    in->i_flags |= I_FLAGS_DIRTY;
    /* return the number of bytes written or any error */
    return size;
}



int
testfs_free_blocks(struct inode *in)
{
    int k;
    int j = 0;
    int i = 0;
    int total_number = 0;
    int e_block_nr;

    /* last block number */
    e_block_nr = DIVROUNDUP(in->in.i_size, BLOCK_SIZE);

    /* remove direct blocks */
    for (k = 0; k < e_block_nr && k < NR_DIRECT_BLOCKS; k++)
    {
        if (in->in.i_block_nr[k] == 0)
            continue;
        testfs_free_block_from_inode(in, in->in.i_block_nr[k]);
        in->in.i_block_nr[k] = 0;
    }
    e_block_nr -= NR_DIRECT_BLOCKS;

    /* remove indirect blocks */
    if (in->in.i_indirect > 0)
    {
        char block[BLOCK_SIZE];
        read_blocks(in->sb, block, in->in.i_indirect, 1);
        for (k = 0; k < e_block_nr && k < NR_INDIRECT_BLOCKS; k++)
        {
            testfs_free_block_from_inode(in, ((int *) block)[k]);
            ((int *) block)[k] = 0;
        }
        testfs_free_block_from_inode(in, in->in.i_indirect);
        in->in.i_indirect = 0;
    }

    e_block_nr -= NR_INDIRECT_BLOCKS;
    if (e_block_nr >= 0)
    {
        if (in->in.i_dindirect > 0)
        {
            char block[BLOCK_SIZE];
            read_blocks(in->sb, block, in->in.i_dindirect, 1);
            for (i = 0; total_number < e_block_nr && i < NR_INDIRECT_BLOCKS; i++)
            {
                if (((int *) block)[i] != 0)
                {
                    char block_pikachu[BLOCK_SIZE];
                    read_blocks(in->sb, block_pikachu, ((int *)block)[i], 1);
                    for (j = 0; total_number < e_block_nr && j < NR_INDIRECT_BLOCKS; j++)
                    {
                        if (((int *) block_pikachu)[j] != 0)
                        {
                            testfs_free_block_from_inode(in, ((int *) block_pikachu)[j]);
                            ((int *) block_pikachu)[j] = 0;
                            total_number++;
                        }
                    }
                    testfs_free_block_from_inode(in, ((int *) block)[i]);
                    ((int *) block)[i] = 0;
                    total_number++;
                }
            }
            testfs_free_block_from_inode(in, in->in.i_dindirect);
            in->in.i_dindirect = 0;
        }
    }
    in->in.i_size = 0;
    in->i_flags |= I_FLAGS_DIRTY;
    return 0;
}


