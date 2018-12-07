#include <fcntl.h>
#include "semtool.h"

int getSemaphore (key_t semkey)
{
    int semid = 0;
    struct sembuf sbuf;

    /* Get semaphore ID associated with this key. */
    if ((semid = semget(semkey, 0, 0)) == - 1)
    {

        /* Semaphore does not exist - Create. */
        if ((semid = semget(semkey, 1, IPC_CREAT | IPC_EXCL | S_IRUSR |
                                       S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) != - 1)
        {
            /* Initialize the semaphore. */
            sbuf.sem_num = 0;
            sbuf.sem_op = 1;  /* This is the number of runs
	                             without queuing. */
            sbuf.sem_flg = 0;
            if (semop(semid, &sbuf, 1) == - 1)
            {
                perror("IPC error: semop");
                exit(1);
            }
        } else if (errno == EEXIST)
        {
            if ((semid = semget(semkey, 0, 0)) == - 1)
            {
                perror("IPC error 1: semget");
                exit(1);
            }
        } else
        {
            perror("IPC error 2: semget");
            exit(1);
        }
    }

//    printf("semid: %d\n", semid);

    return semid;
}


int sem_create (key_t key)
{
    int semid = semget(key, 1, 0666 | IPC_CREAT | IPC_EXCL);
    if (semid == - 1)
        ERR_EXIT("semget");

    return semid;
}

int sem_open (key_t key)
{
    int semid = semget(key, 0, 0);
    if (semid == - 1)
        ERR_EXIT("semget");

    return semid;
}

int sem_p (int semid)
{
    struct sembuf sb = {0, - 1, 0};
//    struct sembuf sb = {1, +1,SEM_UNDO} ;
    int ret = semop(semid, &sb, 1);
    if (ret == - 1)
        ERR_EXIT("semop");

    return ret;
}

int sem_v (int semid)
{
    struct sembuf sb = {0, 1, 0};
//    struct sembuf sb = {1, -1, IPC_NOWAIT|SEM_UNDO} ;
    int ret = semop(semid, &sb, 1);
    if (ret == - 1)
        ERR_EXIT("semop");

    return ret;
}

int sem_d (int semid)
{
    int ret = semctl(semid, 0, IPC_RMID, 0);
    if (ret == - 1)
        ERR_EXIT("semctl");
    return ret;
}

int sem_setval (int semid, int val)
{
    union semun su;
    su.val = val;
    int ret = semctl(semid, 0, SETVAL, su);
    if (ret == - 1)
        ERR_EXIT("semctl");

//    printf("value updated...\n");
    return ret;
}

int sem_getval (int semid)
{
    int ret = semctl(semid, 0, GETVAL, 0);
    if (ret == - 1)
        ERR_EXIT("semctl");

    printf("current val is %d\n", ret);
    return ret;
}

int sem_getmode (int semid)
{
    union semun su;
    struct semid_ds sem;
    su.buf = &sem;
    int ret = semctl(semid, 0, IPC_STAT, su);
    if (ret == - 1)
        ERR_EXIT("semctl");

    printf("current permissions is %o\n", su.buf->sem_perm.mode);
    return ret;
}

int sem_setmode (int semid, char *mode)
{
    union semun su;
    struct semid_ds sem;
    su.buf = &sem;

    int ret = semctl(semid, 0, IPC_STAT, su);
    if (ret == - 1)
        ERR_EXIT("semctl");

    printf("current permissions is %o\n", su.buf->sem_perm.mode);
    sscanf(mode, "%o", (unsigned int *) &su.buf->sem_perm.mode);
    ret = semctl(semid, 0, IPC_SET, su);
    if (ret == - 1)
        ERR_EXIT("semctl");

    printf("permissions updated...\n");

    return ret;
}

void usage (void)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "semtool -c\n");
    fprintf(stderr, "semtool -d\n");
    fprintf(stderr, "semtool -p\n");
    fprintf(stderr, "semtool -v\n");
    fprintf(stderr, "semtool -s <val>\n");
    fprintf(stderr, "semtool -g\n");
    fprintf(stderr, "semtool -f\n");
    fprintf(stderr, "semtool -m <mode>\n");
}
