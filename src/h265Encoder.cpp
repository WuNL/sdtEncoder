#include <utility>

//
// Created by wunl on 18-8-5.
//

#include <include/h265Encoder.h>

#include "h265Encoder.h"
#include <utime.h>

#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080
#define MSDK_ZERO_MEMORY(VAR)                    {memset(&VAR, 0, sizeof(VAR));}

void h265Encoder::run ()
{
    started_ = true;
    pthread_create(&pthreadId_, nullptr, &h265Encoder::start, static_cast<void *>(this));
}

int h265Encoder::join ()
{
    return 0;
}

h265Encoder::h265Encoder (initParams p) : encoder(std::move(p)),
                                          insertIDR(false),
                                          updateBitrate_(false)
{

}

void *h265Encoder::start (void *threadarg)
{
    auto this_ptr = static_cast<h265Encoder *>(threadarg);

    this_ptr->encodeBuffer();
}

void h265Encoder::encodeBuffer ()
{

}

int h265Encoder::initEncoder ()
{
//    if(options.values.Bitrate!=CAPTURE_FRAMERATE || options.values.Width!=CAPTURE_WIDTH || options.values.Height!=CAPTURE_HEIGHT)
    useVPP = false;

    impl = MFX_IMPL_HARDWARE;
    ver = {{0, 1}};
    sts = Initialize(impl, ver, &session, nullptr);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    memset(&mfxEncParams, 0, sizeof(mfxEncParams));

    mfxEncParams.mfx.CodecId = MFX_CODEC_HEVC;

    mfxEncParams.mfx.GopOptFlag = MFX_GOP_CLOSED;
    mfxEncParams.mfx.GopPicSize = (mfxU16) 120;
    mfxEncParams.mfx.IdrInterval = (mfxU16) 1;
    mfxEncParams.mfx.GopRefDist = (mfxU16) 1;

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

    m_InitExtParams_ENC.push_back(reinterpret_cast<mfxExtBuffer *>(pCodingOption));
    m_InitExtParams_ENC.push_back(reinterpret_cast<mfxExtBuffer *>(pCodingOption2));

    mfxEncParams.ExtParam = m_InitExtParams_ENC.data();
    mfxEncParams.NumExtParam = (mfxU16) m_InitExtParams_ENC.size();


    if (mfxEncParams.mfx.CodecId == MFX_CODEC_HEVC)
    {
        char filename[200];
        sprintf(filename, "/home/wunl/Videos/Encoder_%d_%d", params.v_width, params.v_height);
#ifdef SAVE_CODECED
        MSDK_FOPEN(fSink, (filename + std::string(".265")).c_str(), "wb");
#endif
#ifdef SAVE_RAW
        MSDK_FOPEN(fRawSink, (filename + std::string(".yuv")).c_str(), "wb");
#endif
    }

    params.framerate = 60;

    if (mfxEncParams.mfx.CodecId == MFX_CODEC_HEVC)
    {
//        MFX_TARGETUSAGE_BALANCED
//        MFX_TARGETUSAGE_BEST_QUALITY
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


    // Load the HEVC plugin
    mfxPluginUID codecUID;
    bool success = true;
    codecUID = msdkGetPluginUID(MFX_IMPL_HARDWARE, MSDK_VENCODE, mfxEncParams.mfx.CodecId);
    if (AreGuidsEqual(codecUID, MSDK_PLUGINGUID_NULL))
    {
        printf("Get Plugin UID for HEVC is failed.\n");
        success = false;
    }

    printf("Loading HEVC plugin: %s\n", ConvertGuidToString(codecUID));

    //On the success of get the UID, load the plugin
    if (success)
    {
        sts = MFXVideoUSER_Load(session, &codecUID, ver.Major);
        if (sts < MFX_ERR_NONE)
        {
            printf("Loading HEVC plugin failed\n");
            success = false;
        }
    }

    // Initialize VPP parameters
    memset(&VPPParams, 0, sizeof(VPPParams));
    // Input data V4L2
    VPPParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    VPPParams.vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    VPPParams.vpp.In.CropX = 0;
    VPPParams.vpp.In.CropY = 0;
    VPPParams.vpp.In.CropW = static_cast<mfxU16>(1920);
    VPPParams.vpp.In.CropH = static_cast<mfxU16>(1080);
    VPPParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.In.FrameRateExtN = static_cast<mfxU16>(params.framerate);
    VPPParams.vpp.In.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    VPPParams.vpp.In.Width = static_cast<mfxU16>(1920);
    VPPParams.vpp.In.Height =
            static_cast<mfxU16>((MFX_PICSTRUCT_PROGRESSIVE == VPPParams.vpp.In.PicStruct) ?
                                MSDK_ALIGN16(1080) :
                                MSDK_ALIGN32(1080));
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


h265Encoder::~h265Encoder ()
{
    printf("~h265Encoder\n");
    if (mfxENC)
    {
        mfxENC->Close();
        delete mfxENC;
    }

    // session closed automatically on destruction


    MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);
    if (! surfaceBuffers) MSDK_SAFE_DELETE_ARRAY(surfaceBuffers);
    if (fSink)
        fclose(fSink);
    if (fRawSink)
        fclose(fRawSink);
    Release();
}

int h265Encoder::updateBitrate (int target_kbps, int target_fps)
{
    std::cout << "h265Encoder::updateBitrate with kbps fps :" << target_kbps << " " << target_fps << std::endl;

    mfxVideoParam param;
    memset(&param, 0, sizeof(param));
    mfxStatus status;
    mfxENC->GetVideoParam(&param);

    if (params.bitrate != target_kbps && target_kbps > 100)
    {
        params.bitrate = target_kbps;

        param.mfx.TargetKbps = static_cast<mfxU16>(target_kbps);
        param.mfx.MaxKbps = static_cast<mfxU16>(target_kbps * 1.2);

        status = mfxENC->Reset(&param);
        MSDK_CHECK_RESULT(status, MFX_ERR_NONE, status);

        memset(&param, 0, sizeof(param));
        mfxENC->GetVideoParam(&param);
        std::cout << "after reset, Target Kbps:" << param.mfx.TargetKbps << "  " << param.mfx.MaxKbps << std::endl;
    }

    return 0;
}

int h265Encoder::forceKeyFrame (bool insertKeyFrame)
{
    insertIDR = true;
    return 0;
}

int h265Encoder::encodeBuffer (void *buffer)
{
    if (! buffer)
        return MFX_ERR_NULL_PTR;
    int nSurfIdxIn = 0, nSurfIdxOut = 0;
    static mfxU32 nFrame = 0;
    mfxSyncPoint syncp;
//    nEncSurfIdx = GetFreeSurfaceIndex(pEncSurfaces, nEncSurfNum);   // Find free frame surface
//    MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nEncSurfIdx, MFX_ERR_MEMORY_ALLOC);
//
//    sts = LoadRawFrameFromV4l2(pEncSurfaces[nEncSurfIdx], buffer);
//
//    MSDK_RETURN_ON_ERROR(sts);



    nSurfIdxIn = GetFreeSurfaceIndex(pVPPSurfacesIn, nSurfNumVPPIn);        // Find free input frame surface
    pVPPSurfacesIn[nSurfIdxIn]->Data.TimeStamp = nFrame * 90000 / VPPParams.vpp.Out.FrameRateExtN;

    sts = LoadRawFrameFromV4l2(pVPPSurfacesIn[nSurfIdxIn], (uint8_t *) buffer);
    MSDK_RETURN_ON_ERROR(sts);

    nSurfIdxOut = GetFreeSurfaceIndex(pVPPSurfacesOut, nSurfNumVPPOutEnc);     // Find free output frame surface
    MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nSurfIdxOut, MFX_ERR_MEMORY_ALLOC);

    for (;;)
    {
        // Process a frame asychronously (returns immediately)
        sts = mfxVPP->RunFrameVPPAsync(pVPPSurfacesIn[nSurfIdxIn], pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);

        //skip a frame

        if (MFX_ERR_MORE_DATA == sts)
        {
            nSurfIdxIn = GetFreeSurfaceIndex(pVPPSurfacesIn, nSurfNumVPPIn);
            MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nSurfIdxIn, MFX_ERR_MEMORY_ALLOC);
            pVPPSurfacesIn[nSurfIdxIn]->Data.TimeStamp = nFrame * 90000 / VPPParams.vpp.Out.FrameRateExtN;
            return sts;
        }

        //add (often duplicate) a frame
        if (MFX_ERR_MORE_SURFACE == sts)
        {
            //todo
        }


        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
        } else
            break;

    }

    for (;;)
    {
        // Encode a frame asychronously (returns immediately)
        if (insertIDR)
        {
            mfxEncodeCtrl ctrl;
            memset(&ctrl, 0, sizeof(ctrl));
            if (mfxEncParams.mfx.CodecId == MFX_CODEC_AVC)
                ctrl.FrameType = (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF);
            else if (mfxEncParams.mfx.CodecId == MFX_CODEC_HEVC)
                ctrl.FrameType = (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF);
            sts = mfxENC->EncodeFrameAsync(&ctrl, pVPPSurfacesOut[nSurfIdxOut], &mfxBS, &syncp);
            printf("IDR frame insert success\n");
            insertIDR = false;
        } else
        {
            sts = mfxENC->EncodeFrameAsync(NULL, pVPPSurfacesOut[nSurfIdxOut], &mfxBS, &syncp);
        }

        //printf("********************\n");
        if (MFX_ERR_NONE < sts && ! syncp)       // Repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts) MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
        } else if (MFX_ERR_NONE < sts && syncp)
        {
            sts = MFX_ERR_NONE;     // Ignore warnings if output is available
            break;
        } else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            // Allocate more bitstream buffer memory here if needed...
            break;
        } else
            break;
    }


    if (MFX_ERR_NONE == sts)
    {
        sts = session.SyncOperation(syncp, 60000);      // Synchronize. Wait until encoded frame is ready
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        ++ nFrame;
    }
    return 0;
}
