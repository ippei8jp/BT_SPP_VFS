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

#ifdef  SPP_CLIENT_MODE         // SPP クライアントモード
// 接続先情報
const char remote_device_name[] = REMOTE_DEVICE_NAME;   // デバイス名
esp_bd_addr_t   host_bd_address;                        // BDアドレス
bool            found_bd_addr   = false;
uint8_t         host_service_channel1;                  // サービスチャネル
bool            found_scn1      = false;
uint8_t         host_service_channel2;                  // サービスチャネル
bool            found_scn2      = false;
#endif  // SPP_CLIENT_MODE

// ================================================================================================
// USAGE
// ================================================================================================
static void main_loop_usage(void)
{
    printf("==== Mailn loop USAGE ===========================================\n");
    printf("    ? : Disp this message\n");                  // usage
    printf("    q : Exit from main loop\n");                // ループを抜ける
    printf("    r : Reboot system\n");                      // リブート
    printf("    L : Show paired devices\n");                // ペアリング済みデバイスを表示
    printf("    C : Remove paired devices\n");              // ペアリング済みデバイスをすべて削除
#ifdef  SPP_CLIENT_MODE         // SPP クライアントモード
    printf("    a : Enter the BD address Manually\n");      // BD addressの手動入力
    printf("    d : Start name discovery\n");               // Name Discoveryの開始
    printf("    D : Stop name discovery\n");                // Name Discoveryの停止
    printf("    e : Start service discovery(SPP)\n");       // サービス検出開始(SPP)
    printf("    f : Connect 1st channel\n");                // 接続(チャネル1)
    printf("    g : Connect 2nd channel\n");                // 接続(チャネル2)
#endif  // SPP_CLIENT_MODE
    printf("    Z : close all channels\n");                 // すべてのチャネルを切断
    printf("=================================================================\n");
}

// ================================================================================================
// メインルーチン
// ================================================================================================
void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "==== application start ====================");
    // NVS初期化
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialize nvs failed: %s", esp_err_to_name(err));
        abort();
    }

    // Bluetooth初期化
    err = spp_init();
    if (err != ESP_OK) {
        abort();
    }

    main_loop_usage();
    // メインループ
    while (1) {
        // 無限ループ
        bool    term_flag = false;
        int     in_key = uart_getchar_nowait();
        switch (in_key) {
          case '?' :                                    // usage
            main_loop_usage();
            break;
          case 'q' :                                    // ループを抜ける
            term_flag = true;
            break;
          case 'r' :                                    // reboot
            esp_restart();
            break;
          case 'L' :                                    // ペアリング済みデバイスを表示
            show_paired_devices();
            break;
          case 'C' :                                    // ペアリング済みデバイスをすべて削除
            remove_all_paired_devices();
            break;
#ifdef  SPP_CLIENT_MODE         // SPP クライアントモード
          case 'a' :                                    // BD addressの手動入力 *********************************
            printf("**** input target BD address : ");
            fflush(stdout);
            esp_bd_addr_t   tmp_addr;
            char            bd_addr_buff[20];
            uart_gets(bd_addr_buff, sizeof(bd_addr_buff));
            if (str_to_bdaddr(bd_addr_buff, tmp_addr)) {
                printf("    Input BD_ADDR : %s\n", bdaddr_to_str(tmp_addr, NULL));
                memcpy(host_bd_address, tmp_addr, sizeof(esp_bd_addr_t));
                found_bd_addr = true;
            }
            else {
                printf("    !! INPUT ERROR !!\n");
            }
            break;
          case 'd' :                                    // discovery開始 *********************************
            ;
            esp_bt_inq_mode_t inq_mode = ESP_BT_INQ_MODE_GENERAL_INQUIRY;
            /*
                照会モード
                ESP_BT_INQ_MODE_GENERAL_INQUIRY:        General inquiry mode
                ESP_BT_INQ_MODE_LIMITED_INQUIRY:        Limited inquiry mode
            */
            uint8_t inq_len = 30;
            /*
                照会時間
                単位は1.28sec    設定可能範囲は0x01～0x30
                設定値 30 → 38.4sec
            */
            uint8_t inq_num_rsps = 0;
            /*
                受信可能応答数
                0設定時は制限なし
            */
            esp_bt_gap_start_discovery(inq_mode, inq_len, inq_num_rsps);
            break;
          case 'D' :                                    // discovery停止 *********************************
            esp_bt_gap_cancel_discovery();
            break;
          case 'e' :                                    // サービス検出 *********************************
            if (found_bd_addr) {
                esp_spp_start_discovery(host_bd_address);
            } else { 
                ESP_LOGE(TAG, "BD addr not found");
            }
            break;
          case 'f' :                                    // 接続(チャネル1) *********************************
            if (found_scn1) {
                esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_MASTER, host_service_channel1, host_bd_address);
            } else {
                ESP_LOGE(TAG, "service channel not found");
            }
            break;
          case 'g' :                                    // 接続(チャネル2) *********************************
            if (found_scn2) {
                esp_spp_connect(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_MASTER, host_service_channel2, host_bd_address);
            } else {
                ESP_LOGE(TAG, "service channel not found");
            }
            break;
#endif  // SPP_CLIENT_MODE
          case 'Z' :                                    // すべてのチャネルを切断 *********************************
            spp_close_all_handle();
            break;
        }
        if (term_flag) {
            // 終了フラグ
            break;
        }

        // Task watchdog のトリガを防止するため、vTaskDelay()をコールしておく
        vTaskDelay(1000 / portTICK_PERIOD_MS);          // 1秒待つ
    }
}

