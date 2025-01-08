# espSPI_to_UVC
A dual core ESP32 dev board is set up as an SPI slave and USB UVC device. This allows it to be an interface between a graphical ESP32 project and a PC. It could be used to allow a GUI or game on an MCU to be recorded without the image quality loss inherent an attempts to video record from a TFT display.

## Process
Ideally I would like to be able to emulate a hardware SPI LCD display driver on an MCU but I suspect that the ESP32's SPI slave architecture and API are unable to do this. The system used in this project requires a SPI sender function to run on the MCU with the display which is a SPI master. The functions here set up an interface MCU as an SPI slave which regulates data flow from the master via a handshake line. One core of the S3 manages data from SPI into pingpong receive buffers and Espressif's tinyusb implementation. The second core runs a software RGB565 to JPG converter, frames from this are sent as a MJPEG stream as a USB UVC device. The common video formats for UVC are MJPEG and H264, MJPEG was chosen as being simpler to encode, but more importantly, each frame is independent. The USB UVC runs as interupt-driven tasks seperately from the SPI reception and JPG encoding and can maintain a USB 'camera' link with a host PC even if the SPI signal is suspended.
As presented the code is fixed for 128 x 128 pixels but it can be readily adapted to other resolutions. Note that the JPG converter task is the bottleneck and, of course, slows with image complexity.
A framerate of up to 20fps is acheiveable if the host can send a image at that rate.

## Code history
The code was developed from Espressif examples for communication between [two MCUs](https://github.com/espressif/esp-idf/blob/master/examples/peripherals/spi_slave/receiver/main/app_main.c) and for an MCU to behave as a [dual camera device](https://components.espressif.com/components/espressif/usb_device_uvc/versions/1.0.0/examples/usb_dual_uvc_device?language=en). This project is reasonable to be considered a project in its own right as it:
* merges the two examples
* includes a simple block and checksum protocol
* recognises and acts on a stalled SPI line

## Hardware
For reasonable performance a dual core ESP32S3 at 240 MHz is required. Future versions such as ESP32P4 are likely to allow high frame rates and larger images to be encoded. PSRAM of 2MB is required for the various frame buffers. There are no obvious reasons why the code cannot be ported to equivalent NXP/STM/RPI MCUs with SPI and USB systems. I developed the project on a [Waveshare Pico S3](https://www.waveshare.com/wiki/ESP32-S3-Pico) board which simplifies connections as it has a UART to USB bridge and USB hub on-board that allow flash, serial montoring and USB functionality through a single USB-C connector. The default SPI lines are accessible and not used for on-board peripherals.
