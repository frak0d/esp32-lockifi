#include <bit>
#include <atomic>
#include <thread>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>

#define LOG_LEVEL log::level::info

#include "log.hpp"
#include "users.hpp"
#include "http_api.hpp"

#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <driver/gpio.h>
#include <driver/touch_pad.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <lwip/ip.h>
#include <esp_err.h>
#include <esp_flash.h>
#include <esp_littlefs.h>

using namespace std::literals;
#define TRY(...) ESP_ERROR_CHECK(__VA_ARGS__)
std::atomic<int> keep_unlocked{0}, trust_level{0};

constexpr auto TOUCH_PIN     = TOUCH_PAD_NUM9;
constexpr auto RED_LED_PIN   = GPIO_NUM_25;
constexpr auto GREEN_LED_PIN = GPIO_NUM_26;
constexpr auto SOLENOID_PIN  = GPIO_NUM_2;

void success_feedback()
{
    gpio_set_level(GREEN_LED_PIN, 1);
    std::this_thread::sleep_for(300ms);
    gpio_set_level(GREEN_LED_PIN, 0);
    std::this_thread::sleep_for(200ms);
    gpio_set_level(GREEN_LED_PIN, 1);
    std::this_thread::sleep_for(300ms);
    gpio_set_level(GREEN_LED_PIN, 0);
}

void faliure_feedback()
{
    gpio_set_level(RED_LED_PIN, 1);
    std::this_thread::sleep_for(100ms);
    gpio_set_level(RED_LED_PIN, 0);
    std::this_thread::sleep_for(200ms);
    gpio_set_level(RED_LED_PIN, 1);
    std::this_thread::sleep_for(100ms);
    gpio_set_level(RED_LED_PIN, 0);
    std::this_thread::sleep_for(200ms);
    gpio_set_level(RED_LED_PIN, 1);
    std::this_thread::sleep_for(100ms);
    gpio_set_level(RED_LED_PIN, 0);
}

void door_wardern(gpio_num_t pin)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    
    while(1)
    {
        if (keep_unlocked > 0)
        {
            gpio_set_level(pin, 1);
            std::this_thread::sleep_for(100ms);
            keep_unlocked -= 100;
        }
        else
        {
            gpio_set_level(pin, 0);
            std::this_thread::sleep_for(100ms);
        }
    }
}

void unlock_signal()
{
    if (trust_level >= 3)
    {
        keep_unlocked = 5000;
        success_feedback();
    }
    else faliure_feedback();
}

constexpr auto arr2mac(const uint8_t* mac_arr)
{
    uint8_t mac_buf[8]{};
    std::reverse_copy(mac_arr, mac_arr+6, mac_buf);
    return std::bit_cast<mac_address>(mac_buf);
}

void on_client_got_ip(void*, esp_event_base_t, int32_t, void* event_data)
{
    auto data = (ip_event_ap_staipassigned_t*)event_data;
    online_clients[data->ip.addr] = arr2mac(data->mac);
}

void on_client_connect(void*, esp_event_base_t, int32_t, void* event_data)
{
    auto mac = arr2mac(((wifi_event_ap_staconnected_t*)event_data)->mac);
    access_logger.add_log(mac, true);
    
    if (user_manager.check_user(mac))
    {
        const auto& user = user_manager.get_user(mac);
        trust_level += user.level;
        log::info("Trusted Agent %s (lvl %u) (%s) Connected, Trust = %d\n",
            mac2str(mac).c_str(), user.level, user.name.c_str(), trust_level.load());
    }
    else
    {
        log::warn("Unknown Agent %s Connected, Ignoring...\n", mac2str(mac).c_str());
    }
}

void on_client_disconnect(void*, esp_event_base_t, int32_t, void* event_data) 
{
    auto mac = arr2mac(((wifi_event_ap_staconnected_t*)event_data)->mac);
    access_logger.add_log(mac, false);
    std::erase_if(online_clients, [mac](const auto& pair){return pair.second == mac;});
    
    if (user_manager.check_user(mac))
    {
        const auto& user = user_manager.get_user(mac);
        trust_level -= user.level;
        log::info("Trusted Agent %s (lvl %u) (%s) Disconnected, Trust = %d\n",
            mac2str(mac).c_str(), user.level, user.name.c_str(), trust_level.load());
    }
    else
    {
        log::warn("Unknown Agent %s Disconnected, Ignoring...\n", mac2str(mac).c_str());
    }
}

