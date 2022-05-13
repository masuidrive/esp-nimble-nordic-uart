#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "nimble-nordic-uart.h"

void echoTask(void *parameter) {
  static char mbuf[CONFIG_NORDIC_UART_MAX_LINE_LENGTH + 1];

  for (;;) {
    size_t item_size;
    if (nordic_uart_rx_buf_handle) {
      const char *item = (char *)xRingbufferReceive(nordic_uart_rx_buf_handle, &item_size, portMAX_DELAY);

      if (item) {
        int i;
        for (i = 0; i < item_size; ++i) {
          if (item[i] >= 'a' && item[i] <= 'z')
            mbuf[i] = item[i] - 0x20;
          else
            mbuf[i] = item[i];
        }
        mbuf[item_size] = '\0';

        nordic_uart_sendln(mbuf);
        puts(mbuf);
        vRingbufferReturnItem(nordic_uart_rx_buf_handle, (void *)item);
      }
    } else {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }

  vTaskDelete(NULL);
}

void app_main(void) {
  nordic_uart_start("Nordic UART", NULL);
  xTaskCreate(echoTask, "echoTask", 5000, NULL, 1, NULL);
}
