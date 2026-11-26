cmake_minimum_required(VERSION 3.21)

# CRT statico: nessun runtime da installare.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(HeicViewerThumbs VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libheif CONFIG REQUIRED)
find_package(libde265 CONFIG QUIET)

add_library(HeicViewerThumbs SHARED
    dllmain.cpp
    heic_decode.cpp
    video_decode.cpp
    thumbnail.cpp
    log.cpp
    HeicViewer.def
)

target_compile_definitions(HeicViewerThumbs PRIVATE
    UNICODE _UNICODE NOMINMAX
    _WIN32_WINNT=0x0A00 WINVER=0x0A00
    LIBHEIF_STATIC_BUILD LIBDE265_STATIC_BUILD
)

if(MSVC)
    target_compile_options(HeicViewerThumbs PRIVATE /W3 /utf-8)
endif()

target_link_libraries(HeicViewerThumbs PRIVATE
    heif
    windowscodecs ole32 oleaut32 uuid shell32 advapi32 gdi32
    mfplat mfreadwrite mfuuid
)

foreach(t de265 libde265 libde265::de265 libde265::libde265)
    if(TARGET ${t})
        target_link_libraries(HeicViewerThumbs PRIVATE ${t})
        break()
    endif()
endforeach()
