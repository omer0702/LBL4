add_library(stats_worker STATIC 
            src/stats_worker.cpp)

target_include_directories(stats_worker PUBLIC
    #${CMAKE_SOURCE_DIR}/controller/user_ebpf/maps_manager/include
    #${CMAKE_SOURCE_DIR}/controller/user_ebpf/session_manager/include

    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../../include
    ${CMAKE_SOURCE_DIR}/../../../session
)

target_link_libraries(stats_worker PUBLIC maps_manager session_manager)