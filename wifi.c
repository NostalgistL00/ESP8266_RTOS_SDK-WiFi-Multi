#include "wifi.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "lwip/err.h"
#include "lwip/sys.h"
static const char *TAG = "wifi.c";
uint8_t Auto_reconnect=1;
static EventGroupHandle_t Wifi_Connect_Event_Handle = NULL;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==event_handler begin==");
#endif
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
#if (DEBUG_WIFI_C == 1)
         ESP_LOGI(TAG, "Connected");
#endif
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        if (xEventGroupGetBits(Wifi_Connect_Event_Handle) == BIT_FROM_NVS)
        {
#if (DEBUG_WIFI_C == 1)
            ESP_LOGI(TAG, "Connected from NVS");
#endif
            xEventGroupSetBits(Wifi_Connect_Event_Handle, BIT_OKAY);
        }
        else if (xEventGroupGetBits(Wifi_Connect_Event_Handle) == BIT_FROM_HTTP)
        {
#if (DEBUG_WIFI_C == 1)
            ESP_LOGI(TAG, "Connected from HTTP");
#endif
            wifi_info_t ap_info = {
                .ssid = ""};
            strcpy(ap_info.ssid, (char *)&(event->ssid));
            strcpy(ap_info.passcode, (char *)arg);
            wifi_save_to_nvs(&ap_info);
            xEventGroupSetBits(Wifi_Connect_Event_Handle, BIT_OKAY);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Disconnected,%d",Auto_reconnect);
#endif
        if(Auto_reconnect){
            wifi_connect_from_nvs();
        }        
    }
#if (DEBUG_WIFI_C == 1)
            ESP_LOGI(TAG, "==event_handler end==");
#endif
}

connection_state_t wifi_connect_from_nvs(void)
{
#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==wifi_connect_from_nvs begin==");
#endif

    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    uint8_t AP_Num_In_NVS = 0;
    esp_err_t err = nvs_get_u8(my_handle, KEY_AP_NUM_IN_NVS, &AP_Num_In_NVS);
    if (err == ESP_ERR_NVS_INVALID_NAME || AP_Num_In_NVS != MAX_AP_NUM)
    {

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "no ap in nvs flash,need to init.");
#endif

        nvs_set_u8(my_handle, KEY_AP_NUM_IN_NVS, MAX_AP_NUM);
        nvs_close(my_handle);
    }

    wifi_info_t wifi_info = {
        .ssid = "",
        .passcode = ""};
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        }};

    for (uint8_t i = 0; i < AP_Num_In_NVS; i++)
    {
        wifi_get_max_rssi_ap_in_nvs(&wifi_info, i + 1);
        if (strlen(wifi_info.ssid) == 0)
        {

#if (DEBUG_WIFI_C == 1)
            ESP_LOGI(TAG, "No ap in nvs flash.");
            ESP_LOGI(TAG, "==wifi_connect_from_nvs end==");
#endif

            return CONNECT_FROM_NVS_FAIL;
        }
        strcpy((char *)(wifi_config.sta.ssid), (char *)(wifi_info.ssid));
        strcpy((char *)(wifi_config.sta.password), (char *)(wifi_info.passcode));

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "connecting to ssid:%s,password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
#endif

        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &event_handler, NULL);
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
        esp_wifi_connect();

        xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);
        xEventGroupSetBits(Wifi_Connect_Event_Handle, BIT_FROM_NVS);

        EventBits_t uxBits = xEventGroupWaitBits(Wifi_Connect_Event_Handle,
                                                 BIT_OKAY, pdFALSE, pdFALSE, 20000 / portTICK_PERIOD_MS);
        esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &event_handler);
        switch (uxBits & (BIT_OKAY | BIT_NVS_ERROR))
        {
        case BIT_OKAY:
#if (DEBUG_WIFI_C == 1)
            ESP_LOGI(TAG, "Okay");
            ESP_LOGI(TAG, "==wifi_connect_from_nvs end==");
#endif
            xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);
            return CONNECT_FROM_NVS_OK;
            break;
        case CONNECT_OVERTIME:
#if (DEBUG_WIFI_C == 1)
            ESP_LOGI(TAG, "Connection over time,maybe passcode or ssid is wrong.");
            ESP_LOGI(TAG, "==wifi_connect_from_nvs end==");
