cmake_minimum_required(VERSION 3.20)
project(dinput8_wrapper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Proxy DLL target
add_library(dinput8 SHARED
    src/dllmain.cpp
    src/proxy.cpp
    src/logger.cpp
    src/config.cpp
    src/ffb_filter.cpp
    src/wrapper_effect.cpp
    src/wrapper_device8.cpp
    src/wrapper_dinput8.cpp
    dinput8.def
)

target_include_directories(dinput8 PRIVATE src)

target_compile_definitions(dinput8 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    DIRECTINPUT_VERSION=0x0800
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(dinput8 PRIVATE
    ole32
    dxguid
)

# Suppress noisy warnings from Windows SDK COM macros
target_compile_options(dinput8 PRIVATE
    -Wno-microsoft-exception-spec
)

# Copy config to build directory
if(EXISTS "${CMAKE_SOURCE_DIR}/dinput8.ini")
    configure_file(dinput8.ini "${CMAKE_BINARY_DIR}/dinput8.ini" COPYONLY)
endif()
