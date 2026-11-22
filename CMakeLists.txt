cmake_minimum_required(VERSION 3.21)

# CRT statico: la DLL non dipende da nessun runtime da installare.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HeicViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libheif CONFIG REQUIRED)

add_library(HeicViewer SHARED
    src/dllmain.cpp
    src/heic_decode.cpp
    src/thumbnail.cpp
    src/wic_decoder.cpp
    src/HeicViewer.def
)

target_compile_definitions(HeicViewer PRIVATE
    UNICODE _UNICODE NOMINMAX
    _WIN32_WINNT=0x0A00 WINVER=0x0A00
    LIBHEIF_STATIC_BUILD
)

if(MSVC)
    target_compile_options(HeicViewer PRIVATE /W3 /utf-8)
endif()

target_link_libraries(HeicViewer PRIVATE
    heif
    windowscodecs ole32 oleaut32 uuid shell32 advapi32 gdi32
)
