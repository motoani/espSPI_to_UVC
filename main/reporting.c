// Some routines to report on performance
// With sender at 2Mhz / 160 Mhz SPI Rx 8fps
// At same clock, SPI send alone is 10 or 11fps
// Suggestting that slowest point is JPG encode

#include "stdint.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "structures.h"
#include "reporting.h"

void fps_report_cb( TimerHandle_t seconds_h)
{
    static char TAG[] = "fps_report ";

    extern struct s_framerate fps;
    static struct s_framerate old_fps = {0,0}; // A starting value
    char bar_buffer[50] = {0}; // A buffer where a string will be made
    const char messages[2][7] ={"SPI Rx", "UVC Tx"};

    static uint16_t odd_even = 0;
    uint32_t sent;

    // Alternate between the performance fields
    if (odd_even)
    {
        sent = fps.uvc_sent - old_fps.uvc_sent;
    }
    else
    {
        sent = fps.spi_received - old_fps.spi_received;
    }
    if (sent > 49) sent = 49; // Prohibit buffer overrun
    // Make a bar of throughput
    for (int i = 0; i < sent; i++)
    {
        if (i % 5) bar_buffer[i] = '-';
            else bar_buffer[i] = '|';
    }
    bar_buffer[sent] = 0x00; // String null terminator

    // Alternate the text
    ESP_LOGI(TAG,"%s %s",messages[odd_even],bar_buffer);
    //ESP_LOGI(TAG,"%d",(int)fps.uvc_sent);

    // Note the position when last called
    old_fps.spi_received = fps.spi_received;
    old_fps.uvc_sent = fps.uvc_sent;

    odd_even = odd_even ^ 0x01; //Swap for next iteration
}
