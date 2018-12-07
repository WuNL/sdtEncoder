
#include <include/videoPipeline.h>

#include "videoPipeline.h"

videoPipeline::videoPipeline (initParams &p) : captureDevice(NULL),
                                               pri_videopipeline(NULL),
                                               pkt(NULL),
                                               nethandle(NULL),
                                               isRun(true),
                                               isReleased(false)
{
    params = p;
}

videoPipeline::~videoPipeline ()
{
    //dtor
}

void videoPipeline::start ()
{
    init();
    pthread_create(&thread, NULL, run, (void *) this);
}

void videoPipeline::cancel ()
{
    isRun = false;
    //pthread_cancel(thread);
}

void videoPipeline::join ()
{
    void *status;
    int rc = pthread_join(thread, &status);
    if (rc)
    {
        printf("ERROR; return code from pthread_join() is %d\n", rc);
        exit(- 1);
    }
    printf("Main: completed join with thread %ld having a status of %ld\n", thread, (long) status);
}

void *videoPipeline::run (void *threadarg)
{
    videoPipeline *ptr = (videoPipeline *) threadarg;
    while (ptr->GetisRun())
    {
        ptr->videoInPipeline();
    }
    printf("videoPipeline ready to release!\n");
    ptr->realRelease();
    return NULL;
}

void videoPipeline::init ()
{

}

void videoPipeline::realRelease ()
{

}

void videoPipeline::videoInPipeline ()
{

}



