//
// Created by sdt on 18-8-6.
//

#include "conductor.h"
#include <cstring>
#include "videoEncoderFactory.h"

const initParamsRet conductor::initEncoder (initParams &p)
{
    initParamsRet ret;
    memset(&ret, 0x00, sizeof(initParamsRet));


    return ret;
}

int conductor::findEncoderByName (std::string &name)
{

    return - 1;
}

conductor::conductor () : errHandleCallback_(nullptr),
                          notifyCloseCallback_(nullptr),
                          InitDoneCallback_(nullptr) {}

const bool conductor::destroyEncoder (destroyParams &p)
{

    return true;
}

const bool conductor::updateBitrate (initParams &p)
{

    return true;
}

void conductor::forceKeyFrame (forceKeyFrameParams &p)
{

    std::cout << "forceKeyFrame finish" << std::endl;
}
