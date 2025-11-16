cmake_minimum_required(VERSION 3.10)
project(HTTPServer)

include_directories(src/main/cpp/include)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Set output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Add source files
add_executable(httpserver src/main.c src/settings.c src/socket.c src/logger.c src/http.c src/client.c src/lib/cJSON.c src/safe_snprintf.c)
