#include "packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/timeb.h>
#include <string.h>

#define H264    96
#define MAX_OUTBUF_SIZE 10 * 1024    // 10k should be enough, normally it's less then MTU (1500)

typedef struct
{
    // Little Endian
    // byte 0
    unsigned char csrc_len :4;    // contains the number of CSRC identifiers that follow the fixed header.
    unsigned char extension :1;    // extension bit
    unsigned char padding :1;    // padding
    unsigned char version :2;    // version, current is 2
    // byte 1
    unsigned char payload :7;    // payload
    unsigned char marker :1;    // marker
    // bytes 2, 3
    uint16_t seq_no;
    // bytes 4-7
    uint32_t timestamp;
    // bytes 8-11
    uint32_t ssrc;    // sequence number
} rtp_header;

typedef struct
{
    unsigned char TYPE :5;
    unsigned char NRI :2;
    unsigned char F :1;
} nalu_header;    // 1 Bytes

typedef struct
{
    unsigned char hevc_header[2];
} nalu_header_hevc;    // 2 Bytes

typedef struct
{
    //byte 0
    unsigned char TYPE :5;
    unsigned char NRI :2;
    unsigned char F :1;
} fu_indicator;    // 1 BYTE

typedef struct
{
    //byte 0
    unsigned char TYPE :5;
    unsigned char R :1;
    unsigned char E :1;
    unsigned char S :1;
} fu_header;    // 1 BYTE

typedef struct
{
    int startcodeprefix_len;    // 0x000001 or 0x00000001
    char *data;    // nalu data
    int len;    // nalu length
    int forbidden_bit;    //
    int nal_reference_idc;    //
    int nal_unit_type;    // nalu types
} nalu_t;

struct pac_handle
{
    void *inbuf;
    void *outbuf;
    int outbuf_ptr;
    char *next_nalu_ptr;
    int inbuf_size;
    int FU_counter;
    int last_FU_size;
    int FU_index;
    int inbuf_complete;
    int nalu_complete;
    int nalu_type;
    nalu_t nalu;
    unsigned short seq_num;
    U32 ts_start_millisec;        // timestamp in millisecond
    U32 ts_current_sample;        // timestamp in 1/90000.0 unit

    struct pac_param params;
};

static U32 get_current_millisec (void)
{
    struct timeb tb;
    ftime(&tb);
    return 1000 * tb.time + tb.millitm;
}

// this slice is start of a Frame
static int is_start_code4 (char *buf)
{
    if (buf[0] != 0 || buf[1] != 0 || buf[2] != 0 || buf[3] != 1)    // 0x00000001
        return 0;
    else
        return 1;
}

// not start of a frame
static int is_start_code3 (char *buf)
{
    if (buf[0] != 0 || buf[1] != 0 || buf[2] != 1)    // 0x000001
        return 0;
    else
        return 1;
}

static int get_next_nalu (struct pac_handle *handle)
{
    if (! handle->next_nalu_ptr)    // reach data end, no next nalu
    {
        return 0;
    }

    if (is_start_code3(handle->next_nalu_ptr))    // check 0x000001 first
    {
        handle->nalu.startcodeprefix_len = 3;
    } else
    {
        if (is_start_code4(handle->next_nalu_ptr))    // check 0x00000001
        {
            handle->nalu.startcodeprefix_len = 4;
        } else
        {
            printf("!!! No any startcode found\n");
            return - 1;
        }
    }

    // found the next start code
    int startcode_found = 0;
    char *cur_nalu_ptr = handle->next_nalu_ptr;    // rotate, save the next ptr
    char *next_ptr = cur_nalu_ptr + handle->nalu.startcodeprefix_len;    // skip current start code
    while (! startcode_found)
    {
        next_ptr ++;

        if ((next_ptr - (char *) handle->inbuf) >= handle->inbuf_size)    // reach data end
        {
            handle->next_nalu_ptr = NULL;    // no more nalus
            break;
        }

        if (is_start_code3(next_ptr) || is_start_code4(next_ptr))    // found the next start code
        {
            handle->next_nalu_ptr = next_ptr;    // next ptr
            break;
        }
    }

    handle->nalu.data = cur_nalu_ptr + handle->nalu.startcodeprefix_len;    // exclude the start code
    handle->nalu.len = next_ptr - cur_nalu_ptr
                       - handle->nalu.startcodeprefix_len;
    handle->nalu.forbidden_bit = (handle->nalu.data[0] & 0x80) >> 7;    // 1 bit, 0b1000 0000
    handle->nalu.nal_reference_idc = (handle->nalu.data[0] & 0x60) >> 5;    // 2 bit, 0b0110 0000
    handle->nalu.nal_unit_type = (handle->nalu.data[0] & 0x1f);    // 5 bit, 0b0001 1111

    return 1;
}

