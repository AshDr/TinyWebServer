#ifndef __SQLCONNPOOL_HPP
#define __SQLCONNPOOL_HPP

#include <mysql/mysql.h>
#include <cstring>
#include <queue>
#include <mutex>
#include <thread>
#include <semaphore.h>
#include "../log/log.hpp"

class SqlConnPool {
public:
    static SqlConnPool *Instance();  //单例模式
    
    MYSQL *GetConn();

    void FreeConn(MYSQL* conn);

    int GetFreeConnCount();

    void Init(const char* host, int port, const char* user, const char* pwd, const char* dbName, int connSize);

    void ClosePool();

private:
    SqlConnPool();
    
    ~SqlConnPool();

    int MAX_CONN;
    
    int m_usecount; //正在使用的

    int m_freecount; // 空闲的

    std::queue<MYSQL*> m_conn_que;

    std::mutex m_mtx;

    sem_t m_semid;

};



#endif