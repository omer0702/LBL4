add_library(io_epoll STATIC
    src/io_epoll.cpp
)

target_include_directories(io_epoll PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/io_epoll/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/protocol/include
    ${CMAKE_SOURCE_DIR}/build/debug/controller/proto
)