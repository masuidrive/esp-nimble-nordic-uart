#include <stdint.h>
#include <stdio.h>

#include "esp-nimble-nordic-uart.h"
#include "esp_log.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

void notifyTask(void *parameter) {
  static char mbuf[CONFIG_NORDIC_UART_MAX_LINE_LENGTH + 1];

  for (;;) {
    size_t item_size;
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
      vRingbufferReturnItem(nordic_uart_rx_buf_handle, (void *)item);
    }
  }

  vTaskDelete(NULL);
}

void app_main(void) {
  nordic_uart_start();
  xTaskCreate(notifyTask, "notifyTask", 5000, NULL, 1, NULL);
}
