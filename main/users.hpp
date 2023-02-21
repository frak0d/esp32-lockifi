#pragma once

#include <map>
#include <set>
#include <thread>
#include <chrono>
#include <cstdio>
#include <string>
#include <cstdint>
#include <utility>
#include <cassert>

#include "log.hpp"
#include "types.hpp"
#include <esp_netif.h>

using namespace std::literals;

/*constexpr*/ std::set<mac_address> admin_list{
    #include "admins.conf"
};

std::map<ipv4_address, mac_address> online_clients;

class user_manager_t
{
    bool list_updated = false;
    const char* filename = nullptr;
    std::map<mac_address, std::string> user_dict{
        #include "users.conf"
    };
    
    void userfile_update_loop()
    {
        while(1)
        {
            if (list_updated)
            {
                FILE* userfile = fopen(filename, "w");
                
                if (userfile)
                {
                    for (const auto& [mac, username] : user_dict)
                        fprintf(userfile, "%llx %s\n", mac, username.c_str());
                    
                    fclose(userfile);
                }
                else log::error("Unable to Open %s\n", filename);
            }
            std::this_thread::sleep_for(750ms);
        }
    }
    
public:
    
    bool init(const char* fs_path)
    {
        filename = fs_path;
        std::thread(&user_manager_t::userfile_update_loop, this).detach();
        FILE* userfile = fopen(filename, "r");
        
        if (userfile)
        {
            mac_address mac;
            char username[128]{};
            
            while (2 == fscanf(userfile, "%llx %[^\n]", &mac, username))
                user_dict[mac] = username;
            
            fclose(userfile);
            return true;
        }
        else
        {
            log::warn("Skipping Database Loading... Missing File\n");
            return false;
        }
    }
    
    auto& get_user_dict()
    {
        return user_dict;
    }
    
    void add_user(const mac_address mac, const std::string& username)
    {
        list_updated = true;
        user_dict[mac] = username;
        log::info("Added '%s' with mac %s\n",
             username.c_str(), mac2str(mac).c_str());
    }
    
    void remove_user(const mac_address mac)
    {
        list_updated = true;
        log::info("Removed '%s' with mac %s\n",
             user_dict.at(mac).c_str(), mac2str(mac).c_str());
        user_dict.erase(mac);
    }
    
    bool check_user(const mac_address mac)
    {
        return user_dict.contains(mac);
    }
    
    auto get_username(const mac_address mac)
    {
        return user_dict.at(mac);
    }
    
} user_manager;

///////////////////////////////////////////////////////////////////////////////

class access_logger_t
{
    FILE* logfile;
    
public:
    
    bool init(const char* fs_path)
    {
        logfile = std::fopen(fs_path, "ab+");
        return logfile;
    }
    
    FILE* get_file_ptr()
    {
        return logfile;
    }
    
    std::string raw_logs()
    {
        size_t file_sz;
        //get file size
        std::string data;
        data.resize(file_sz);
        //read data
        return data;
    }
    
    auto read_logs()
    {
        std::vector<std::pair<unix_timestamp, mac_address>> access_logs;
        
        //parse file here
        
        return access_logs;
    }
    
    void add_log(mac_address mac, bool connected)
    {
        //unix_timestamp time;
        //time = get network time()
        //std::fwrite(&time, sizeof(unix_timestamp), 1, logfile);
        //std::fwrite(&mac, sizeof(mac_address), 1, logfile);
    }
    
} access_logger;
