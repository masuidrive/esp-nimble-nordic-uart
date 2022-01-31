#pragma once

#include "sdkconfig.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

extern RingbufHandle_t nordic_uart_rx_buf_handle;

esp_err_t nordic_uart_send(const char *message);
esp_err_t nordic_uart_sendln(const char *message);

enum nordic_uart_callback_type {
  NORDIC_UART_DISCONNECTED,
  NORDIC_UART_CONNECTED,
};

esp_err_t nordic_uart_start(const char *device_name, void (*callback)(enum nordic_uart_callback_type callback_type));
void nordic_uart_stop(void);
