#ifndef __THREADPOOL_HPP
#define __THREADPOOL_HPP

#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cassert>
#include <utility>

class ThreadPool {
public:
    explicit ThreadPool(size_t ThreadCount = 0): m_pool_ptr(std::make_shared<Pool>()) {
        assert(ThreadCount > 0);
        for(size_t i = 0; i < ThreadCount; i++) {
            std::thread([pool_ptr = m_pool_ptr]{
                std::unique_lock<std::mutex> lck(pool_ptr->mtx);
                while(true) {
                    if(!pool_ptr->tasks.empty()) {
                        auto task = std::move(pool_ptr->tasks.front());
                        pool_ptr->tasks.pop();
                        lck.unlock(); //执行task1的时候可以解锁
                        task();
                        lck.lock();
                    }else if(pool_ptr->is_close) {
                        break;
                    }else {
                        pool_ptr->cond.wait(lck);
                        //消费者
                    }

                }
            }).detach(); //分离线程。当一个分离状态的线程结束时，它的资源会自动被释放，无需显式调用 join() 函数
            //它们可以在后台运行，执行一些独立的任务，而不会阻塞主线程。
        };
    }
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    ~ThreadPool() {
        if(static_cast<bool>(m_pool_ptr)) {
            std::lock_guard<std::mutex> lck(m_pool_ptr->mtx);
            m_pool_ptr->is_close = true;
        }
        m_pool_ptr->cond.notify_all();
    }

    //添加函数任务
    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> lck(m_pool_ptr->mtx);
            m_pool_ptr->tasks.emplace(std::forward<F>(task));
        }
        m_pool_ptr->cond.notify_one();
    }



private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool is_close;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> m_pool_ptr;
};


#endif