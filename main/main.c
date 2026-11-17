// WTF-RF-Testbench: controlled laboratory 802.11 beacon injector for XIAO ESP32-C5.
// Uses an explicitly labeled SSID and a locally administered MAC, never a third-party identity.
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"

#define LED GPIO_NUM_27
#define SEND_PIN GPIO_NUM_24   // D5: tap once; hold >= 0.6 s for a continuous 100 ms burst
#define CH_PIN GPIO_NUM_11     // D6: cycle channel 1 -> 6 -> 11
#define POWER_PIN GPIO_NUM_12  // D7: cycle TX setting; values subject to regulatory/driver limits
#define DEBOUNCE_MS 55
#define HOLD_START_MS 600
#define REPEAT_MS 100
#define LED_PULSE_MS 70
static const char *TAG = "WTF_TEST";
static const uint8_t channels[] = {1, 6, 11};
// Values passed to esp_wifi_set_max_tx_power are quarter-dBm units; actual
// chip/PHY output may be clamped or quantized by calibration/driver policy.
static const int8_t power_qdbm[] = {8, 32, 60}; // request ~2, 8, 15 dBm
static const char lab_ssid[] = "WTF-LAB-01";
static uint8_t lab_mac[6];
static unsigned channel_index = 1; // initial channel 6
static unsigned power_index = 0;   // initial low-power setting
static uint32_t tx_ok=0, tx_fail=0;
static int64_t led_until_ms=0;

// Asynchronous, active-low onboard LED status animations. Configuration
// acknowledgments preempt the brief TX pulse, but never delay radio or GPIO
// handling. Pattern counts encode the NEW selection, not the previous one.
typedef struct {
    bool active;
    bool illuminated;
    uint8_t count;
    uint8_t completed;
    uint16_t on_ms;
    uint16_t off_ms;
    int64_t transition_ms;
} led_pattern_t;
static led_pattern_t led_pattern;

static void led_start_pattern(uint8_t count, uint16_t on_ms,
                              uint16_t off_ms, int64_t now_ms) {
    led_pattern = (led_pattern_t){
        .active = true,
        .illuminated = true,
        .count = count,
        .completed = 0,
        .on_ms = on_ms,
        .off_ms = off_ms,
        .transition_ms = now_ms + on_ms,
    };
}

static void led_update(int64_t now_ms) {
    if (led_pattern.active && now_ms >= led_pattern.transition_ms) {
        if (led_pattern.illuminated) {
            led_pattern.illuminated = false;
            led_pattern.completed++;
            if (led_pattern.completed >= led_pattern.count) {
                led_pattern.active = false;
            } else {
                led_pattern.transition_ms = now_ms + led_pattern.off_ms;
            }
        } else {
            led_pattern.illuminated = true;
            led_pattern.transition_ms = now_ms + led_pattern.on_ms;
        }
    }
    const bool on = led_pattern.active
        ? led_pattern.illuminated
        : (now_ms < led_until_ms || (now_ms % 2500) < 35);
    gpio_set_level(LED, on ? 0 : 1);
}

