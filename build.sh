#!/bin/bash

# Exit on error
set -e

PROJECT_NAME="obs-omniversify-tts"
BUILD_DIR="build"
PLUGIN_DEST="$HOME/.config/obs-studio/plugins/$PROJECT_NAME/bin/64bit/"

echo "🚀 Starting CLEAN build process for $PROJECT_NAME..."

# Remove old build directory for a fresh start
if [ -d "$BUILD_DIR" ]; then
    echo "🧹 Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo "🛠️ Configuring with CMake..."
cmake ..

# Build the project
echo "📦 Building project..."
make -j$(nproc)

# Create destination directory if it doesn't exist
echo "📂 Preparing installation directory..."
mkdir -p "$PLUGIN_DEST"

# Install the plugin
echo "🚚 Installing plugin to OBS..."
cp "$PROJECT_NAME.so" "$PLUGIN_DEST"

echo "✅ Build and Installation complete!"
echo "🚀 Launching OBS Studio..."
obs &
