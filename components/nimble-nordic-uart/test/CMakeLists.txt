idf_component_register(
  SRC_DIRS "."
  SRCS "test_nimble.c" "test_buffer.c" "test_nimble" "test_buffer"
  INCLUDE_DIRS "."
  REQUIRES nimble-nordic-uart
)