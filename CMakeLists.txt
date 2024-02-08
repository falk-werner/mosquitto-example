cmake_minimum_required (VERSION 3.22)
project(mosquitto-example
    VERSION 1.0.0
    DESCRIPTION "MQTT examples using mosquitto")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

find_package(PkgConfig)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)

add_executable(mqtt_pub src/mqtt_pub.c)
target_link_libraries(mqtt_pub PRIVATE ${MOSQUITTO_LIBRARIES})
target_include_directories(mqtt_pub PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
target_compile_options(mqtt_pub PRIVATE ${MOSQUITTO_CFLAGS_OTHER})

add_executable(mqtt_sub src/mqtt_sub.c)
target_link_libraries(mqtt_sub PRIVATE ${MOSQUITTO_LIBRARIES})
target_include_directories(mqtt_sub PRIVATE ${MOSQUITTO_INCLUDE_DIRS})
target_compile_options(mqtt_sub PRIVATE ${MOSQUITTO_CFLAGS_OTHER})
