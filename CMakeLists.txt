# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

idf_component_register(
  REQUIRED_IDF_TARGETS
    "esp32"
    "esp32s3"
    "esp32c3"
  INCLUDE_DIRS
    "src"
  SRCS
    "src/main.c"
  REQUIRES
    bt
  PRIV_REQUIRES
    ${ESP_NIMBLE_PRIV_REQUIRES}
)
project(esp-nimble-nordic-uart)