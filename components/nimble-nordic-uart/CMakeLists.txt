idf_component_register(
  REQUIRED_IDF_TARGETS
    "esp32"
    "esp32s3"
    "esp32c3"
  INCLUDE_DIRS
    "include"
  SRCS
    "main.c" "nimble.c" "buffer.c"
  REQUIRES
    bt unity
  PRIV_REQUIRES
    ${ESP_NIMBLE_PRIV_REQUIRES}
)
project(nimble-nordic-uart)