extern "C"
void app_main()
{
    TRY(esp_event_loop_create_default());
    esp_log_level_set("wifi", ESP_LOG_WARN);
    std::thread(door_wardern, SOLENOID_PIN).detach();
    
    TRY(gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT));
    TRY(gpio_set_direction(GREEN_LED_PIN, GPIO_MODE_OUTPUT));
    
    /////////////////////////////////////////////
    
    esp_vfs_littlefs_conf_t lfs_config
    {
        .base_path = "/lfs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true
    };
    
    TRY(esp_vfs_littlefs_register(&lfs_config));
    
    // print filesystem info
    {
        size_t total, used = 0;
        auto ret = esp_littlefs_info(lfs_config.partition_label, &total, &used);
        
        if (ret != ESP_OK)
            log::warn("Failed to get LittleFS partition information (%s)\n", esp_err_to_name(ret));
        else
            log::info("Partition used/total: %zu/%zu\n", used, total);
    }
    
    user_manager.init("/lfs/user_list");
    access_logger.init("/lfs/access_logs");
    
    /////////////////////////////////////////////
    
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES or
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        TRY(nvs_flash_erase());
        TRY(nvs_flash_init());
    }
    
    /////////////////////////////////////////////
    
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    wifi_config_t wifi_ap_cfg
    {.ap{
        .ssid = "E-Yantra LAB",
        .password = "no_JUGAAD_4u",
        .authmode = WIFI_AUTH_WPA2_PSK,
        .max_connection = ESP_WIFI_MAX_CONN_NUM,
    }};
    
    wifi_config_t wifi_sta_cfg
    {.sta{
        .ssid = "PU@CAMPUS",
        .password = "1234567890",
    }};
    
    /////////////////////////////////////////////
    
    TRY(esp_netif_init());
    auto wifiAP = esp_netif_create_default_wifi_ap();
    //auto wifiSTA = esp_netif_create_default_wifi_sta();
    TRY(esp_netif_dhcps_start(wifiAP));
    
    esp_netif_dns_info_t cloudflare_dns, google_dns, open_dns;
    IP4_ADDR(&google_dns.ip.u_addr.ip4, 8,8,8,8);
    IP4_ADDR(&cloudflare_dns.ip.u_addr.ip4, 1,1,1,1);
    IP4_ADDR(&open_dns.ip.u_addr.ip4, 208,67,222,222);
    
    TRY(esp_netif_set_dns_info(wifiAP, ESP_NETIF_DNS_MAIN, &cloudflare_dns));
    //TRY(esp_netif_set_dns_info(wifiAP, ESP_NETIF_DNS_BACKUP, &open_dns));
    //TRY(esp_netif_set_dns_info(wifiAP, ESP_NETIF_DNS_FALLBACK, &google_dns));
    
    TRY(esp_wifi_init(&wifi_cfg));
    TRY(esp_wifi_set_mode(WIFI_MODE_AP));
    //TRY(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    TRY(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_cfg));
    //TRY(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_cfg));
    TRY(esp_wifi_start());
    //TRY(esp_wifi_connect());
    
    TRY(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &on_client_got_ip, NULL));
    TRY(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &on_client_connect, NULL));
    TRY(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &on_client_disconnect, NULL));
    
    /////////////////////////////////////////////
    
    httpd_handle_t http_server;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    
    TRY(httpd_start(&http_server, &httpd_config));
    TRY(httpd_register_uri_handler(http_server, &Api::ping));
    TRY(httpd_register_uri_handler(http_server, &Api::access_logs));
    TRY(httpd_register_uri_handler(http_server, &Api::user_list));
    TRY(httpd_register_uri_handler(http_server, &Api::add_user));
    TRY(httpd_register_uri_handler(http_server, &Api::remove_user));
    TRY(httpd_register_uri_handler(http_server, &Api::check_user));
    
    /////////////////////////////////////////////
    
    TRY(touch_pad_init());
    TRY(touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V7, TOUCH_HVOLT_ATTEN_1V5));
    TRY(touch_pad_config(TOUCH_PIN, 0));
    TRY(touch_pad_filter_start(10/*ms*/));
    
    uint16_t touch_value{1500};
    int16_t touch_threshold{1000};
    
    while(1)
    {
        std::this_thread::sleep_for(200ms);
        
        if (touch_pad_read_filtered(TOUCH_PIN, &touch_value) != ESP_OK)
            log::warn("Error reading touchpad !!\n");
        
        log::debug("Touch Value: %5u, Touch Threshold: %5d\n",
                    touch_value,      touch_threshold);
        
        auto absolute_difference = touch_value - touch_threshold;
        if (absolute_difference < 0) absolute_difference *= -1; //math.h log() conflicts with log::
        
        if (absolute_difference > 1000)
            touch_threshold = (touch_value + touch_threshold)/2;
        
        if (touch_value < touch_threshold)
        {
            unlock_signal();
            log::info("Touch Recieved!\n");
        }
    }
}
