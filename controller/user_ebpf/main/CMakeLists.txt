add_executable(lb_app
    src/main.cpp
)

target_include_directories(lb_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/io_epoll/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/handle_maps/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/recovery/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/protocol/include

)

target_link_libraries(lb_app 
    io_epoll
    handle_maps
    recovery
    proto_files
    protocol
    session_manager
    handlers
)