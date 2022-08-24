#include "Buffer.h"
#include "Types.h"

#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>


/**
 * @readv() 
 * system call reads iovcnt buffers from the file associated with the fd into the iov-buffers
 * 将统一缓冲区的数据写入一个不连续的内存地址块
 * nice tip: Write the data of the same buffer to a non-contiguous memory address block
 * struct iovec {
 *    void  *iov_base;    Starting address 
 *    size_t iov_len;     Number of bytes to transfer 
 * };
 */

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;


/**
 * read data from fd  |  Poller works in LT mode(Reminder every time a message comes)
 * Buffer-buffers are sized! 
 * But when reading data from fd, not know the final size of TCP-Buffers
 */
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0}; // memory space on the stack
    struct iovec vec[2];

    const size_t writable = writableBytes(); // The remaining writable space of the underlying buffer
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (implicit_cast<size_t>(n) <= writable)
    {
        writerIndex_ += n;
    }
    else //extrabuf have data
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // writerIndex_ begin to write (n - writable)data
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}