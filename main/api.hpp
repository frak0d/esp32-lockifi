#pragma once

#include "users.hpp"

#include <stdio.h>
#include <esp_err.h>
#include <esp_http_server.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

namespace Api
{

bool check_admin(httpd_req_t* req)
{
    int sockfd = httpd_req_to_sockfd(req);
    
    char ipstr[INET_ADDRSTRLEN];
    sockaddr_in6 addr;
    socklen_t addr_size = sizeof(addr);
    
    if (getpeername(sockfd, (sockaddr*)&addr, &addr_size) < 0)
    {
        log::info("Error getting client IP\n");
        return false;
    }
    
    inet_ntop(AF_INET, &addr.sin6_addr.un.u32_addr[3], ipstr, sizeof(ipstr));
    log::info("Client IP %s\n", ipstr);
    
    //if (ip corresponds to any connected admin mac)
        return true;
    //else
    //    return false;
}

bool get_content(httpd_req_t* req, std::string& dst_buf, size_t max_len)
{
    dst_buf.resize(max_len);
    log::debug("Content Length: %llx\n", req->content_len);
    //if (req->content_len > dst_buf.size())
    //    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "content length too long");
    
    auto ret = httpd_req_recv(req, dst_buf.data(), max_len);
    
    if (ret > 0)
    {
        dst_buf.resize(ret);
        httpd_resp_send(req, nullptr, 0);
        return true;
    }
    else
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            httpd_resp_send_408(req);
        else
            httpd_resp_send_500(req);
        
        return false;
    }
}

esp_err_t ping_fn(httpd_req_t* req)
{
    if (!check_admin(req)) return 0;
    
    return httpd_resp_send(req, "UwU", 3);
}

esp_err_t access_logs_fn(httpd_req_t* req)
{
    if (!check_admin(req)) return 0;
    
    auto logfile = fopen("/lfs/access_logs", "rb");
    if (!logfile)
    {
        log::error("access_log_fn: unable to open logfile\n");
        return httpd_resp_send_500(req);
    }
    
    size_t sz{};
    char buf[512]{};
    esp_err_t ret;
    
    while(1)
    {
        sz = fread(buf, 1, sizeof(buf), logfile);
        
        if (sz < sizeof(buf))
        {
            if (feof(logfile))
            {
                if (httpd_resp_send_chunk(req, buf, sz) != ESP_OK) goto on_error;
                break; //file finished
            }
            else goto on_error;
        }
        else if (httpd_resp_send_chunk(req, buf, sz) != ESP_OK) goto on_error;
    }
    
    fclose(logfile);
    return ESP_OK;
    
on_error:
    fclose(logfile);
    return httpd_resp_send_500(req);
}

esp_err_t user_list_fn(httpd_req_t* req)
{
    if (!check_admin(req)) return 0;
    
    esp_err_t ret;
    auto& user_dict = user_manager.get_user_dict();
    
    for (const auto& [mac,name] : user_dict)
    {
        auto buf = mac2str(mac);
        buf += ' '; buf += name; buf += '\n';
        ret = httpd_resp_send_chunk(req, buf.data(), buf.size());
        if (ret != ESP_OK) return ret;
    }
    return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t add_user_fn(httpd_req_t* req)
{
    if (!check_admin(req)) return 0;
    
    std::string content;
    
    if (!get_content(req, content, 128))
        return ESP_FAIL;
    
    auto name = content.substr(18);
    auto  mac = str2mac(content.substr(0, 17));
    user_manager.add_user(mac, name);
    
    return httpd_resp_send(req, "OK", 2);
}

esp_err_t remove_user_fn(httpd_req_t* req)
{
    if (!check_admin(req)) return 0;
    
    std::string content;
    
    if (!get_content(req, content, 17))
        return ESP_FAIL;
    
    auto mac = str2mac(content);
    
    if (user_manager.check_user(mac))
    {
        user_manager.remove_user(mac);
        return httpd_resp_send(req, "OK", 2);
    }
    else return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "User Not Found");
}

esp_err_t check_user_fn(httpd_req_t* req)
{
    if (!check_admin(req)) return 0;
    
    std::string content;
    
    if (!get_content(req, content, 17))
        return ESP_FAIL;
    
    auto mac = str2mac(content);
    
    if (user_manager.check_user(mac))
        return httpd_resp_sendstr(req, user_manager.get_username(mac).c_str());
    else
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "User Not Found");
}

/////////////////////////////////////////////

httpd_uri_t ping
{
    .uri      = "/ping",
    .method   = HTTP_GET,
    .handler  = ping_fn
};
httpd_uri_t access_logs
{
    .uri      = "/access_logs",
    .method   = HTTP_GET,
    .handler  = access_logs_fn
};
httpd_uri_t user_list
{
    .uri      = "/user_list",
    .method   = HTTP_GET,
    .handler  = user_list_fn
};
httpd_uri_t add_user
{
    .uri      = "/add_user",
    .method   = HTTP_POST,
    .handler  = add_user_fn
};
httpd_uri_t remove_user
{
    .uri      = "/remove_user",
    .method   = HTTP_POST,
    .handler  = remove_user_fn
};
httpd_uri_t check_user
{
    .uri      = "/check_user",
    .method   = HTTP_POST,
    .handler  = check_user_fn
};

}
