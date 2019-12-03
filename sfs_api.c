#include "sfs_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <strings.h>
#include <inttypes.h>
#include "disk_emu.h"

#define DISK_NAME "sfs_will_guthrie.disk"
#define MAGIC_NUMBER 0xACBD0005

#define NUM_BLOCKS 1024  //Max number of blocks
#define BLOCK_SIZE 1024
#define NUM_INODES 100  //Max number of files
#define ROOT_INODE 0
#define MAX_FILE_SIZE BLOCK_SIZE * ((BLOCK_SIZE / sizeof(int)) + 12)

#define BITMAP_SIZE (NUM_BLOCKS / (sizeof(int) * 8))  // Calculates size of int array to have a bit for each block
#define INODE_TABLE_SIZE (NUM_INODES / (sizeof(int) * 8))  // Calculates size of int array to have a bit for each inode
#define SetBit(A,k)     ( A[(k/32)] |= (1 << (k%32)) )
#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) )
#define TestBit(A,k)    ( A[(k/32)] & (1 << (k%32)) )

inode_t inode_table[NUM_INODES];  // Hold all inodes in memory
int inode_status_table[INODE_TABLE_SIZE];  // For each bit, 1 = occupied, 0 = not occupied
int block_bitmap[BITMAP_SIZE];  // For each bit, 1 = occupied, 0 = not occupied

file_descriptor fd_table[NUM_INODES];  // Holds inode index, pointer and r/w pointer for each file
directory_entry root_dir[NUM_INODES];  // Holds inode number and file name for each file
int current_file_inode_num;  // Tracks the inode number of the current file in directory

superblock_t superblock;

int indirect_block[BLOCK_SIZE / sizeof(int)];

void init_inode_status_table() {
    // Must initialize bitmap to all zeros before use
    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        inode_status_table[i] = 0;
    }
}

void init_bitmap_status_table(){
    // Must initialize bitmap to all zeros before use
    for (int i = 0; i < BITMAP_SIZE; i++){
        block_bitmap[i] = 0;
    }
}

void set_inode(int inode_num, int mode, int link_cnt, int uid, int gid, int file_size,
        const int direct_ptrs[12], int indirect_ptr) {
    inode_table[inode_num].mode = mode;
    inode_table[inode_num].link_cnt = link_cnt;
    inode_table[inode_num].uid = uid;
    inode_table[inode_num].gid = gid;
    inode_table[inode_num].file_size = file_size;
    inode_table[inode_num].indirect_ptr = indirect_ptr;

    for (int i = 0; i < 12; i++) {
        inode_table[inode_num].direct_ptrs[i] = direct_ptrs[i];
    }

    SetBit(inode_status_table, inode_num);
}

void rm_inode(int inode_num) {
    inode_table[inode_num].mode = -1;
    inode_table[inode_num].link_cnt = -1;
    inode_table[inode_num].uid = -1;
    inode_table[inode_num].gid = -1;
    inode_table[inode_num].file_size = -1;
    inode_table[inode_num].indirect_ptr = -1;

    for (int i = 0; i < 12; i++){
        inode_table[inode_num].direct_ptrs[i] = -1;
    }

    ClearBit(inode_status_table, inode_num);
}

void init_super(){
    superblock.magic_number = MAGIC_NUMBER;
    superblock.block_size = BLOCK_SIZE;
    superblock.sfs_size = NUM_BLOCKS;
    superblock.inode_table_len = NUM_INODES;
    superblock.root_dir_inode_ptr = ROOT_INODE;
}

void init_inode_table(){
    for (int i = 0; i < NUM_INODES; i++){
        rm_inode(i);
    }
}

void init_file_descriptor_table(){
    for (int i = 0; i < NUM_INODES; i++){
        fd_table[i].inode_index = -1;
    }
}

