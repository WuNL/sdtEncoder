#include "capture.h"

void quit (const char *msg)
{
    fprintf(stderr, "[%s] %d: %s\n", msg, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

int xioctl (int fd, int request, void *arg)
{
    for (int i = 0; i < 100; i ++)
    {
        int r = ioctl(fd, request, arg);
        if (r != - 1 || errno != EINTR) return r;
    }
    return - 1;
}

capture::capture () : out_buff(NULL), fSink(NULL)
{
    //ctor
    camera = camera_open("/dev/video0", 1920, 1080);
    camera_init();
    //是否打印v4l2格式支持情况
    if (0)
    {
        struct v4l2_fmtdesc fmt;
        struct v4l2_capability cap;
        struct v4l2_format stream_fmt;
        int ret;
        //当前视频设备支持的视频格式
        memset(&fmt, 0, sizeof(fmt));
        fmt.index = 0;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while ((ret = ioctl(camera->fd, VIDIOC_ENUM_FMT, &fmt)) == 0)
        {
            fmt.index ++;

            printf("{pixelformat = %c%c%c%c},description = '%s'\n",
                   fmt.pixelformat & 0xff, (fmt.pixelformat >> 8) & 0xff,
                   (fmt.pixelformat >> 16) & 0xff, (fmt.pixelformat >> 24) & 0xff,
                   fmt.description);
        }
    }
    cinfo.err = jpeg_std_error(&jerr);//绑定错误处理结构对象
    jpeg_create_decompress(&cinfo);//初始化cinfo结构

    //是否存解码出的YUV文件
    if (1)
    {
        fSink = fopen("test.yuv", "w");
    }
}

capture::~capture ()
{
    //dtor
    if (fSink) fclose(fSink);
    jpeg_destroy_decompress(&cinfo);// 释放资源
    free(out_buff);

    camera_stop();
    camera_finish();
    camera_close();

}

camera_t *capture::camera_open (const char *device, uint32_t width, uint32_t height)
{
    int fd = open(device, O_RDWR | O_NONBLOCK, 0);
    if (fd == - 1)
        fd = open("/dev/video1", O_RDWR | O_NONBLOCK, 0);
    if (fd == - 1) quit("open");
    camera = (camera_t *) malloc(sizeof(camera_t));
    camera->fd = fd;
    camera->width = width;
    camera->height = height;
    camera->buffer_count = 0;
    camera->buffers = NULL;
    camera->head.length = 0;
    camera->head.start = NULL;
    return camera;
}


void capture::camera_init ()
{
    struct v4l2_capability cap;
    if (xioctl(camera->fd, VIDIOC_QUERYCAP, &cap) == - 1) quit("VIDIOC_QUERYCAP");
    if (! (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) quit("no capture");
    if (! (cap.capabilities & V4L2_CAP_STREAMING)) quit("no streaming");

    struct v4l2_cropcap cropcap;
    memset(&cropcap, 0, sizeof cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera->fd, VIDIOC_CROPCAP, &cropcap) == 0)
    {
        struct v4l2_crop crop;
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;
        if (xioctl(camera->fd, VIDIOC_S_CROP, &crop) == - 1)
        {
            // cropping not supported
        }
    }

    struct v4l2_format format;
    memset(&format, 0, sizeof format);
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = camera->width;
    format.fmt.pix.height = camera->height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(camera->fd, VIDIOC_S_FMT, &format) == - 1) quit("VIDIOC_S_FMT");

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof req);
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(camera->fd, VIDIOC_REQBUFS, &req) == - 1) quit("VIDIOC_REQBUFS");
    camera->buffer_count = req.count;
    camera->buffers = (buffer_t *) calloc(req.count, sizeof(buffer_t));

    size_t buf_max = 0;
    for (size_t i = 0; i < camera->buffer_count; i ++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(camera->fd, VIDIOC_QUERYBUF, &buf) == - 1)
            quit("VIDIOC_QUERYBUF");
        if (buf.length > buf_max) buf_max = buf.length;
        camera->buffers[i].length = buf.length;
        camera->buffers[i].start = (uint8_t *)
                mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                     camera->fd, buf.m.offset);
        if (camera->buffers[i].start == MAP_FAILED) quit("mmap");
    }
    camera->head.start = (uint8_t *) malloc(buf_max);
}


void capture::camera_start ()
{
    for (size_t i = 0; i < camera->buffer_count; i ++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == - 1) quit("VIDIOC_QBUF");
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera->fd, VIDIOC_STREAMON, &type) == - 1)
        quit("VIDIOC_STREAMON");
}

void capture::camera_stop ()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(camera->fd, VIDIOC_STREAMOFF, &type) == - 1)
        quit("VIDIOC_STREAMOFF");
}

