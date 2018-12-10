#ifndef VIDEOPIPELINE_H
#define VIDEOPIPELINE_H

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "capture.h"
#include "h264Encoder.h"
#include "h265Encoder.h"
#include "fakeEncoder.h"
#include "packet.h"
#include "network.h"

using namespace std;

class videoPipeline
{
public:
    explicit videoPipeline (initParams &p);

    virtual ~videoPipeline ();

    bool GetisRun ()
    {
        return isRun;
    }

    void SetisRun (bool val)
    {
        isRun = val;
    }

    void insertIdr ()
    {
        if (videoEncoder)
            videoEncoder->forceKeyFrame(true);
    }

    void update (const string &msg)
    {

    }

    void updateOptions (CmdOptions options)
    {

    }

    void start ();

    //阻塞，等待这个线程工作完成
    void join ();

    //对这个线程发出停止信号，线程在时机合适时停止运行
    void cancel ();

    //负责创建各个模块的实例
    void init ();

private:

    static void *run (void *threadarg);

    static void release (void *threadarg)
    {
        auto *ptr = (videoPipeline *) threadarg;
        ptr->realRelease();
    }

    void realRelease ();

    void videoInPipeline ();


protected:

private:
    std::string encoder_name;
public:
    const string &getEncoder_name () const;

private:
    initParams params;

    bool isRun, isReleased;

    capture *captureDevice;
    encoder *videoEncoder;
    packet *pkt;
    struct net_handle *nethandle;

    pthread_t thread;
    void *cap_buf, *cvt_buf, *hd_buf, *enc_buf, *pac_buf;
};

#endif // VIDEOPIPELINE_H
