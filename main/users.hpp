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
				
				if (userfile)
				{
					for (const auto& [mac, username] : user_dict)
						fprintf(userfile, "%llx %s\n", mac, username.c_str());
					
					fclose(userfile);
				}
				else log::error("Unable to Open %s\n", filename);
			}
			std::this_thread::sleep_for(500ms);
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
	
	void load_defaults()
	{
		// just in case filesystem gets currupted
		user_dict[0xb898ad2edd88] = "Tanishq Banyal";
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