static void dump_nalu (const nalu_t *nalu)
{
    if (! nalu) return;

    printf("*** nalu len: %d, ", nalu->len);
    printf("nal_unit_type: %x\n", nalu->nal_unit_type);
}


packet::~packet ()
{
    //dtor
    if (! handle)
        free(handle);
}

//h264Packet::h264Packet()
//{
//
//}

h264Packet::~h264Packet ()
{
    pack_close();
}

struct pac_handle *h264Packet::pack_open ()
{
    handle = (struct pac_handle *) malloc(
            sizeof(struct pac_handle));
    if (! handle) return NULL;

    CLEAR(*handle);
    handle->outbuf = malloc(MAX_OUTBUF_SIZE);
    if (! handle->outbuf)
    {
        printf("--- Failed to malloc RTP output buffer of size: %d", MAX_OUTBUF_SIZE);
        goto err0;
    }
    handle->outbuf_ptr = 0;
    handle->FU_index = 0;
    handle->nalu.data = NULL;
    handle->seq_num = 0;
    handle->ts_current_sample = 0;
    handle->params.max_pkt_len = pacp.max_pkt_len;
    handle->params.ssrc = pacp.ssrc;
    handle->ts_start_millisec = get_current_millisec();    // save the startup time

    printf("+++ Pack Opened\n");
    return handle;

    err0:
    free(handle);
    return NULL;
}

void h264Packet::pack_close ()
{
    free(handle->outbuf);
    free(handle);
    printf("+++ Pack Closed\n");
}

void h264Packet::pack_put (void *inbuf, int isize)
{
    handle->inbuf = inbuf;
    handle->next_nalu_ptr = (char *) handle->inbuf;
    handle->inbuf_size = isize;
    handle->FU_counter = 0;
    handle->last_FU_size = 0;
    handle->FU_index = 0;
    handle->inbuf_complete = 0;
    handle->nalu_complete = 1;    // start a new nalu
}


