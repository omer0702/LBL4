add_library(monitor SHARED src/monitor.cpp)

target_include_directories(monitor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(monitor PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "monitor"
)

find_package(Threads REQUIRED)
target_link_libraries(monitor PRIVATE
    Threads::Threads
)