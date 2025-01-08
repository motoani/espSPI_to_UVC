#include <stdio.h>
#include <stdint.h>
#include "string.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_err.h"

#include "usb_device_uvc.h"
#include "esp_camera.h" // Contains JPG encoder
#include "usb_cam.h"

#include "structures.h"

// Takes a char buffer, swpabytes, jpeg encode and prepares for UVC stream

    // Link to global buffer parameters
    extern uint8_t * uvc_out;
    extern size_t uvc_out_len;

    extern uint8_t * out[];
    extern size_t out_len[];
    extern uint16_t ping;

    extern char * recvbuf[];

    extern struct s_framerate fps;

    extern EventGroupHandle_t encode_evnt_grp;


void encode_task(void *arg)
{
    portMUX_TYPE *my_spinlock;
    uint16_t * imgbuf;
    static char TAG[] = "encode_task";

    ESP_LOGI(TAG, "Entering encode task");

    // To initialise this task, before the infinite loop, so called on first entry

    // To block UVC accessing buffer at instant of pingpong-ing
    // Allocate the spinlock dynamically
    //my_spinlock = malloc(sizeof(portMUX_TYPE));
    // Initialize the spinlock dynamically
    //portMUX_INITIALIZE(my_spinlock);


    // Note that uint16_t RGB565 must be byte-swapped prior to jpeg encoder
    // So that buffer is big Endian
    imgbuf = heap_caps_malloc((TRANS_BLOCK_COUNT * TRANS_BLOCK_SIZE), MALLOC_CAP_32BIT);
    if (imgbuf == NULL)
        {
            ESP_LOGE(TAG,"Failed to allocate swapbytes frame buffer");
            exit(1);
        }

// The infinite task loop
while(1)
{
    //ESP_LOGI(TAG, "Running encode event");

    xEventGroupWaitBits(encode_evnt_grp, START_JPG, true, true, portMAX_DELAY);

    // pong is opposite of ping flag for accessing buffers
    const uint16_t pong = ping ^ 0x01; // XOR of one bit is a NOT without needing if() branch

    // Convert two chars into uint16_t with intrinsic swapbytes
    for (uint32_t i = 0; i < (128 * 128); i++) imgbuf[i] = (uint16_t)(256 * recvbuf[pong][i*2] + recvbuf[pong][(i*2)+1]);

    if (fmt2jpg((uint8_t *) imgbuf, 128 * 128 * sizeof(uint16_t), UVC_CAM1_FRAME_WIDTH, UVC_CAM1_FRAME_HEIGHT, PIXFORMAT_RGB565, 100, &out[pong], &out_len[pong]))
        {
            // jpg encoding is a slow task and limits frame rate

            //xEventGroupSetBits(encode_evnt_grp,UVC_HALT);
            //vTaskDelay(20); // Allow UVC cb to complete?
            // Block ISR for UVC during thsi switch
            //taskENTER_CRITICAL(my_spinlock);
            // Access the resource
           // An index of 0 or 1 is used and branching avoided
            free(out[ping]); // Free the previous buffer
            uvc_out = out[pong];
            uvc_out_len = out_len[pong];

            ping = pong; // Switch buffers

            //taskEXIT_CRITICAL(my_spinlock); // Allow stream to run now pingpong is done
            //xEventGroupClearBits(encode_evnt_grp,UVC_HALT);
        }
        else
        {
            ESP_LOGE(TAG,"Failed to convert to JPG");
            exit(1);
        }
        // The task is ready to go around again
        xEventGroupSetBits(encode_evnt_grp, JPG_READY);
    } // End of infinite loop
} // End of encode_task
