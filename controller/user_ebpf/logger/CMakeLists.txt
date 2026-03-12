add_library(logger STATIC src/logger.cpp)

target_include_directories(logger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/controller/proto
)

target_link_libraries(logger PUBLIC proto_files)