#endif
            xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);
            break;
        default:
            ESP_LOGI(TAG, "ERROR OCCUR:%s,%d", __FILE__, __LINE__);
            ESP_LOGI(TAG, "==wifi_connect_from_nvs end==");
            while (1)
                ;
            break;
        }
    }
    return CONNECT_FROM_NVS_FAIL;
}

void wifi_init(void)
{

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==wifi_init begin==");
#endif

    wifi_nvs_check_or_reset();
    nvs_flash_init();
    tcpip_adapter_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL);
    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    size_t required_size_wifi_info;
    nvs_get_blob(my_handle, KEY_MY_INFO, NULL, &required_size_wifi_info);
    wifi_info_t *wifi_info = (wifi_info_t *)calloc(sizeof(wifi_info_t), 1);
    nvs_get_blob(my_handle, KEY_MY_INFO, wifi_info, &required_size_wifi_info);
    nvs_close(my_handle);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "Read SSID and Passcode from nvs flash.");
    ESP_LOGI(TAG, "wifi_set_ap_info(ssid:%s,passcode:%s);", wifi_info->ssid, wifi_info->passcode);
#endif
    wifi_set_ap_info((char *)wifi_info->ssid, (char *)wifi_info->passcode);
    free(wifi_info);

    esp_wifi_start();

    Wifi_Connect_Event_Handle = xEventGroupCreate();
    xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "%s", (wifi_connect_from_nvs() == CONNECT_FROM_NVS_OK) ? "connected from nvs" : "fail to connect from nvs");
#else
    wifi_connect_from_nvs();
#endif

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==wifi_init end==");
#endif
}

captive_portal_t wifi_need_captive_portal(void)
{
    wifi_ap_record_t current_ap_info;
    wifi_sta_list_t sta_info;
    esp_wifi_ap_get_sta_list(&sta_info);
    if ((esp_wifi_sta_get_ap_info(&current_ap_info) != ESP_OK) && (sta_info.num <= 1))
    {
        return CAPTIVE_PORTAL_YES;
    }
    else
    {
        return CAPTIVE_PORTAL_NO;
    }
}

connection_state_t wifi_connect_from_http(char *ssid, char *passcode)
{
#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==wifi_connct_from_http begin==");
    ESP_LOGI(TAG, "wifi_connct_from_http,ssid:%s,passcode:%s", ssid, passcode);
#endif

    uint8_t ssid_len = strlen(ssid);
    uint8_t passcode_len = strlen(passcode);

    if (ssid_len == 0)
    {
        return CONNECT_FROM_HTTP_FAIL;
    }
    esp_wifi_disconnect();
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &event_handler, passcode);
    wifi_config_t wifi_config = {0};
    strcpy((char *)&wifi_config.sta.ssid, ssid);
    strcpy((char *)&wifi_config.sta.password, passcode);
    if (passcode_len)
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "wifi_config.sta.ssid:%s,wifi_config.sta.passcode:%s", wifi_config.sta.ssid, wifi_config.sta.password);
#endif

    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_connect();

    xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);
    xEventGroupSetBits(Wifi_Connect_Event_Handle, BIT_FROM_HTTP);
    EventBits_t uxBits = xEventGroupWaitBits(Wifi_Connect_Event_Handle,
                                             BIT_NVS_ERROR | BIT_OKAY, pdFALSE, pdFALSE, 30000 / portTICK_PERIOD_MS);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &event_handler);
    switch (uxBits & (BIT_OKAY | BIT_NVS_ERROR))
    {
    case BIT_OKAY:
#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Okay");
        ESP_LOGI(TAG, "==wifi_connct_from_http end==");
#endif
        xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);
        return CONNECT_FROM_HTTP_OK;
        break;
    case BIT_NVS_ERROR:
#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "NVS Error");
        ESP_LOGI(TAG, "==wifi_connct_from_http end==");
#endif
        xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);
        return CONNECT_FROM_HTTP_FAIL;
        break;
    case CONNECT_OVERTIME:
#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Connection over time,maybe passcode or ssid is wrong.");
        ESP_LOGI(TAG, "==wifi_connct_from_http end==");