void init_root(){
    for (int i = 0; i < NUM_INODES; i++){
        root_dir[i].inode_num = -1;
        for (int j = 0; j < (MAX_FILENAME_LEN + MAX_EXTENSION_LEN + 1); j++){
            root_dir[i].name[j] = '\0';
        }
    }
}

int calc_inode_table_blocks() {
    int numInodeBlocks = (sizeof(inode_table)/BLOCK_SIZE);
    if (sizeof(inode_table) % BLOCK_SIZE != 0) numInodeBlocks++;
    return numInodeBlocks;
}

int calc_root_dir_blocks() {
    int num_root_dir_blocks = sizeof(root_dir)/BLOCK_SIZE;
    if (sizeof(root_dir) % BLOCK_SIZE != 0) num_root_dir_blocks++;
    return num_root_dir_blocks;
}

int get_file_inode(char* path) {
    char* buffer = malloc(sizeof(char) * (MAX_FILENAME_LEN + MAX_EXTENSION_LEN + 1));

    for (int i = 0; i < NUM_INODES; i++) {
        if (root_dir[i].inode_num != -1) {
            strcpy(buffer, root_dir[i].name);
            if (strcmp(buffer, path) == 0){
                free(buffer);
                return root_dir[i].inode_num;
            }
        }
    }
    free(buffer);
    return -1;
}


void mksfs(int fresh) {
    if (fresh == 1) {
        init_bitmap_status_table();
        init_inode_status_table();
        init_inode_table();
        init_file_descriptor_table();
        init_root();
        init_super();
        current_file_inode_num = 0;

        init_fresh_disk(DISK_NAME, BLOCK_SIZE, NUM_BLOCKS);


        // Set super block as taken and write it to disk
        SetBit(block_bitmap, 0);
        write_blocks(0, 1, &superblock);

        // Get number of blocks required for inode table and root directory
        int num_inode_table_blocks = calc_inode_table_blocks();
        int num_root_dir_blocks = calc_root_dir_blocks();

        // Create inode for rootdir
        // TODO: Should this not just loop for i<12?
        int root_data_ptrs[12];
        for (int i = 0; i < num_root_dir_blocks; i++) {
            root_data_ptrs[i] = i + num_inode_table_blocks + 1;
        }

        set_inode(ROOT_INODE, 0, num_root_dir_blocks, 0, 0, -1, root_data_ptrs, -1);

        // Allocate blocks for inode_table and write it
        for (int i = 1; i < num_inode_table_blocks + 1; i++) {
            SetBit(block_bitmap, i);
        }
        write_blocks(1, num_inode_table_blocks, &inode_table);

        // Allocate blocks for root_dir and write it
        void* root_buffer = malloc(BLOCK_SIZE * num_root_dir_blocks);
        memcpy(root_buffer, &root_dir, sizeof(root_dir));
        for (int i = num_inode_table_blocks + 1; i < num_root_dir_blocks + (num_inode_table_blocks + 1); i++) {
            SetBit(block_bitmap, i);
        }
        write_blocks(inode_table[ROOT_INODE].direct_ptrs[0], num_root_dir_blocks, root_buffer);
        free(root_buffer);

        // Allocate block and write inode_table_bitmap
        SetBit(block_bitmap, NUM_BLOCKS - 2);
        write_blocks(NUM_BLOCKS - 2, 1, &inode_status_table);

        // Allocate block and write bitmap
        SetBit(block_bitmap, NUM_BLOCKS - 1);
        write_blocks(NUM_BLOCKS - 1, 1, &block_bitmap);

    } else {  // If opening a previously created filesystem
        init_file_descriptor_table();
        init_disk(DISK_NAME, BLOCK_SIZE, NUM_BLOCKS);
        current_file_inode_num = 0;

        // Read superblock
        void* buffer = malloc(BLOCK_SIZE);
        read_blocks(0, 1, buffer);
        memcpy(&superblock, buffer, sizeof(superblock_t));
        free(buffer);

        // Read the inode table into memory
        int numInodeBlocks = calc_inode_table_blocks();
        buffer = malloc(BLOCK_SIZE * numInodeBlocks);
        read_blocks(1, numInodeBlocks, buffer);
        memcpy(&inode_table, buffer, sizeof(inode_table));
        free(buffer);

        // Read the inode table status into memory
        buffer = malloc(BLOCK_SIZE);
        read_blocks(NUM_BLOCKS - 2, 1 , buffer);
        memcpy(&inode_status_table, buffer, sizeof(inode_status_table));
        free(buffer);

        // Read the bitmap into memory
        buffer = malloc(BLOCK_SIZE);
        read_blocks(NUM_BLOCKS - 1, 1, buffer);
        memcpy(&block_bitmap, buffer, sizeof(block_bitmap));
        free(buffer);

        // Read the root directory into memory
        buffer = malloc(BLOCK_SIZE * (inode_table[ROOT_INODE].link_cnt));
        read_blocks(inode_table[ROOT_INODE].direct_ptrs[0], inode_table[ROOT_INODE].link_cnt, buffer);
        memcpy(&root_dir, buffer, sizeof(root_dir));
        free(buffer);
    }
}

