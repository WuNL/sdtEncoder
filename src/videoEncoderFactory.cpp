//
// Created by wunl on 18-8-7.
//

#include "videoEncoderFactory.h"
#include "config.h"

std::unique_ptr<videoPipeline> video_encoder_factory::create (initParams &p)
{
    std::unique_ptr<videoPipeline> ret;
#ifdef USE_INTEL
    ret = std::unique_ptr<videoPipeline>(new videoPipeline(p));
    return ret;
#else
    ret = std::unique_ptr<videoPipeline>(new videoPipeline(p));
    return ret;
#endif
    return nullptr;
}
