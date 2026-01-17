cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(ChatServer)
add_compile_options(-Wall -Wextra -Werror)


add_library(Network SHARED src/network/network.cpp)
add_library(ThreadPool SHARED src/thread_pool/thread_pool.cpp)
add_library(Crypt SHARED src/crypt/crypt.cpp)

target_include_directories(Network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(ThreadPool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(Crypt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(Threads REQUIRED)
target_link_libraries(ThreadPool PRIVATE Threads::Threads)

target_link_libraries(Crypt PRIVATE OpenSSL)

add_executable(ChatServer
  src/core.cpp
  src/server.cpp
)

target_link_libraries(ChatServer PRIVATE Network ThreadPool Crypt)
