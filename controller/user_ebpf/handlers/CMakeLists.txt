add_library(handlers STATIC
    src/handlers.cpp
)

target_include_directories(handlers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/handlers/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/protocol/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/session_manager/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/maps_manager/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/ebpf_loader
    ${CMAKE_SOURCE_DIR}/model/routing_logic/include

)

target_link_libraries(handlers
    protocol
    proto_files
    session_manager
)