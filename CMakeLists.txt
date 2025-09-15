cmake_minimum_required(VERSION 3.10)
project(HTTPServer)

# Set C standard
set(CMAKE_C_STANDARD 99)

# Set output directory for the executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Add source files
add_executable(httpserver app/main.c app/properties.c)