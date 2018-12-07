#ifndef NETWORK_H
#define NETWORK_H

#include <string.h>

typedef unsigned int U32;
#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define UNUSED(expr) do { (void)(expr); } while (0)

enum net_t
{
    UDP = 0, TCP
};

struct net_param
{
    enum net_t type;        // UDP or TCP?
    char *serip;            // server ip, eg: "127.0.0.1"
    int serport;            // server port, eg: 8000
};

struct net_handle;

struct net_handle *net_open (struct net_param params);

int net_send (struct net_handle *handle, void *data, int size);

int net_recv (struct net_handle *handle, void *data, int size);

void net_close (struct net_handle *handle);

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

struct net_handle
{
    int sktfd;
    struct sockaddr_in server_sock;
    int sersock_len;

    struct net_param params;
};

struct net_handle *net_open (struct net_param params);

void net_close (struct net_handle *handle);

int net_send (struct net_handle *handle, void *data, int size);

int net_recv (struct net_handle *handle, void *data, int size);


class network
{
public:
    network (struct net_param params);

    virtual ~network ();

    struct net_handle *net_open (struct net_param params);

    int net_send (void *data, int size);

    int net_recv (void *data, int size);

    void net_close ();

protected:

private:
    struct net_handle *handle;
};

#endif // NETWORK_H
