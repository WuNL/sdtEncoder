#include <utility>

//
// Created by wunl on 18-8-5.
//

#include <include/encoder.h>

#include "encoder.h"
#include <assert.h>


encoder::~encoder ()
{
    started_ = false;

    pthread_mutex_destroy(&count_mutex);
    pthread_cond_destroy(&count_threshold_cv);
    pthread_cond_destroy(&finish_threshold_cv);
//    pthread_exit(NULL);

}

encoder::encoder (initParams p) : errHandleCallback_(nullptr), notifyCloseCallback_(nullptr),
                                  encoder_name(std::move(p.encoder_name)),
                                  params(p),
                                  started_(false),
                                  initFinished(false),
                                  readyToEndThread(false)
{
    /*初始化互斥量和条件变量对象*/
    pthread_mutex_init(&count_mutex, nullptr);
    pthread_cond_init(&count_threshold_cv, nullptr);
    pthread_cond_init(&finish_threshold_cv, nullptr);
}
