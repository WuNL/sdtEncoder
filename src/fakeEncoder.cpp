#include <utility>

//
// Created by wunl on 18-8-7.
//

#include <include/fakeEncoder.h>

#include "fakeEncoder.h"

#include <utime.h>

void fakeEncoder::run ()
{
    started_ = true;
    pthread_create(&pthreadId_, nullptr, &fakeEncoder::start, static_cast<void *>(this));
}

int fakeEncoder::join ()
{
    return 0;
}

fakeEncoder::fakeEncoder (initParams p) : encoder(std::move(p))
{

}

void *fakeEncoder::start (void *threadarg)
{
    auto this_ptr = static_cast<fakeEncoder *>(threadarg);

    this_ptr->encodeBuffer();
}

void fakeEncoder::encodeBuffer ()
{
    while (started_)
    {
        usleep(1000000);
        std::cout << "sleeping!" << std::endl;
    }
}

int fakeEncoder::forceKeyFrame (bool insertKeyFrame)
{
    return 0;
}
