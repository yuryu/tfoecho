#include <iostream>
#include <cstring>

static void cerror(const char *str)
{
    std::cerr << str << ": " << std::strerror(errno) << std::endl;
}

class FD
{
private:
    int fd_;
public:
    FD(int fd): fd_(fd) {}
    ~FD()
    {
        this->close();
    }
    operator int() const
    {
        return fd_;
    }
    void close()
    {
        if( fd_ == -1 ) return;
        ::close(fd_);
        fd_ = -1;
    }
};

