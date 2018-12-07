#include "shmfifo.h"
#include "semtool.h"

#include <assert.h>

shmfifo_t *shmfifo_init (int key, int blksize, int blocks)
{
    auto *fifo = (shmfifo_t *) malloc(sizeof(shmfifo_t));
    assert(fifo != NULL);
    memset(fifo, 0, sizeof(shmfifo_t));

    int shmid;
    shmid = shmget(key, 0, 0);
    int size = sizeof(shmhead_t) + blksize * blocks;
    if (shmid == - 1)
    {
        fifo->shmid = shmget(key, static_cast<size_t>(size), IPC_CREAT | 0666);
        if (fifo->shmid == - 1)
            ERR_EXIT("shmget");

        fifo->p_shm = (shmhead_t *) shmat(fifo->shmid, nullptr, 0);
        if (fifo->p_shm == (shmhead_t *) - 1)
            ERR_EXIT("shmat");

        fifo->p_payload = (char *) (fifo->p_shm + 1);

        fifo->p_shm->blksize = static_cast<unsigned int>(blksize);
        fifo->p_shm->blocks = static_cast<unsigned int>(blocks);
        fifo->p_shm->rd_index = 0;
        fifo->p_shm->wr_index = 0;


//        fifo->sem_mutex = sem_create(key);
//        fifo->sem_full = sem_create(key + 1);
//        fifo->sem_empty = sem_create(key + 2);

        fifo->sem_mutex = getSemaphore(key);
        fifo->sem_full = getSemaphore(key + 1);
        fifo->sem_empty = getSemaphore(key + 2);

        sem_setval(fifo->sem_mutex, 1);
        sem_setval(fifo->sem_full, blocks);
        sem_setval(fifo->sem_empty, 0);

        printf("fifo init\n");
    } else
    {
        fifo->shmid = shmid;
        fifo->p_shm = (shmhead_t *) shmat(fifo->shmid, nullptr, 0);
        if (fifo->p_shm == (shmhead_t *) - 1)
            ERR_EXIT("shmat");

        fifo->p_payload = (char *) (fifo->p_shm + 1);

        fifo->sem_mutex = getSemaphore(key);
        fifo->sem_full = getSemaphore(key + 1);
        fifo->sem_empty = getSemaphore(key + 2);

        sem_setval(fifo->sem_mutex, 1);
        sem_setval(fifo->sem_full, blocks);
        sem_setval(fifo->sem_empty, 0);

        printf("fifo existed\n");
    }
    fifo->p_shm->wr_index = 0;
    fifo->p_shm->rd_index = 0;
//    printf("init wr idx=%d    rd idx = %d\n", fifo->p_shm->wr_index, fifo->p_shm->rd_index);
    return fifo;
}

void shmfifo_put (shmfifo_t *fifo, const void *buf)
{
    sem_p(fifo->sem_full);
    sem_p(fifo->sem_mutex);

    memcpy(fifo->p_payload + fifo->p_shm->blksize * fifo->p_shm->wr_index,
           buf, fifo->p_shm->blksize);
    fifo->p_shm->wr_index = (fifo->p_shm->wr_index + 1) % fifo->p_shm->blocks;
//    printf("put wr idx=%d    rd idx = %d\n", fifo->p_shm->wr_index, fifo->p_shm->rd_index);
    sem_v(fifo->sem_mutex);
    sem_v(fifo->sem_empty);
}

void shmfifo_get (shmfifo_t *fifo, void *buf)
{
    sem_p(fifo->sem_empty);
    sem_p(fifo->sem_mutex);

    memcpy(buf, fifo->p_payload + fifo->p_shm->blksize * fifo->p_shm->rd_index,
           fifo->p_shm->blksize);
    fifo->p_shm->rd_index = (fifo->p_shm->rd_index + 1) % fifo->p_shm->blocks;
//    printf("get wr idx=%d    rd idx = %d\n", fifo->p_shm->wr_index, fifo->p_shm->rd_index);
    sem_v(fifo->sem_mutex);
    sem_v(fifo->sem_full);
}

void shmfifo_destroy (shmfifo_t *fifo)
{
//    sem_d(fifo->sem_mutex);
//    sem_d(fifo->sem_full);
//    sem_d(fifo->sem_empty);

    shmdt(fifo->p_shm);
    shmctl(fifo->shmid, IPC_RMID, nullptr);
    free(fifo);
    fifo = nullptr;
}
