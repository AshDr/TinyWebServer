#include "buffer.hpp"
#include <strings.h>
#include <sys/uio.h>
#include <unistd.h>

Buffer::Buffer(int initBufferSize): m_buffer(initBufferSize),m_readpos(0),m_writepos(0){}

size_t Buffer::ReadableBytes() const {
    return m_writepos - m_readpos;
}

size_t Buffer::WritableBytes() const {
    return m_buffer.size() - m_writepos;
}

//已经读了多少
size_t Buffer::PrependableBytes() const { 
    return m_readpos;
}

//返回当前开始读的位置
const char* Buffer::Peek() const {
    return BeginPtr_() + m_readpos;
}


//读len这么多
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    m_readpos += len;
}

//缓冲区清零
void Buffer::RetrieveAll() {
    bzero(&m_buffer[0], m_buffer.size()); // std::fill ?
    m_readpos = 0;
    m_writepos = 0;
}

//取到end指针指向的地址位置?
void Buffer::RetrieveUntil(const char *end) {
    assert(Peek() <= end);
    Retrieve(end - Peek());
}

//将所有数据转换为string返回
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}


//当前开始写的位置
char* Buffer::BeginWrite() {
    return BeginPtr_() + m_writepos;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + m_writepos;
}


//根据循环buffer大小与len的关系判断是否扩充
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {
        m_buffer.resize(m_writepos + len + 1);
    }else {
        size_t ReadableNum = ReadableBytes();
        std::copy(BeginPtr_() + m_readpos, BeginPtr_() + m_writepos, BeginPtr_());
        // 否则把这一段移动到前面去，把后面腾出地方来
        m_readpos = 0;
        m_writepos = m_readpos + ReadableNum;
        assert(ReadableNum == ReadableBytes());
    }
}



void Buffer::EnsureWritable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

//确认写这么多
void Buffer::HasWritten(size_t len) {
    m_writepos += len;
}

//向buffer中写
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWritable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& t_buffer) {
    Append(t_buffer.Peek(), t_buffer.ReadableBytes());
}


void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}


ssize_t Buffer::ReadFd(int fd, int *Errno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    iov[0].iov_base = BeginPtr_() + m_writepos; //第一个缓冲区的位置
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2); //将数据填充到 iov 数组指定的多个缓冲区中

    if(len < 0) {
        *Errno = errno;
    }else if(static_cast<size_t>(len) <= writable) {
        m_writepos += len;
    }else {
        //超过buffer可容纳长度
        m_writepos = m_buffer.size();
        Append(buff, len - writable); // 将写到buff中的剩下那一段利用Append塞进去
        //如果失败的话 在makespace会触发assert
    }
    return len;
}

//将buffer中能读的内容全写到fd中
ssize_t Buffer::WriteFd(int fd, int *Errno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);

    if(len < 0) {
        *Errno = errno;
        return len;
    }
    m_readpos += len;
    return len;
}



char* Buffer::BeginPtr_() {
    return &*m_buffer.begin();
}

const char* Buffer::BeginPtr_() const{
    return  &*m_buffer.begin();
}
