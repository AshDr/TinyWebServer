#ifndef __BLOCKQUEUE_HPP
#define __BLOCKQUEUE_HPP

#include <cstddef>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>
#include <cassert>


//阻塞队列
template<typename T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);
    ~BlockDeque();
    void clear();
    bool empty();
    bool full();
    void Close();
    size_t size();
    size_t capacity();
    T front();
    T back();
    void push_back(const T& item);
    void push_front(const T& item);
    bool pop(T &item);
    bool pop(T&teim, int timeout);
    void flush();
private:
    std::deque<T> m_deq;
    size_t m_cap;
    std::mutex m_mtx;
    bool m_is_close;
    std::condition_variable m_cond_consumer;
    std::condition_variable m_cond_producer;
};

template<typename T>
BlockDeque<T>::BlockDeque(size_t t_Maxcap): m_cap(t_Maxcap){
    assert(t_Maxcap > 0);
    m_is_close = false;
}

template<typename T>
BlockDeque<T>::~BlockDeque<T>() {
    Close();
}

template<typename T>
void BlockDeque<T>::Close() {
    {
        std::lock_guard<std::mutex> lck(m_mtx);
        m_deq.clear();
        m_is_close = true;
    }
    m_cond_producer.notify_all();
    m_cond_consumer.notify_all();
}


template<typename T>
void BlockDeque<T>::flush() {
    m_cond_consumer.notify_one();
}


template<typename T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_deq.clear();
}

template<typename T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_deq.front();
}


template<typename T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_deq.back();
}

template<typename T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_deq.size();
}


template<typename T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_cap;
}

template<typename T>
void BlockDeque<T>::push_back(const T& item) {
    std::unique_lock<std::mutex> lck(m_mtx);
    while(m_deq.size() >= m_cap) {
        //队列长度大于容量说明目前队列消息过多需要先处理
        m_cond_producer.wait(lck);

    }
    m_deq.push_back(item);
    m_cond_consumer.notify_one();
}

template<typename T>
void BlockDeque<T>::push_front(const T&item) {
    std::unique_lock<std::mutex> lck(m_mtx);
    while(m_deq.size() >= m_cap) {
        m_cond_producer.wait(lck);

    }
    m_deq.push_front(item);
    m_cond_consumer.notify_one();
}


template<typename T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_deq.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_deq.size() >= m_cap;
}

template<typename T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> lck(m_mtx);
    while(m_deq.empty()) {
        m_cond_consumer.wait(lck);
        if(m_is_close) {
            return false;
        }
    }
    item = m_deq.front();
    m_deq.pop_front();
    m_cond_producer.notify_one();
    return true;
}


template<typename T>
bool BlockDeque<T>::pop(T& item, int timeout) {
    std::unique_lock<std::mutex> lck(m_mtx);
    while(m_deq.empty()) {
        //当超时发生时，等待线程会被唤醒，并且会尝试重新获取互斥锁执行后续的代码
        //因此，超时时的行为就像是等待线程被唤醒一样，但是没有其他线程的通知。
        /*
        超时发生时，等待线程虽然被唤醒，但并不意味着条件已经满足。可能是由于超时而唤醒，
        而不是其他线程发出的通知。因此，在这种情况下，等待线程无法确保共享资源已经处于可用状态,
        因此需要返回获取失败
        */
        if(m_cond_consumer.wait_for(lck, std::chrono::seconds(timeout))
            == std::cv_status::timeout
        ){
            return false;
        }
        if(m_is_close) {
            return false;
        }
    }
    item = m_deq.front();
    m_deq.pop_front();
    m_cond_producer.notify_one();
    return true;
}

#endif