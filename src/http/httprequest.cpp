#include "httprequest.hpp"
#include <algorithm>
#include <cstdio>
#include <mysql/mysql.h>
#include <regex>
#include <unordered_map>
#include <unordered_set>

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/index","/register","/login","/welcome",
    "/video", "/picture"  
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0},{"/login.html", 1}
};


void HttpRequest::Init() {
    m_body = m_method = m_version = m_path = "";

    m_state = REQUEST_LINE;

    m_header.clear();
    m_post.clear();
}


bool HttpRequest::IsKeepAlive() const {
    if(m_header.count("Connection") == 1) {
        return (m_header.find("Connection")->second == "keep-alive") && (m_version == "1.1");
    }
    return false;
}

bool HttpRequest::parse(Buffer& buffer) {
    const char CRLF[] = "\r\n";

    if(buffer.ReadableBytes() <= 0) {
        return false;
    }

    while(buffer.ReadableBytes() && m_state != FINISH) { //没解析完就继续解析
        const char* lineEnd = std::search(buffer.Peek(), buffer.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buffer.Peek(), lineEnd);
        switch(m_state) {
            case REQUEST_LINE: {
                if(!ParseRequestLine(line)) {
                    return false;
                }
                ParsePath();
            }break;
            case HEADERS: {
                ParseHeader(line);
                if(buffer.ReadableBytes() <= 2) {
                    m_state = FINISH;
                }
            }break;
            case BODY: {
                ParseBody(line);
            }
            default:break;
        }
        if(lineEnd == buffer.BeginWriteConst()) break;
        buffer.RetrieveUntil(lineEnd + 2); //跳过这么多
    }
    LOG_DEBUG("[%s],[%s],[%s]", m_method.c_str(), m_path.c_str(), m_version.c_str());
    return true;
}


void HttpRequest::ParsePath() {
    if(m_path == "/") {
        m_path = "/index.html";
    }else {
        for(auto &item: DEFAULT_HTML) {
            if(item == m_path) {
                m_path += ".html";
                break;
            }
        }
    }
}


bool HttpRequest::ParseRequestLine(const std::string& line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); 
    std::smatch submatches;
    if(std::regex_match(line, submatches, pattern)) {
        //submatches[0]是整个匹配内容 ()代表第几组
        m_method = submatches[1];   
        m_path = submatches[2];   
        m_version = submatches[3];
        m_state = HEADERS;   
        return true;
    }
    LOG_ERROR("Request Error! Match line failed");
    return false;
} 

void HttpRequest::ParseHeader(const std::string& line) {
    std::regex pattern("^([^:]*): ?(.*)$"); //.matches any character (except for line terminators)
    // ?matches the previous token between zero and one times, as many times as possible, giving back as needed
    std::smatch submatches;
    if(std::regex_match(line, submatches, pattern)) {
        m_header[submatches[1]] = submatches[2];
    }else {
        m_state = BODY;
    }
}

void HttpRequest::ParseBody(const std::string& line) {
    m_body = line;
    ParsePost();
    m_state = FINISH;
    LOG_DEBUG("Body:%s, len: %d", line.c_str(), line.size());
}


// something wrong
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch - '0';
}

void HttpRequest::ParsePost() {
    if(m_method == "POST" && m_header["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded();
        if(DEFAULT_HTML_TAG.count(m_path)) {
            int tag = DEFAULT_HTML_TAG.find(m_path)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool islogin = (tag == 1);
                if(UserVerify(m_post["username"], m_post["password"], islogin)) {
                    m_path = "/welcome.html";
                }else {
                    m_path = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded() {
    if(m_body.size() == 0) return ;
    
    std::string key,val;
    int num = 0, n = m_body.size();
    int i = 0, j = 0;
    for(; i < n; i++) {
        char ch = m_body[i];
        switch(ch) {
            case '=': {
                key = m_body.substr(j, i - j);
                j = i + 1;
            }break;
            case '+': {
                m_body[i] = ' ';
            }break;
            case '%': {
                num = ConverHex(m_body[i + 1]) * 16 + ConverHex(m_body[i + 2]);
                m_body[i + 2] = num % 10 + '0';
                m_body[i + 1] = num / 10 + '0';
                i += 2;
            }break;
            case '&': {
                val = m_body.substr(j, i - j);
                j = i + 1;
                m_post[key] = val;
                LOG_DEBUG("%s = %s", key.c_str(), val.c_str());
            }break;
            default:break;
        }
    }
    assert(j <= i);
    if(m_post.count(key) == 0 && j < i) {
        val = m_body.substr(j, i - j);
        m_post[key] = val;
    }
}


bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool islogin) {
    if(name == "" || pwd == "") return false;
    LOG_INFO("Verify name:%s, pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRAII(&sql, SqlConnPool::Instance());
    assert(sql != nullptr);
    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD* fields = nullptr;
    MYSQL_RES* res = nullptr;

    if(!islogin) flag = true;
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);
    if(mysql_query(sql, order)) { //Zero for success. Nonzero if an error occurred.
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    j = mysql_num_fields(res); //Returns the number of columns in a result set.
    fields = mysql_fetch_fields(res);
    // Returns the definition of one column of a result set as a MYSQL_FIELD structure. 
    // Call this function repeatedly to retrieve information about all columns in the result set.

    while(MYSQL_ROW row = mysql_fetch_row(res)) {//retrieves the next row of a result set:
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        //注册行为 且 用户名未被使用
        if(islogin) {
            if(pwd == password) flag = true;
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    //注册行为 且 用户名未被使用
    if(!islogin && flag == true) {
        LOG_DEBUG("register!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INFO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        if(mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }

    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const {
    return m_path;
}

std::string& HttpRequest::path() {
    return m_path;
}

std::string HttpRequest::method() const {
    return m_method;
}

std::string HttpRequest::version() const {
    return m_version;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}