void capture::camera_finish ()
{
    for (size_t i = 0; i < camera->buffer_count; i ++)
    {
        munmap(camera->buffers[i].start, camera->buffers[i].length);
    }
    free(camera->buffers);
    camera->buffer_count = 0;
    camera->buffers = NULL;
    free(camera->head.start);
    camera->head.length = 0;
    camera->head.start = NULL;
}

void capture::camera_close ()
{
    if (close(camera->fd) == - 1) quit("close");
    free(camera);
}


int capture::camera_capture ()
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(camera->fd, VIDIOC_DQBUF, &buf) == - 1) return FALSE;
    memcpy(camera->head.start, camera->buffers[buf.index].start, buf.bytesused);
    camera->head.length = buf.bytesused;
    if (xioctl(camera->fd, VIDIOC_QBUF, &buf) == - 1) return FALSE;
    return TRUE;
}

int capture::camera_frame (struct timeval timeout)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(camera->fd, &fds);
    int r = select(camera->fd + 1, &fds, 0, 0, &timeout);
    if (r == - 1) quit("select");
    if (r == 0) return FALSE;
    return camera_capture();
}

int capture::camera_frame_and_decode (struct timeval timeout, unsigned char *&outbuffer, uint32_t &width_,
                                      uint32_t &height_)
{
    camera_frame(timeout);
    jpeg_mem_src(&cinfo, camera->head.start, camera->head.length);
    jpeg_read_header(&cinfo, TRUE);//获取文件信息
    cinfo.out_color_space = JCS_YCbCr;
    cinfo.raw_data_out = TRUE;
    cinfo.do_fancy_upsampling = FALSE;
    jpeg_start_decompress(&cinfo);//开始解压缩
    unsigned long width = cinfo.output_width;//图像宽度
    unsigned long height = cinfo.output_height;//图像高度
    unsigned short depth = cinfo.output_components;//图像深度

    width_ = width;
    height_ = height;

    if (! out_buff)
    {
        out_buff = (unsigned char *) malloc(width * height * 3 / 2);
        memset(out_buff, 1, sizeof(unsigned char) * width * height * 3 / 2);//清0
    } else
    {

    }
    JSAMPARRAY dst_buf[3];
    JSAMPROW dst_row1[cinfo.output_height];
    JSAMPROW dst_row2[cinfo.output_height];
    JSAMPROW dst_row3[cinfo.output_height];
    dst_buf[0] = dst_row1;
    dst_buf[1] = dst_row2;
    dst_buf[2] = dst_row3;

    unsigned char *line;
    line = out_buff;
    int i = 0;
    for (i = 0; i < cinfo.image_height; i ++, line += cinfo.output_width)
        dst_buf[0][i] = line;
    if (2 == cinfo.comp_info[0].v_samp_factor)
    {
        /* file has 420 -- all fine */
        line = out_buff + cinfo.output_width * cinfo.output_height;
        for (i = 0; i < cinfo.image_height; i += 2, line += cinfo.output_width / 2)
            dst_buf[1][i / 2] = line;

        line = out_buff + cinfo.output_width * cinfo.output_height * 5 / 4;
        for (i = 0; i < cinfo.output_width; i += 2, line += cinfo.output_width / 2)
            dst_buf[2][i / 2] = line;

        int y_read_line = 8;
        for (int i = 0; cinfo.output_scanline < cinfo.output_height; i += 2 * y_read_line)
        {
            int j = jpeg_read_raw_data(&cinfo, dst_buf, 2 * y_read_line);
            dst_buf[0] += 2 * y_read_line;
            dst_buf[1] += y_read_line;
            dst_buf[2] += y_read_line;
//            printf("%d---%d\n",i,j);
        }
    } else
    {
        /* file has 422 -- drop lines */
        line = out_buff + cinfo.output_width * cinfo.output_height;
        for (i = 0; i < cinfo.output_height; i += 2, line += cinfo.output_width / 2)
        {
            dst_buf[1][i + 0] = line;
            dst_buf[1][i + 1] = line;
        }

        line = out_buff + cinfo.output_width * cinfo.output_height * 5 / 4;
        for (i = 0; i < cinfo.output_height; i += 2, line += cinfo.output_width / 2)
        {
            dst_buf[2][i + 0] = line;
            dst_buf[2][i + 1] = line;
        }

        int y_read_line = 8;
        for (int i = 0; cinfo.output_scanline < cinfo.output_height; i += y_read_line)
        {
            int j = jpeg_read_raw_data(&cinfo, dst_buf, y_read_line);
            dst_buf[0] += y_read_line;
            dst_buf[1] += y_read_line;
            dst_buf[2] += y_read_line;
//            printf("%d---%d\n",i,j);
        }
    }
    int x = 0;
    jpeg_finish_decompress(&cinfo);//解压缩完毕
    //fwrite(out_buff, sizeof(unsigned char), width * height *3/2,fSink);
    outbuffer = out_buff;
}
