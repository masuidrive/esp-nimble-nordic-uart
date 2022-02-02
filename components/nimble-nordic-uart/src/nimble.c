#include "nimble-nordic-uart.h"

#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <freertos/FreeRTOS.h>

static const char *TAG = "NORDIC UART";

// #define CONFIG_NORDIC_UART_MAX_LINE_LENGTH 256
// #define CONFIG_NORDIC_UART_RX_BUFFER_SIZE 4096
#define BLE_SEND_MTU 128

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

static void (*_nordic_uart_callback)(enum nordic_uart_callback_type callback_type) = NULL;

static int uart_receive(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
  for (int i = 0; i < ctxt->om->om_len; ++i) {
    const char c = ctxt->om->om_data[i];
    _nordic_uart_linebuf_append(c);
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
             {0},
         }},
    {0}};

static int ble_gap_event_cb(struct ble_gap_event *event, void *arg);

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

  int err = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event_cb, NULL);
  if (err) {
    ESP_LOGE(TAG, "Advertising start failed: err %d", err);
  }
}

static int ble_gap_event_cb(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    ESP_LOGI(TAG, "BLE_GAP_EVENT_CONNECT %s", event->connect.status == 0 ? "OK" : "Failed");
    if (event->connect.status == 0) {
      ble_conn_hdl = event->connect.conn_handle;
      _nordic_uart_buf_init();
      if (_nordic_uart_callback)
        _nordic_uart_callback(NORDIC_UART_CONNECTED);
    } else {
      ble_app_advertise();
    }
    break;
  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "BLE_GAP_EVENT_DISCONNECT");
    if (_nordic_uart_callback)
      _nordic_uart_callback(NORDIC_UART_DISCONNECTED);
    _nordic_uart_buf_deinit();
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

static void ble_app_on_sync_cb(void) {
  int ret = ble_hs_id_infer_auto(0, &ble_addr_type);
  if (ret != 0) {
    ESP_LOGE(TAG, "Error ble_hs_id_infer_auto: %d", ret);
  }
  ble_app_advertise();
}

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/nimble/index.html#_CPPv434esp_nimble_hci_and_controller_initv
static void ble_host_task(void *param) {
  nimble_port_run(); // This function will return only when nimble_port_stop() is executed.
  nimble_port_freertos_deinit();
  _nordic_uart_buf_deinit();
}

// Split the message in BLE_SEND_MTU and send it.
esp_err_t _nordic_uart_send(const char *message) {
  const int len = strlen(message);
  if (len == 0)
    return ESP_OK;
  // Split the message in BLE_SEND_MTU and send it.
  for (int i = 0; i < len; i += BLE_SEND_MTU) {
    int err;
    struct os_mbuf *om;
    int err_count = 0;
  do_notify:
    om = ble_hs_mbuf_from_flat(&message[i], MIN(BLE_SEND_MTU, len - i));
    err = ble_gattc_notify_custom(ble_conn_hdl, notify_char_attr_hdl, om);
    if (err == BLE_HS_ENOMEM && err_count++ < 10) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      goto do_notify;
    }
    if (err)
      return ESP_FAIL;
  }
  return ESP_OK;
}

/***
 *
 * Note:
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/nimble/index.html
 */
esp_err_t _nordic_uart_start(const char *device_name, void (*callback)(enum nordic_uart_callback_type callback_type)) {
  // already initialized will return ESP_FAIL
  if (_nordic_uart_linebuf_initialized()) {
    ESP_LOGE(TAG, "Already initialized");
    return ESP_FAIL;
  }

  if (nvs_flash_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to nvs_flash_init");
    return ESP_FAIL;
  }

  _nordic_uart_callback = callback;

  // Initialize NimBLE
  esp_err_t ret = esp_nimble_hci_and_controller_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_nimble_hci_and_controller_init() failed with error: %d", ret);
    return ESP_FAIL;
  }
  nimble_port_init();

  // Initialize the NimBLE Host configuration
  // Bluetooth device name for advertisement
  ble_svc_gap_device_name_set(device_name);
  ble_svc_gap_init();
  ble_svc_gatt_init();

  ble_gatts_count_cfg(gat_svcs);
  ble_gatts_add_svcs(gat_svcs);

  ble_hs_cfg.sync_cb = ble_app_on_sync_cb;

  // crete NimBLE thread
  nimble_port_freertos_init(ble_host_task);

  return ESP_OK;
}

void _nordic_uart_stop(void) {
  esp_err_t rc = ble_gap_adv_stop();
  if (rc) {
    // if already stoped BLE, some error is raised. but no problem.
    ESP_LOGD(TAG, "Error in stopping advertisement with err code = %d", rc);
  }

  int ret = nimble_port_stop();
  if (ret == 0) {
    nimble_port_deinit();
    ret = esp_nimble_hci_and_controller_deinit();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "esp_nimble_hci_and_controller_deinit() failed with error: %d", ret);
    }
  }

  _nordic_uart_callback = NULL;
}
