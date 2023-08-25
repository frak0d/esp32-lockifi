#pragma once

#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <string>
#include <cstdint>
#include <utility>
#include <cassert>
#include <fstream>
#include <filesystem>

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
    std::mutex mtx;
    bool list_updated = false;
    const char* filename = nullptr;
    std::map<mac_address, user_info_t> user_dict;
    
    void userfile_update_loop()
    {
        while(1)
        {
            mtx.lock();
            if (list_updated)
            {
                bool err = true;
                FILE* userfile = fopen("/lfs/user_list", "w");
                
                if (userfile)
                {
                    for (const auto& [mac, user] : user_dict)
                        fprintf(userfile, "%c %012llx %s\n", user.level+'0', mac, user.name.c_str());
                    
                    err = fclose(userfile);
                }
                
                if (err)
                    log::error("Error saving user_list, No changes made!\n");
                else
                {
                    list_updated = false;
                    log::info("Saved user_list to flash!\n");
                }
            }
            mtx.unlock();
            std::this_thread::sleep_for(1s);
        }
    }
    
public:
    
    void init()
    {
        if (LOG_LEVEL == log::level::debug)
        {
            std::ifstream f{"/lfs/user_list"};
            std::string data{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
            log::info("---\n%s---\n", data.c_str());
        }
        
        FILE* userfile = fopen("/lfs/user_list", "r");
        
        if (userfile)
        {
            uint8_t level;
            mac_address mac;
            char username[128]{};
            
            while (3 == fscanf(userfile, "%c %llx %[^\n] ", &level, &mac, username))
            {                                        // ^ this space at end of format string consumes newline
                level -= '0'; // ascii to decimal
                log::info("loaded:_%d_%012llx_%s_\n", level, mac, username);
                
                // discarding currupt entries
                if (level >= 0 and level <= 4)
                    user_dict[mac] = {username, level};
            }
            
            fclose(userfile);
        }
        else
        {
            log::warn("Skipping Database Loading... Missing File\n");
        }
        
        try // checking hardcoded default users
        {
            if (not(user_dict.at(0xAABBCCDDEEFF).level == 4
                and user_dict.at(0x1EA7DEADBEEF).level == 4)) throw 1;
        }
        catch (...)
        {
            log::warn("Had to rebuild default user record!\n");
            user_dict[0xAABBCCDDEEFF] = {"Lab PC 1", 4};
            user_dict[0x1EA7DEADBEEF] = {"Admin Laptop", 4};
            list_updated = true;
        }
        
        // start save loop after loading, else it may erase contents
        std::thread(&user_manager_t::userfile_update_loop, this).detach();
    }
    
    auto& get_user_dict()
    {
        return user_dict;
    }
    
    void add_user(const mac_address mac, const std::string& username, const uint8_t level)
    {
        mtx.lock();
        user_dict[mac] = {username, level};
        list_updated = true;
        mtx.unlock();
        
        log::info("Added '%s' with mac %s (lvl %u)\n",
            username.c_str(), mac2str(mac).c_str(), level);
    }
    
    void remove_user(const mac_address mac)
    {
        log::info("Removed '%s' with mac %s (lvl %u)\n",
            user_dict.at(mac).name.c_str(), mac2str(mac).c_str(), user_dict.at(mac).level);
        
        mtx.lock();
        user_dict.erase(mac);
        list_updated = true;
        mtx.unlock();
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
    
    bool init()
    {
        logfile = std::fopen("/lfs/access_logs", "ab+");
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
