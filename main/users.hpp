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

std::map<ipv4_address, mac_address> online_clients;

struct user_info_t
{
    std::string name;
    uint8_t level;
};

class user_manager_t
{
    bool list_updated = false;
    const char* filename = nullptr;
    std::map<mac_address, user_info_t> user_dict;
    
    void userfile_update_loop()
    {
        while(1)
        {
            if (list_updated)
            {
                FILE* userfile = fopen(filename, "w");
                
                if (userfile)
                {
                    for (const auto& [mac, user] : user_dict)
                        fprintf(userfile, "%c %llx %s\n", user.level+'0', mac, user.name.c_str());
                    
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
        bool success{0};
        filename = fs_path;
        std::thread(&user_manager_t::userfile_update_loop, this).detach();
        FILE* userfile = fopen(filename, "r");
        
        if (userfile)
        {
            uint8_t level;
            mac_address mac;
            char username[128]{};
            
            while (3 == fscanf(userfile, "%c %llx %[^\n]", &level, &mac, username))
            {
                level -= '0'; //ascii to decimal
                user_dict[mac] = {username, level};
            }
            
            fclose(userfile);
            success = true;
        }
        else
        {
            log::warn("Skipping Database Loading... Missing File\n");
            success = false;
        }
        
        user_dict[0xCAFEBABEB00B] = {"Admin Phone", 4}; //cannot be overridden
        user_dict[0x1EA7DEADBEEF] = {"Admin Laptop",4};
        
        return success;
    }
    
    auto& get_user_dict()
    {
        return user_dict;
    }
    
    void add_user(const mac_address mac, const std::string& username, const uint8_t level)
    {
        list_updated = true;
        user_dict[mac] = {username, level};
        log::info("Added '%s' with mac %s (lvl %u)\n",
            username.c_str(), mac2str(mac).c_str(), level);
    }
    
    void remove_user(const mac_address mac)
    {
        list_updated = true;
        log::info("Removed '%s' with mac %s (lvl %u)\n",
            user_dict.at(mac).name.c_str(), mac2str(mac).c_str(), user_dict.at(mac).level);
        user_dict.erase(mac);
    }
    
    bool check_user(const mac_address mac)
    {
        return user_dict.contains(mac);
    }
    
    auto get_user(const mac_address mac)
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
