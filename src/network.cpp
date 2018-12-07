#include "network.h"

network::network (struct net_param params)
{
    //ctor
    net_open(params);
}

network::~network ()
{
    //dtor
    net_close();
}

struct net_handle *network::net_open (struct net_param params)
{
    handle = (struct net_handle *) malloc(
            sizeof(struct net_handle));
    if (! handle)
    {
        printf("--- malloc net handle failed\n");
        return NULL;
    }

    CLEAR(*handle);
    handle->params.type = params.type;
    handle->params.serip = params.serip;
    handle->params.serport = params.serport;

    if (handle->params.type == TCP)
        handle->sktfd = socket(AF_INET, SOCK_STREAM, 0);
    else
        // UDP
        handle->sktfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (handle->sktfd < 0)
    {
        printf("--- create socket failed\n");
        free(handle);
        return NULL;
    }

    handle->server_sock.sin_family = AF_INET;
    handle->server_sock.sin_port = htons(handle->params.serport);
    handle->server_sock.sin_addr.s_addr = inet_addr(handle->params.serip);
    handle->sersock_len = sizeof(handle->server_sock);

    int ret = connect(handle->sktfd, (struct sockaddr *) &handle->server_sock,
                      handle->sersock_len);
    if (ret < 0)
    {
        printf("--- connect to server failed\n");
        close(handle->sktfd);
        free(handle);
        return NULL;
    }

    printf("+++ Network Opened\n");
    return handle;
}

int network::net_send (void *data, int size)
{
    return send(handle->sktfd, data, size, 0);
}

int network::net_recv (void *data, int size)
{
    return recv(handle->sktfd, data, size, 0);
}

void network::net_close ()
{
    close(handle->sktfd);
    free(handle);
    printf("+++ Network Closed\n");
}


struct net_handle *net_open (struct net_param params)
{
    struct net_handle *handle = (struct net_handle *) malloc(
            sizeof(struct net_handle));
    if (! handle)
    {
        printf("--- malloc net handle failed\n");
        return NULL;
    }

    CLEAR(*handle);
    handle->params.type = params.type;
    handle->params.serip = params.serip;
    handle->params.serport = params.serport;

    if (handle->params.type == TCP)
        handle->sktfd = socket(AF_INET, SOCK_STREAM, 0);
    else
        // UDP
        handle->sktfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (handle->sktfd < 0)
    {
        printf("--- create socket failed\n");
        free(handle);
        return NULL;
    }

    handle->server_sock.sin_family = AF_INET;
    handle->server_sock.sin_port = htons(handle->params.serport);
    handle->server_sock.sin_addr.s_addr = inet_addr(handle->params.serip);
    handle->sersock_len = sizeof(handle->server_sock);

    int ret = connect(handle->sktfd, (struct sockaddr *) &handle->server_sock,
                      handle->sersock_len);
    if (ret < 0)
    {
        printf("--- connect to server failed\n");
        close(handle->sktfd);
        free(handle);
        return NULL;
    }

    printf("+++ Network Opened\n");
    return handle;
}

void net_close (struct net_handle *handle)
{
    close(handle->sktfd);
    free(handle);
    printf("+++ Network Closed\n");
}

int net_send (struct net_handle *handle, void *data, int size)
{
    return send(handle->sktfd, data, size, 0);
}

int net_recv (struct net_handle *handle, void *data, int size)
{
    return recv(handle->sktfd, data, size, 0);
}
