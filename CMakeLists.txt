cmake_minimum_required(VERSION 3.20)

project(ChatServer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror)

# Find dependencies
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PostgreSQL REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)


# Network
add_library(Network SHARED src/network/network.cpp)
target_include_directories(Network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ThreadPool
add_library(ThreadPool SHARED src/thread_pool/thread_pool.cpp)
target_include_directories(ThreadPool PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(ThreadPool PRIVATE Threads::Threads)

# Crypt (OpenSSL)
add_library(Crypt SHARED src/crypt/crypt.cpp)
target_include_directories(Crypt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(Crypt PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# DB (PostgreSQL) - Renamed from myDB
add_library(DB SHARED src/db/db.cpp)
target_include_directories(DB PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PostgreSQL_INCLUDE_DIRS}
)
target_link_libraries(DB PRIVATE ${PostgreSQL_LIBRARIES})


# --- Executable ---

add_executable(ChatServer
    src/core.cpp
    src/server.cpp
)

target_link_libraries(ChatServer PRIVATE
    Network
    ThreadPool
    Crypt
    DB
)
