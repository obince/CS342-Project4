#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include "simplefs.h"

#define USED 1
#define NOTUSED 0
#define FCB_COUNT 128
#define FILENAME 110
#define DIR_ENTRY_SIZE 128
// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly. EYV

struct OpenFile {
    char file_name[FILENAME];
    int used;
    int mode;
    int fcb_num;
    int read_index;
};

struct SuperBlock {
    int num_blocks;
    char dummy[BLOCKSIZE-(sizeof(int))];
};

struct DirectoryEntry {
    char file_name[FILENAME];
    int FCB_index;
    int used;
    char dummy[DIR_ENTRY_SIZE - (sizeof(int)*2) - (sizeof(char)*FILENAME)];
};

struct FCB {
    int FCB_index;
    int used;
    int index_table_addr;
    char dummy[BLOCKSIZE - (sizeof(int) * 3)];
};

struct OpenFile open_files[16];
// ========================================================


// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk.
int read_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("read error\n");
	return -1;
    }
    return (0);
}

// write block k into the virtual disk.
int write_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("write error\n");
	return (-1);
    }
    return 0;
}

/**********************************************************************
   The following functions are to be called by applications directly.
***********************************************************************/

// this function is partially implemented.
int create_format_vdisk (char *vdiskname, unsigned int m)
{
    char command[1000];
    int size;
    int num = 1;
    int count;
    size  = num << m;
    count = size / BLOCKSIZE;
    //    printf ("%d %d", m, size);
    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, count);
    //printf ("executing command = %s\n", command);
    system (command);

    // now write the code to format the disk below.
    // .. your code...

    sfs_mount(vdiskname);

    struct SuperBlock* sb = (struct SuperBlock*) malloc(sizeof(struct SuperBlock));

    sb -> num_blocks = count;

    if(write_block(sb, 0) != 0){
        fprintf(stderr, "%s\n", "Superblock write err");
        return -1;
    }

    uint8_t* bitmap = (uint8_t *) malloc(sizeof(uint8_t) * BLOCKSIZE);
    for( int i = 0; i < BLOCKSIZE; ++i) {
        bitmap[i] = 0x00;
    }

    bitmap[0] = 0xFF;
    bitmap[1] = 0xF0;

    if(write_block(bitmap, 1)){
        fprintf(stderr, "%s\n", "Bitmap write err");
        return -1;
    }

    sfs_umount();

    free(sb);
    free(bitmap);
    return (0);
}


// already implemented
int sfs_mount (char *vdiskname)
{
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it.
    vdisk_fd = open(vdiskname, O_RDWR);
    return(0);
}


// already implemented
int sfs_umount ()
{
    fsync (vdisk_fd); // copy everything in memory to disk
    close (vdisk_fd);
    return (0);
}


int sfs_create(char *filename)
{
    void* dir_buffer = malloc(sizeof(char) * BLOCKSIZE);
    void* fcb_buffer = malloc(sizeof(char) * BLOCKSIZE);
    struct DirectoryEntry* cur_dir_entry;
    struct FCB* cur_fcb;
    bool found = false;
    int i, j, k;

    for(i = 5; i <= 8; ++i){
        read_block(dir_buffer, i);
        for(j = 0; j < 32; ++j){
            cur_dir_entry = ((struct DirectoryEntry*) dir_buffer)[j];
            if( cur_dir_entry->used == NOTUSED) {
                cur_dir_entry->file_name = filename;
                cur_dir_entry->used = USED;
                found = true;
                break;
            }
        }
        if(found){
            break;
        }
    }
    
    if(!found){
        fprintf(stderr, "%s\n", "sfs_create not found error");
        return -1;
    }

    for(k = 9; k <= 12; ++k) {
        read_block(fcb_buffer, k);
        for(int m = 0; m < 32; ++m){
            cur_fcb = ((struct FCB*) fcb_buffer)[m];
            if( cur_fcb->used == NOTUSED){
                cur_fcb->used = USED;
                cur_fcb->index_table_addr = find_available_block();
                cur_dir_entry->FCB_index = cur_fcb->FCB_index;
            }
        }
    }

    return (0);
}

int find_available_block(){
    uint8_t* bitmap_buf = (uint8_t*) malloc(BLOCKSIZE);
    int i, j, empty_block;
    bool found = false;
    for(i = 1; i <= 4; ++i){
        read_block(bitmap_buf, i);
        for( j = 0; j < BLOCKSIZE; ++j){
            uint8_t cur_byte = bitmap_buf[j];
            if (cur_byte != 0xFF){
                empty_block = find_zero_byte(cur_byte);
                found = true;

                cur_byte = cur_byte | (1 << (7 - empty_block));
                bitmap_buf[j] = cur_byte;
                write_block(bitmap_buf, i);
                break;
            }
        }
        if(found){
            break;
        }
    }

    if(!found){
        return -1;
    }

    int available_block_idx = ((i - 1) * BLOCKSIZE) + (8 * j) + empty_block;
    return available_block_idx;
}

int find_zero_byte(uint8_t bitmap_byte){
    bool zeros[8];
    for(int i = 0; i < 8; ++i){
        if(bitmap_byte % 2 == 0){
            zeros[i] = true;
        } else {
            zeros[i] = false;
        }
        bitmap_byte = bitmap_byte >> 1;
    }
    for(int i = 7; i >= 0; --i){
        if( zeros[i]){
            return (7 - i);
        }
    }
    return -1;
}

int sfs_open(char *file, int mode)
{
    return (0);
}

int sfs_close(int fd){
    return (0);
}

int sfs_getsize (int  fd)
{
    return (0);
}

int sfs_read(int fd, void *buf, int n){
    return (0);
}

int sfs_append(int fd, void *buf, int n)
{
    return (0);
}

int sfs_delete(char *filename)
{
    return (0);
}

