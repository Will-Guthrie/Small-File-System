#include <glob.h>
#include <stdint-gcc.h>

#ifndef COMP_310_FILE_SYSTEM_SFS_API_H
#define COMP_310_FILE_SYSTEM_SFS_API_H

//TODO: Maybe move this to bottom
#endif //COMP_310_FILE_SYSTEM_SFS_API_H

#define MAXFILENAME 16
#define MAX_FNAME_LENGTH MAXFILENAME
#define MAX_FILENAME_LEN 16
#define MAX_EXTENSION_LEN 3
#define NUM_BLOCKS 1024

//TODO: Choose datatypes here, is uint64 needed?
typedef struct superblock_t {
    uint64_t magic_number;
    uint64_t block_size;
    uint64_t sfs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode_ptr;
} superblock_t;

//TODO: Maybe remove unsigned?
typedef struct inode_t {
    unsigned int mode;
    unsigned int link_cnt;
    unsigned int uid;
    unsigned int gid;
    unsigned int file_size;
    unsigned int direct_ptrs[12];
    unsigned int indirect_ptr;
} inode_t;

typedef struct file_descriptor {
    uint64_t inode_index;
    inode_t* inode;
    uint64_t r_ptr;
    uint64_t w_ptr;
} file_descriptor;

typedef struct directory_entry{
    int inode_num;
    char name[MAX_FILENAME_LEN + MAX_EXTENSION_LEN + 1];
} directory_entry;

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_frseek(int fileID,
               int loc);
int sfs_fwseek(int fileID,
               int loc);
int sfs_fwrite(int fileID,
               char *buf, int length);
int sfs_fread(int fileID,
              char *buf, int length);
int sfs_remove(char *file);
