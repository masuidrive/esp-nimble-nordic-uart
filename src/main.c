#include <stdint.h>
#include <stdio.h>

#include "esp-nimble-nordic-uart.h"

#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "freertos/ringbuf.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "NORDIC UART";

// #define CONFIG_NORDIC_UART_MAX_LINE_LENGTH 256
// #define CONFIG_NORDIC_UART_RX_BUFFER_SIZE 4096
// #define CONFIG_NORDIC_UART_DEVICE_NAME "MY BLE DEVICE"
#define BLE_MTU 128

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
#define SERVICE_UUID 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e

// 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
#define CHAR_UUID_RX 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e

// 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
#define CHAR_UUID_TX 0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e

static uint8_t ble_addr_type;

static uint16_t ble_conn_hdl;
static uint16_t notify_char_attr_hdl;

static char *rx_line_buffer = NULL;
static size_t rx_line_buffer_pos = 0;

// the ringbuffer is an interface with external
RingbufHandle_t nordic_uart_rx_buf_handle;

static int uart_receive(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  for (int i = 0; i < ctxt->om->om_len; ++i) {
    const char c = ctxt->om->om_data[i];
    switch (c) {
    // Skip \r
    case '\r':
      break;

    // send a line to ringbuffer
    case '\n':
      rx_line_buffer[rx_line_buffer_pos] = '\0';
      rx_line_buffer_pos = 0;
      UBaseType_t res =
          xRingbufferSend(nordic_uart_rx_buf_handle, rx_line_buffer, rx_line_buffer_pos, pdMS_TO_TICKS(1000));
      if (res != pdTRUE) {
        ESP_LOGE(TAG, "Failed to send item");
        return -1;
      }
      break;

    // push char to local line buffer
    default:
      if (rx_line_buffer_pos < CONFIG_NORDIC_UART_MAX_LINE_LENGTH - 1) {
        rx_line_buffer[rx_line_buffer_pos++] = c;
      } else {
        ESP_LOGE(TAG, "local line buffer overflow");
        return -1;
      }
      break;
    }
  }
  return 0;
}

// notify GATT callback is no operation.
static int uart_noop(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  return 0;
}

static const struct ble_gatt_svc_def gat_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID128_DECLARE(SERVICE_UUID),
     .characteristics =
         (struct ble_gatt_chr_def[]){

             {.uuid = BLE_UUID128_DECLARE(CHAR_UUID_RX),
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
              .access_cb = uart_receive},

             {.uuid = BLE_UUID128_DECLARE(CHAR_UUID_TX),
              .flags = BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &notify_char_attr_hdl,
              .access_cb = uart_noop},

             {0}}},
    {0}};

static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_app_advertise(void) {
  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof(fields));

  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_DISC_LTD;
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  fields.name = (uint8_t *)ble_svc_gap_device_name();
  fields.name_len = strlen(ble_svc_gap_device_name());
  fields.name_is_complete = 1;

  ble_gap_adv_set_fields(&fields);
  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    ESP_LOGI(TAG, "BLE_GAP_EVENT_CONNECT %s", event->connect.status == 0 ? "OK" : "Failed");
    ble_conn_hdl = event->connect.conn_handle;
    if (event->connect.status != 0) {
      ble_app_advertise();
    }
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "BLE_GAP_EVENT_DISCONNECT");
    ble_app_advertise();
    break;
  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI(TAG, "BLE_GAP_EVENT_ADV_COMPLETE");
    ble_app_advertise();
    break;
  case BLE_GAP_EVENT_SUBSCRIBE:
    ESP_LOGI(TAG, "BLE_GAP_EVENT_SUBSCRIBE");
    break;
  default:
    break;
  }
  return 0;
}

void ble_app_on_sync(void) {
  ble_hs_id_infer_auto(0, &ble_addr_type);
  ble_app_advertise();
}

void host_task(void *param) { nimble_port_run(); }

// Split the message in BLE_MTU and send it.
esp_err_t nordic_uart_send(const char *message) {
  const int len = strlen(message);
  // Split the message in BLE_MTU and send it.
  for (int i = 0; i < len; i += BLE_MTU) {
    int err;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(&message[i], MIN(BLE_MTU, len - i));
  do_notify:
    err = ble_gattc_notify_custom(ble_conn_hdl, notify_char_attr_hdl, om);
    if (err == BLE_HS_ENOMEM) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      goto do_notify;
    }
    if (err)
      return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t nordic_uart_sendln(const char *message) {
  if (strlen(message) > 0) {
    int err1 = nordic_uart_send(message);
    if (err1)
      return ESP_FAIL;
  }
  int err2 = nordic_uart_send("\r\n");
  if (err2)
    return ESP_FAIL;

  return ESP_OK;
}

esp_err_t nordic_uart_start(void) {
  nvs_flash_init();

  // Buffer for receive BLE and split it with /\r*\n/
  rx_line_buffer = malloc(CONFIG_NORDIC_UART_MAX_LINE_LENGTH + 1);
  rx_line_buffer_pos = 0;
  nordic_uart_rx_buf_handle = xRingbufferCreate(CONFIG_NORDIC_UART_RX_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
  if (nordic_uart_rx_buf_handle == NULL) {
    ESP_LOGE(TAG, "Failed to create ring buffer");
    return ESP_FAIL;
  }

  // Initialize NimBLE
  esp_nimble_hci_and_controller_init();
  nimble_port_init();

  ble_svc_gap_device_name_set(CONFIG_NORDIC_UART_DEVICE_NAME);
  ble_svc_gap_init();
  ble_svc_gatt_init();

  ble_gatts_count_cfg(gat_svcs);
  ble_gatts_add_svcs(gat_svcs);

  ble_hs_cfg.sync_cb = ble_app_on_sync;
  nimble_port_freertos_init(host_task);

  return ESP_OK;
}

void nordic_uart_stop(void) {
  free(rx_line_buffer);
  rx_line_buffer = NULL;

  vRingbufferDelete(nordic_uart_rx_buf_handle);
  nordic_uart_rx_buf_handle = NULL;

  nimble_port_freertos_deinit();
}
