#pragma once

#include <string>
#include <cstdio>
#include <cassert>
#include <cstdint>

using mac_address = std::uint64_t;
using unix_timestamp = std::int64_t;

std::string mac2str(mac_address mac)
{
	char buf[20]{};
	std::string mac_str;
	std::snprintf(buf, sizeof(buf), "%012lx", mac);
	
	for (int i=1 ; i < 12 ; i+=2)
	{
		mac_str += buf[i-1];
		mac_str += buf[i];
		mac_str += ':';
	}
	
	mac_str.pop_back();
	return mac_str;
}

mac_address str2mac(const std::string& str)
{
	mac_address mac{0};
	assert(str.size() == 17);
	std::string buf;
	
	int s = 2;
	const auto len = str.length();
	
	for (int i=0 ; i < len ; ++i)
	{
		if (s--) buf += str[i];
		else s = 2;
	}
	
	std::sscanf(buf.c_str(), "%012lx", &mac);
	return mac;
}
