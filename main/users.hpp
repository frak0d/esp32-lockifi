#pragma once

#include <map>
#include <thread>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <cassert>

#include "log.hpp"
#include "types.hpp"

using namespace std::literals;

class access_logger_t
{
	FILE* logfile;
	
public:
	
	bool init(const char* fs_path)
	{
		logfile = std::fopen(fs_path, "ab+");
		return logfile;
	}
	
	auto read_logs()
	{
		std::vector<std::pair<unix_timestamp, mac_address>> access_logs;
		
		//parse file here
		
		return access_logs;
	}
	
	void add_log(mac_address mac)
	{
		unix_timestamp time;
		//time = get network time()
		std::fwrite(&time, sizeof(unix_timestamp), 1, logfile);
		std::fwrite(&mac, sizeof(mac_address), 1, logfile);
	}
	
} access_logger;

///////////////////////////////////////////////////////////////////////////////

class user_manager_t
{
	bool list_updated = false;
	const char* filename = nullptr;
	std::map<mac_address, std::string> user_dict;
	
	void userfile_update_loop()
	{
		while(1)
		{
			if (list_updated)
			{
				FILE* userfile = fopen(filename, "w");
				
				if (!userfile)
					log::error("Unable to Open %s\n", filename);
				else
					for (const auto& [mac, username] : user_dict)
						fprintf(userfile, "%lx %s\n", mac, username.c_str());
				
				fclose(userfile);
			}
			std::this_thread::sleep_for(500ms);
		}
	}
	
public:
	
	bool init(const char* fs_path)
	{
		puts("here1");
		filename = fs_path;
		std::thread(&user_manager_t::userfile_update_loop, this).detach();
		FILE* userfile = fopen(filename, "w");
		
		if (!userfile)
		{
			puts("here2");
			log::error("Unable to Open %s\n", filename);
			return false;
		}
		else
		{
			mac_address mac;
			char username[128] = "";
			puts("heeeere");
			while (2 == fscanf(userfile, "%lx %s\n", &mac, username))
				user_dict[mac] = username;
			
			fclose(userfile);
		}
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
		user_dict.erase(mac);
		log::info("Removed '%s' with mac %s\n",
			 user_dict.at(mac).c_str(), mac2str(mac).c_str());
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
