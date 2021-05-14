#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "simplefs.h"

long double durationMsec(long int sec, long int usec);

int main(int argc, char **argv)
{
    int ret;
    int fd[3];
    int i;
    int c;
    char buffer[1024];
    char buffer2[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int size;
    char vdiskname[200];
    struct timeval start;
    struct timeval end;
    long double create_elapsed, write_elapsed, read_elapsed;
    int wr_count;

    // printf ("started\n");

    if (argc != 3) {
        printf ("usage: test  <vdiskname> <write_read_count>\n");
        exit(0);
    }
    strcpy (vdiskname, argv[1]);
    wr_count = 1 << atoi(argv[2]);

    ret = sfs_mount (vdiskname);
    if (ret != 0) {
        printf ("could not mount \n");
        exit (1);
    }

    // printf ("creating files\n");

    gettimeofday(&start, NULL);
    sfs_create ("file1.bin");
    gettimeofday(&end, NULL);
    create_elapsed = durationMsec(end.tv_sec, end.tv_usec) - durationMsec(start.tv_sec, start.tv_usec);

    fd[0] = sfs_open("file1.bin", MODE_APPEND);
    int write_count = 0;

    gettimeofday(&start, NULL);
    for (i = 0; i < wr_count; ++i) {
        memcpy (buffer, buffer2, 1); // just to show memcpy
        write_count += sfs_append(fd[0], (void *) buffer, 1);
    }
    gettimeofday(&end, NULL);
    write_elapsed = durationMsec(end.tv_sec, end.tv_usec) - durationMsec(start.tv_sec, start.tv_usec);

    printf("Write Count: %d\n", write_count);
    sfs_close(fd[0]);

    fd[0] = sfs_open("file1.bin", MODE_READ);
    size = sfs_getsize(fd[0]);

    int buffer_cnt[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    gettimeofday(&start, NULL);
    int read_count = 0;
    for (i = 0; i < size; ++i) {
        read_count += sfs_read (fd[0], (void *) buffer, 1);
    }
    printf("Read count: %d\n", read_count);
    gettimeofday(&end, NULL);
    read_elapsed = durationMsec(end.tv_sec, end.tv_usec) - durationMsec(start.tv_sec, start.tv_usec);

    sfs_close (fd[0]);

    sfs_delete ("file1.bin");
    
    ret = sfs_umount();

    printf("Creation duration / Read duration / Write duration for wr_count = %d\n", wr_count);
    printf("%10.2Lf\t\t%7.2Lf\t\t%10.2Lf\n\n", create_elapsed, read_elapsed, write_elapsed);
}

long double durationMsec(long int sec, long int usec){
    long double elapsed = sec * ((long double) 1000) + usec / ((long double) 1000);
    return elapsed;
}
