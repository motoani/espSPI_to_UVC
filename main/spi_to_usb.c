/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdint.h>
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/spi_slave.h"
#include "driver/gpio.h"

#include "usb_device_uvc.h"

#include "structures.h"
#include "usb_cam.h"
#include "reporting.h"
#include "encode.h"
#include "receive.h"
#include "spi.h"

struct s_framerate fps; // A structure to track throughput

TaskHandle_t encode_task_h = NULL;
TaskHandle_t receive_task_h = NULL;
EventGroupHandle_t encode_evnt_grp;


// More dev work on Waveshare Pico which has USB hub which
// should simplify USB and UART debugging
// Waveshare Pico has internal 2MB PSRAM and external 16MB quad Flash

// RTOS 
TimerHandle_t seconds_h; 

// Ouput buffer information will be set here
// Lazy global to begin with
uint8_t * out[2] = {NULL}; // Set to NULL initially so we when know it's malloc'd
size_t out_len[2];

uint8_t * uvc_out = NULL; // Buffer pointer for UVC streamer
size_t uvc_out_len = 0;

uint8_t * uvc_local; // Pointer to the local UVC JPG buffer
    

uint16_t ping = 0x00;

void app_main(void)
{
    static const char TAG[] = "Main";

    encode_evnt_grp = xEventGroupCreate();
    // Clear all event flags before sgtarting tasks
    xEventGroupClearBits(encode_evnt_grp, 0xff);

    // Zero the various framerate counters
    fps.spi_received = 0;
    fps.uvc_sent = 0;

    const int dummy = 0;
    // Set up RTOS tasks
    seconds_h = xTimerCreate(
        "Seconds timer",
        pdMS_TO_TICKS(1000),
        pdTRUE,
        (void *) dummy,
        fps_report_cb);
    // and set it running
    xTimerStart(seconds_h,0); // Wait time is irrelevant

    //Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        .mode = 1, // Default 0
        .spics_io_num = GPIO_CS,
        .queue_size = 4, // As each block is being sent individually a small queue is enough
        .flags = 0,
//        .post_setup_cb = my_post_setup_cb,
//        .post_trans_cb = my_post_trans_cb
    };

    //Configuration for the handshake line
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(GPIO_HANDSHAKE),
    };
    //gpio_set_pull_mode(GPIO_HANDSHAKE, GPIO_PULLUP_ONLY);

    //Configure handshake line as output
    gpio_config(&io_conf);
    gpio_set_level(GPIO_HANDSHAKE, 0);

    //Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected.
    gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    //Initialize SPI slave interface
    ESP_ERROR_CHECK(spi_slave_initialize(RCV_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO));

    // Allocate for the UVC local image buffer, raw size will be plenty
    uvc_local = heap_caps_malloc((TRANS_BLOCK_COUNT * TRANS_BLOCK_SIZE), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (uvc_local == NULL)
        {
            ESP_LOGE(TAG,"Failed to allocate UVC local frame buffer");
            exit(1);
        }

    // Make a task for the jpg encoder
    xTaskCreatePinnedToCore(encode_task, "Encode task", 8192, NULL, 6, &encode_task_h, 1);

    // Make a task for the SPI receiver
    xTaskCreatePinnedToCore(receive_task, "Receive task", 8192, NULL, 6, &receive_task_h, 0);

    // Start UVC processes
    ESP_ERROR_CHECK(usb_cam1_init());
    ESP_ERROR_CHECK(uvc_device_init());

    while(1)
    {
    // Start both tasks
    //ESP_LOGI(TAG, "Looping");
    xEventGroupSetBits(encode_evnt_grp, START_RX | START_JPG);
    // Brief pause, which shouldn't slow things as both tasks are ongoing...
    vTaskDelay(10);
    // Wait for both JPG encoder and SPI RX to be completed
    xEventGroupWaitBits(encode_evnt_grp, RX_READY | JPG_READY, true, true, portMAX_DELAY);
    } // end of infinite while
} // end of main

