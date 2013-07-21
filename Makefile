CXX = g++
CXXFLAGS = -O -std=c++11 -Wall
TARGETS = client server

.PHONY: all clean

all: $(TARGETS)

client: client.cpp common.h
	$(CXX) $(CXXFLAGS) -pthread -o $@ $<

server: server.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) 

