add_library(session_manager STATIC
    src/session_manager.cpp
)


target_include_directories(session_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/controller/user_ebpf/session_manager/include
)