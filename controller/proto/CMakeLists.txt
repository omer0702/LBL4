find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++)
pkg_check_modules(PROTOBUF REQUIRED protobuf)


set(STATS_PROTO "internal_stats.proto")
set(PROTO_SRC
    service.proto
)

get_filename_component(stats_proto_abs "${STATS_PROTO}" ABSOLUTE)
get_filename_component(proto_path "${stats_proto_abs}" PATH)

set(INTERNAL_STATS_SRCS "${CMAKE_CURRENT_BINARY_DIR}/internal_stats.pb.cc")
set(INTERNAL_STATS_HDRS "${CMAKE_CURRENT_BINARY_DIR}/internal_stats.pb.h")
set(INTERNAL_STATS_GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/internal_stats.grpc.pb.cc")
set(INTERNAL_STATS_GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/internal_stats.grpc.pb.h")

add_custom_command(
    OUTPUT ${INTERNAL_STATS_SRCS} ${INTERNAL_STATS_HDRS} ${INTERNAL_STATS_GRPC_SRCS} ${INTERNAL_STATS_GRPC_HDRS}
    COMMAND protobuf::protoc
         ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
         --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
         --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin
         -I ${proto_path}
         ${stats_proto_abs}
    DEPENDS ${stats_proto_abs}
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC})

add_library(proto_files STATIC
    ${INTERNAL_STATS_GRPC_SRCS}
    ${INTERNAL_STATS_GRPC_HDRS}
    ${INTERNAL_STATS_SRCS}
    ${INTERNAL_STATS_HDRS}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(proto_files PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GRPC_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
)

target_link_libraries(proto_files PUBLIC
    ${PROTOBUF_LIBRARIES}
    ${GRPC_LIBRARIES}
)