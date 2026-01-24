cmake_minimum_required(VERSION 3.14)
project(Simple-Torrent-Client)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

find_package(cpr QUIET)

if(cpr_FOUND)
    message(STATUS "Using system CPR")
else()
    message(STATUS "System CPR not found, downloading...")
    
    include(FetchContent)
    
    cmake_policy(SET CMP0135 NEW)
    
    FetchContent_Declare(
        cpr
        GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 1.10.5
        GIT_SHALLOW TRUE
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    
    FetchContent_MakeAvailable(cpr)
endif()

include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

add_subdirectory(src)
