#ifndef __HTTP_RESPONSE_H
#define __HTTP_RESPONSE_H

#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unordered_map>
#include "../buffer/buffer.hpp"
#include "../log/log.hpp"


class HttpResponse {
public:
    HttpResponse();
    
    ~HttpResponse();

    void Init(const std::string& srcdir, std::string& path, bool iskeepalive = false, int code = -1);

    void MakeResponse(Buffer& buff);

    //取消文件内存映射
    void UnmapFile();

    char* File();

    size_t FileLen() const;

    //错误信息的content
    void ErrorContent(Buffer& buff, std::string message);

    //状态码
    int Code()const {return m_code;}

private:
    //写返回 状态行
    void AddStateLine(Buffer& buff);

    void AddHeader(Buffer& buff);

    void AddContent(Buffer& buff);

    //错误信息的html
    void ErrorHtml();
    
    std::string GetFileType();

    int m_code;
    
    bool m_iskeepalive;

    std::string m_path;

    std::string m_srcdir;

    char* m_mmFile;

    struct stat m_mmFileStat;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    
    static const std::unordered_map<int, std::string> CODE_STATUS;

    static const std::unordered_map<int, std::string> CODE_PATH;

};


#endif