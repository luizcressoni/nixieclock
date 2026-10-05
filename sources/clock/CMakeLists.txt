cmake_minimum_required(VERSION 3.10)
project(nixie)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Overriding this variable wholesale drops the -DNDEBUG that CMake puts in the Release flags,
# which left every assert() in the build enabled. -mtune is the ARM1176 of the Pi Zero; -march
# stays at plain armv6 so the binary keeps running on anything else in the family.
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=armv6 -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -ffast-math")

# OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(utils)
include_directories(camera)

set(UTILS
    src/utils/cJSON.cpp
    src/utils/json_parser.cpp
    src/utils/proc_find.cpp
    src/utils/signals.cpp
    src/utils/median.cpp
    src/utils/ctimer.cpp
)

set (LOGGER
    src/logger/logger.cpp
)

set(MODULATION
    src/nixie/modulation/modulation.cpp
    src/nixie/modulation/sinusoidal.cpp
    src/nixie/modulation/flash.cpp     
    src/nixie/modulation/ramp.cpp
)

set(HARDWARE
    src/nixie/hardware/hardware.cpp
    src/nixie/hardware/dimmer.cpp
    src/nixie/hardware/gpio.cpp
    src/nixie/hardware/pwm.cpp
    src/nixie/hardware/vu.cpp
    src/nixie/hardware/lamps.cpp
    src/nixie/hardware/numbers.cpp
    src/nixie/hardware/rgb.cpp
)

set(DYNAMIC_LIBS
    pthread
    pigpio
    curl
    logger
)

# Camera target
add_executable(camera
    src/camera/main.cpp ${UTILS}
    src/camera/capture.cpp
    src/camera/face_detection.cpp 
    src/camera/motion_detection.cpp
)
target_include_directories(camera PRIVATE utils)
target_link_libraries(camera ${OpenCV_LIBS} ${DYNAMIC_LIBS})

# Nixie target
add_executable(nixie
    src/nixie/main.cpp
    src/nixie/actions.cpp
    src/nixie/fsm.cpp src/nixie/fsm.h
    src/nixie/timethread.cpp
    src/nixie/config.cpp
    src/nixie/wifi.cpp
    src/nixie/weather.cpp
    ${UTILS}
    ${MODULATION}
    ${HARDWARE}
)
target_include_directories(nixie PRIVATE utils modulation hardware)
target_link_libraries(nixie ${DYNAMIC_LIBS})

#cgi-bin target
add_executable(nixie.cgi
    src/cgi-bin/main.cpp ${UTILS}
    src/cgi-bin/parse_stdin.cpp
)
target_include_directories(nixie.cgi PRIVATE utils)
target_link_libraries(nixie.cgi ${DYNAMIC_LIBS})

add_library(logger SHARED ${LOGGER})