int h264Packet::pack_get (void **poutbuf, int *outsize)
{
    int ret;

    if (handle->inbuf_complete) return 0;

    // clear output buffer first
    memset(handle->outbuf, 0,
           MAX_OUTBUF_SIZE);    // !!! this is important, missing this may cause werid problems, like VLC displays nothing when the buf is small!
    char *tmp_outbuf = (char *) handle->outbuf;


    rtp_header test;

    // set common rtp header
    rtp_header *rtp_hdr;
    rtp_hdr = (rtp_header *) tmp_outbuf;
    rtp_hdr->payload = H264;
    rtp_hdr->version = 2;
    rtp_hdr->marker = 0;
    rtp_hdr->ssrc = htonl(handle->params.ssrc);    // constant for a RTP session
    if (handle->nalu_complete)    // current nalu complete, find the next nalu in inbuf
    {
        ret = get_next_nalu(handle);
        if (ret <= 0)    // no more nalus
        {
            handle->inbuf_complete = 1;
            return 0;
        }


//		dump_nalu(&handle->nalu);

        rtp_hdr->seq_no = htons(handle->seq_num ++);    // increase for every RTP packet
        handle->ts_current_sample = (U32) ((get_current_millisec() - handle->ts_start_millisec) *
                                           90.0);    // calculate the timestamp for a new NALU
        rtp_hdr->timestamp = htonl(handle->ts_current_sample);

        // handle the new NALU
        if (handle->nalu.len <= handle->params.max_pkt_len)    // no need to fragment
        {
            rtp_hdr->marker = 1;
            nalu_header *nalu_hdr;
            nalu_hdr = (nalu_header *) (tmp_outbuf + 12);
            nalu_hdr->F = handle->nalu.forbidden_bit;
            nalu_hdr->NRI = handle->nalu.nal_reference_idc;
            nalu_hdr->TYPE = handle->nalu.nal_unit_type;
            char *nalu_payload = tmp_outbuf + 13;    // 12 Bytes RTP header + 1 Byte NALU header
            *outsize = handle->nalu.len + 12;
            if (MAX_OUTBUF_SIZE < *outsize)    // check size
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload, handle->nalu.data + 1, handle->nalu.len - 1);    // exclude the nalu header

            handle->nalu_complete = 1;

            *poutbuf = handle->outbuf;
            return 1;
        } else    // fragment needed
        {
            // calculate the fragments
            if (handle->nalu.len % handle->params.max_pkt_len == 0)    // in case divide exactly
            {
                handle->FU_counter = handle->nalu.len
                                     / handle->params.max_pkt_len - 1;
                handle->last_FU_size = handle->params.max_pkt_len;
            } else
            {
                handle->FU_counter = handle->nalu.len
                                     / handle->params.max_pkt_len;
                handle->last_FU_size = handle->nalu.len
                                       % handle->params.max_pkt_len;
            }
            handle->FU_index = 0;

            // it's the first FU
            rtp_hdr->marker = 0;
            fu_indicator *fu_ind = (fu_indicator *) (tmp_outbuf + 12);
            fu_ind->F = handle->nalu.forbidden_bit;
            fu_ind->NRI = handle->nalu.nal_reference_idc;
            fu_ind->TYPE = 28;    // FU_A

            fu_header *fu_hdr = (fu_header *) (tmp_outbuf + 13);
            fu_hdr->E = 0;
            fu_hdr->R = 0;
            fu_hdr->S = 1;    // start bit
            fu_hdr->TYPE = handle->nalu.nal_unit_type;
            char *nalu_payload = tmp_outbuf + 14;
            *outsize = handle->params.max_pkt_len + 14;    // RTP header + FU indicator + FU header
            if (MAX_OUTBUF_SIZE < *outsize)
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload, handle->nalu.data + 1,
                   handle->params.max_pkt_len);

            handle->nalu_complete = 0;    // not complete
            handle->FU_index ++;

            *poutbuf = handle->outbuf;
            return 1;
        }
    } else    // send remaining FUs
    {
        rtp_hdr->seq_no = htons(handle->seq_num ++);
        rtp_hdr->timestamp = htonl(
                handle->ts_current_sample);    // it's a continuation to the last NALU, no need to recalculate

        // check if it's the last FU
        if (handle->FU_index == handle->FU_counter)    // the last FU
        {
            rtp_hdr->marker = 1;    // the last FU
            fu_indicator *fu_ind = (fu_indicator *) (tmp_outbuf + 12);
            fu_ind->F = handle->nalu.forbidden_bit;
            fu_ind->NRI = handle->nalu.nal_reference_idc;
            fu_ind->TYPE = 28;

            fu_header *fu_hdr = (fu_header *) (tmp_outbuf + 13);
            fu_hdr->R = 0;
            fu_hdr->S = 0;
            fu_hdr->TYPE = handle->nalu.nal_unit_type;
            fu_hdr->E = 1;    // the last EU
            char *nalu_payload = tmp_outbuf + 14;
            *outsize = handle->last_FU_size - 1 + 14;
            if (MAX_OUTBUF_SIZE < *outsize)
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload,
                   handle->nalu.data + 1
                   + handle->FU_index * handle->params.max_pkt_len,
                   handle->last_FU_size - 1);    // minus the nalu header

            handle->nalu_complete = 1;    // this nalu is complete
            handle->FU_index = 0;

            *poutbuf = handle->outbuf;
            return 1;
        } else    // middle FUs
        {
            rtp_hdr->marker = 0;
            fu_indicator *fu_ind = (fu_indicator *) (tmp_outbuf + 12);
            fu_ind->F = handle->nalu.forbidden_bit;
            fu_ind->NRI = handle->nalu.nal_reference_idc;
            fu_ind->TYPE = 28;

            fu_header *fu_hdr = (fu_header *) (tmp_outbuf + 13);
            fu_hdr->R = 0;
            fu_hdr->S = 0;
            fu_hdr->TYPE = handle->nalu.nal_unit_type;
            fu_hdr->E = 0;

            char *nalu_payload = tmp_outbuf + 14;
            *outsize = handle->params.max_pkt_len + 14;
            if (MAX_OUTBUF_SIZE < *outsize)
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload,
                   handle->nalu.data + 1
                   + handle->FU_index * handle->params.max_pkt_len,
                   handle->params.max_pkt_len);

            handle->FU_index ++;

            *poutbuf = handle->outbuf;
            return 1;
        }
    }
}

h265Packet::~h265Packet ()
{
    pack_close();
}

