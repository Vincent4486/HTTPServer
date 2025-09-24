cmake_minimum_required(VERSION 3.10)
project(HTTPServer)

include_directories(src/main/cpp/include)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Set output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Include Homebrew paths for cJSON
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

# Add source files
add_executable(httpserver src/main/cpp/main.c src/main/cpp/settings.c src/main/cpp/socket.c src/main/cpp/client.c src/main/cpp/logger.c src/main/cpp/http.c)

# Link with cJSON
target_link_libraries(httpserver PRIVATE cjson)