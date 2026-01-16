add_library(maps_manager STATIC
    src/maps_manager.cpp
)

target_include_directories(maps_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/ebpf_loader
    ${CMAKE_SOURCE_DIR}/model/routing_logic/include
    ${CMAKE_SOURCE_DIR}/model/kernel_ebpf/common
)