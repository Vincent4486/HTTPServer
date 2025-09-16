#!/bin/zsh

# Exit immediately if a command fails
set -e

# Run CMake configuration
cmake .

# Build the project
make

# Run the executable
./httpserver