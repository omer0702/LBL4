add_library(io_epoll STATIC
    src/io_epoll.cpp
)

target_include_directories(io_epoll PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/session_manager/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/io_epoll/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/protocol/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/handlers/include
    ${CMAKE_SOURCE_DIR}/build/debug/controller/proto
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/maps_manager/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/ebpf_loader
    ${CMAKE_SOURCE_DIR}/model/routing_logic/include

)