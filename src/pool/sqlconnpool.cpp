#include "sqlconnpool.hpp"
#include <mutex>
#include <mysql/mysql.h>

SqlConnPool::SqlConnPool() {
    m_usecount = 0;
    m_freecount = 0;
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connpool;
    return &connpool;
}


void SqlConnPool::Init(const char *host, int port, const char *user, 
            const char *pwd, const char *dbName, int connSize) {
    assert(connSize > 0);
    for(int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if(!sql) {
            LOG_ERROR("MySQL init error!");
            assert(0);
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);

        if(!sql) {
            LOG_ERROR("MySQL connect error!");
        }
        m_conn_que.push(sql);
    }
    MAX_CONN = connSize;
    sem_init(&m_semid, 0, MAX_CONN); //设置信号量
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL* sql = nullptr;
    if(m_conn_que.empty()) {
        LOG_WARN("SqlConnPool is busy");
        return nullptr;
    }
    sem_wait(&m_semid);
    {
        std::lock_guard<std::mutex> lck(m_mtx);
        sql = m_conn_que.front();
        m_conn_que.pop();
    }
    return sql;
}


void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql != nullptr);
    std::lock_guard<std::mutex> lck(m_mtx);
    m_conn_que.push(sql);
    sem_post(&m_semid);
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> lck(m_mtx);
    while(!m_conn_que.empty()) {
        auto item = m_conn_que.front();
        m_conn_que.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_conn_que.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}



