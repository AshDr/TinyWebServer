#ifndef __SQLCONNRAII_HPP
#define __SQLCONNRAII_HPP

#include "sqlconnpool.hpp"
class SqlConnRAII {
public:

    //函数需要修改调用者提供的指针指向的内存地址。通过使用二级指针，函数可以修改指针指向的地址，而不仅仅是指针指向的内容。
    //???
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool) { 
        assert(connpool);
        *sql = connpool->GetConn();
        m_sql = *sql;
        m_connpool = connpool;
    }

    ~SqlConnRAII() {
        if(m_sql) {
            m_connpool->FreeConn(m_sql);
        }
    }
private:
    MYSQL* m_sql;
    SqlConnPool* m_connpool;
};


#endif