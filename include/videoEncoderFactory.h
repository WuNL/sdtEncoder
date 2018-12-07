//
// Created by wunl on 18-8-7.
//

#ifndef ENCODER_VIDEOENCODERFACTORY_H
#define ENCODER_VIDEOENCODERFACTORY_H

#include <bits/unique_ptr.h>
#include "videoPipeline.h"

class video_encoder_factory
{
public:
    static std::unique_ptr<videoPipeline> create (initParams &p);
};


#endif //ENCODER_VIDEOENCODERFACTORY_H