#endif
        xEventGroupClearBits(Wifi_Connect_Event_Handle, 0xFFFFFFFF);
        wifi_connect_from_nvs();
        return CONNECT_FROM_HTTP_FAIL;
        break;
    default:
        ESP_LOGI(TAG, "ERROR OCCUR:%s,%d", __FILE__, __LINE__);
        ESP_LOGI(TAG, "==wifi_connct_from_http end==");
        while (1)
            ;
        break;
    }
}

softap_name_set_t wifi_set_ap_info(char *ssid, char *passcode)
{

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_set_ap_info begin==");
    ESP_LOGI(TAG, "function para,ssid:%s,passcode:%s", ssid, passcode);

#endif

    if (strlen(ssid) == 0)
    {
        wifi_set_ap_to_default();
        return SOFTAP_SET_NAME_DEFAULT;
    }

    char *empty_passcode = "";
    if (strlen(passcode) < 8)
    {
#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Passcode length lessthan 8,the passcode will be set to empty.");
#endif
        passcode = empty_passcode;
    }
    nvs_flash_init();
    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .max_connection = MAX_CONNECTION,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    wifi_info_t wifi_info = {0};

    strcpy((char *)&wifi_info.ssid, ssid);
    strcpy((char *)&wifi_info.passcode, passcode);
    strcpy((char *)&wifi_config.ap.ssid, ssid);
    strcpy((char *)&wifi_config.ap.password, passcode);
    nvs_set_blob(my_handle, KEY_MY_INFO, &wifi_info, sizeof(wifi_info));
    nvs_commit(my_handle);
    nvs_close(my_handle);

    if (strlen(passcode) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "ssid:%s,passcode:%s", wifi_config.ap.ssid, wifi_config.ap.password);
    ESP_LOGI(TAG, "==function:wifi_set_ap_info end==");
#endif

    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    return SOFTAP_SET_NAME_OK;
}

void wifi_set_ap_to_default(void)
{
#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_set_ap_to_default begin==");
#endif

    nvs_flash_init();
    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);
    uint8_t flag_use_default = 0;
    nvs_get_u8(my_handle, KEY_IS_INITED, &flag_use_default);
    if (flag_use_default != 1)
    {
        wifi_info_t wifi_info = {0};
        strcpy((char *)&wifi_info.ssid, DEFAULT_SSID);
        strcpy((char *)&wifi_info.ssid, DEFAULT_PASSCODE);
        nvs_set_u8(my_handle, KEY_IS_INITED, (uint8_t)1);
        nvs_set_blob(my_handle, KEY_MY_INFO, &wifi_info, sizeof(wifi_info_t));
    }

    wifi_info_t wifi_info = {
        .ssid = DEFAULT_SSID,
        .passcode = DEFAULT_PASSCODE,
    };

    nvs_set_blob(my_handle, KEY_MY_INFO, &wifi_info, sizeof(wifi_info_t));
    nvs_commit(my_handle);
    nvs_close(my_handle);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = DEFAULT_SSID,
            .ssid_len = strlen(DEFAULT_SSID),
            .password = DEFAULT_PASSCODE,
            .max_connection = MAX_CONNECTION,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    if (strlen(DEFAULT_PASSCODE) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "ssid:%s,passcode:%s", wifi_config.ap.ssid, wifi_config.ap.password);
    ESP_LOGI(TAG, "==function:wifi_set_ap_to_default end==");
#endif

    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
}

void wifi_get_max_rssi_ap_in_nvs(wifi_info_t *ap_info, uint8_t rank)
{

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_get_max_rssi_ap_in_nvs begin==");
    ESP_LOGI(TAG, "function para,rank:%d", rank);

#endif

    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    uint8_t AP_Num_In_NVS = 0;
    nvs_get_u8(my_handle, KEY_AP_NUM_IN_NVS, &AP_Num_In_NVS);

    wifi_info_t *aps_in_nvs = (wifi_info_t *)calloc(AP_Num_In_NVS * sizeof(wifi_info_t), 1);
    for (uint8_t i = 0; i < AP_Num_In_NVS; i++)
    {
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", i + 1);
        strcat(Key_AP_INFO, Key_AP_INFO_num);

        size_t required_size_wifi_info;
        nvs_get_blob(my_handle, Key_AP_INFO, NULL, &required_size_wifi_info);
        nvs_get_blob(my_handle, Key_AP_INFO, aps_in_nvs + i, &required_size_wifi_info);

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Key:%s,ssid:%s,passcode:%s", Key_AP_INFO, (aps_in_nvs + i)->ssid, (aps_in_nvs + i)->passcode);
#endif
    }

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "Start Scan");
#endif

    uint16_t ap_count;
    wifi_ap_record_t *ap_record;
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&ap_count);
    ap_record = (wifi_ap_record_t *)calloc(ap_count * sizeof(wifi_ap_record_t), 1);
    esp_wifi_scan_get_ap_records(&ap_count, ap_record);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "Number of aps:%d", ap_count);
