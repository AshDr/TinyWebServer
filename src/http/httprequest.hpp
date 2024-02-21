#ifndef __HTTPREQUEST_HPP
#define __HTTPREQUEST_HPP

#include <cstring>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>
#include <unordered_map>
#include <unordered_set>
#include "../buffer/buffer.hpp"
#include "../log/log.hpp"
#include "../pool/sqlconnpool.hpp"
#include "../pool/sqlconnRAII.hpp"

class HttpRequest {
public:

    //解析状态
    enum PARSE_STATE {
        REQUEST_LINE, //请求行
        HEADERS, //请求头部
        BODY, //数据字段
        FINISH
    };
    
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest() {
        Init();
    }
    
    ~HttpRequest() = default;

    void Init();

    bool parse(Buffer& buff);


    std::string path() const;
    
    std::string& path();

    std::string method() const;

    std::string version() const;

    std::string GetPost(const std::string& key) const;

    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;   

    /* 
    todo 

    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    
    */

private:

    //解析请求行
    bool ParseRequestLine(const std::string& line); 

    //解析请求头
    void ParseHeader(const std::string& line); 
    
    //解析请求体
    void ParseBody(const std::string& line); 

    void ParsePath();

    void ParsePost();

    void ParseFromUrlencoded();

    //身份验证
    static bool UserVerify(const std::string& name, const std::string& pwd, bool islogin);

    PARSE_STATE m_state;

    std::string m_method, m_path, m_version, m_body;

    std::unordered_map<std::string, std::string> m_header;
    std::unordered_map<std::string, std::string> m_post;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;

    static int ConverHex(char ch);
};




#endif