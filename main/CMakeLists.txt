set(srcs  "spi_to_usb.c" "usb_cam1.c" "reporting.c" "encode.c" "receive.c")

idf_component_register(SRCS  "${srcs}"
                    INCLUDE_DIRS "."
                    )

