#include <atomic>
#include <thread>
#include <cstddef>
#include <cstdint>

extern "C"
{
    void app_main();
    
    #include <esp_wifi.h>
    #include <esp_wifi_types.h>
    #include <driver/gpio.h>
    #include <driver/touch_pad.h>
}

using namespace std::literals;
#define TRY(...) ESP_ERROR_CHECK(__va_args__)
std::atomic<int> keep_unlocked{0}, trust_agents{0};

constexpr int TOUCH_THRESHOLD = 800;
constexpr int TOUCH_PIN     = GPIO_NUM_0;
constexpr int RED_LED_PIN   = GPIO_NUM_30;
constexpr int GREEN_LED_PIN = GPIO_NUM_31;
constexpr int SOLENOID_PIN  = GPIO_NUM_32;

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
    ++trust_agents;
    uint8_t* mac = &((wifi_event_ap_staconnected_t*)event_data)->mac;
    ESP_LOGI("Trust Agent %02x:%02x:%02x:%02x:%02x:%02x Connected, Total: %d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], trust_agents);
}

void on_sta_disconnect(void*, esp_event_base_t, int32_t, void* event_data) 
{
    --trust_agents;
    uint8_t* mac = &((wifi_event_ap_staconnected_t*)event_data)->mac;
    ESP_LOGI("Trust Agent %02x:%02x:%02x:%02x:%02x:%02x Disconnected, Total: %d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], trust_agents);
}

void app_main()
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    std::thread(door_wardern, SOLENOID_PIN).detach();
    
    gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_LED_PIN, GPIO_MODE_OUTPUT);
    
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
    {
        .ap.ssid = "Robotics Lab",
        .ap.password = "12345678",
        .ap.authmode = WIFI_AUTH_WPA2_PSK,
        .ap.max_connections = ESP_WIFI_MAX_CONN_NUM,
    };
    
    wifi_config_t wifi_sta_cfg
    {
        .sta.ssid = "WIFI@CAMPUS",
        .sta.password = "1234567890",
    };
    
    /////////////////////////////////////////////
    
    TRY(esp_wifi_init(&wifi_cfg));
    TRY(esp_wifi_set_mode(WIFI_MODE_APSTA));
    TRY(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    TRY(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_cfg));
    TRY(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_cfg));
    TRY(esp_wifi_start());
    TRY(esp_wifi_connect());
    
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
        touch_pad_read_raw_data(i, &touch_value);
        touch_pad_read_filtered(i, &filtered_touch_value);
        ESP_LOGI("Raw: %4d, Filtered: %4d\n", i, touch_value, filtered_touch_value);
        
        if (filtered_touch_value > touch_threshold)
            on_unlock_signal();
    }
}