typedef struct {gpio_num_t pin; int raw, stable; int64_t changed, pressed_at, next_repeat;} button_t;
static button_t buttons[]={{SEND_PIN,1,1,0,0,0},{CH_PIN,1,1,0,0,0},{POWER_PIN,1,1,0,0,0}};
static int64_t millis(void){return esp_timer_get_time()/1000;}
static void error_blink(void) {
    for(;;){
        for(int i=0;i<3;i++){gpio_set_level(LED,0);vTaskDelay(pdMS_TO_TICKS(100));gpio_set_level(LED,1);vTaskDelay(pdMS_TO_TICKS(100));}
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

// Standard 802.11 beacon: 24-byte management header, 12-byte fixed params,
// then SSID, supported rates, and DS channel information elements.
// No invented Flock MAC/vendor identifiers. The receiving detector's optional
// lab rule checks the exact SSID and its real received RSSI.
static size_t build_beacon(uint8_t *out, size_t cap, uint8_t channel) {
    const size_t len=sizeof(lab_ssid)-1;
    if(cap<36+2+len+4+3) return 0;
    memset(out,0,cap);
    out[0]=0x80; out[1]=0x00;  // beacon subtype
    memset(out+4,0xff,6);    // broadcast destination
    memcpy(out+10,lab_mac,6); // source
    memcpy(out+16,lab_mac,6); // BSSID
    out[32]=100; out[33]=0;   // beacon interval, 100 TU
    out[34]=0x01; out[35]=0x00; // ESS capabilities, no associated clients
    size_t pos=36;
    out[pos++]=0; out[pos++]=(uint8_t)len;
    memcpy(out+pos,lab_ssid,len); pos+=len;
    out[pos++]=1; out[pos++]=2; out[pos++]=0x82; out[pos++]=0x84; // 1,2 Mbps
    out[pos++]=3; out[pos++]=1; out[pos++]=channel; // DS parameter IE
    return pos;
}

static void send_one(void){
    uint8_t frame[80];
    size_t n=build_beacon(frame,sizeof(frame),channels[channel_index]);
    if(!n) {tx_fail++;return;}
    esp_err_t err=esp_wifi_80211_tx(WIFI_IF_STA,frame,(int)n,true);
    if(err==ESP_OK){
        ++tx_ok;
        led_until_ms=millis()+LED_PULSE_MS;
        ESP_LOGI(TAG,"TX OK #%lu ch=%u power_request=%d qdBm",(unsigned long)tx_ok,channels[channel_index],power_qdbm[power_index]);
    }else{
        ++tx_fail;
        ESP_LOGW(TAG,"TX FAIL #%lu: %s",(unsigned long)tx_fail,esp_err_to_name(err));
    }
}
static void change_channel(void){
    unsigned new_index=(channel_index+1)%3;
    esp_err_t err=esp_wifi_set_channel(channels[new_index],WIFI_SECOND_CHAN_NONE);
    if(err==ESP_OK){
        channel_index=new_index;
        // Short flashes: 1 = channel 6, 2 = channel 11, 3 = channel 1.
        led_start_pattern((uint8_t)(channel_index+1), 100, 140, millis());
        ESP_LOGI(TAG,"Channel now %u",channels[channel_index]);
    }
    else ESP_LOGW(TAG,"Channel change failed: %s",esp_err_to_name(err));
}
static void change_power(void){
    unsigned new_index=(power_index+1)%3;
    esp_err_t err=esp_wifi_set_max_tx_power(power_qdbm[new_index]);
    if(err==ESP_OK){
        power_index=new_index;
        // Longer flashes: 1 = 2 dBm, 2 = 8 dBm, 3 = 15 dBm.
        led_start_pattern((uint8_t)(power_index+1), 300, 170, millis());
        int8_t applied=0;
        if(esp_wifi_get_max_tx_power(&applied)==ESP_OK)
            ESP_LOGI(TAG,"TX power request %d qdBm; driver reports %d qdBm",power_qdbm[power_index],applied);
    }else ESP_LOGW(TAG,"TX power change failed: %s",esp_err_to_name(err));
}
static void on_press(int i,int64_t t){
    if(i==0){buttons[0].pressed_at=t;buttons[0].next_repeat=t+HOLD_START_MS;send_one();}
    else if(i==1) change_channel();
    else change_power();
}
void app_main(void){
    gpio_reset_pin(LED);gpio_set_direction(LED,GPIO_MODE_OUTPUT);gpio_set_level(LED,1);
    const gpio_config_t cfg={.pin_bit_mask=(1ULL<<SEND_PIN)|(1ULL<<CH_PIN)|(1ULL<<POWER_PIN),
        .mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE,.pull_down_en=GPIO_PULLDOWN_DISABLE,.intr_type=GPIO_INTR_DISABLE};
    if(gpio_config(&cfg)!=ESP_OK)error_blink();
    esp_err_t e=nvs_flash_init();
    if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){
        if (nvs_flash_erase()!=ESP_OK) {
            error_blink();
        }
        e=nvs_flash_init();
    }
    if(e!=ESP_OK||esp_netif_init()!=ESP_OK||esp_event_loop_create_default()!=ESP_OK)error_blink();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&init)!=ESP_OK||esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK)error_blink();
    if(esp_wifi_start()!=ESP_OK)error_blink();
    if(esp_wifi_get_mac(WIFI_IF_STA,lab_mac)!=ESP_OK)error_blink();
    // The Espressif-assigned station MAC is already unique. Use its locally
    // administered counterpart solely for identifying this isolated lab rig.
    lab_mac[0]=(lab_mac[0]|0x02)&0xfe;
    if(esp_wifi_set_mac(WIFI_IF_STA,lab_mac)!=ESP_OK)error_blink();
    if(esp_wifi_set_channel(channels[channel_index],WIFI_SECOND_CHAN_NONE)!=ESP_OK)error_blink();
    if(esp_wifi_set_max_tx_power(power_qdbm[power_index])!=ESP_OK)error_blink();
    ESP_LOGI(TAG,"WTF-RF-Testbench v0.1.0 ready: ch=%u, SSID=%s, D5=send/hold, D6=channel, D7=power",channels[channel_index],lab_ssid);
    // Startup: one unmistakable 600 ms ready lamp, distinct from button patterns.
    led_start_pattern(1, 600, 0, millis());
    for(;;){
        const int64_t t=millis();
        for(int i=0;i<3;i++){
            button_t *b=&buttons[i];
            int val=gpio_get_level(b->pin);
            if(val!=b->raw){b->raw=val;b->changed=t;}
            if(val!=b->stable && t-b->changed>=DEBOUNCE_MS){
                b->stable=val;
                if(val==0)on_press(i,t);
            }
        }
        if(buttons[0].stable==0 && t>=buttons[0].next_repeat){
            send_one();buttons[0].next_repeat=t+REPEAT_MS;
        }
        // All LED patterns are advanced here without sleeping for a flash.
        led_update(t);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
