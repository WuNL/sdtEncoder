#ifndef SHMFIFO_H_INCLUDED
#define SHMFIFO_H_INCLUDED


#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <memory.h>


#define ERR_EXIT(m) \
        do \
        { \
                perror(m); \
                exit(EXIT_FAILURE); \
        } while(0)

typedef struct shmHead
{
    unsigned int blksize;
    unsigned int blocks;
    unsigned int rd_index;
    unsigned int wr_index;
} shmhead_t;


typedef struct shmfifo
{
    shmhead_t *p_shm;
    char *p_payload;
    int shmid;
    int sem_mutex;
    int sem_full;
    int sem_empty;
} shmfifo_t;


shmfifo_t *shmfifo_init (int key, int blksize, int blocks);

void shmfifo_put (shmfifo_t *fifo, const void *buf);

void shmfifo_get (shmfifo_t *fifo, void *buf);

void shmfifo_destroy (shmfifo_t *fifo);

int getSemaphore (key_t semkey);

#endif // SHMFIFO_H_INCLUDED
