#include <bit>
#include <atomic>
#include <thread>
#include <cstddef>
#include <cstdint>
#include <cstring>

#define LOG_LEVEL log::level::debug
#include "log.hpp"
#include "users.hpp"

extern "C"
{
    void app_main();
    
    #include <esp_wifi.h>
    #include <esp_wifi_types.h>
    #include <driver/gpio.h>
    #include <driver/touch_pad.h>
    #include <esp_log.h>
    #include <nvs_flash.h>
}

using namespace std::literals;
#define TRY(...) ESP_ERROR_CHECK(__VA_ARGS__)
std::atomic<int> keep_unlocked{0}, trust_agents{0};

constexpr auto TOUCH_THRESHOLD = 800;
constexpr auto TOUCH_PIN     = TOUCH_PAD_NUM9;
constexpr auto RED_LED_PIN   = GPIO_NUM_25;
constexpr auto GREEN_LED_PIN = GPIO_NUM_26;
constexpr auto SOLENOID_PIN  = GPIO_NUM_17;

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

void on_unlock_signal()
{
    if (trust_agents > 0)
    {
        keep_unlocked += 5000;
        success_feedback();
    }
    else faliure_feedback();
}

void on_sta_connect(void*, esp_event_base_t, int32_t, void* event_data)
{
    auto& mac_arr = ((wifi_event_ap_staconnected_t*)event_data)->mac;
    
    uint8_t mac_buf[8]{};
	std::memcpy(mac_buf, mac_arr, sizeof(mac_arr));
	auto mac = std::bit_cast<mac_address>(mac_buf);
    access_logger.add_log(mac, true);
    
    if (user_manager.check_user(mac))
    {
        ++trust_agents;
        log::info("Trusted Agent %s Connected, Total: %d\n",
             mac2str(mac).c_str(), trust_agents.load());
    }
    else
    {
        log::warn("Unknown Agent %s Connected, Ignoring...\n",
             mac2str(mac).c_str(), trust_agents.load());
    }
}

void on_sta_disconnect(void*, esp_event_base_t, int32_t, void* event_data) 
{
    auto& mac_arr = ((wifi_event_ap_staconnected_t*)event_data)->mac;
    
    uint8_t mac_buf[8]{};
	std::memcpy(mac_buf, mac_arr, sizeof(mac_arr));
	auto mac = std::bit_cast<mac_address>(mac_buf);
    access_logger.add_log(mac, false);
    
    if (user_manager.check_user(mac))
    {
        --trust_agents;
        log::info("Trusted Agent %s Disconnected, Total: %d\n",
             mac2str(mac).c_str(), trust_agents.load());
    }
    else
    {
        log::warn("Unknown Agent %s Disconnected, Ignoring...\n",
             mac2str(mac).c_str(), trust_agents.load());
    }
}

void app_main()
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    std::thread(door_wardern, SOLENOID_PIN).detach();
    
    gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_LED_PIN, GPIO_MODE_OUTPUT);
    
    /////////////////////////////////////////////
    
    if(!user_manager.init("/lfs/user_list"))
        user_manager.load_defaults(); else;
    
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
    
    TRY(esp_wifi_init(&wifi_cfg));
    TRY(esp_wifi_set_mode(WIFI_MODE_APSTA));
    TRY(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    TRY(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_cfg));
    TRY(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_cfg));
    TRY(esp_wifi_start());
    TRY(esp_wifi_connect());
    
    TRY(esp_event_loop_create_default());
    TRY(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_sta_connect, NULL));
    TRY(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_sta_disconnect, NULL));
    
    /////////////////////////////////////////////
    
    TRY(touch_pad_init());
    TRY(touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V));
    TRY(touch_pad_config(TOUCH_PIN, 0));
    TRY(touch_pad_filter_start(10/*ms*/));
    
    uint16_t touch_value, filtered_touch_value;
    
    while(1)
    {
        touch_pad_read_raw_data(TOUCH_PIN, &touch_value);
        touch_pad_read_filtered(TOUCH_PIN, &filtered_touch_value);
        log::debug("Raw: %4d, Filtered: %4d\n", touch_value, filtered_touch_value);
        
        if (filtered_touch_value > TOUCH_THRESHOLD)
            on_unlock_signal();
    }
}
