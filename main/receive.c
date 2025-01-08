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

#include "structures.h"
#include "test_image_128.h"

#include "spi.h"
#include "receive.h"

extern TaskHandle_t receive_task_h;
extern struct s_framerate fps;
extern EventGroupHandle_t encode_evnt_grp;


uint8_t * recvbuf[2];

uint8_t make_checksum(uint8_t * source, uint16_t source_len)
{
    // Calculate 8 bit checksum of an 8 bit source

    uint8_t chksum = 0;

    for (int i = 0; i < source_len; i++)
        {
            chksum += source[i];
        }
   return(chksum);
} // End of make_checksum 

void copy_test_image(void)
{
    // Put a 'test card' image in both buffers so that something to send when no input
    for (int i = 0; i < 2; i++)
    {
        memcpy(recvbuf[i],test_image_128,TRANS_BLOCK_COUNT * TRANS_BLOCK_SIZE);
    }
} // End of copy_test_image

void receive_task(void *arg)
{
    extern uint16_t ping;

    static char TAG[] = "Receive task";
    uint8_t checksums_dx[TRANS_BLOCK_COUNT] = {0}; // Checksum sent
    uint8_t checksums_rx[TRANS_BLOCK_COUNT] = {0}; // Checksum of the received block of image data

    char checksum_report[TRANS_BLOCK_COUNT + 1] = {0};  // A small zero-terminated space for an array
    bool checksum_error = false;

    uint8_t * block_buf_ptr;

    bool building;              // Track whether a frame build is in progress
    bool has_recieved = false;  // Track whether frames have been received recently

    // Allocate an incoming spi block buffers
    // Buffers to hold a leading number, data and then byte checksum
    
    // Allocate where incoming will be stored, ensure 8 bit and word aligned length
    block_buf_ptr = (uint8_t *)heap_caps_malloc((((TRANS_BLOCK_SIZE + 2) + 3) & ~ 0x03), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    if (block_buf_ptr == NULL)
    {
        ESP_LOGI(TAG,"Failed to allocate incoming block buffer");
        exit(1);
    }
    

    spi_slave_transaction_t spi_transaction; // Only need one as not queing them

    // Declare and clear the transaction items
    // Only one needed as a queue isn't being used
    spi_transaction.length = (TRANS_BLOCK_SIZE + 2) * 8;
    spi_transaction.rx_buffer = block_buf_ptr;
    spi_transaction.tx_buffer = NULL;

    ESP_LOGI(TAG,"Setting a source image buffer");

    // A single buffer for the entire frame, but it will be filled in blocks
    // Although the image is uint16_t RGB565, the incoming SPI data is char
    // A pair of single buffers for the entire frame, but each will be filled in blocks
    for (int i = 0; i < 2; i++)
    {
    recvbuf[i] = heap_caps_malloc((TRANS_BLOCK_COUNT * TRANS_BLOCK_SIZE), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (recvbuf[i] == NULL)
        {
            ESP_LOGE(TAG,"Failed to allocate source frame buffer %d", i);
            exit(1);
        }
    }

    //  Push a test image to begin with
    copy_test_image();

    gpio_set_level(GPIO_HANDSHAKE, 0);
    ESP_LOGI(TAG,"Holding handshake low");

    while(1) // Task loop forever
    {
    // Fetch SPI image here
    xEventGroupWaitBits(encode_evnt_grp, START_RX, true, true, portMAX_DELAY);
    //ESP_LOGI(TAG,"Running receive event");

    building = true; // Set a flag to show a frame is being made
    do
    {   // Keep going until a frame is complete
        
        //ESP_LOGI(TAG,"Receiving a frame");
        gpio_set_level(GPIO_HANDSHAKE, 1);
            vTaskDelay(pdMS_TO_TICKS(2)); // Make sure it's high for long enough for fall to be seen!
        gpio_set_level(GPIO_HANDSHAKE, 0);
        // Sender is falling edge triggered

        // SPI receive will happen in background and the following will block until done
        // Working one block at a time seems best
        esp_err_t ret = spi_slave_transmit(RCV_HOST, &spi_transaction, pdMS_TO_TICKS(250)); // Only wait a second for next block
        //ESP_LOGI(TAG,"Rx %d", (int)ret);
        
        if (ret == ESP_OK)
        {
            // Block received correctly before timeout
            // Copy the block received into the recv image buffer 
            const uint8_t block = block_buf_ptr[0]; // Find the block just received by accesing first byte

            //ESP_LOGI(TAG,"Block Rx %d",(int)block);

            // Ensure block is legitimate
            if (block >=0 && block < TRANS_BLOCK_COUNT)
            {
                // Copy image from third byte onwards into frame receive buffer
                const uint8_t * dest_ptr = &recvbuf[ping][block * TRANS_BLOCK_SIZE];
                memcpy(dest_ptr, &block_buf_ptr[2], TRANS_BLOCK_SIZE);
                checksums_dx[block] = block_buf_ptr[1]; // Fetch checksum from the SPI block 
                // Find the checksum of the image data as received
                checksums_rx[block] = make_checksum(dest_ptr, TRANS_BLOCK_SIZE);

                // Compare _dx and _rx
                if (checksums_dx[block] == checksums_rx[block]) checksum_report[block] = '-';
                else
                {
                    checksum_report[block] = 'X';
                    checksum_error = true;
                    // One option here is to pull previously received block into this frame
                    // to 'correct' a checksum error
                }
            }
            else
            {
                ESP_LOGI(TAG, "Halting SPI sender");
                // It's not a good block so need to halt the SPI sender   
                gpio_set_level(GPIO_HANDSHAKE, 1);
                // Block send until SPI buffers should be well clear 
                vTaskDelay(pdMS_TO_TICKS(300));
            }
        
            // The task is ready to go around again
            if (block == TRANS_BLOCK_COUNT - 1)
            {
                fps.spi_received++; // Count the frames received over SPI bus
                has_recieved = true; // At least one final frame block is known
                // Only show a report if there was an error
                if (checksum_error)
                {
                    ESP_LOGE(TAG,"Rx Checksum report %s",checksum_report);
                } // End of if for checksum error
                checksum_error = false; // Reset the checksum error flag at the frame end
                building = false; // A whole image is in the buffer
            } // End of if for a whole image received
        } //End of ESP_OK for spi rx

        if (ret == SPI_RX_TIMEOUT)
        {
            //ESP_LOGI(TAG, "SPI timeout");
            // Nothing from sender, but keep UVC Tx live
            if (has_recieved)
            {
                const bool pong = ping ^ 0x01; // Force to 'other' buffer
                // Copy previous live buffer into this buffer so the final frame is displayed and held
                memcpy(recvbuf[ping], recvbuf[pong], TRANS_BLOCK_COUNT * TRANS_BLOCK_SIZE);
                //has_recieved = false;                
            }
            else
            {
                //vTaskDelay(pdMS_TO_TICKS(3000)); // Hold final frame
                copy_test_image(); //Put test image into the buffers
            }
            building = false; // Quit the loop even though (or because?) no live image
        }
    } // End of while for a frame
    while (building);

    xEventGroupSetBits(encode_evnt_grp, RX_READY); // Signal that rx is ready
    } // End of infinite task while loop
} // End of receive_task
