/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"

#include "esp_vfs.h"
#include "sys/unistd.h"

#include "spp_test.h"
#include "spp_init.h"
#include "spp_open_hdr.h"
#include "bt_utils.h"
#include "uart_console.h"

// LOG表示用TAG(関数名にしておく)
#define TAG                 __func__


#define SPP_DATA_LEN    100        // 受信バッファ長

// ================================================================================================
// SPPデータタスク(エコーバックを行う)
// ================================================================================================
void spp_data_task(void* param)
{
    static uint8_t spp_data[SPP_DATA_LEN];

    int size_r = 0;
    int size_w = 0;
    int fd = (int)param;

    do {
        /* controll the log frequency, retry after 1s */
        // vTaskDelay(1000 / portTICK_PERIOD_MS);

        size_r = read(fd, spp_data, SPP_DATA_LEN);
        if (size_r == -1) {
            // クローズされたなど
            ESP_LOGI(TAG, "read : fd = %d data_len = %d", fd, size_r);
            break;
        }
        else if (size_r == 0) {
            // 受信データなし
            // ESP_LOGI(TAG, "read : fd = %d data_len = %d", fd, size_r);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        else {
            // 受信データ表示
            ESP_LOGI(TAG, "read : fd = %d data_len = %d", fd, size_r);
            esp_log_buffer_hex(TAG, spp_data, size_r);
            // エコーバック
            size_w = write(fd, spp_data, size_r);
            ESP_LOGI(TAG, "write : fd = %d data_len = %d", fd, size_w);
        }
    } while (1);

    // タスク終了
    vTaskDelete(NULL);
}

// パラメータテーブル
struct _open_hdr_params     open_hdr_params[OPEN_HDR_NUM] = {0};


// ================================================================================================
// SPP open event handler(オープンイベント時の処理)
// ================================================================================================
void spp_open_handler(uint32_t bd_handle, int fd, esp_bd_addr_t bda)
{
    TaskHandle_t    task_handle;
    int             idx;
    
    for (idx = 0; idx < OPEN_HDR_NUM; idx++) {
        if (!open_hdr_params[idx].use) {
            // 未使用のパラメータテーブルが見つかった
            break;
        }
    }
    if (idx >= OPEN_HDR_NUM) {
        ESP_LOGE(TAG, "Tasks reached the upper limit");
        return;
    }
    ESP_LOGV(TAG, "Parameter table index : %d", idx);

    // データタスクの生成
    BaseType_t ret;
    ret = xTaskCreate(spp_data_task, "spp_data_task", 4096, (void *)fd, 5, &task_handle);
    if (ret == pdPASS) {
        open_hdr_params[idx].use            = true;
        memcpy(open_hdr_params[idx].bda, bda, sizeof(esp_bd_addr_t)) ;
        open_hdr_params[idx].bd_handle      = bd_handle;
        open_hdr_params[idx].fd             = fd;
        open_hdr_params[idx].task_handle    = task_handle;
        ESP_LOGI(TAG, "echo back task created");
    }
    else {
        ESP_LOGE(TAG, "echo back task create error %d", ret);
    }

    return;
}



// ================================================================================================
// SPP close event handler(クローズイベント時の処理)
// ================================================================================================
void spp_close_handler(uint32_t bd_handle)
{
    int             idx;
    
    for (idx = 0; idx < OPEN_HDR_NUM; idx++) {
        if (open_hdr_params[idx].use && open_hdr_params[idx].bd_handle == bd_handle) {
            // 対象のパラメータテーブルが見つかった
            break;
        }
    }
    if (idx >= OPEN_HDR_NUM) {
        ESP_LOGE(TAG, "Tasks reached the upper limit");
        return;
    }
    ESP_LOGV(TAG, "Parameter table index : %d", idx);
    
    ESP_LOGI(TAG, "echo back task tarminate");
    vTaskDelete(open_hdr_params[idx].task_handle);
    open_hdr_params[idx].use = false;

    return;
}

// ================================================================================================
// Close all BD handle
// ================================================================================================
void spp_close_all_handle(void)
{
    int             idx;
    
    for (idx = 0; idx < OPEN_HDR_NUM; idx++) {
        if (open_hdr_params[idx].use ) {
            ESP_LOGI(TAG, "close handle %d", open_hdr_params[idx].bd_handle);
            // 切断処理
            esp_spp_disconnect(open_hdr_params[idx].bd_handle);
            open_hdr_params[idx].use = false;
        }
    }
    return;
}
