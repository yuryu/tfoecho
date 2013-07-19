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
#include <netinet/tcp.h>
#include <netdb.h>

#define DEFAULT_PORT "32345"
#define TFO_QLEN 128
#define LISTEN_QLEN 128
#define BUFFER_SIZE 1024

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
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
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
        std::cerr << "setsockopt:" << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }
    if( listen(sock, LISTEN_QLEN) == -1 )
    {
        close(sock);
        std::cerr << "listen:" << strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }
    return sock;
}

void server(const std::string &port)
{
    int sfd = open_socket(port);
    std::vector<char> buf(BUFFER_SIZE);

    while( 1 )
    {
        struct sockaddr addr;
        socklen_t addrlen = sizeof(addr);
        int s = accept(sfd, &addr, &addrlen);
        if( s == -1 )
        {
            if( errno == EINTR ) continue;
            close(sfd);
            std::cerr << "accept:" << strerror(errno) << std::endl;
            std::exit(EXIT_FAILURE);
        }
        ssize_t rs;
        while( (rs = recv(s, &buf[0], buf.size(), 0)) != 0 )
        {
            if( rs == -1 )
            {
                if( errno == EINTR ) continue;
                std::cerr << "recv:" << strerror(errno) << std::endl;
                break;
            }
            send(s, &buf[0], rs, 0);
        }
        close(s);
    }
    close(sfd);
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
