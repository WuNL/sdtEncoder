#ifndef PACKET_H
#define PACKET_H

#include "stdint.h"
#include <stdio.h>

typedef unsigned int U32;
#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define UNUSED(expr) do { (void)(expr); } while (0)
struct pac_param
{
    int max_pkt_len;    // maximum packet length, better be less than MTU(1500)
    int ssrc;            // identifies the synchronization source, set the value randomly, with the intent that no two synchronization sources within the same RTP session will have the same SSRC
};
struct pac_handle;


class packet
{
public:
    packet (uint32_t len, uint32_t ssrc)
    {
        pacp.max_pkt_len = len;
        pacp.ssrc = ssrc;
    }

    virtual ~packet () = 0;

    virtual struct pac_handle *pack_open () = 0;

    virtual void pack_put (void *inbuf, int isize) = 0;

    virtual int pack_get (void **poutbuf, int *outsize) = 0;

    virtual void pack_close () = 0;

protected:
    struct pac_param pacp;
    struct pac_handle *handle;
private:
};


class h264Packet : public packet
{
public:
    h264Packet (uint32_t len, uint32_t ssrc) : packet(len, ssrc)
    {
        pack_open();
    }

    virtual ~h264Packet ();

    struct pac_handle *pack_open ();

    /**
     * @brief Put one or more NALUs to be packed
     * @param handle the pack handle
     * @param inbuf the buffer pointed to one or more NALUs
     * @param isize the inbuf size
     */
    void pack_put (void *inbuf, int isize);

    /**
     * @brief Get a requested packet
     * @param handle the pack handle
     * @param poutbuf the out packet
     * @param outsize the size of the packet
     * @note the bufsize should be bigger than *outsize
     */
    int pack_get (void **poutbuf, int *outsize);

    void pack_close ();

protected:

private:

};


class h265Packet : public packet
{
public:
    h265Packet (uint32_t len, uint32_t ssrc) : packet(len, ssrc)
    {
        pack_open();
    }

    virtual ~h265Packet ();

    struct pac_handle *pack_open ();

    /**
     * @brief Put one or more NALUs to be packed
     * @param handle the pack handle
     * @param inbuf the buffer pointed to one or more NALUs
     * @param isize the inbuf size
     */
    void pack_put (void *inbuf, int isize);

    /**
     * @brief Get a requested packet
     * @param handle the pack handle
     * @param poutbuf the out packet
     * @param outsize the size of the packet
     * @note the bufsize should be bigger than *outsize
     */
    int pack_get (void **poutbuf, int *outsize);

    void pack_close ();

protected:

private:
};

#endif // PACKET_H
