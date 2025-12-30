add_executable(protocol_client src/protocol_client.cpp)
add_executable(protocol_server src/protocol_server.cpp)

target_link_libraries(protocol_client PRIVATE
    protocol
    monitor
    ${CMAKE_DL_LIBS}
)

target_link_libraries(protocol_server PRIVATE
    protocol 
)

target_include_directories(protocol_client PRIVATE 
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/protocol/include
    ${CMAKE_BINARY_DIR}/controller/proto
)

set_target_properties(protocol_client PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
)