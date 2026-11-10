cmake_minimum_required(VERSION 3.22.1)
project(mali_g720_tbdr_interceptor C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Otimizações para arquitetura ARMv8-A (CSF/TBDR)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_compile_options(
        -O3
        -fPIC
        -fvisibility=hidden
        -Wall
        -Wextra
        -flto
        -ffast-math
        -DVK_USE_PLATFORM_ANDROID_KHR
    )
    add_link_options(
        -flto
        -Wl,--exclude-libs,ALL
        -Wl,--hash-style=gnu
    )
endif()

include_directories(
    src/include
)

# Arquivos de código-fonte
set(SOURCES
    src/vk_layer_entry.cpp
    src/barrier_reducer.cpp
    src/tbdr_optimizer.cpp
    src/feature_fallbacks.cpp
)

# Gera a biblioteca compartilhada .so (libvulkan_mali_tbdr.so)
add_library(vulkan_mali_tbdr SHARED ${SOURCES})

# Links com bibliotecas do sistema Android
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(vulkan_mali_tbdr
        PRIVATE
        log
        android
    )
endif()

# Define símbolos públicos de exportação da Layer / Driver Vulkan
set_target_properties(vulkan_mali_tbdr PROPERTIES
    OUTPUT_NAME "vulkan_mali_tbdr"
    PREFIX "lib"
    SUFFIX ".so"
)
