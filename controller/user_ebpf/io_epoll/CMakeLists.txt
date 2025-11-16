add_library(io_epoll STATIC
    src/io_epoll.cpp
)

target_include_directories(io_epoll PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)