#endif

    int16_t max_rssi = -32768, max_num = 0;
    uint8_t k = 0;
    for (uint8_t i = 0; i < ap_count; i++)
    {
        uint16_t j = 0;
        for (; j < AP_Num_In_NVS; j++)
        {
            if (strcmp((char *)(ap_record + i)->ssid, (char *)(aps_in_nvs + j)->ssid) == 0)
            {

#if (DEBUG_WIFI_C == 1)
                ESP_LOGI(TAG, "Find ap in nvs flash,ssid:%s,rssi:%d", (char *)(ap_record + i)->ssid, (ap_record + i)->rssi);
#endif

                max_rssi = (ap_record + i)->rssi;
                max_num = j;
                k++;
            }
        }
        if (k == rank)
        {
            break;
        }
    }

    if (max_rssi == -32768)
    {
        strcpy((char *)ap_info->ssid, "");
        strcpy((char *)ap_info->passcode, "");

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "No ap matches.");
#endif
    }
    else
    {
        strcpy((char *)ap_info->ssid, (aps_in_nvs + max_num)->ssid);
        strcpy((char *)ap_info->passcode, (aps_in_nvs + max_num)->passcode);

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Final max rssi ap is num:%d,ssid:%s,passcode:%s", max_num, ap_info->ssid, ap_info->passcode);
#endif
    }
    nvs_commit(my_handle);
    nvs_close(my_handle);
    free(aps_in_nvs);
    free(ap_record);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_get_max_rssi_ap_in_nvs end==");
#endif
}

void wifi_save_to_nvs(wifi_info_t *ap_info)
{

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_save_to_nvs begin==");
#endif

    wifi_delete_by_ssid(ap_info->ssid);
    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    uint8_t AP_Num_In_NVS = 0;
    nvs_get_u8(my_handle, KEY_AP_NUM_IN_NVS, &AP_Num_In_NVS);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "AP_Num_In_NVS:%d", AP_Num_In_NVS);
#endif

    wifi_info_t *aps_in_nvs = (wifi_info_t *)calloc((AP_Num_In_NVS + 1) * sizeof(wifi_info_t), 1);
    for (uint8_t i = 0; i < AP_Num_In_NVS; i++)
    {
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", i + 1);
        strcat(Key_AP_INFO, Key_AP_INFO_num);

        size_t required_size_wifi_info;
        nvs_get_blob(my_handle, Key_AP_INFO, NULL, &required_size_wifi_info);
        nvs_get_blob(my_handle, Key_AP_INFO, aps_in_nvs + i + 1, &required_size_wifi_info);

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Key:%s", Key_AP_INFO);
        ESP_LOGI(TAG, "read->.ssid:%s,passcode:%s", (aps_in_nvs + i + 1)->ssid, (aps_in_nvs + i + 1)->passcode);
#endif
    }
    *aps_in_nvs = *ap_info;
    for (uint8_t i = 0; i < AP_Num_In_NVS; i++)
    {
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", i + 1);
        strcat(Key_AP_INFO, Key_AP_INFO_num);
        nvs_set_blob(my_handle, Key_AP_INFO, aps_in_nvs + i, sizeof(wifi_info_t));

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Key:%s", Key_AP_INFO);
        ESP_LOGI(TAG, "write->ssid:%s,passcode:%s", (aps_in_nvs + i)->ssid, (aps_in_nvs + i)->passcode);
#endif
    }

    nvs_commit(my_handle);
    nvs_close(my_handle);
    free(aps_in_nvs);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_save_to_nvs end==");
#endif
}

void wifi_save_to_nvs_from_string(char *ssid, char *passcode)
{

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_save_to_nvs_from_string begin==");
#endif

    wifi_delete_by_ssid(ssid);
    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    uint8_t AP_Num_In_NVS = 0;
    nvs_get_u8(my_handle, KEY_AP_NUM_IN_NVS, &AP_Num_In_NVS);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "AP_Num_In_NVS:%d", AP_Num_In_NVS);
