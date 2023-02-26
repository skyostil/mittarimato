idf_component_register(
  SRCS
    "display.cc"
    "distance_sensor.cc"
    "i2c.cc"
    "main.cc"
    "spi.cc"
    "rainbow_fx.cc"
  INCLUDE_DIRS ""
  REQUIRES pthread)
component_compile_options("-faligned-new")
