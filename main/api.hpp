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
    sockaddr_in6 addr;
    socklen_t addr_size = sizeof(addr);
    int sockfd = httpd_req_to_sockfd(req);
    
    if (getpeername(sockfd, (sockaddr*)&addr, &addr_size) < 0)
    {
        log::info("Error getting client IP\n");
        return false;
    }
    
    // last 32 bits of ipv6 should gives us ipv4
    auto ip4 = addr.sin6_addr.un.u32_addr[3];
    
    mac_address mac;
    
    try {mac = online_clients.at(ip4);} catch(...)
    {
        log::warn("Time Traveler Detected !!\n");
        return false;
    }
    
    if (admin_list.contains(mac))
    {
        if (user_manager.check_user(mac))
            log::info("API Access Granted to %s (%s)\n", mac2str(mac).c_str(), user_manager.get_username(mac).c_str());
        else
            log::info("API Access Granted to %s (UNKNOWN)\n", mac2str(mac).c_str());
        return true;
    }
    else
    {
        if (user_manager.check_user(mac))
            log::info("API Access Denied to %s (%s)\n", mac2str(mac).c_str(), user_manager.get_username(mac).c_str());
        else
            log::info("API Access Denied to %s (UNKNOWN)\n", mac2str(mac).c_str());
        return false;
    }
}
/*
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
*/
esp_err_t ping_fn(httpd_req_t* req)
{
    return httpd_resp_sendstr(req, "UwU");
}

esp_err_t access_logs_fn(httpd_req_t* req)
{
    if (!check_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "access denied");
    
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
    return httpd_resp_send_chunk(req, nullptr, 0);;
    
on_error:
    fclose(logfile);
    return httpd_resp_send_500(req);
}

esp_err_t user_list_fn(httpd_req_t* req)
{
    if (!check_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "access denied");
    
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
    if (!check_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "access denied");
    
    std::string content;
    
    mac_address mac;
    std::string name, query;
    auto query_len = httpd_req_get_url_query_len(req);
    
    if (query_len)
    {
        query.resize(query_len);
        if (httpd_req_get_url_query_str(req, query.data(), query.size()+1) != ESP_OK)
            goto query_error;
        
        char buf[13]{}; // eg. 1a2b3c3d2e1f
        if (httpd_query_key_value(query.c_str(), "mac", buf, sizeof(buf)) != ESP_OK)
            goto query_error;
        
        if (std::sscanf(buf, "%012llx", &mac) != 1)
            goto query_error;
        
        name.resize(48);
        if (httpd_query_key_value(query.c_str(), "name", name.data(), name.size()) != ESP_OK)
            goto query_error;
        
        for (char& ch : name) if (ch == '~') ch = ' '; // all spaces must be replaced with ~ by client
    }
    else goto query_error;
    
    user_manager.add_user(mac, name);
    return httpd_resp_send(req, "OK", 2);
    
query_error:
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid query");
}

esp_err_t remove_user_fn(httpd_req_t* req)
{
    if (!check_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "access denied");
    
    mac_address mac;
    std::string query;
    auto query_len = httpd_req_get_url_query_len(req);
    
    if (query_len)
    {
        query.resize(query_len);
        if (httpd_req_get_url_query_str(req, query.data(), query.size()+1) != ESP_OK)
            goto query_error;
        
        char buf[13]{}; // eg. 1a2b3c3d2e1f
        if (httpd_query_key_value(query.c_str(), "mac", buf, sizeof(buf)) != ESP_OK)
            goto query_error;
        
        if (std::sscanf(buf, "%012llx", &mac) != 1)
            goto query_error;
    }
    else goto query_error;
    
    if (user_manager.check_user(mac))
    {
        user_manager.remove_user(mac);
        return httpd_resp_send(req, "OK", 2);
    }
    else return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "User Not Found");
    
query_error:
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid query");
}

esp_err_t check_user_fn(httpd_req_t* req)
{
    if (!check_admin(req))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "access denied");
    
    mac_address mac;
    std::string query;
    auto query_len = httpd_req_get_url_query_len(req);
    
    if (query_len)
    {
        query.resize(query_len);
        if (httpd_req_get_url_query_str(req, query.data(), query.size()+1) != ESP_OK)
            goto query_error;
        
        char buf[13]{}; // eg. 1a2b3c3d2e1f
        if (httpd_query_key_value(query.c_str(), "mac", buf, sizeof(buf)) != ESP_OK)
            goto query_error;
        
        if (std::sscanf(buf, "%012llx", &mac) != 1)
            goto query_error;
    }
    else goto query_error;
    
    if (user_manager.check_user(mac))
        return httpd_resp_sendstr(req, user_manager.get_username(mac).c_str());
    else
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "User Not Found");
    
query_error:
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid query");
}

/////////////////////////////////////////////

httpd_uri_t ping
{
    .uri     = "/ping",
    .method  = HTTP_GET,
    .handler = ping_fn
};
httpd_uri_t access_logs
{
    .uri     = "/access_logs",
    .method  = HTTP_GET,
    .handler = access_logs_fn
};
httpd_uri_t user_list
{
    .uri     = "/user_list",
    .method  = HTTP_GET,
    .handler = user_list_fn
};
httpd_uri_t add_user
{
    .uri     = "/add_user",
    .method  = HTTP_GET,
    .handler = add_user_fn
};
httpd_uri_t remove_user
{
    .uri     = "/remove_user",
    .method  = HTTP_GET,
    .handler = remove_user_fn
};
httpd_uri_t check_user
{
    .uri     = "/check_user",
    .method  = HTTP_GET,
    .handler = check_user_fn
};

}
