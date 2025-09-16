#!/bin/zsh

# Exit immediately if a command fails
set -e

# Run CMake configuration
cmake .

# Build the project
make

echo "\ndebug.sh: Successfully built the project.\nNow running the executable...\n"

# Run the executable
./httpserver