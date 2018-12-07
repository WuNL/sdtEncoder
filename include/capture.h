#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <jpeglib.h>
#include <jerror.h>
#include <sys/timeb.h>


typedef struct
{
    uint8_t *start;
    size_t length;
} buffer_t;

typedef struct
{
    int fd;
    uint32_t width;
    uint32_t height;
    size_t buffer_count;
    buffer_t *buffers;
    buffer_t head;
} camera_t;

class capture
{
public:
    capture ();

    virtual ~capture ();

    camera_t *camera_open (const char *device, uint32_t width, uint32_t height);

    void camera_start ();

    void camera_stop ();

    void camera_finish ();

    void camera_close ();

    int camera_frame (struct timeval timeout);

    int camera_frame_and_decode (struct timeval timeout, unsigned char *&outbuffer, uint32_t &width, uint32_t &height);


protected:
    inline int minmax (int min, int v, int max)
    {
        return (v < min) ? min : (max < v) ? max : v;
    }

    void camera_init ();

    int camera_capture ();

private:
    camera_t *camera;
    unsigned char *out_buff;
    FILE *fSink;
    struct jpeg_decompress_struct cinfo;//JPEG图像在解码过程中，使用jpeg_decompress_struct类型的结构体来表示，图像的所有信息都存储在结构体中
    struct jpeg_error_mgr jerr;//定义一个标准的错误结构体，一旦程序出现错误就会调用exit()函数退出进程
};

#endif // CAPTURE_H
