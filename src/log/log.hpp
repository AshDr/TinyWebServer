#ifndef __LOG_HPP
#define __LOG_HPP

#include <bits/types/FILE.h>
#include <memory>
#include <mutex>
#include <cstring>
#include <string>
#include <thread>
#include <sys/time.h>
#include <stdarg.h>
#include <cassert>
#include <sys/stat.h>
#include "../buffer/buffer.hpp"
#include "blockqueue.hpp"

class Log {
public:
    void init(int level, const char* path = "./log", const char* suffix = ".log", int MaxQueueCap = 1024);

    //单例模式 获取日志实例
    static Log* Instance();

    //刷新日志线程
    static void FlushLogThread();

    //写日志 还要有级别
    void write(int level, const char* format, ...);
    
    //刷新
    void flush();
    
    //获取日志级别 0 debug  1 info  2 warn  3 error
    int GetLevel();

    //设置？
    void SetLevel(int level);

    //判断是否日志可写
    bool IsOpen();
    
private:
    Log();
    virtual ~Log();
    void AppendLogLevelTitle(int level);
    void AsynWrite();

private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

    const char* m_path;
    const char* m_suffix;
    int m_MAX_LINES;

    int m_LineCount; //行计数

    int m_today; //今天的日期

    bool m_isopen;
    Buffer m_buff;

    int m_level;
    bool m_isasync;

    FILE* m_fp;
    //控制一个阻塞队列实例
    std::unique_ptr<BlockDeque<std::string>> m_deq_ptr;
    //控制一个写线程
    std::unique_ptr<std::thread> write_thread_ptr;
    std::mutex m_mtx;

};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if(log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    }while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);



#endif