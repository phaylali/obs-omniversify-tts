cmake_minimum_required(VERSION 3.16)

project(obs-omniversify-multichat-plugin VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find OBS Studio
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network WebSockets)

# Plugin Source Files
set(PLUGIN_SOURCES
    src/plugin-main.cpp
    src/tts-dock.cpp
    src/tts-dock.hpp
    src/backend.cpp
    src/backend.hpp
)

# Create the plugin shared library
add_library(${PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Tell the plugin where to find backend/main.py at runtime:
#  - installed location (AUR package) and dev checkout (this source tree)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    BACKEND_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/backend"
    BACKEND_DEV_DIR="${CMAKE_CURRENT_SOURCE_DIR}/backend"
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::WebSockets
)

# Define plugin properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${PROJECT_NAME}"
)

# Installation: OBS loads modules from lib/obs-plugins on Arch
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib/obs-plugins
)

# Backend scripts (python) — models download at first run into XDG data dir
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/backend/
    DESTINATION share/${PROJECT_NAME}/backend
    FILES_MATCHING
        PATTERN "*.py"
        PATTERN "pyproject.toml" EXCLUDE
        PATTERN "uv.lock" EXCLUDE
        PATTERN "models" EXCLUDE
        PATTERN ".venv" EXCLUDE
)
