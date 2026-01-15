add_library(ebpf_loader 
    src/ebpf_loader.cpp)

target_include_directories(ebpf_loader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ebpf_loader PRIVATE
    bpf
    elf
    z
)
