# Rivulet - Modern CEF-Spout Video Sharing Application
# Copyright (c) 2025 Rivulet Project
# Based on proven CEF-Spout architecture with modern implementation

cmake_minimum_required(VERSION 3.21)

# Project definition
project(Rivulet 
    VERSION 1.0.0
    DESCRIPTION "High-performance web content to Spout2 video sharing"
    LANGUAGES CXX
)

# Modern C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use static runtime library to match CEF
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Build configuration
set(CMAKE_CONFIGURATION_TYPES Debug Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Use folders in IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Windows-specific settings
if(WIN32)
    # Unicode support
    add_definitions(-DUNICODE -D_UNICODE)
    
    # High performance GPU selection
    add_compile_definitions(
        NOMINMAX  # Prevent min/max macro conflicts
        WIN32_LEAN_AND_MEAN  # Reduce Windows header size
    )
endif()

# CEF Configuration - Use local cef directory
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef")

# Set CEF paths
set(CEF_INCLUDE_DIR "${CEF_ROOT}")
set(CEF_LIBRARY_DEBUG "${CEF_ROOT}/Debug/libcef.lib")
set(CEF_LIBRARY_RELEASE "${CEF_ROOT}/Release/libcef.lib")
set(CEF_DLL_WRAPPER "${CEF_ROOT}/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib")
set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
set(CEF_RESOURCE_DIR "${CEF_ROOT}/Resources")

# Print CEF information
message(STATUS "CEF_ROOT: ${CEF_ROOT}")
message(STATUS "CEF_INCLUDE_DIR: ${CEF_INCLUDE_DIR}")
message(STATUS "CEF_DLL_WRAPPER: ${CEF_DLL_WRAPPER}")

# Check if CEF files exist
if(NOT EXISTS ${CEF_DLL_WRAPPER})
    message(FATAL_ERROR "CEF DLL wrapper not found at: ${CEF_DLL_WRAPPER}")
endif()
if(NOT EXISTS ${CEF_LIBRARY_RELEASE})
    message(FATAL_ERROR "CEF library not found at: ${CEF_LIBRARY_RELEASE}")
endif()

# Spout2 configuration
set(SPOUT2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/Spout2")
set(SPOUT2_INCLUDE_DIR "${SPOUT2_ROOT}/SPOUTSDK/SpoutGL")
set(SPOUT2_DX_INCLUDE_DIR "${SPOUT2_ROOT}/SPOUTSDK/SpoutDirectX/SpoutDX")
set(SPOUT2_SOURCES
    ${SPOUT2_INCLUDE_DIR}/Spout.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutCopy.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutDirectX.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutFrameCount.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutGL.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutGLextensions.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutReceiver.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutSender.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutSenderNames.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutSharedMemory.cpp
    ${SPOUT2_INCLUDE_DIR}/SpoutUtils.cpp
    ${SPOUT2_DX_INCLUDE_DIR}/SpoutDX.cpp
)

# Create Spout2 library
add_library(Spout2 STATIC ${SPOUT2_SOURCES})
target_include_directories(Spout2 PUBLIC ${SPOUT2_INCLUDE_DIR} ${SPOUT2_DX_INCLUDE_DIR})

# Add necessary preprocessor definitions for Spout2
target_compile_definitions(Spout2 PRIVATE
    WIN32_LEAN_AND_MEAN=0  # We need full Windows headers for multimedia
)

target_link_libraries(Spout2 
    PUBLIC 
        d3d11 
        dxgi 
        opengl32
        winmm  # For multimedia timer functions
)

# Rivulet application
set(RIVULET_SOURCES
    src/main.cpp
    src/application.cpp
    src/application.h
    src/d3d11_device.cpp
    src/d3d11_device.h
    src/web_layer.cpp
    src/web_layer.h
    src/spout_sender.cpp
    src/spout_sender.h
)

# Create main executable
add_executable(${PROJECT_NAME} WIN32 ${RIVULET_SOURCES})

# Target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_ENVIRONMENT "PATH=${CEF_ROOT}/Release;${CEF_ROOT}/Resources;$ENV{PATH}"
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${SPOUT2_INCLUDE_DIR}
    ${SPOUT2_DX_INCLUDE_DIR}
    ${CEF_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${CEF_LIBRARY_RELEASE}
    ${CEF_DLL_WRAPPER}
    Spout2
    d3d11
    dxgi
    comctl32.lib
    crypt32.lib 
    delayimp.lib
    gdi32.lib
    rpcrt4.lib
    shlwapi.lib
    wintrust.lib
    ws2_32.lib
)

# Copy CEF binary and resource files
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # Copy main CEF DLLs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/libcef.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/chrome_elf.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/d3dcompiler_47.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/libEGL.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/libGLESv2.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/vk_swiftshader.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/vulkan-1.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/dxcompiler.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/dxil.dll" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/vk_swiftshader_icd.json" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/v8_context_snapshot.bin" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    # Copy CEF resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying CEF binaries and resources"
)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")