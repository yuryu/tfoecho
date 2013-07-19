tfoecho
=======

Test code for TCP Fast Open

## TCP Fast Open

TCP Fast Open is an extension to TCP Protocol which enables to transfer data in SYN packets.
For more details, read an LWN article: http://lwn.net/Articles/508865/

## Requirements

- Linux 3.7 or later
- C++11 compliant compiler

## Build

    g++ -o server -std=c++11 server.cpp
    g++ -o client -std=c++11 client.cpp

## Run

Before running:

Make sure to enable TCP Fast open on both sides:

    sudo sysctl -w net.ipv4.tcp_fastopen=3

On server:

    ./server

On Client:

    ./client <server addr> <message size> <test count>

for example, `./client 192.168.1.1 1024 100` sends random packets of 1024 bytes 100 times.
