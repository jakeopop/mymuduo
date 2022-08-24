#pragma once

#include <vector>
#include <assert.h>
#include <string>

/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size

// The buffer type definition at the bottom of the network library
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == initialSize);
        assert(prependableBytes() == kCheapPrepend);
    }

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    // Returns the starting address of the readable data in the buffer
    const char* peek() const { return begin() + readerIndex_;} 
    
    // onMessage  Buffer  ->   string 
    void retrieve(size_t len) 
    {
        assert(len <= readableBytes());
        if (len < readableBytes())
        {
            readerIndex_ += len;
        }
        else
        {
            retrieveAll();
        }
    }
    void retrieveAll() 
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // Convert the Buffer data reported by the onMessage to string type and return it
    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); } // Readable data length
    std::string retrieveAsString(size_t len) 
    {
        assert(readableBytes() >= len);
        std::string result(peek(), len);
        retrieve(len); // The readable data in the buffer is read out, buffer must be reset here.
        return result;
    }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    // Expansion: Append the len memory into the writable buffer 
    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite()); // data -> buffer
        writerIndex_ += len;
    }

    // Read data directly into buffer 从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
    // Write data directly into buffer 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);

private:
    char* begin() { return &*buffer_.begin(); }  // The address of the first element of the underlying vector array
    const char* begin() const { return &*buffer_.begin(); }

    /*
        --> prependableBytes() = kCheapPrepend + white
        --> writableBytes() + prependableBytes() == kCheapPrepend + white + write
        --> white + write < len !!!
        kCheapPrepend |white| reader | write |
        kCheapPrepend |      len      | reader |
    */
    void makeSpace(size_t len)
    {
       if (writableBytes() + prependableBytes() < len + kCheapPrepend)
       {
            // FIXME: move readable data
            buffer_.resize(writerIndex_ + len); // think carefully!!!
       }
       else
       {
            // move readable data to the front, make space inside buffer
            assert(kCheapPrepend < readerIndex_);
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = kCheapPrepend + readable;
            assert(readable == readableBytes());
       }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};