int sfs_getnextfilename(char *fname) {
    if (current_file_inode_num < NUM_INODES) {
        while (root_dir[current_file_inode_num].inode_num == -1) {
            current_file_inode_num++;
            if (current_file_inode_num >= NUM_INODES) {
                current_file_inode_num = 0;
                return 0;
            }
        }
        strcpy(fname, root_dir[current_file_inode_num].name);
        current_file_inode_num++;
        return 1;
    } else {
        current_file_inode_num = 0;
        return 0;
    }
}

int sfs_getfilesize(const char* path) {
    int file_inode = get_file_inode(path);
    if (file_inode != -1) {
        return inode_table[file_inode].file_size;
    } else {
        return -1;
    }
}

int sfs_fopen(char *name) {
    if (strlen(name) > MAX_FILENAME_LEN + MAX_EXTENSION_LEN + 1) {
        return -1;
    }

    int first_open_file_desc = -1;
    for (int i = 0; i < NUM_INODES; i++) {
        if (fd_table[i].inode_index == -1) {
            first_open_file_desc = i;
            break;
        }
    }

    if (first_open_file_desc != -1) {
        int file_inode = get_file_inode(name);
        if (file_inode != -1) {  // File already exists

            // Check if file already open
            for (int i = 0; i < NUM_INODES; i++){
                if (fd_table[i].inode_index == file_inode){
                    return -1;
                }
            }

            // Open file
            fd_table[first_open_file_desc].inode_index = file_inode;
            fd_table[first_open_file_desc].inode = &(inode_table[file_inode]);
            fd_table[first_open_file_desc].w_ptr = inode_table[file_inode].file_size;  // Open in append mode
            fd_table[first_open_file_desc].r_ptr = inode_table[file_inode].file_size;

            return first_open_file_desc;
        } else {  // File does not exist

            // Get first open inode
            int first_open_inode = -1;
            for (int i = 1; i < NUM_INODES; i++) {
                if (!TestBit(inode_status_table, i)) {
                    first_open_inode = i;
                    SetBit(inode_status_table, i);
                    break;
                }
            }

            // If there are no free inodes, fail
            if (first_open_inode == -1){
                return -1;
            }

            // Get new dir entry
            int first_open_in_root_dir = -1;
            for (int i = 0; i < NUM_INODES; i++){
                if (root_dir[i].inode_num == -1) {
                    first_open_in_root_dir = i;
                    break;
                }
            }

            // If root dir is full, fail
            if (first_open_in_root_dir == -1){
                return -1;
            }


            // Select first empty block
            int first_empty_block = -1;
            for (int i = 0; i < NUM_BLOCKS; i++) {
                if (!TestBit(block_bitmap, i)) {
                    first_empty_block = i;
                    SetBit(block_bitmap, i);
                    break;
                }
            }

            // If no empty blocks, fail
            if (first_empty_block == -1){
                return -1;
            }

            // Set up direct access blocks
            int data_ptrs[12];
            data_ptrs[0] = first_empty_block;
            for (int i = 1; i < 12; i++){
                data_ptrs[i] = -1;
            }

            fd_table[first_open_file_desc].inode_index = first_open_inode;

            root_dir[first_open_in_root_dir].inode_num = first_open_inode;
            strcpy(root_dir[first_open_in_root_dir].name, name);

            // Set up the inode
            set_inode(first_open_inode, 0, 1, 0, 0, 0, data_ptrs, -1);
            fd_table[first_open_file_desc].inode = &(inode_table[first_open_inode]);
            fd_table[first_open_file_desc].w_ptr = inode_table[first_open_inode].file_size;
            fd_table[first_open_file_desc].r_ptr = inode_table[first_open_inode].file_size;

            // Get size of root dir
            int num_root_dir_blocks = (sizeof(root_dir)/BLOCK_SIZE);
            if (sizeof(root_dir) % BLOCK_SIZE != 0){
                num_root_dir_blocks += 1;
            }

            // Write root dir
            void *buffer = malloc(BLOCK_SIZE * num_root_dir_blocks);
            memcpy(buffer, &root_dir, sizeof(root_dir));
            write_blocks(inode_table[ROOT_INODE].direct_ptrs[0], num_root_dir_blocks, buffer);
            free(buffer);

            inode_table[ROOT_INODE].file_size += 1;

            // Write inode table
            int numInodeBlocks = calc_inode_table_blocks();
            write_blocks(1, numInodeBlocks, &inode_table);

            // Write inode status
            write_blocks(1022, 1, &inode_status_table);

            // Write bitmap
            write_blocks(1023, 1, &block_bitmap);

            return first_open_file_desc;
        }
    } else {
        return -1;
    }
}