struct pac_handle *h265Packet::pack_open ()
{
    handle = (struct pac_handle *) malloc(
            sizeof(struct pac_handle));
    if (! handle) return NULL;

    CLEAR(*handle);
    handle->outbuf = malloc(MAX_OUTBUF_SIZE);
    if (! handle->outbuf)
    {
        printf("--- Failed to malloc RTP output buffer of size: %d", MAX_OUTBUF_SIZE);
        goto err0;
    }
    handle->outbuf_ptr = 0;
    handle->FU_index = 0;
    handle->nalu.data = NULL;
    handle->seq_num = 0;
    handle->ts_current_sample = 0;
    handle->params.max_pkt_len = pacp.max_pkt_len;
    handle->params.ssrc = pacp.ssrc;
    handle->ts_start_millisec = get_current_millisec();    // save the startup time

    printf("+++ Pack Opened\n");
    return handle;

    err0:
    free(handle);
    return NULL;
}

void h265Packet::pack_close ()
{
    free(handle->outbuf);
    free(handle);
    printf("+++ Pack Closed\n");
}

void h265Packet::pack_put (void *inbuf, int isize)
{
    handle->inbuf = inbuf;
    handle->next_nalu_ptr = (char *) handle->inbuf;
    handle->inbuf_size = isize;
    handle->FU_counter = 0;
    handle->last_FU_size = 0;
    handle->FU_index = 0;
    handle->inbuf_complete = 0;
    handle->nalu_complete = 1;    // start a new nalu
}


static int get_next_nalu_hevc (struct pac_handle *handle)
{
    if (! handle->next_nalu_ptr)    // reach data end, no next nalu
    {
        return 0;
    }

    if (is_start_code3(handle->next_nalu_ptr))    // check 0x000001 first
    {
        handle->nalu.startcodeprefix_len = 3;
    } else
    {
        if (is_start_code4(handle->next_nalu_ptr))    // check 0x00000001
        {
            handle->nalu.startcodeprefix_len = 4;
        } else
        {
            printf("!!! No any startcode found\n");
            return - 1;
        }
    }

    // found the next start code
    int startcode_found = 0;
    char *cur_nalu_ptr = handle->next_nalu_ptr;    // rotate, save the next ptr
    char *next_ptr = cur_nalu_ptr + handle->nalu.startcodeprefix_len;    // skip current start code
    while (! startcode_found)
    {
        next_ptr ++;

        if ((next_ptr - (char *) handle->inbuf) >= handle->inbuf_size)    // reach data end
        {
            handle->next_nalu_ptr = NULL;    // no more nalus
            break;
        }

        if (is_start_code3(next_ptr) || is_start_code4(next_ptr))    // found the next start code
        {
            handle->next_nalu_ptr = next_ptr;    // next ptr
            break;
        }
    }

    handle->nalu.data = cur_nalu_ptr + handle->nalu.startcodeprefix_len;    // exclude the start code
    handle->nalu.len = next_ptr - cur_nalu_ptr
                       - handle->nalu.startcodeprefix_len;
    handle->nalu_type = (handle->nalu.data[0] >> 1) & 0x3F;
    static int fseq = 0;
    if (handle->nalu_type >= 16 && handle->nalu_type <= 21)
        printf("I frame-------type:%d with frame seq %d\n", handle->nalu_type, fseq);
    fseq ++;
    return 1;
}


