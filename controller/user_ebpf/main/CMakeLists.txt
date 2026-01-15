add_executable(lb_app
    src/main.cpp
)

target_include_directories(lb_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/io_epoll/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/maps_manager/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/recovery/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/protocol/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/ebpf_loader

)

target_link_libraries(lb_app 
    io_epoll
    maps_manager
    recovery
    proto_files
    protocol
    session_manager
    handlers
    routing_logic
    ebpf_loader
    bpf
)