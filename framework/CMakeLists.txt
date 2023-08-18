cmake_minimum_required(VERSION 3.11 FATAL_ERROR)
project(unscatter)

# === SANITIZERS =============================================================

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

# === DEPENDENCIES =============================================================

add_subdirectory(external)

FetchContent_MakeAvailable(pcgrand)
FetchContent_MakeAvailable(cli11)
FetchContent_MakeAvailable(plog)
FetchContent_MakeAvailable(pteditor)

# === unscatter =============================================================

include_directories(
    include 
    external
    ${cli11_SOURCE_DIR}/include
    ${plog_SOURCE_DIR}/include
    ${pcgrand_SOURCE_DIR}/include
    ${pteditor_SOURCE_DIR}
)


add_executable(
    unscatter 
    src/main.cpp
    src/addr.cpp
    src/bitwise-framework.cpp
    src/naive-framework.cpp
    src/phys-addr.cpp
    src/virt-addr.cpp
    src/oracles/oracle.cpp
    src/oracles/slice-oracle.cpp  
    src/oracles/slice-timing-oracle.cpp 
    src/oracles/utag-oracle.cpp
    src/oracles/drama-oracle.cpp
    )
    
target_link_libraries(
    unscatter
    CLI11::CLI11
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Os -g -pg -DNO_LIVEPATCH")
