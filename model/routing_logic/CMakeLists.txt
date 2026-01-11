add_library(routing_logic
    src/maglev_builder.cpp
    src/scorer.cpp
)

target_include_directories(routing_logic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/model/routing_logic/include
)
