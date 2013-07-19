tfoecho
=======

Test code for TCP Fast Open

## TCP Fast Open
TCP Fast Open is an extension to TCP Protocol which enables to transfer data in SYN packets.
For more details, read an LWN article: http://lwn.net/Articles/508865/

## Build

    g++ -o server -std=c++11 server.cpp
    g++ -o client -std=c++11 client.cpp

## Run

On server:

    ./server

On Client:

    ./client <server addr> <message size> <test count>

for example, `./client 192.168.1.1 1024 100` sends random packets of 1024 bytes 100 times.
