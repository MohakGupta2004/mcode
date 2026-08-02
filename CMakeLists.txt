cmake_minimum_required(VERSION 3.15)

project(claude-code)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-fcoroutines)
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr REQUIRED)

add_executable(claude_cpp src/main.cpp src/storage/storage.cpp  src/conversation/conversation.cpp src/commander/commander.cpp src/providers/provider_manager.cpp src/providers/openrouter_provider.cpp src/providers/ollama_provider.cpp src/config/config.cpp src/tools/tool_manager.cpp src/tools/read_file.cpp src/tools/create_file.cpp src/tools/create_directory.cpp src/tools/edit_file.cpp src/tools/delete_file.cpp src/tools/delete_directory.cpp src/tools/command_executor.cpp src/utils/tool_initializer.cpp src/utils/provider_initializer.cpp src/tools/search_file.cpp src/tools/websearch.cpp)

target_link_libraries(
    claude_cpp
    PRIVATE
    nlohmann_json::nlohmann_json
)

target_link_libraries(claude_cpp PRIVATE cpr::cpr)

target_include_directories(claude_cpp PRIVATE src)
