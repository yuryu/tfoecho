#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <random>
#include <functional>
#include <limits>
#include <algorithm>
#include <thread>
#include <atomic>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "common.h"

#define SERVER_PORT "32345"

int getsock(const std::string &server_addr, const std::vector<char> &msg_to_send)
{
    struct addrinfo hint, *res;
    
    std::memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    
    int e = getaddrinfo(server_addr.c_str(), SERVER_PORT, &hint, &res);
    if( e == -1 )
    {
        cerror("getaddrinfo");
        std::exit(EXIT_FAILURE);
    }
    int sock;
    struct addrinfo *rp; 
    for( rp = res; rp != NULL; rp = rp->ai_next )
    {
        // Need to create the socket again when EINTR is returned
redo:
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if( sock == -1 )
            continue;
        ssize_t r = sendto(sock, &msg_to_send[0], msg_to_send.size(),
                MSG_FASTOPEN, rp->ai_addr, rp->ai_addrlen);
        if( r == -1 && errno == EINTR )
        {
            close(sock);
            goto redo;
        }
        // Exit the loop when sendto succeeded
        if( r != -1 )
            break;
        // Try without TCP FastOpen
        r = connect(sock, rp->ai_addr, rp->ai_addrlen);
        if( r == -1 && errno == EINTR )
        {
            close(sock);
            goto redo;
        }
        if( r != -1 )
        {
            ssize_t sent;
            do
            {
                sent = send(sock, &msg_to_send[0], msg_to_send.size(), 0);
            } while( sent == -1 && sent == EINTR );
            // Success
            if( r != -1 )
                break;
        }
        close(sock);
    }
    freeaddrinfo(res);
    if( rp == NULL )
    {
        std::cerr << "Couldn't get a socket." << std::endl;
        std::exit(EXIT_FAILURE);
    }
    return sock;
}

void send_recv(const std::string &server_addr, const int msg_size, std::atomic_int &count)
{
    std::vector<char> msg_buf(msg_size), recv_buf(msg_size);
    
    std::mt19937 rand_engine;
    std::uniform_int_distribution<char> distribution(
        std::numeric_limits<char>::min(), std::numeric_limits<char>::max());
    auto generator = std::bind(distribution, rand_engine);
    while( count-- > 0 )
    {
        std::generate(msg_buf.begin(), msg_buf.end(), generator);
        FD sock(getsock(server_addr, msg_buf));
        shutdown(sock, SHUT_WR);
        ssize_t read_bytes = 0;
        while( read_bytes != msg_size )
        {
            ssize_t r;
            do
            {
                r = recv(sock, &recv_buf[read_bytes], recv_buf.size() - read_bytes, 0);
            } while( r == -1 && errno == EINTR );
            if( r == 0 ) break;
            if( r == -1 )
            {
                cerror("recv");
                std::exit(EXIT_FAILURE);
            }
            read_bytes += r;
        }
        if( msg_buf != recv_buf )
            std::cerr << "Receive mismatch." << std::endl;
    }
}

int main(int argc, char *argv[])
{
    if( argc <= 4 )
    {
        std::cerr << "Usage: client [server addr] [msg size] [count] [thread]" << std::endl;
        return EXIT_FAILURE;
    }
    
    std::string server_addr(argv[1]);
    int msg_size = std::atoi(argv[2]);
    std::atomic_int count(std::atoi(argv[3]));
    int nthreads = std::atoi(argv[4]);

    std::vector<std::thread> threads;
    threads.reserve(nthreads);
    for( int i = 0; i < nthreads; ++i )
        threads.push_back(std::thread(send_recv, server_addr, msg_size, std::ref(count)));
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
}

