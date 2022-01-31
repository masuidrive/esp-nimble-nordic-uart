idf_component_register(
  SRC_DIRS "."
  SRCS
    "test_main.c"
  INCLUDE_DIRS "."
  REQUIRES cmock nimble-nordic-uart
)