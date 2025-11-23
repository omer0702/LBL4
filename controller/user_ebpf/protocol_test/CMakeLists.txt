add_executable(protocol_client src/protocol_client.cpp)

target_include_directories(protocol_client PUBLIC 
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/protocol/include
)

target_link_libraries(protocol_client 
    protocol 
    proto_files
)