#endif

    wifi_info_t *aps_in_nvs = (wifi_info_t *)calloc((AP_Num_In_NVS + 1) * sizeof(wifi_info_t), 1);
    for (uint8_t i = 0; i < AP_Num_In_NVS; i++)
    {
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", i + 1);
        strcat(Key_AP_INFO, Key_AP_INFO_num);

        size_t required_size_wifi_info;
        nvs_get_blob(my_handle, Key_AP_INFO, NULL, &required_size_wifi_info);
        nvs_get_blob(my_handle, Key_AP_INFO, aps_in_nvs + i + 1, &required_size_wifi_info);

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Key:%s", Key_AP_INFO);
        ESP_LOGI(TAG, "read->.ssid:%s,passcode:%s", (aps_in_nvs + i + 1)->ssid, (aps_in_nvs + i + 1)->passcode);
#endif
    }

    wifi_info_t ap_info={0};
    strcpy(ap_info.passcode,passcode);
    strcpy(ap_info.ssid,ssid);
    *aps_in_nvs = ap_info;
    for (uint8_t i = 0; i < AP_Num_In_NVS; i++)
    {
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", i + 1);
        strcat(Key_AP_INFO, Key_AP_INFO_num);
        nvs_set_blob(my_handle, Key_AP_INFO, aps_in_nvs + i, sizeof(wifi_info_t));

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Key:%s", Key_AP_INFO);
        ESP_LOGI(TAG, "write->ssid:%s,passcode:%s", (aps_in_nvs + i)->ssid, (aps_in_nvs + i)->passcode);
#endif
    }

    nvs_commit(my_handle);
    nvs_close(my_handle);
    free(aps_in_nvs);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_save_to_nvs end==");
#endif
}

uint8_t wifi_nvs_check_or_reset(void)
{
#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_nvs_check_or_reset begin==");
#endif
    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    uint8_t flag_use_default = 0;
    nvs_get_u8(my_handle, KEY_IS_INITED, &flag_use_default);
    if (flag_use_default != 1)
    {
        nvs_set_u8(my_handle, KEY_IS_INITED, 1);
        nvs_set_u8(my_handle, KEY_AP_NUM_IN_NVS, MAX_AP_NUM);
        nvs_commit(my_handle);
        nvs_close(my_handle);

        wifi_set_ap_to_default();
        wifi_clear_ap_in_nvs();

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "First time set ap,reseted");
        ESP_LOGI(TAG, "MAX_AP_NUM:%d", MAX_AP_NUM);
        ESP_LOGI(TAG, "==function:wifi_nvs_check_or_reset end==");
#endif

        return 1;
    }
    uint8_t AP_Num_In_NVS = 0;
    nvs_get_u8(my_handle, KEY_AP_NUM_IN_NVS, &AP_Num_In_NVS);
    nvs_commit(my_handle);
    nvs_close(my_handle);
    if (AP_Num_In_NVS != MAX_AP_NUM)
    {
        wifi_clear_ap_in_nvs();

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Max_AP_NUM is not the AP_Num_In_NVS,reseted");
        ESP_LOGI(TAG, "MAX_AP_NUM:%d,AP_Num_In_NVS:%d", MAX_AP_NUM, AP_Num_In_NVS);
        ESP_LOGI(TAG, "==function:wifi_nvs_check_or_reset end==");
#endif

        return 2;
    }

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "NVS Value is Okay,MAX_AP_NUM:%d", AP_Num_In_NVS);
    ESP_LOGI(TAG, "==function:wifi_nvs_check_or_reset end==");
#endif

    return 0;
}

void wifi_clear_ap_in_nvs(void)
{
#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_clear_ap_in_nvs begin==");
#endif

    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    nvs_set_u8(my_handle, KEY_IS_INITED, 1);
    nvs_set_u8(my_handle, KEY_AP_NUM_IN_NVS, MAX_AP_NUM);
    for (uint8_t i = 0; i < MAX_AP_NUM; i++)
    {
        wifi_info_t wifi_info_empty = {0};
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", i + 1);
        strcat(Key_AP_INFO, Key_AP_INFO_num);
        nvs_set_blob(my_handle, Key_AP_INFO, &wifi_info_empty, sizeof(wifi_info_t));

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Key:%s", Key_AP_INFO);
        ESP_LOGI(TAG, "write->ssid:%s,passcode:%s", wifi_info_empty.ssid, wifi_info_empty.passcode);
#endif
    }
    nvs_commit(my_handle);
    nvs_close(my_handle);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_clear_ap_in_nvs end==");
#endif
}

