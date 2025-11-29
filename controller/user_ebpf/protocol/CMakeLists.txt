cmake_minimum_required(VERSION 3.12)

project(lb_protocol)

set(CMAKE_CXX_STANDARD 17)

file(GLOB PROTOCOL_HEADERS "include/*.h")
file(GLOB PROTOCOL_SOURCES "src/*.cpp")

add_library(protocol STATIC
    ${PROTOCOL_HEADERS}
    ${PROTOCOL_SOURCES}
)

target_include_directories(protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(protocol PUBLIC
    proto_files
    protobuf::libprotobuf
)