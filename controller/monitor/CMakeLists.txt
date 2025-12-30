add_library(monitor SHARED src/monitor.cpp)

target_include_directories(monitor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(monitor PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "monitor"
)

