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

    int index = findEncoderByName(p.encoder_name);
    if (index != - 1)
    {
        // TODO
        // 删除旧pipeline，创新新的
    } else
    {
        std::unique_ptr<videoPipeline> e = video_encoder_factory::create(p);
        encoderVec_.push_back(std::move(e));
        ret.encoder_name = p.encoder_name;
        ret.success = true;
        encoderVec_.back()->start();
    }

    if (InitDoneCallback_)
    {
        InitDoneCallback_();
    }

    return ret;
}

int conductor::findEncoderByName (std::string &name)
{
    for (int i = 0; i < encoderVec_.size(); ++ i)
    {
        if (encoderVec_[i]->getEncoder_name() == name)
            return i;
    }
    return - 1;
}

conductor::conductor () : errHandleCallback_(nullptr),
                          notifyCloseCallback_(nullptr),
                          InitDoneCallback_(nullptr) {}

const bool conductor::destroyEncoder (destroyParams &p)
{
    for (auto &i : encoderVec_)
    {
        i->SetisRun(false);
        i.release();
    }
    encoderVec_.clear();

    return true;
}

const bool conductor::updateBitrate (initParams &p)
{

    return true;
}

void conductor::forceKeyFrame (forceKeyFrameParams &p)
{
    for (auto &i : encoderVec_)
    {
        i->insertIdr();
    }
    std::cout << "forceKeyFrame finish" << std::endl;
}
