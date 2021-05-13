#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include "simplefs.h"

#define USED 1
#define NOTUSED 0
#define FCB_COUNT 128
#define FILENAME 110
#define DIR_ENTRY_SIZE 128
#define FCB_SIZE 128

/**
    FUNCTION DECLARATIONS
**/
int find_zero_byte(uint8_t bitmap_byte);
int find_available_block();
int FCB_index_to_block(int FCB_index);
uint8_t deleted_result( uint8_t cur_byte, int bit_map_offset);
int delete_block(int block_number);
int a;
// Global Variables =======================================
int vdisk_fd; // Global virtual disk file descriptor. Global within the library.
              // Will be assigned with the vsfs_mount call.
              // Any function in this file can use this.
              // Applications will not use  this directly. EYV

struct OpenFile {
    char file_name[FILENAME];
    int used;
    int mode;
    int FCB_index;
    int index_table_addr;
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
    int file_size;
    char dummy[FCB_SIZE - (sizeof(int) * 4)];
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

    struct FCB* cur_FCB = (struct FCB*) malloc(sizeof(struct FCB) * 32);

    for(int i = 9; i <= 12; i++) {
        read_block(cur_FCB, i);
        for( int j = 0; j < 32; j++) {
            cur_FCB[j].FCB_index = ((i-9) * 32) + j;
        }
        write_block(cur_FCB, i);
    }

    free(cur_FCB);

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

    if(vdisk_fd == -1)
        return -1;

    for( int i = 0; i < 16; i++) {
        open_files[i].used = NOTUSED;
    }
    return(0);
}


// already implemented
int sfs_umount ()
{
    fsync (vdisk_fd); // copy everything in memory to disk
    close (vdisk_fd);
    return (0);
}