uint8_t wifi_delete_by_ssid(char *ssid)
{

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "==function:wifi_delete_by_ssid begin==");
    ESP_LOGI(TAG, "ssid you want to delete:%s", ssid);
#endif

    nvs_handle my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);

    uint8_t AP_Num_In_NVS = 0;
    nvs_get_u8(my_handle, KEY_AP_NUM_IN_NVS, &AP_Num_In_NVS);

#if (DEBUG_WIFI_C == 1)
    ESP_LOGI(TAG, "AP_Num_In_NVS:%d", AP_Num_In_NVS);
#endif

    wifi_info_t *aps_in_nvs = (wifi_info_t *)calloc((AP_Num_In_NVS) * sizeof(wifi_info_t), 1);
    for (uint8_t i = 0; i < AP_Num_In_NVS; i++)
    {
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", i + 1);
        strcat(Key_AP_INFO, Key_AP_INFO_num);

        size_t required_size_wifi_info;
        nvs_get_blob(my_handle, Key_AP_INFO, NULL, &required_size_wifi_info);
        nvs_get_blob(my_handle, Key_AP_INFO, aps_in_nvs + i, &required_size_wifi_info);

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "Key:%s", Key_AP_INFO);
        ESP_LOGI(TAG, "read->.ssid:%s,passcode:%s", (aps_in_nvs + i)->ssid, (aps_in_nvs + i)->passcode);
#endif
    }

    uint8_t num = 0;
    for (num = 0; num < AP_Num_In_NVS; num++)
    {
        if (strcmp((aps_in_nvs + num)->ssid, ssid) == 0)
        {
            break;
        }
    }

    if (num == AP_Num_In_NVS)
    {

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "no ssid matches,return");
#endif

        free(aps_in_nvs);
        nvs_commit(my_handle);
        nvs_close(my_handle);
        return 0;
    }
    else
    {

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "found ssid in nvs:%d", num);
#endif

        for (uint8_t i = num; i < AP_Num_In_NVS - 1; i++)
        {
            char Key_AP_INFO[32] = KEY_AP_INFO;
            char Key_AP_INFO_num[16] = "";
            sprintf(Key_AP_INFO_num, "%d", i + 1);
            strcat(Key_AP_INFO, Key_AP_INFO_num);
            nvs_set_blob(my_handle, Key_AP_INFO, aps_in_nvs + i + 1, sizeof(wifi_info_t));

#if (DEBUG_WIFI_C == 1)
            ESP_LOGI(TAG, "Key:%s", Key_AP_INFO);
            ESP_LOGI(TAG, "write->ssid:%s,passcode:%s", (aps_in_nvs + i + 1)->ssid, (aps_in_nvs + i + 1)->passcode);
#endif
        }

        wifi_info_t wifi_info_empty = {0};
        char Key_AP_INFO[32] = KEY_AP_INFO;
        char Key_AP_INFO_num[16] = "";
        sprintf(Key_AP_INFO_num, "%d", MAX_AP_NUM);
        strcat(Key_AP_INFO, Key_AP_INFO_num);
        nvs_set_blob(my_handle, Key_AP_INFO, &wifi_info_empty, sizeof(wifi_info_t));
        nvs_commit(my_handle);
        nvs_close(my_handle);
        free(aps_in_nvs);

#if (DEBUG_WIFI_C == 1)
        ESP_LOGI(TAG, "==function:wifi_delete_by_ssid end==");
#endif

        return 1;
    }
}


// void wifi_list_ap_around(ap_info_t* ap_info, uint8_t* num){

//     #if (DEBUG_WIFI_C == 1)
//     ESP_LOGI(TAG,"==function:wifi_list_ap_around begin==");
//     #endif

//     esp_wifi_scan_start(NULL, true);
//     esp_wifi_scan_get_ap_records(&num, ap_info);

//     #if (DEBUG_WIFI_C == 1)
//     ESP_LOGI(TAG,"==function:wifi_list_ap_around end==");
//     #endif

// }