
#include <include/videoPipeline.h>

#include "videoPipeline.h"

videoPipeline::videoPipeline (initParams &p) : encoder_name(p.encoder_name),
                                               captureDevice(nullptr),
                                               videoEncoder(nullptr),
                                               pkt(nullptr),
                                               nethandle(nullptr),
                                               isRun(true),
                                               isReleased(false)
{
    params = p;
}

videoPipeline::~videoPipeline ()
{
    //dtor
}

void videoPipeline::start ()
{
    init();
    pthread_create(&thread, nullptr, run, (void *) this);
}

void videoPipeline::cancel ()
{
    isRun = false;
    //pthread_cancel(thread);
}

void videoPipeline::join ()
{
    void *status;
    int rc = pthread_join(thread, &status);
    if (rc)
    {
        printf("ERROR; return code from pthread_join() is %d\n", rc);
        exit(- 1);
    }
    printf("Main: completed join with thread %ld having a status of %ld\n", thread, (long) status);
}

void *videoPipeline::run (void *threadarg)
{
    auto *ptr = (videoPipeline *) threadarg;
    while (ptr->GetisRun())
    {
        ptr->videoInPipeline();
    }
    printf("videoPipeline ready to release!\n");
    ptr->realRelease();
    return nullptr;
}

void videoPipeline::init ()
{
    captureDevice = nullptr;
    captureDevice = new capture;


    videoEncoder = nullptr;
    if (params.codec == std::string("h264"))
    {
        videoEncoder = new h264Encoder(params);
        h264Packet *h264pkt = nullptr;
        h264pkt = new h264Packet(static_cast<uint32_t>(params.max_payload_len), 10);
        pkt = (packet *) h264pkt;
    } else if (params.codec == std::string("h265"))
    {
        videoEncoder = new h265Encoder(params);
        h265Packet *h265pkt = nullptr;
        h265pkt = new h265Packet(static_cast<uint32_t>(params.max_payload_len), 10);
        pkt = (packet *) h265pkt;
    } else
        videoEncoder = new fakeEncoder(params);

    videoEncoder->initEncoder();

    nethandle = nullptr;
    struct net_param netp;

    netp.type = UDP;
    netp.serip = const_cast<char *>(params.dstIP.c_str());
    netp.serport = params.port;

    nethandle = net_open(netp);
    if (! nethandle)
    {
        printf("--- Open network failed\n");
        return;
    }
    isReleased = false;
}

void videoPipeline::realRelease ()
{
    if (captureDevice)
        delete captureDevice;
    if (videoEncoder)
        delete videoEncoder;
    if (pkt)
        delete pkt;
    if (nethandle)
        net_close(nethandle);
    isReleased = true;
}

void videoPipeline::videoInPipeline ()
{
    std::cout << "void videoPipeline::videoInPipeline ()" << std::endl;
    captureDevice->camera_start();
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    /* skip 5 frames for booting a cam */
    for (int i = 0; i < 5; i ++)
    {
        captureDevice->camera_frame(timeout);
    }
    //接受capture得到的YUV，格式为YUV420
    unsigned char *yuvbuffer = NULL;
    uint32_t height, width;
    mfxStatus sts = MFX_ERR_NONE;
    sts = MFX_ERR_NONE;
    int cnt = 0;
    while ((MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts) && GetisRun())
    {
        cnt ++;
        captureDevice->camera_frame_and_decode(timeout, yuvbuffer, width, height);

        videoEncoder->encodeBuffer(yuvbuffer);

        //skip a frame; used for 60 fps to 30 fps
        if (MFX_ERR_MORE_DATA == sts)
            continue;

        if (videoEncoder->mfxBS.DataLength == 0)
            continue;
        pkt->pack_put(videoEncoder->mfxBS.Data + videoEncoder->mfxBS.DataOffset, videoEncoder->mfxBS.DataLength);

        int pac_len;
        while (pkt->pack_get(&pac_buf, &pac_len) == 1)
        {
            int ret = net_send(nethandle, pac_buf, pac_len);
            if (ret != pac_len)
            {
                printf("send pack data failed, size: %d, err: %s\n", pac_len,
                       strerror(errno));
            }
        }
        videoEncoder->mfxBS.DataLength = 0;


        sts = MFX_ERR_NONE;
        MSDK_BREAK_ON_ERROR(sts);



        //printf("Frame number: %d\r", nFrame);
        fflush(stdout);

        memset(yuvbuffer, 1, sizeof(unsigned char) * width * height * 3 / 2);//清0
    }
}

const string &videoPipeline::getEncoder_name () const
{
    return encoder_name;
}



