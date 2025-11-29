set(PROTO_SRC
    service.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC})

add_library(proto_files STATIC
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(proto_files PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(proto_files PUBLIC
    protobuf::libprotobuf
)