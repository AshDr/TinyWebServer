#ifndef __BUFFER_HPP
#define __BUFFER_HPP

#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <cassert>
#include <atomic>

class Buffer {
public:
    Buffer(int initBufferSize = 1024);
    ~Buffer() = default;
    size_t WritableBytes() const;
    size_t ReadableBytes() const;
    size_t PrependableBytes() const;
    
    const char* Peek() const;
    void EnsureWritable(size_t len);
    void HasWritten(size_t len);
    
    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);
    void RetrieveAll();
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& t_buffer);

    ssize_t ReadFd(int fd, int* Errno);
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();
    const char* BeginPtr_() const;
    void MakeSpace_(size_t len);
    std::vector<char> m_buffer;
    std::atomic<std::size_t> m_readpos;
    std::atomic<std::size_t> m_writepos;
};



#endif //! End of Buffer.hpp