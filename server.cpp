#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>
#include <netdb.h>

#define DEFAULT_PORT "32345"
#define TFO_QLEN 128
#define LISTEN_QLEN 128
#define BUFFER_SIZE 4096
#define MAX_EVENTS 32

#include "common.h"

int open_socket(const std::string &port)
{
    struct addrinfo hint, *res;
    
    std::memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
    int e = getaddrinfo(NULL, port.c_str(), &hint, &res);
    if( e != 0 )
    {
        std::cerr << "getaddrinfo: " << gai_strerror(e) << std::endl;
        std::exit(EXIT_FAILURE);
    }
    
    int sock;
    struct addrinfo *rp;
    for( rp = res; rp != NULL; rp = rp->ai_next )
    {
        sock = socket(rp->ai_family, rp->ai_socktype | SOCK_NONBLOCK, rp->ai_protocol);
        if( sock == -1 ) continue;
        if( bind(sock, rp->ai_addr, rp->ai_addrlen) == 0 ) break;
        close(sock);
    }
    if( rp == NULL )
    {
        std::cerr << "Couldn't bind." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    int protocol = rp->ai_protocol;
    freeaddrinfo(res);
    
    const int qlen = TFO_QLEN;
    e = setsockopt(sock, protocol, TCP_FASTOPEN, &qlen, sizeof(qlen));
    if( e == -1 )
    {
        close(sock);
        cerror("setsockopt");
        std::exit(EXIT_FAILURE);
    }
    if( listen(sock, LISTEN_QLEN) == -1 )
    {
        close(sock);
        cerror("listen");
        std::exit(EXIT_FAILURE);
    }
    return sock;
}

int accept_and_register(int sfd, int epollfd)
{
    // Accept a new connection
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    int s;
    do
    {
        s = accept4(sfd, &addr, &addrlen, SOCK_NONBLOCK);
    } while( s == -1 && errno == EINTR );
    if( s == -1 )
    {
        cerror("accept");
        std::exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = s;
    if( epoll_ctl(epollfd, EPOLL_CTL_ADD, s, &ev) == -1 )
    {
        perror("epoll_ctl: EPOLL_CTL_ADD");
        std::exit(EXIT_FAILURE);
    }
    return s;
}

void process_client_socket(int s, int epollfd)
{
    static std::vector<char> buf(BUFFER_SIZE);
    ssize_t rs;
    while( (rs = recv(s, &buf[0], buf.size(), 0)) != 0 )
    {
        if( rs == -1 )
        {
            if( errno == EINTR ) continue;
            if( errno == EAGAIN || errno == EWOULDBLOCK ) return;
            cerror("recv");
            break;
        }
        ssize_t sent;
        // EAGAIN or EWOULDBLOCK is retuned when the socket buffer is full
        // We should try with another socket for maximum performance, however,
        // we just wait until the buffer becomes available as we don't have
        // enough buffer for that.
        do {
            sent = send(s, &buf[0], rs, 0);
        } while( sent == -1 && 
                 (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) );
        if( sent == -1 )
        {
            cerror("send");
            break;
        }
    }
    if( epoll_ctl(epollfd, EPOLL_CTL_DEL, s, NULL) == -1 )
        cerror("epoll_ctl: EPOLL_CTL_DEL");
    close(s);
}

void server(const std::string &port)
{
    FD sfd(open_socket(port));
    
    struct epoll_event ev, events[MAX_EVENTS];

    FD epollfd(epoll_create1(0));
    if( epollfd == -1 )
    {
        cerror("epoll_create1");
        std::exit(EXIT_FAILURE);
    }
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if( epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1 )
    {
        perror("epoll_ctl: EPOLL_CTL_ADD");
        std::exit(EXIT_FAILURE);
    }

    while( 1 )
    {
        const int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if( nfds == -1 )
        {
            if( errno == EINTR ) continue;
            cerror("epoll_wait");
            std::exit(EXIT_FAILURE);
        }

        for( int i = 0; i < nfds; ++i )
        {
            if( events[i].data.fd == sfd )
                accept_and_register(sfd, epollfd);
            else
                process_client_socket(events[i].data.fd, epollfd);
        }
    }
}

int main(int argc, char *argv[])
{
    std::string port;
    if( argc <= 1 )
    {
        port = DEFAULT_PORT;
    } else {
        port = argv[1];
    }

    server(port);
}
