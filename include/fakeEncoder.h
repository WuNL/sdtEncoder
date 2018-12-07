//
// Created by wunl on 18-8-7.
//

#ifndef ENCODER_FAKEENCODER_H
#define ENCODER_FAKEENCODER_H

#include "encoder.h"

class fakeEncoder : public encoder
{
public:
    virtual void run () override;

    explicit fakeEncoder (initParams p);

    virtual int join () override;

    virtual int forceKeyFrame (bool insertKeyFrame) override;

private:
    int encodeBuffer (void *in, void *out) override {}

    static void *start (void *threadarg);

    void encodeBuffer ();
};


#endif //ENCODER_FAKEENCODER_H
