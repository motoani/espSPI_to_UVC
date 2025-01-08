#pragma once

#define RCV_HOST    SPI2_HOST

// The core SPI GPIO lines should be the default ones as these have less propogation delays
// Handshake line is your choice

#define GPIO_HANDSHAKE      40
#define GPIO_MOSI           11
#define GPIO_MISO           -1 // Unused data from slave to host
#define GPIO_SCLK           12
#define GPIO_CS             10

