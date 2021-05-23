#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

extern RingbufHandle_t nordic_uart_rx_buf_handle;

esp_err_t nordic_uart_send(const char *message);
esp_err_t nordic_uart_sendln(const char *message);

void nordic_uart_start(void);