int sfs_fclose(int fileID) {
    if (fd_table[fileID].inode_index == -1) {
        return -1;
    } else {
        fd_table[fileID].inode_index = -1;
        return 0;
    }
}

int sfs_frseek(int fileID, int loc) {
    if (inode_table[fd_table[fileID].inode_index].file_size < loc) {
        return -1;
    } else {
        fd_table[fileID].r_ptr = loc;
        return 0;
    }
}

int sfs_fwseek(int fileID, int loc) {
    if (inode_table[fd_table[fileID].inode_index].file_size < loc) {
        return -1;
    } else {
        fd_table[fileID].w_ptr = loc;
        return 0;
    }
}

int sfs_fwrite(int fileID, char *buf, int length) {
    if (fileID < 0) {return -1;}  // not valid file ID
    if (length <= 0) {return length;}  // nothing to write

    int inode_to_write = fd_table[fileID].inode_index;
    int bytes_to_write = length;

    if (inode_to_write == -1) {return -1;}  // file not in fd_table

    //TODO: Deviation
    int required_bytes = inode_table[inode_to_write].file_size + length;
    if (required_bytes > MAX_FILE_SIZE) {
        required_bytes = MAX_FILE_SIZE;
        bytes_to_write = MAX_FILE_SIZE - inode_table[inode_to_write].file_size;
    }

    int blocks_needed = required_bytes / BLOCK_SIZE;
    if (required_bytes % BLOCK_SIZE != 0) required_bytes++;

    int blocks_to_add = blocks_needed - inode_table[inode_to_write].link_cnt;

    // If file is larger than 12 blocks, load indirect pointer
    void* buffer = malloc(BLOCK_SIZE);
    if (inode_table[inode_to_write].link_cnt > 12){
        read_blocks(inode_table[inode_to_write].indirect_ptr, 1, buffer);
        memcpy(&indirect_block, buffer, BLOCK_SIZE);
    }
    // Else if file will become larger than 12 blocks, create indirect pointer
    else if (inode_table[inode_to_write].link_cnt + blocks_to_add > 12) {
        int indirect_ptr = -1;
        for (int i = 0; i < NUM_BLOCKS; i++) {
            if(!TestBit(block_bitmap, i)){
                indirect_ptr = i;
                SetBit(block_bitmap, i);
                break;
            }
        }
        inode_table[inode_to_write].indirect_ptr = indirect_ptr;
    }
    free(buffer);

    if (blocks_to_add > 0) {
        // Add new blocks needed
        for (int i = inode_table[inode_to_write].link_cnt; i < blocks_needed; i++) {
            int new_block = -1;
            for (int j = 0; j < NUM_BLOCKS; j++) {
                if(!TestBit(block_bitmap, j)){
                    new_block = j;
                    SetBit(block_bitmap, j);
                    break;
                }
            }
            // No space found
            if (new_block == -1) {
                return -1;
            } else {
                if (i >= 12) {
                    indirect_block[i - 12] = new_block;
                } else {
                    inode_table[inode_to_write].direct_ptrs[i] = new_block;
                }
            }
        }
    } else {
        blocks_to_add = 0;
    }

    int starting_block = fd_table[fileID].w_ptr / BLOCK_SIZE;
    int offset = fd_table[fileID].w_ptr % BLOCK_SIZE;

    // Load file with extra space for write
    buffer = malloc(BLOCK_SIZE * blocks_needed);
    //TODO: Small diff
    for (int i = starting_block; i < blocks_needed; i++) {
        if (i >= 12) {
            read_blocks(indirect_block[i - 12], 1, (buffer + (i-starting_block) * BLOCK_SIZE));
        } else {
            read_blocks(inode_table[inode_to_write].direct_ptrs[i], 1, (buffer + (i-starting_block) * BLOCK_SIZE));
        }
    }

    memcpy((buffer + offset), buf, bytes_to_write);

    // Write file back to disk
    for (int i = starting_block; i < blocks_needed; i++) {
        if (i >= 12) {
            write_blocks(indirect_block[i - 12], 1, (buffer + (i - starting_block) * BLOCK_SIZE));
        } else {
            write_blocks(inode_table[inode_to_write].direct_ptrs[i], 1, (buffer + (i-starting_block) * BLOCK_SIZE));
        }
    }

    free(buffer);

    // Update file system stats
    if (inode_table[inode_to_write].file_size < required_bytes) {
        inode_table[inode_to_write].file_size = required_bytes;
    }

    inode_table[inode_to_write].link_cnt += blocks_to_add;
    fd_table[fileID].w_ptr = required_bytes;

    if (inode_table[inode_to_write].link_cnt > 12) {
        write_blocks(inode_table[inode_to_write].indirect_ptr, 1, &indirect_block);
    }

    // Write new stats to disk
    int size_of_inodes = calc_inode_table_blocks();
    write_blocks(1, size_of_inodes, &inode_table);

    write_blocks(NUM_BLOCKS - 2, 1, &inode_status_table);

    write_blocks(NUM_BLOCKS - 1, 1, &block_bitmap);

    return bytes_to_write;
}

