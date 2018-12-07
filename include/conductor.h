//
// Created by sdt on 18-8-6.
//

#ifndef ENCODER_CONDUCTOR_H
#define ENCODER_CONDUCTOR_H

#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <boost/utility.hpp>

#include "encoder.h"
#include "videoPipeline.h"


class conductor : public boost::noncopyable
{
public:
    typedef std::function<void ()> ErrHandleCallback;
    typedef std::function<void ()> NotifyCloseCallback;
    typedef std::function<void ()> InitDoneCallback;

    conductor ();

    void setErrHandleCallback (const ErrHandleCallback &cb)
    {
        errHandleCallback_ = cb;
    }

    void setInitDoneCallback_ (const InitDoneCallback &InitDoneCallback_)
    {
        conductor::InitDoneCallback_ = InitDoneCallback_;
    }

    const initParamsRet initEncoder (initParams &p);

    const bool destroyEncoder (destroyParams &p);

    const bool updateBitrate (initParams &p);

    void forceKeyFrame (forceKeyFrameParams &p);

    const int getSize ()
    {
        return static_cast<const int>(encoderVec_.size());
    }

private:
    int findEncoderByName (std::string &name);

private:

    ErrHandleCallback errHandleCallback_;
    NotifyCloseCallback notifyCloseCallback_;
    InitDoneCallback InitDoneCallback_;

private:

    std::vector<std::unique_ptr<videoPipeline> > encoderVec_;
};


#endif //ENCODER_CONDUCTOR_H