int h265Packet::pack_get (void **poutbuf, int *outsize)
{
    int ret;

    if (handle->inbuf_complete) return 0;

    // clear output buffer first
    memset(handle->outbuf, 0,
           MAX_OUTBUF_SIZE);    // !!! this is important, missing this may cause werid problems, like VLC displays nothing when the buf is small!
    char *tmp_outbuf = (char *) handle->outbuf;
    // set common rtp header
    rtp_header *rtp_hdr;
    rtp_hdr = (rtp_header *) tmp_outbuf;
    rtp_hdr->payload = H264;
    rtp_hdr->version = 2;
    rtp_hdr->marker = 0;
    rtp_hdr->ssrc = htonl(handle->params.ssrc);    // constant for a RTP session
    //int nal_type = (tmp_outbuf[0] >> 1) & 0x3F;
    int nal_type;
    if (handle->nalu_complete)    // current nalu complete, find the next nalu in inbuf
    {
        ret = get_next_nalu_hevc(handle);
        nal_type = handle->nalu_type;
        if (ret <= 0)    // no more nalus
        {
            handle->inbuf_complete = 1;
            return 0;
        }

        rtp_hdr->seq_no = htons(handle->seq_num ++);    // increase for every RTP packet
        handle->ts_current_sample = (U32) ((get_current_millisec() - handle->ts_start_millisec) *
                                           90.0);    // calculate the timestamp for a new NALU

        rtp_hdr->timestamp = htonl(handle->ts_current_sample);

        // handle the new NALU
        if (handle->nalu.len <= handle->params.max_pkt_len)    // no need to fragment
        {
            rtp_hdr->marker = 1;

            char *nalu_payload = tmp_outbuf + 12;    // 12 Bytes RTP header + 1 Byte NALU header
            *outsize = handle->nalu.len + 12;
            if (MAX_OUTBUF_SIZE < *outsize)    // check size
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload, handle->nalu.data, handle->nalu.len);    // exclude the nalu header

            handle->nalu_complete = 1;

            *poutbuf = handle->outbuf;
            return 1;
        } else    // fragment needed
        {
            // calculate the fragments
            if (handle->nalu.len % handle->params.max_pkt_len == 0)    // in case divide exactly
            {
                handle->FU_counter = handle->nalu.len
                                     / handle->params.max_pkt_len - 1;
                handle->last_FU_size = handle->params.max_pkt_len;
            } else
            {
                handle->FU_counter = handle->nalu.len
                                     / handle->params.max_pkt_len;
                handle->last_FU_size = handle->nalu.len
                                       % handle->params.max_pkt_len;
            }
            handle->FU_index = 0;



            // it's the first FU
            rtp_hdr->marker = 0;
            char *naluhdr = (char *) (tmp_outbuf + 12);
            naluhdr[0] = 49 << 1;
            naluhdr[1] = 1;


            naluhdr[2] = nal_type;
            naluhdr[2] |= 1 << 7;


            char *nalu_payload = tmp_outbuf + 15;
            *outsize = handle->params.max_pkt_len + 15;    // RTP header + HEVC payload header + FU header
            if (MAX_OUTBUF_SIZE < *outsize)
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload, handle->nalu.data + 2,
                   handle->params.max_pkt_len);

            handle->nalu_complete = 0;    // not complete
            handle->FU_index ++;

            *poutbuf = handle->outbuf;
            return 1;
        }
    } else    // send remaining FUs
    {
        rtp_hdr->seq_no = htons(handle->seq_num ++);
        rtp_hdr->timestamp = htonl(
                handle->ts_current_sample);    // it's a continuation to the last NALU, no need to recalculate
        // check if it's the last FU
        if (handle->FU_index == handle->FU_counter)    // the last FU
        {
            rtp_hdr->marker = 1;    // the last FU
            char *naluhdr = (char *) (tmp_outbuf + 12);
            naluhdr[0] = 49 << 1;
            naluhdr[1] = 1;
            naluhdr[2] = nal_type;
            naluhdr[2] |= 1 << 6;

            char *nalu_payload = tmp_outbuf + 15;
            *outsize = handle->last_FU_size + 15 - 2;
            if (MAX_OUTBUF_SIZE < *outsize)
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload,
                   handle->nalu.data + 2
                   + handle->FU_index * handle->params.max_pkt_len,
                   handle->last_FU_size);    // minus the nalu header

            handle->nalu_complete = 1;    // this nalu is complete
            handle->FU_index = 0;

            *poutbuf = handle->outbuf;
            return 1;
        } else    // middle FUs
        {
            rtp_hdr->marker = 0;    // the mid FU
            char *naluhdr = (char *) (tmp_outbuf + 12);
            naluhdr[0] = 49 << 1;
            naluhdr[1] = 1;
            naluhdr[2] = nal_type;
            naluhdr[2] &= ~ (1 << 7);

            char *nalu_payload = tmp_outbuf + 15;
            *outsize = handle->params.max_pkt_len + 15;
            if (MAX_OUTBUF_SIZE < *outsize)
            {
                printf("--- RTP max output buffer size %d < pack size %d\n", MAX_OUTBUF_SIZE,
                       *outsize);
                abort();
            }
            memcpy(nalu_payload,
                   handle->nalu.data + 2
                   + handle->FU_index * handle->params.max_pkt_len,
                   handle->params.max_pkt_len);

            handle->FU_index ++;

            *poutbuf = handle->outbuf;
            return 1;
        }
    }

}
