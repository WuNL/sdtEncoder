#include <utility>

//
// Created by wunl on 18-8-5.
//

#include <include/h264Encoder.h>

#include "h264Encoder.h"
#include <utime.h>

#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080
#define MSDK_ZERO_MEMORY(VAR)                    {memset(&VAR, 0, sizeof(VAR));}

void h264Encoder::run ()
{
    started_ = true;
    pthread_create(&pthreadId_, nullptr, &h264Encoder::start, static_cast<void *>(this));
}

int h264Encoder::join ()
{
    return 0;
}

h264Encoder::h264Encoder (initParams p) : encoder(std::move(p)),
                                          insertIDR(false),
                                          updateBitrate_(false)
{

}

void *h264Encoder::start (void *threadarg)
{
    auto this_ptr = static_cast<h264Encoder *>(threadarg);

    this_ptr->encodeBuffer();
}

void h264Encoder::encodeBuffer ()
{

}

int h264Encoder::initEncoder ()
{
//    if(options.values.Bitrate!=CAPTURE_FRAMERATE || options.values.Width!=CAPTURE_WIDTH || options.values.Height!=CAPTURE_HEIGHT)
    useVPP = false;

    impl = MFX_IMPL_HARDWARE;
    ver = {{0, 1}};
    sts = Initialize(impl, ver, &session, nullptr);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    memset(&mfxEncParams, 0, sizeof(mfxEncParams));

    mfxEncParams.mfx.CodecId = MFX_CODEC_AVC;

    mfxEncParams.mfx.GopOptFlag = MFX_GOP_STRICT;
    mfxEncParams.mfx.GopPicSize = (mfxU16) 120;
    mfxEncParams.mfx.IdrInterval = (mfxU16) 1;
    mfxEncParams.mfx.GopRefDist = (mfxU16) 1;


    mfxEncParams.mfx.GopRefDist = 1;
    mfxEncParams.AsyncDepth = 1;
    mfxEncParams.mfx.NumRefFrame = 1;

    //取消每帧附带的sei。实际发现取消后容易花屏
    std::vector<mfxExtBuffer *> m_InitExtParams_ENC;
    auto *pCodingOption = new mfxExtCodingOption;
    MSDK_ZERO_MEMORY(*pCodingOption);
    pCodingOption->Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    pCodingOption->Header.BufferSz = sizeof(mfxExtCodingOption);
//    pCodingOption->RefPicMarkRep = MFX_CODINGOPTION_OFF;
//    pCodingOption->SingleSeiNalUnit = MFX_CODINGOPTION_OFF;
    pCodingOption->NalHrdConformance = MFX_CODINGOPTION_ON;
    pCodingOption->VuiVclHrdParameters = MFX_CODINGOPTION_ON;
//    pCodingOption->AUDelimiter = MFX_CODINGOPTION_OFF;

    auto *pCodingOption2 = new mfxExtCodingOption2;
    MSDK_ZERO_MEMORY(*pCodingOption2);
    pCodingOption2->Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    pCodingOption2->Header.BufferSz = sizeof(mfxExtCodingOption2);
    pCodingOption2->RepeatPPS = MFX_CODINGOPTION_ON;
    pCodingOption2->MaxSliceSize = (size_t) 1100;

    m_InitExtParams_ENC.push_back(reinterpret_cast<mfxExtBuffer *>(pCodingOption));
    m_InitExtParams_ENC.push_back(reinterpret_cast<mfxExtBuffer *>(pCodingOption2));

    mfxEncParams.ExtParam = m_InitExtParams_ENC.data();
    mfxEncParams.NumExtParam = (mfxU16) m_InitExtParams_ENC.size();

    if (mfxEncParams.mfx.CodecId == MFX_CODEC_AVC)
    {
        char filename[200];
        sprintf(filename, "/home/sdt/Videos/%d_%d", params.v_width, params.v_height);
#ifdef SAVE_CODECED
        MSDK_FOPEN(fSink, (filename + std::string(".264")).c_str(), "wb");
#endif
#ifdef SAVE_RAW
        MSDK_FOPEN(fRawSink, (filename + std::string(".yuv")).c_str(), "wb");
#endif
    }


    if (mfxEncParams.mfx.CodecId == MFX_CODEC_AVC)
    {
        mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;

        mfxEncParams.mfx.TargetKbps = static_cast<mfxU16>(params.bitrate);
        mfxEncParams.mfx.MaxKbps = static_cast<mfxU16>(params.bitrate * 1.2);
        mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        mfxEncParams.mfx.FrameInfo.FrameRateExtN = static_cast<mfxU16>(params.framerate);
        mfxEncParams.mfx.FrameInfo.FrameRateExtD = 1;
        mfxEncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        mfxEncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        mfxEncParams.mfx.FrameInfo.CropX = 0;
        mfxEncParams.mfx.FrameInfo.CropY = 0;
        mfxEncParams.mfx.FrameInfo.CropW = static_cast<mfxU16>(params.v_width);
        mfxEncParams.mfx.FrameInfo.CropH = static_cast<mfxU16>(params.v_height);
        // Width must be a multiple of 16
        // Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        mfxEncParams.mfx.FrameInfo.Width = static_cast<mfxU16>(params.v_width);
        mfxEncParams.mfx.FrameInfo.Height =
                static_cast<mfxU16>((MFX_PICSTRUCT_PROGRESSIVE == mfxEncParams.mfx.FrameInfo.PicStruct) ?
                                    MSDK_ALIGN16(params.v_height) :
                                    MSDK_ALIGN32(params.v_height));

        mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }


    // Initialize VPP parameters
    memset(&VPPParams, 0, sizeof(VPPParams));
    // Input data V4L2
    VPPParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    VPPParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    VPPParams.vpp.In.CropX = 0;
    VPPParams.vpp.In.CropY = 0;
    VPPParams.vpp.In.CropW = static_cast<mfxU16>(params.v_width);
    VPPParams.vpp.In.CropH = static_cast<mfxU16>(params.v_height);
    VPPParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.In.FrameRateExtN = static_cast<mfxU16>(params.framerate);
    VPPParams.vpp.In.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    VPPParams.vpp.In.Width = static_cast<mfxU16>(params.v_width);
    VPPParams.vpp.In.Height =
            static_cast<mfxU16>((MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.In.PicStruct) ?
                                MSDK_ALIGN16(params.v_height) :
                                MSDK_ALIGN32(params.v_height));
    // Output data
    VPPParams.vpp.Out.FourCC = MFX_FOURCC_NV12;
    VPPParams.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    VPPParams.vpp.Out.CropX = 0;
    VPPParams.vpp.Out.CropY = 0;
    VPPParams.vpp.Out.CropW = static_cast<mfxU16>(params.v_width);
    VPPParams.vpp.Out.CropH = static_cast<mfxU16>(params.v_height);
    VPPParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.Out.FrameRateExtN = static_cast<mfxU16>(params.framerate);
    VPPParams.vpp.Out.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    VPPParams.vpp.Out.Width = static_cast<mfxU16>(params.v_width);
    VPPParams.vpp.Out.Height =
            static_cast<mfxU16>((MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.Out.PicStruct) ?
                                MSDK_ALIGN16(VPPParams.vpp.Out.CropH) :
                                MSDK_ALIGN32(VPPParams.vpp.Out.CropH));

    VPPParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    //5. Create Media SDK encoder
    mfxENC = new MFXVideoENCODE(session);
    // Create Media SDK VPP component
    mfxVPP = new MFXVideoVPP(session);


    // Query number of required surfaces for encoder
    memset(&EncRequest, 0, sizeof(EncRequest));
    sts = mfxENC->QueryIOSurf(&mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Query number of required surfaces for VPP
    memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest) * 2);
    sts = mfxVPP->QueryIOSurf(&VPPParams, VPPRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT;     // surfaces are shared between VPP output and encode input

    // Determine the required number of surfaces for VPP input and for VPP output (encoder input)
    nSurfNumVPPIn = VPPRequest[0].NumFrameSuggested;
    nSurfNumVPPOutEnc = EncRequest.NumFrameSuggested + VPPRequest[1].NumFrameSuggested;

    EncRequest.NumFrameSuggested = nSurfNumVPPOutEnc;



    //4. Allocate surfaces for VPP: In
    // - Width and height of buffer must be aligned, a multiple of 32
    // - Frame surface array keeps pointers all surface planes and general frame info
    width = (mfxU16) MSDK_ALIGN32(MAX_WIDTH);
    height = (mfxU16) MSDK_ALIGN32(MAX_HEIGHT);
    mfxU8 bitsPerPixel = 12;        // NV12 format is a 12 bits per pixel format
    auto surfaceSize = static_cast<mfxU32>(width * height * bitsPerPixel / 8);
    auto *surfaceBuffersIn = (mfxU8 *) new mfxU8[surfaceSize * nSurfNumVPPIn];

    pVPPSurfacesIn = new mfxFrameSurface1 *[nSurfNumVPPIn];
    MSDK_CHECK_POINTER(pVPPSurfacesIn, MFX_ERR_MEMORY_ALLOC);
    for (int i = 0; i < nSurfNumVPPIn; i ++)
    {
        pVPPSurfacesIn[i] = new mfxFrameSurface1;
        memset(pVPPSurfacesIn[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pVPPSurfacesIn[i]->Info), &(VPPParams.vpp.In), sizeof(mfxFrameInfo));
        pVPPSurfacesIn[i]->Data.Y = &surfaceBuffersIn[surfaceSize * i];
        pVPPSurfacesIn[i]->Data.U = pVPPSurfacesIn[i]->Data.Y + width * height;
        pVPPSurfacesIn[i]->Data.V = pVPPSurfacesIn[i]->Data.U + 1;
        pVPPSurfacesIn[i]->Data.Pitch = width;
        if (true)
        {
            ClearYUVSurfaceSysMem(pVPPSurfacesIn[i], width, height);
        }
    }


    // Allocate surfaces for VPP: Out
    width = (mfxU16) MSDK_ALIGN32(MAX_WIDTH);
    height = (mfxU16) MSDK_ALIGN32(MAX_HEIGHT);
    surfaceSize = static_cast<mfxU32>(width * height * bitsPerPixel / 8);
    auto *surfaceBuffersOut = (mfxU8 *) new mfxU8[surfaceSize * nSurfNumVPPOutEnc];

    pVPPSurfacesOut = new mfxFrameSurface1 *[nSurfNumVPPOutEnc];
    MSDK_CHECK_POINTER(pVPPSurfacesOut, MFX_ERR_MEMORY_ALLOC);
    for (int i = 0; i < nSurfNumVPPOutEnc; i ++)
    {
        pVPPSurfacesOut[i] = new mfxFrameSurface1;
        memset(pVPPSurfacesOut[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pVPPSurfacesOut[i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));
        pVPPSurfacesOut[i]->Data.Y = &surfaceBuffersOut[surfaceSize * i];
        pVPPSurfacesOut[i]->Data.U = pVPPSurfacesOut[i]->Data.Y + width * height;
        pVPPSurfacesOut[i]->Data.V = pVPPSurfacesOut[i]->Data.U + 1;
        pVPPSurfacesOut[i]->Data.Pitch = width;
    }

    // Initialize extended buffer for frame processing
    // - Denoise           VPP denoise filter
    // - mfxExtVPPDoUse:   Define the processing algorithm to be used
    // - mfxExtVPPDenoise: Denoise configuration
    // - mfxExtBuffer:     Add extended buffers to VPP parameter configuration
    mfxExtVPPDoUse extDoUse;
    memset(&extDoUse, 0, sizeof(extDoUse));
    mfxU32 tabDoUseAlg[1];
    extDoUse.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
    extDoUse.Header.BufferSz = sizeof(mfxExtVPPDoUse);
    extDoUse.NumAlg = 1;
    extDoUse.AlgList = tabDoUseAlg;
    tabDoUseAlg[0] = MFX_EXTBUFF_VPP_DENOISE;

    mfxExtVPPDenoise denoiseConfig;
    memset(&denoiseConfig, 0, sizeof(denoiseConfig));
    denoiseConfig.Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
    denoiseConfig.Header.BufferSz = sizeof(mfxExtVPPDenoise);
    denoiseConfig.DenoiseFactor = 100;        // can be 1-100

    mfxExtBuffer *ExtBuffer[2];
    ExtBuffer[0] = (mfxExtBuffer *) &extDoUse;
    ExtBuffer[1] = (mfxExtBuffer *) &denoiseConfig;
    VPPParams.NumExtParam = 2;
    VPPParams.ExtParam = (mfxExtBuffer **) &ExtBuffer[0];



    //8 Initialize the Media SDK encoder
    sts = mfxENC->Init(&mfxEncParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    //5. Initialize Media SDK VPP
    sts = mfxVPP->Init(&VPPParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);



    // Retrieve video parameters selected by encoder.
    // - BufferSizeInKB parameter is required to set bit stream buffer size

    memset(&par, 0, sizeof(par));
    sts = mfxENC->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //6. Prepare Media SDK bit stream buffer

    memset(&mfxBS, 0, sizeof(mfxBS));
    mfxBS.MaxLength = static_cast<mfxU32>(par.mfx.BufferSizeInKB * 1000);
    mfxBS.Data = new mfxU8[mfxBS.MaxLength];
    MSDK_CHECK_POINTER(mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

    initFinished = true;
    pthread_cond_signal(&count_threshold_cv);
    return 1;
}


h264Encoder::~h264Encoder ()
{
    printf("~h264Encoder\n");
    if (mfxENC)
    {
        mfxENC->Close();
        delete mfxENC;
    }


    MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);
    if (! surfaceBuffers) MSDK_SAFE_DELETE_ARRAY(surfaceBuffers);
    if (fSink)
        fclose(fSink);
    if (fRawSink)
        fclose(fRawSink);
}

int h264Encoder::updateBitrate (int target_kbps, int target_fps)
{
    if (abs(params.bitrate - target_kbps) < 100)
        return 0;
    mfxVideoParam param;
    memset(&param, 0, sizeof(param));
    mfxStatus status;
    mfxENC->GetVideoParam(&param);
    std::cout << "before reset, query kbps:" << param.mfx.TargetKbps << "   " << param.mfx.BufferSizeInKB << "   "
              << param.mfx.InitialDelayInKB << std::endl;

    if (params.bitrate != target_kbps && target_kbps > 100)
    {
        params.bitrate = target_kbps;

        std::vector<mfxExtBuffer *> m_InitExtParams_ENC;

        auto *pCodingOption = new mfxExtCodingOption;
        MSDK_ZERO_MEMORY(*pCodingOption);
        pCodingOption->Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
        pCodingOption->Header.BufferSz = sizeof(mfxExtCodingOption);
        pCodingOption->NalHrdConformance = MFX_CODINGOPTION_ON;
        pCodingOption->VuiVclHrdParameters = MFX_CODINGOPTION_ON;

        auto *pCodingOption2 = new mfxExtCodingOption2;
        MSDK_ZERO_MEMORY(*pCodingOption2);
        pCodingOption2->Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
        pCodingOption2->Header.BufferSz = sizeof(mfxExtCodingOption2);
        pCodingOption2->RepeatPPS = MFX_CODINGOPTION_ON;
        pCodingOption2->MaxSliceSize = (size_t) 1100;

        m_InitExtParams_ENC.push_back(reinterpret_cast<mfxExtBuffer *>(pCodingOption));
        m_InitExtParams_ENC.push_back(reinterpret_cast<mfxExtBuffer *>(pCodingOption2));

        param.ExtParam = m_InitExtParams_ENC.data();
        param.NumExtParam = (mfxU16) m_InitExtParams_ENC.size();

        param.mfx.TargetKbps = static_cast<mfxU16>(target_kbps);
        param.mfx.MaxKbps = static_cast<mfxU16>(target_kbps * 1.2);

        status = mfxENC->Reset(&param);
        MSDK_CHECK_RESULT(status, MFX_ERR_NONE, status);
    }
    memset(&param, 0, sizeof(param));
    mfxENC->GetVideoParam(&param);
    std::cout << "after reset, query kbps:" << param.mfx.TargetKbps << "   " << param.mfx.BufferSizeInKB << "   "
              << param.mfx.InitialDelayInKB << std::endl;

    return 0;
}

int h264Encoder::forceKeyFrame (bool insertKeyFrame)
{
    insertIDR = true;
    return 0;
}

int h264Encoder::encodeBuffer (void *in, void *out)
{
    return 0;
}
