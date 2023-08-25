#pragma once

#include "users.hpp"

#include <stdio.h>
#include <esp_err.h>
#include <esp_http_server.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

void unlock_signal(); // defined in main.cpp

namespace Api
{

bool check_admin(httpd_req_t* req)
{
    sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);
    int sockfd = httpd_req_to_sockfd(req);
    
    // ipv6 is disabled in sdkconfig
    if (lwip_getpeername(sockfd, (sockaddr*)&addr, &addr_size) < 0)
    {
        log::info("Error getting client IP\n");
        return false;
    }
    
    uint32_t ip4 = addr.sin_addr.s_addr;
    
    mac_address mac;
    
    try {mac = online_clients.at(ip4);} catch(...)
    {
        log::warn("Request Without IP Detected !!\n");
        return false;
    }
    
    if (user_manager.check_user(mac))
    {
        const auto& [username, level] = user_manager.get_user(mac);
        
        if (level == 0 or level == 4) // 0->Admin, 4->Ultra
        {
            log::info("API Access Granted to %s (lvl %u) (%s)\n", mac2str(mac).c_str(), level, username.c_str());
            return true;
        }
        else
        {
            log::info("API Access Denied to %s (lvl %u) (%s)\n", mac2str(mac).c_str(), level, username.c_str());
            return false;
        }
    }
    else
    {
        log::info("API Access Denied to %s (UNKNOWN)\n", mac2str(mac).c_str());
        return false;
    }
}

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
    
    for (const auto& [mac, user] : user_dict)
    {
        std::string buf;
        buf += user.level+'0'; buf += ' ';
        buf += mac2str(mac)+' '+user.name+'\n';
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
    
    uint8_t level;
    mac_address mac;
    std::string name, query;
    auto query_len = httpd_req_get_url_query_len(req);
    
    if (query_len)
    {
        query.resize(query_len);
        if (httpd_req_get_url_query_str(req, query.data(), query.size()+1) != ESP_OK)
            goto query_error;
        
        if (httpd_query_key_value(query.c_str(), "lvl", (char*)&level, 2) != ESP_OK)
            goto query_error;
        
        level -= '0'; // ascii to decimal
        if (not (level >= 0 and level <= 4))
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
    
    user_manager.add_user(mac, name, level);
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
        return httpd_resp_sendstr(req, user_manager.get_user(mac).name.c_str());
    else
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "User Not Found");
    
query_error:
    return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid query");
}

esp_err_t unlock_fn(httpd_req_t* req)
{    
    if (check_admin(req))
    {
        unlock_signal();
        return httpd_resp_send(req, "OK", 2);
    }
    else
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "access denied");
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
httpd_uri_t unlock
{
    .uri     = "/unlock",
    .method  = HTTP_GET,
    .handler = unlock_fn
};

}