//inş bitti
int sfs_create(char *filename)
{
    void* dir_buffer = malloc(sizeof(char) * BLOCKSIZE);
    void* fcb_buffer = malloc(sizeof(char) * BLOCKSIZE);
    struct DirectoryEntry cur_dir_entry;
    struct FCB cur_fcb;
    bool found = false;
    int i, j, k;

    for(i = 5; i <= 8; ++i){
        read_block(dir_buffer, i);
        for(j = 0; j < 32; ++j){
            cur_dir_entry = ((struct DirectoryEntry*) dir_buffer)[j];
            if( cur_dir_entry.used == NOTUSED) {
                strcpy(cur_dir_entry.file_name, filename);
                cur_dir_entry.used = USED;
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

    found = false;
    for(k = 9; k <= 12; ++k) {
        read_block(fcb_buffer, k);
        for(int m = 0; m < 32; ++m){
            cur_fcb = ((struct FCB*) fcb_buffer)[m];
            if( cur_fcb.used == NOTUSED){

                cur_fcb.used = USED;
                cur_fcb.index_table_addr = find_available_block();

                if(cur_fcb.index_table_addr == -1) {
                    return -1;
                }

                cur_dir_entry.FCB_index = cur_fcb.FCB_index;
                cur_fcb.file_size = 0;

                ((struct DirectoryEntry*) dir_buffer)[j] = cur_dir_entry;
                ((struct FCB*) fcb_buffer)[m] = cur_fcb;

                write_block(dir_buffer, i);
                write_block(fcb_buffer, k);
                found = true;
                break;
            }
        }
        if(found)
            break;
    }

    if(!found){
        fprintf(stderr, "%s\n", "sfs_create not found error");
        return -1;
    }

    free(dir_buffer);
    free(fcb_buffer);
    return (0);
}

//inş doğrudur
int find_available_block(){
    uint8_t* bitmap_buf = (uint8_t*) malloc(BLOCKSIZE);
    int i, j, empty_block;
    bool found = false;
    int available_block_idx;

    for(i = 1; i <= 4; ++i){
        read_block(bitmap_buf, i);
        for( j = 0; j < BLOCKSIZE; ++j){
            uint8_t cur_byte = bitmap_buf[j];
            if (cur_byte != 0xFF){
                empty_block = find_zero_byte(cur_byte);
                found = true;

                struct SuperBlock* sb = (struct SuperBlock*) malloc(sizeof(struct SuperBlock));

                if( read_block(sb,0) != 0) {
                    printf("Super_block read error!!!\n");
                    free(sb);
                    return -1;
                }

                available_block_idx = ((i - 1) * BLOCKSIZE) + (8 * j) + empty_block;

                if(available_block_idx >= (sb->num_blocks - 1)) {
                    printf("No block available!!!\n");
                    free(sb);
                    return -1;
                }

                cur_byte = cur_byte | (1 << (7 - empty_block));
                bitmap_buf[j] = cur_byte;
                write_block(bitmap_buf, i);
                free(sb);
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

    available_block_idx = ((i - 1) * BLOCKSIZE) + (8 * j) + empty_block;

    free(bitmap_buf);
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
    bool found = false;
    int open_file_index;
    int i, j;

    for( open_file_index = 0; open_file_index < 16; open_file_index++) {
        if(open_files[open_file_index].used == NOTUSED) {
            found = true;
            break;
        }
    }

    if(!found)
        return -1;

    found = false;
    struct DirectoryEntry* cur_entry = (struct DirectoryEntry*) malloc(BLOCKSIZE);

    for(i = 5; i <= 8; i++) {
        read_block(cur_entry, i);
        for(j = 0; j < 32; j++) {
            if(cur_entry[j].used == USED && strcmp(cur_entry[j].file_name, file) == 0) {
                found = true;
                break;
            }
        }
        if(found)
            break;
    }

    if(!found)
        return -1;

    int FCB_index = cur_entry[j].FCB_index;

    int FCB_block = FCB_index_to_block(FCB_index);
    int FCB_block_index = FCB_index % 32;

    struct FCB* cur_FCB = (struct FCB*) malloc(BLOCKSIZE);

    read_block(cur_FCB, FCB_block);
    int table_addr = cur_FCB[FCB_block_index].index_table_addr;

    open_files[open_file_index].index_table_addr = table_addr;
    open_files[open_file_index].FCB_index = FCB_index;
    open_files[open_file_index].used = USED;
    open_files[open_file_index].mode = mode;
    open_files[open_file_index].read_index = 0;
    strcpy(open_files[open_file_index].file_name, file);

    free(cur_entry);
    free(cur_FCB);
    return open_file_index;
}

int sfs_close(int fd) {

    if(open_files[fd].used == NOTUSED)
        return -1;

    open_files[fd].used = NOTUSED;
    return (0);
}

int sfs_getsize (int  fd)
{
    int FCB_index = open_files[fd].FCB_index;

    int FCB_block = FCB_index_to_block(FCB_index);
    int FCB_block_index = FCB_index % 32;

    struct FCB* cur_FCB = (struct FCB*) malloc(BLOCKSIZE);

    if(read_block(cur_FCB, FCB_block) != 0) {
        fprintf(stderr, "%s\n", "Block read err");
        return -1;
    }

    int size = cur_FCB[FCB_block_index].file_size;

    free(cur_FCB);
    return size;
}

int sfs_read(int fd, void *buf, int n) {
    int i = 0;

    if(open_files[fd].used == NOTUSED || open_files[fd].mode == MODE_APPEND) {
        fprintf(stderr, "%s\n", "WRONG format read!");
        return -1;
    }

    struct OpenFile cur_file = open_files[fd];

    int FCB_index = cur_file.FCB_index;

    int FCB_block = FCB_index_to_block(FCB_index);
    int FCB_block_index = FCB_index % 32;

    struct FCB* cur_FCB = (struct FCB*) malloc(BLOCKSIZE);

    if(read_block(cur_FCB, FCB_block) != 0) {
        fprintf(stderr, "%s\n", "Block read err");
        return -1;
    }

    int size = cur_FCB[FCB_block_index].file_size;
    int index_addr = cur_FCB[FCB_block_index].index_table_addr;

    int* indices = (int*) malloc(sizeof(int) * 1024);

    if(read_block(indices, index_addr) != 0) {
        fprintf(stderr, "%s\n", "Index read err");
        return -1;
    }

    int read_size = open_files[fd].read_index;

    int index_block = read_size / 4096;
    int offset = read_size % 4096;

    int current_block = indices[index_block];

    if(current_block == 0 || index_block >= 1024) {
        printf("END OF FILE!!!\n");
        return -1;
    }

    int read_goal = 0;

    if( n + read_size < size){
        read_goal = n;
    }
    else {
        read_goal = size - read_size;
    }

    char* cur_buf = (char*) malloc(BLOCKSIZE);

    if(read_block(cur_buf, current_block) != 0) {
        fprintf(stderr, "%s\n", "Current block read err");
        return -1;
    }

    int count = 0;

    for( i = offset; (i < (read_goal + offset)) && (i < BLOCKSIZE); i++) {
        ((char*) buf)[count] = cur_buf[i];
        count++;
    }

    while(count != read_goal) {

        index_block++;

        if(index_block >= 1024) {
            printf("File size limit is reached!!\n");
            open_files[fd].read_index += count;
            return count;
        }

        current_block = indices[index_block];

        if(current_block == 0) {
            return -1;
        }

        if(read_block(cur_buf, current_block) != 0) {
            fprintf(stderr, "%s\n", "Current block read err");
            return -1;
        }
        for(int i = 0; i < BLOCKSIZE; i++) {

            ((char*) buf)[count] = cur_buf[i];
            count++;

            if(count == read_goal)
                break;
        }
    }

    open_files[fd].read_index += count;
    
    free(cur_FCB);
    free(cur_buf);
    free(indices);
    return count;
}

//DEĞİŞECEKLER
//EĞER YENİ BLOCK KULLANILIRSA BİTMAP VE INDEX_NODE DEĞİŞECEK
//SIZE ARTACAK
//BLOCKLARA N KADAR BİŞEY YAZILACAK.
int sfs_append(int fd, void *buf, int n) {

    int i;

    if(open_files[fd].used == NOTUSED || open_files[fd].mode == MODE_READ) {
        fprintf(stderr, "%s\n", "WRONG format append!");
        return -1;
    }


    struct OpenFile cur_file = open_files[fd];

    int FCB_index = cur_file.FCB_index;

    int FCB_block = FCB_index_to_block(FCB_index);
    int FCB_block_index = FCB_index % 32;

    struct FCB* cur_FCB = (struct FCB*) malloc(BLOCKSIZE);

    if(read_block(cur_FCB, FCB_block) != 0) {
        fprintf(stderr, "%s\n", "Block read err");
        return -1;
    }

    int size = cur_FCB[FCB_block_index].file_size;
    int index_addr = cur_FCB[FCB_block_index].index_table_addr;

    int* indices = (int*) malloc(sizeof(int) * 1024);

    if(read_block(indices, index_addr) != 0) {
        fprintf(stderr, "%s\n", "Index read err");
        return -1;
    }

    int index_block = size / 4096;
    int offset = size % 4096;

    int current_block = indices[index_block];

    if(current_block == 0) {
        current_block = find_available_block();

        if(current_block == -1) {
            return -1;
        }

        indices[index_block] = current_block;
        write_block(indices, index_addr);
    }

    char* cur_buf = (char*) malloc(BLOCKSIZE);

    if(read_block(cur_buf, current_block) != 0) {
        fprintf(stderr, "%s\n", "Current block read err");
        return -1;
    }
    //offset + n <= 4096 direk offsetten yaz
    //offset + n > 4096
    char* user_buffer = (char*) buf;
    int count = 0;

    for( i = offset; (i < (n + offset)) && (i < BLOCKSIZE); i++) {
        cur_buf[i] = user_buffer[count];
        count++;
    }

    //size += count;

    if(write_block(cur_buf, current_block) != 0) {
        fprintf(stderr, "%s\n", "Current block write err");
        return -1;
    }

    while(count != n) {
        current_block = find_available_block();

        if(current_block == -1) {
            return -1;
        }

        index_block++;

        if(index_block >= 1024) {
            printf("File size limit is reached!!\n");
            return count;
        }

        indices[index_block] = current_block;
        write_block(indices, index_addr);

        if(read_block(cur_buf, current_block) != 0) {
            fprintf(stderr, "%s\n", "Current block read err");
            return -1;
        }
        for(int i = 0; i < BLOCKSIZE; i++) {
            cur_buf[i] = user_buffer[count];
            count++;

            if(count == n)
                break;
        }
    }

    size += count;

    cur_FCB[FCB_block_index].file_size = size;

    if(write_block(cur_FCB, FCB_block) != 0) {
        fprintf(stderr, "%s\n", "Current block read err");
        return -1;
    }

    free(cur_FCB);
    free(indices);
    free(cur_buf);
    return count;
}

// TODO
// index_tableın olduğu block silinecek
// index table ın her entrysinin olduğu block silenecek
// bu silenenler silinmeden önce 0 olcak
// bitmap update edilecek
int sfs_delete(char *filename) {

    int i, j;
    bool found = false;

    struct DirectoryEntry* dir_buffer = (struct DirectoryEntry*) malloc(BLOCKSIZE);
    int* indices = (int*) malloc(BLOCKSIZE);
    struct DirectoryEntry cur_dir_entry;

    for(i = 5; i <= 8; ++i){
        read_block(dir_buffer, i);
        for(j = 0; j < 32; ++j){
            cur_dir_entry = ((struct DirectoryEntry*) dir_buffer)[j];
            if( cur_dir_entry.used == USED && strcmp(cur_dir_entry.file_name, filename) == 0) {
                found = true;
                break;
            }
        }
        if(found){
            break;
        }
    }

    if(!found) {
        printf("sfs_delete, filename DNE!\n");
        return -1;
    }

    int FCB_index = cur_dir_entry.FCB_index;

    int FCB_block = FCB_index_to_block(FCB_index);
    int FCB_block_index = FCB_index % 32;

    struct FCB* cur_FCB = (struct FCB*) malloc(BLOCKSIZE);

    if(read_block(cur_FCB, FCB_block) != 0) {
        fprintf(stderr, "%s\n", "Block read err");
        return -1;
    }

    int index_table_addr = cur_FCB[FCB_block_index].index_table_addr;

    if(read_block(indices, index_table_addr) != 0) {
        fprintf(stderr, "%s\n", "Index block read err sfs_del");
        return -1;
    }

    for(int k = 0; k < (BLOCKSIZE / sizeof(int)); k++) {
        if(indices[k] == 0)
            break;
        delete_block(indices[k]);
    }

    delete_block(index_table_addr);

    cur_FCB[FCB_block_index].used = NOTUSED;
    cur_FCB[FCB_block_index].index_table_addr = 0;
    cur_FCB[FCB_block_index].file_size = 0;
    cur_dir_entry.used = NOTUSED;

    ((struct DirectoryEntry*) dir_buffer)[j] = cur_dir_entry;

    if(write_block(dir_buffer, i) != 0) {
        fprintf(stderr, "%s\n", "Directory block write err sfs_del");
        return -1;
    }

    if(write_block(cur_FCB, FCB_block) != 0) {
        fprintf(stderr, "%s\n", "FCB block write err sfs_del");
        return -1;
    }

    free(dir_buffer);
    free(indices);
    free(cur_FCB);

    return (0);
}

int delete_block(int block_number) {
    char* cur_buf = (char*) malloc(BLOCKSIZE);
    uint8_t* bit_map_buf = (uint8_t*) malloc(BLOCKSIZE);

    for(int i = 0; i < BLOCKSIZE; i++) {
        cur_buf[i] = 0;
    }

    if(write_block(cur_buf, block_number) != 0) {
        fprintf(stderr, "%s\n", "Block write err");
        return -1;
    }

    int bit_map_block_index = (block_number / (BLOCKSIZE * 8)) + 1;
    int byte_offset = (block_number % (BLOCKSIZE * 8)) / 8;
    int bit_map_offset = (block_number % (BLOCKSIZE * 8)) % 8;


    if(read_block(bit_map_buf, bit_map_block_index) != 0) {
        fprintf(stderr, "%s\n", "Block read err");
        return -1;
    }

    uint8_t cur_byte = bit_map_buf[byte_offset];

    bit_map_buf[byte_offset] = deleted_result(cur_byte, bit_map_offset);

    if(write_block(bit_map_buf, bit_map_block_index) != 0) {
        fprintf(stderr, "%s\n", "Block write err");
        return -1;
    }

    free(cur_buf);
    free(bit_map_buf);
    return 0;
}

// DOUBLE CHECK
uint8_t deleted_result( uint8_t cur_byte, int bit_map_offset) {
    uint8_t num = ((uint8_t) 1 << (7 - bit_map_offset));
    num = num ^ 0xFF;
    cur_byte = num & cur_byte;
    return cur_byte;
}

int FCB_index_to_block(int FCB_index) {

    int FCB_block = (FCB_index / 32) + 9;

    return FCB_block;

}

