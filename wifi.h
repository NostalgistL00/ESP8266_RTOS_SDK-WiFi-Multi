#ifndef _WIFI_H_
#define _WIFI_H_
/* Debug Option */
#define DEBUG_WIFI_C 1

#include <stdint.h>
#define CONNECT_OVERTIME 0
#define KEY_AP_INFO     "AP_INFO_"
#define KEY_AP_NUM_IN_NVS "AP_Num_In_NVS"
#define MAX_AP_NUM 5
#define KEY_IS_INITED   "Inited"
#define KEY_MY_INFO "My_INFO"
#define DEFAULT_SSID  "espressif"
#define DEFAULT_PASSCODE  ""
#define MAX_CONNECTION 4

typedef enum{
    CAPTIVE_PORTAL_YES,
    CAPTIVE_PORTAL_NO,
}captive_portal_t;

typedef enum{
    CONNECT_FROM_HTTP_OK,
    CONNECT_FROM_HTTP_FAIL,
    CONNECT_FROM_NVS_OK,
    CONNECT_FROM_NVS_FAIL,
}connection_state_t;

typedef enum{
    CONNECTION_FROM_NVS,
    CONNECTION_FROM_HTTP,
}connection_source_t;

typedef enum{
    BIT_NVS_ERROR = (1<<0),
    BIT_OKAY      = (1<<1),
    BIT_FROM_HTTP = (1<<2),
    BIT_FROM_NVS  = (1<<3),
}waiting_bit_t;

typedef enum{
    SOFTAP_SET_NAME_DEFAULT,
    SOFTAP_SET_NAME_OK,    
}softap_name_set_t;

typedef struct{
    char ssid[32];
    char passcode [32];
}wifi_info_t;

typedef struct{
    char ssid[32];
    int8_t rssi;
}ap_info_t;

extern uint8_t Auto_reconnect;
void wifi_init(void);
softap_name_set_t wifi_set_ap_info(char *ssid, char *passcode);
void wifi_set_ap_to_default(void);
connection_state_t wifi_connect_from_http(char *ssid, char *passcode);
connection_state_t wifi_connect_from_nvs(void);
captive_portal_t wifi_need_captive_portal(void);
void wifi_get_max_rssi_ap_in_nvs(wifi_info_t *ap_info,uint8_t rank);
void wifi_save_to_nvs(wifi_info_t *ap_info);
void wifi_save_to_nvs_from_string(char *ssid, char *passcode);
uint8_t wifi_nvs_check_or_reset(void);
void wifi_clear_ap_in_nvs(void);
uint8_t wifi_delete_by_ssid(char *ssid);
// void wifi_list_ap_around(ap_info_t* ap_info, uint8_t* num);
#endif 