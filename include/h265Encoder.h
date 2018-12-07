//
// Created by wunl on 18-8-5.
//

#ifndef ENCODER_H265ENCODER_H
#define ENCODER_H265ENCODER_H

#include "encoder.h"


class h265Encoder : public encoder
{
public:
    void run () override;

    explicit h265Encoder (initParams p);

    int join () override;

    int initEncoder () override;

    ~h265Encoder () override;


    int updateBitrate (int target_kbps, int target_fps) override;

    virtual int forceKeyFrame (bool insertKeyFrame) override;

private:
    int encodeBuffer (void *in, void *out) override {}

    static void *start (void *threadarg);

    void encodeBuffer ();


private:
    mfxBitstream mfxBS;

    mfxStatus sts;
    mfxIMPL impl;
    mfxVersion ver;
    MFXVideoSession session;
    MFXVideoENCODE *mfxENC;
    MFXVideoVPP *mfxVPP;
    mfxVideoParam mfxEncParams;
    mfxVideoParam VPPParams;

    mfxFrameAllocRequest EncRequest;
    mfxFrameAllocRequest VPPRequest[2];     // [0] - in, [1] - out
    mfxFrameAllocResponse mfxResponseVPPIn;
    mfxFrameAllocResponse mfxResponseVPPOutEnc;
    mfxVideoParam par;


    mfxU16 nEncSurfNum;
    mfxU16 nSurfNumVPPIn;
    mfxU16 nSurfNumVPPOutEnc;

    mfxU16 width;
    mfxU16 height;
    mfxU8 bitsPerPixel;
    mfxU32 surfaceSize;
    mfxU8 *surfaceBuffers;
    mfxFrameSurface1 **pEncSurfaces;
    mfxFrameSurface1 **pVPPSurfacesIn;
    mfxFrameSurface1 **pVPPSurfacesOut;

    FILE *fSink;
    FILE *fRawSink;

    bool useVPP;
    bool insertIDR;
    bool updateBitrate_;
};


#endif //ENCODER_H265ENCODER_H
