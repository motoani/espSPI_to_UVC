#pragma once

#include "stdint.h"

// SPI data is counted as bytes and bits rather than words
#define TRANS_BLOCK_SIZE 2048
#define TRANS_BLOCK_COUNT 16 // For a 128 x 128 px RGB565 image

#define START_JPG   0x01 // Allow to start encoding task
#define JPG_READY   0x02 // Encoder has finished
#define START_RX    0x04 // Start the SPI reader task
#define RX_READY    0x08 // SPI reciever has read a frame
#define UVC_HALT    0x10 // This is set when pingpong is switching

typedef struct s_framerate
{
    uint32_t uvc_sent;
    uint32_t spi_received;
} s_framerate;
