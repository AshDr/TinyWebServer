#include "httpresponse.hpp"
#include <fcntl.h>
#include <sys/mman.h>


const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};


HttpResponse::HttpResponse() {
    m_code = -1;
    m_path = m_srcdir = "";
    m_iskeepalive = false;
    m_mmFile = nullptr; 
    m_mmFileStat = { 0 };
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}


void HttpResponse::Init(const std::string &srcdir, std::string &path, bool iskeepalive, int code) {
    assert(srcdir != "");
    if(m_mmFile) {
        UnmapFile();
    }
    m_code = code;
    m_iskeepalive = iskeepalive;
    m_path = path;
    m_srcdir = srcdir;
    m_mmFile = nullptr;
    m_mmFileStat = {0};
}


void HttpResponse :: MakeResponse(Buffer &buff) {
    //如果没有该文件或者该文件是文件夹
    if(stat((m_srcdir + m_path).data(), &m_mmFileStat) < 0 || S_ISDIR(m_mmFileStat.st_mode)) {
        m_code = 404;
    }
    else if(!(m_mmFileStat.st_mode & S_IROTH)) {
        m_code = 403;
    }else if(m_code == -1) {

        m_code = 200;
    }
    ErrorHtml();
    AddStateLine(buff);
    AddHeader(buff);
    AddContent(buff);
}

char* HttpResponse::File() {
    return m_mmFile;
}

size_t HttpResponse::FileLen() const {
    return m_mmFileStat.st_size;
}

void HttpResponse::ErrorHtml() {
    if(CODE_PATH.count(m_code) == 1) {
        m_path = CODE_PATH.find(m_code)->second;
        stat((m_srcdir + m_path).data(), &m_mmFileStat);
    }
}

void HttpResponse::AddStateLine(Buffer& buff) {
    std::string status;
    if(CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.find(m_code)->second;
    }else {
        m_code = 400;
        status = CODE_STATUS.find(400)->second; 
    }
    buff.Append("Http/1.1" + std::to_string(m_code) + " " + status + "\r\n");
}

void HttpResponse::AddHeader(Buffer& buff) {
    buff.Append("Connection: ");
    if(m_iskeepalive) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }else {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType() + "\r\n");
}


void HttpResponse::AddContent(Buffer& buff) {
    int srcFd = open((m_srcdir + m_path).data(), O_RDONLY);
    if(srcFd < 0) {
        ErrorContent(buff, "File NotFount!");
        return ;
    }
    LOG_DEBUG("File path %s", (m_srcdir + m_path).data());
    
    //一个文件映射到进程的地址空间中，以便后续可以直接在内存中访问文件内容。
    int* mmRet = (int*)mmap(0, m_mmFileStat.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return ;
    }
    m_mmFile = (char *)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + std::to_string(m_mmFileStat.st_size) + "\r\n\r\n");
}

//停止映射，释放内存
void HttpResponse::UnmapFile() {
    if(m_mmFile) {
        munmap(m_mmFile, m_mmFileStat.st_size);
        m_mmFile = nullptr;
    }
}


std::string HttpResponse::GetFileType() {
    size_t idx = m_path.find_last_of('.');
    if(idx == std::string::npos) {
        return "text/plain";
    }
    std::string suffix = m_path.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, std::string msg) {
    std::string body,status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(m_code) == 1) {
        status = CODE_STATUS.find(m_code)->second;
    }else {
        status = "Bad Request";
    }
    body += std::to_string(m_code) + " : " + status  + "\n";
    body += "<p>" + msg + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}