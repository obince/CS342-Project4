#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "simplefs.h"

int main(int argc, char **argv)
{
    int ret;
    int fd[3];
    int i;
    int c;
    char buffer[1024];
    char buffer2[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    char buffer3[8] = {9, 10, 11, 12, 13, 14, 15, 16};
    int size;
    char vdiskname[200];

    printf ("started\n");

    if (argc != 2) {
        printf ("usage: app  <vdiskname>\n");
        exit(0);
    }
    strcpy (vdiskname, argv[1]);

    ret = sfs_mount (vdiskname);
    if (ret != 0) {
        printf ("could not mount \n");
        exit (1);
    }

    printf ("creating files\n");
    sfs_create ("file1.bin");
    sfs_create ("file2.bin");
    sfs_create ("file3.bin");

    fd[0] = sfs_open("file1.bin", MODE_APPEND);
    fd[1] = sfs_open("file2.bin", MODE_APPEND);
    fd[2] = sfs_open("file3.bin", MODE_APPEND);
    int write_count = 0;
    for (i = 0; i < 1000; ++i) {
        memcpy (buffer, buffer2, 8); // just to show memcpy
        write_count += sfs_append(fd[0], (void *) buffer, 8);
        memcpy (buffer, buffer3, 8); // just to show memcpy
        write_count += sfs_append(fd[1], (void *) buffer, 8);
    }
    printf("Write Count: %d\n", write_count);
    sfs_close(fd[0]);
    sfs_close(fd[1]);

    fd[0] = sfs_open("file1.bin", MODE_READ);
    fd[1] = sfs_open("file2.bin", MODE_READ);
    size = sfs_getsize(fd[1]);
    for(int l = 0; l < 1024; ++l){
        buffer[l] = 0;
    }

    int buffer_cnt[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (i = 0; i < size; ++i) {
        int read_count = sfs_read (fd[1], (void *) buffer, 1);
        c = buffer[0];
        buffer_cnt[c - 9] += 1;
        c = c + 1;
    }

    for(int k = 0; k < 8; ++k){
        printf("Buffer_cnt[%d]: %d\n", k + 9, buffer_cnt[k]);
    }
    sfs_close (fd[0]);
    sfs_close (fd[1]);
    ret = sfs_umount();
}