int sfs_fread(int fileID, char *buf, int length) {
    if (fileID < 0) return -1;

    int inode_to_read = fd_table[fileID].inode_index;
    int bytes_to_read, eof;

    if (inode_to_read == -1) return -1;  // File not found
    if (inode_table[inode_to_read].file_size <= 0) return 0;  // Nothing to read
    if (inode_table[inode_to_read].file_size < fd_table[fileID].r_ptr + length) {  // If we've asked for more bytes than is left
        bytes_to_read = inode_table[inode_to_read].file_size - fd_table[fileID].r_ptr;
        eof = inode_table[inode_to_read].file_size / BLOCK_SIZE;
        if ((inode_table[inode_to_read].file_size % BLOCK_SIZE) != 0) { eof++; }
    } else {
        bytes_to_read = length;
        eof = (fd_table[fileID].r_ptr + length) / BLOCK_SIZE;
        if (((fd_table[fileID].r_ptr + length) % BLOCK_SIZE) != 0) { eof++; }
    }

    int first_block = fd_table[fileID].r_ptr / BLOCK_SIZE;
    int read_offset = fd_table[fileID].r_ptr % BLOCK_SIZE;

    // If indirect pointer exists, load it
    void* buffer = malloc(BLOCK_SIZE);
    if (inode_table[inode_to_read].link_cnt > 12) {
        read_blocks(inode_table[inode_to_read].indirect_ptr, 1, buffer);
        memcpy(&indirect_block, buffer, BLOCK_SIZE);
    }
    free(buffer);

    // Load file
    buffer = malloc(BLOCK_SIZE * eof);
    for (int i = first_block; i < inode_table[inode_to_read].link_cnt && i < eof; i++) {
        if (i >= 12) {
            read_blocks(indirect_block[i-12], 1, (buffer + (i - first_block) * BLOCK_SIZE));
        } else {
            read_blocks(inode_table[inode_to_read].direct_ptrs[i], 1, (buffer + (i - first_block) * BLOCK_SIZE));
        }
    }

    // Get desired data
    memcpy(buf, (buffer + read_offset), bytes_to_read);

    // Move read ptr
    fd_table[fileID].r_ptr += bytes_to_read;

    free(buffer);

    return bytes_to_read;
}

