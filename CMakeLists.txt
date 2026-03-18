


cmake_minimum_required(VERSION 3.16)

project(llm_serving_cache)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(llm_serving_cache
    src/main.cpp
    src/metadata_store.cpp
    src/node_registry.cpp
    src/router.cpp
    src/placement_policy.cpp
    src/coordinator.cpp
)


add_executable(llm_serving_cache_persistent
    src/main_persistent.cpp
    src/kv_metadata_store.cpp
)

target_link_libraries(llm_serving_cache_persistent PRIVATE kv_store)

add_subdirectory(third_party/veristore)

# Link the storage engine library
target_link_libraries(llm_serving_cache PRIVATE kv_store)