#include "log.hpp"
#include "blockqueue.hpp"
#include <bits/types/struct_timeval.h>
#include <bits/types/struct_tm.h>
#include <bits/types/time_t.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <memory>
#include <mutex>
#include <sys/select.h>

Log::Log() {
    m_LineCount = 0;
    m_isasync = false;
    write_thread_ptr = nullptr;
    m_deq_ptr = nullptr;
    m_today = 0;
    m_fp = nullptr; 
}

Log::~Log() {
    if(write_thread_ptr && write_thread_ptr->joinable()) { //如果写线程还没有停止
        while(!m_deq_ptr->empty()) {
            m_deq_ptr->flush();
        }
        m_deq_ptr->Close();
        write_thread_ptr->join();
    }
    if(m_fp) {
        std::lock_guard<std::mutex> lck(m_mtx);
        flush();
        fclose(m_fp);
    }
}


bool Log::IsOpen() {
    return m_isopen;
}

// 获取日志级别
int Log::GetLevel() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_level;
}

//设置日志级别
void Log::SetLevel(int level) {
    std::lock_guard<std::mutex> lck(m_mtx);
    m_level = level;
}

void Log::init(int level = 1, const char* path, const char* suffix, int MaxQueueCap) {
    m_isopen = true;
    m_level = level;
    if(MaxQueueCap > 0) {
        m_isasync = true; // 是异步的
        if(!m_deq_ptr) {
            std::unique_ptr<BlockDeque<std::string>> newdeq(new BlockDeque<std::string>);
            m_deq_ptr = std::move(newdeq); // 使用 std::move 转移指针的所有权

            std::unique_ptr<std::thread> newthread(new std::thread(FlushLogThread));
            
            write_thread_ptr = std::move(newthread);
        }
    }else {
        m_isasync = false;
    }


    m_LineCount = 0;
    time_t timer = time(nullptr);
    tm *systime = localtime(&timer);
    tm t = *systime;
    m_path = path;
    m_suffix = suffix;

    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s%04d_%02d_%02d%s",
        m_path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, m_suffix);
    //log文件的名称命名 path_year_mon_day_suffix
    
    m_today = t.tm_mday;
    {
        std::lock_guard<std::mutex> lck(m_mtx);
        m_buff.RetrieveAll(); //清空buff
        if(m_fp) {
            flush();
            fclose(m_fp);
        }//如果有没关闭的fp，先关闭

        m_fp = fopen(fileName, "a");
        if(m_fp == nullptr) {
            mkdir(m_path, 0777);
            m_fp = fopen(fileName, "a");
        }//打开指定文件
        assert(m_fp != nullptr);
    }  
}

void Log::write(int level, const char* format, ...) {
    timeval now =  {0, 0};  //更加精确
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    tm* sysTime = localtime(&tSec);
    tm t = *sysTime;

    va_list vaList;

    //如果日期不相等 或者文件写满的话
    if(m_today != t.tm_mday || (m_LineCount && (m_LineCount % m_MAX_LINES == 0))) {
        std::unique_lock<std::mutex> lck(m_mtx);
        lck.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d",t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if(m_today != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", m_path, tail, m_suffix);
            m_today = t.tm_mday; //更新m_today
            m_LineCount = 0; //清空计数
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", m_path, tail, (m_LineCount / MAX_LINES), m_suffix);
        }
        lck.lock();

        flush();
        fclose(m_fp);
        m_fp = fopen(newFile, "a");
        //关闭旧文件打开新文件
        assert(m_fp != nullptr);
    } 




    {
        std::unique_lock<std::mutex> lck(m_mtx);
        m_LineCount++;
        int n = snprintf(m_buff.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        m_buff.HasWritten(n); //先向buff写时间
        AppendLogLevelTitle(level); //等级
        
        va_start(vaList, format); //初始化va_list
        int m = vsnprintf(m_buff.BeginWrite(), m_buff.WritableBytes(), format, vaList); //根据format将可变参数输出到数组中
        va_end(vaList); // 结束对可变参数列表的访问

        m_buff.HasWritten(m);//向buff写内容
        m_buff.Append("\n\0", 2); //追加换行符和\0
        if(m_isasync && m_deq_ptr && !m_deq_ptr->full()) {
            //满足条件的话就把buff中的内容全部加入到阻塞队列中
            m_deq_ptr->push_back(m_buff.RetrieveAllToStr());
        }else {
            //否则的话将buff中当前位置开始直到\0输出到文件中
            fputs(m_buff.Peek(), m_fp);
        }
        m_buff.RetrieveAll();
    }
}


void Log::AppendLogLevelTitle(int level) {
    switch(level) {
    case 0:
        m_buff.Append("[debug]: ", 9);
        break;
    case 1:
        m_buff.Append("[info] : ", 9);
        break;
    case 2:
        m_buff.Append("[warn] : ", 9);
        break;
    case 3:
        m_buff.Append("[error]: ", 9);
        break;
    default:
        m_buff.Append("[info] : ", 9);
        break;
    }
}


void Log::flush() {
    if(m_isasync) {
        m_deq_ptr->flush();
        //唤醒一个消费者线程看看执不执行任务
    }
    fflush(m_fp);
    //刷新缓冲区
}

void Log::AsynWrite() {
    std::string str = "";
    while(m_deq_ptr->pop(str)) { //如果阻塞队列有任务
        std::lock_guard<std::mutex> lck(m_mtx);
        fputs(str.c_str(), m_fp);
    }
}

//饿汉模式
Log* Log::Instance() {
    static Log log;
    return &log;
}


//调用异步写
void Log::FlushLogThread() {
    Log::Instance()->AsynWrite();
}


