cmake_minimum_required(VERSION 3.16)

project(obs-omniversify-tts VERSION 0.1.0)

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
)

# Create the plugin shared library
add_library(${PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
    OUTPUT_NAME "obs-omniversify-tts"
)

# Installation (Arch Linux default paths often vary, but this is a standard starting point)
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib/obs-plugins
)
