#include "unity.h"
#include <limits.h>

#include "nimble-nordic-uart.h"

TEST_CASE("nordic_uart_start", "[bt]") {
  TEST_ESP_OK(nordic_uart_start("Nordic UART", NULL));
  nordic_uart_stop();
}

TEST_CASE("double nordic_uart_start", "[bt]") {
  TEST_ESP_OK(nordic_uart_start("Nordic UART", NULL));
  TEST_ESP_ERR(ESP_FAIL, nordic_uart_start("Nordic UART", NULL));
  nordic_uart_stop();
}

TEST_CASE("twice nordic_uart_start and stop", "[bt]") {
  TEST_ESP_OK(nordic_uart_start("Nordic UART", NULL));
  nordic_uart_stop();
  TEST_ESP_OK(nordic_uart_start("Nordic UART", NULL));
  nordic_uart_stop();
}
