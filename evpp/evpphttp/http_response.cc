#include "evpp/evpphttp/http_response.h"

#include <inttypes.h>
namespace evpp {
namespace evpphttp {

HttpResponse::HttpResponse(const HttpRequest& hr): hp_(hr.parser) {
    if (hp_.http_major > 9 || hp_.http_minor > 9) {
        hp_.http_major = 1;
        hp_.http_minor = 1;
    }
    //close_ = is_connection_close(hr.field_value);
    keep_alive_ = is_connection_keep_alive(hr.field_value);
}

void HttpResponse::add_content_len(const int64_t size, Buffer& buf) {
    char len[42];
    snprintf(len, sizeof(len), "Content-Length:%" PRId64 "\r\n", size);
    buf.Append(len, strlen(len));
}

void HttpResponse::add_date(Buffer& buf) {
    char date[50];
#ifndef WIN32
    struct tm cur;
#endif
    struct tm *cur_p;
    time_t t = time(NULL);
#ifdef WIN32
    cur_p = gmtime(&t);
#else
    gmtime_r(&t, &cur);
    cur_p = &cur;
#endif
    if (strftime(date, sizeof(date),
                 "Date:%a, %d %b %Y %H:%M:%S GMT\r\n", cur_p) != 0) {
        buf.Append(date, strlen(date));
    }
}
void HttpResponse::MakeHttpResponse(const int response_code, const int64_t body_size, const std::map<std::string, std::string>& header_field_value, Buffer& buf) {
    //HTTP/%d.%d code reson\r\n
    auto response_code_iter = http_status_code.find(response_code);
    if (response_code_iter == http_status_code.end()) {
        response_code_iter = http_status_code.find(808);
    }
    char status[16];
    snprintf(status, sizeof status, "HTTP/%d.%d %d ", hp_.http_major, hp_.http_minor, response_code_iter->first);
    buf.Append(status);
    buf.Append(response_code_iter->second);
    buf.Append("\r\n");
    if (response_code == 400) { //Bad request
        buf.Append("\r\n");
        keep_alive_ = false;
        return;
    }
    if (hp_.http_major == 1) {
        if (hp_.http_minor >= 1 && header_field_value.find("Date") == header_field_value.end()) {
            add_date(buf);
        }
        if (hp_.http_minor == 0 && keep_alive_) {
            buf.Append("Connection:keep-alive\r\n");
        }
        if ((hp_.http_minor >= 1 || keep_alive_) && need_body(response_code)
                && header_field_value.find("Transfer-Encoding") == header_field_value.end() && header_field_value.find("Content-Length") == header_field_value.end()) {
            add_content_len(body_size, buf);
        }
    }
    if (need_body(response_code) && header_field_value.find("Content-Type") == header_field_value.end()) {
        buf.Append("Content-Type:text/html; charset=ISO-8859-1\r\n");
    }
    auto conn_iter = header_field_value.find("Connection");
    if (!keep_alive_ && conn_iter == header_field_value.end()) {
        buf.Append("Connection:close\r\n");
    }
    if (conn_iter != header_field_value.end()) {
        if (conn_iter->second == "close") {
            keep_alive_ = false;
        }
    }
    for (auto & it : header_field_value) {
        buf.Append(it.first);
        buf.Append(":");
        buf.Append(it.second);
        buf.Append("\r\n");
    }
    buf.Append("\r\n");
}

void HttpResponse::SendReply(const evpp::TCPConnPtr& conn, const int response_code, const std::map<std::string, std::string>& header_field_value, const std::string & response_body) {
    if (!conn || !conn->IsConnected()) {
        return;
    }
    Buffer buf;
    MakeHttpResponse(response_code, response_body.size(), header_field_value, buf);
    conn->Send(&buf);
    if (response_body.size() > 0) {
        conn->Send(response_body);
        //buf.Append(response_body);
    }
    if (!keep_alive_) {
        conn->Close();
    }
}
}
}