int sfs_remove(char *file) {
    int inode_to_remove = get_file_inode(file);
    if (inode_to_remove > 0) {

        // Free all data blocks
        if (inode_table[inode_to_remove].link_cnt <= 12) {
            for (int i = 0; i < inode_table[inode_to_remove].link_cnt; i++) {
                ClearBit(block_bitmap, inode_table[inode_to_remove].direct_ptrs[i]);
            }
        } else { // Also have to free indirect pointer blocks
            void* buffer = malloc(BLOCK_SIZE);
            read_blocks(inode_table[inode_to_remove].indirect_ptr, 1, buffer);
            memcpy(&indirect_block, buffer, BLOCK_SIZE);

            for (int i = 0; i < inode_table[inode_to_remove].link_cnt; i++) {
                if (i < 12) {
                    ClearBit(block_bitmap, inode_table[inode_to_remove].direct_ptrs[i]);
                } else {
                    ClearBit(block_bitmap, indirect_block[i-12]);
                }
            }

            free(buffer);
        }

        rm_inode(inode_to_remove);

        // Remove directory entry
        for (int i = 0; i < NUM_INODES; i++) {
            if (strcmp(root_dir[i].name, file) == 0) {
                root_dir[i].inode_num = -1;
                for (int j = 0; j < MAX_FILENAME_LEN + MAX_EXTENSION_LEN + 1; j++) {
                    root_dir[i].name[j] = '\0';
                }
                break;
            }
        }

        inode_table[ROOT_INODE].file_size--;

        int size_of_root_dir = sizeof(root_dir)/BLOCK_SIZE;
        if (sizeof(root_dir) % BLOCK_SIZE != 0) size_of_root_dir++;

        // Write new root dir to disk
        void *buffer = malloc(BLOCK_SIZE * size_of_root_dir);
        memcpy(buffer, &root_dir, sizeof(root_dir));
        write_blocks(inode_table[ROOT_INODE].direct_ptrs[0], size_of_root_dir, buffer);
        free(buffer);

        int inode_table_size = calc_inode_table_blocks();
        // Write the inode table to disk
        write_blocks(1, inode_table_size, &inode_table);

        // Write the inode status to disk
        write_blocks(1022, 1, &inode_status_table);

        // Write bitmap to disk
        write_blocks(1023, 1, &block_bitmap);

        return 0;
    } else {
        return -1;
    }
}