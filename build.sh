#!/bin/bash

# Exit on error
set -e

PROJECT_NAME="obs-omniversify-tts"
BUILD_DIR="build"
PLUGIN_DEST="$HOME/.config/obs-studio/plugins/$PROJECT_NAME/bin/64bit/"

echo "🚀 Starting CLEAN build process for $PROJECT_NAME..."

# Shutdown existing TTS server if running
echo "🛑 Shutting down previous TTS server on port 6973..."
fuser -k 6973/tcp 2>/dev/null || true

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
OBS_PID=$!

echo "🎙️ Launching TTS Backend..."
cd ..
cd backend
LD_LIBRARY_PATH=/opt/rocm/lib uv run python main.py &
TTS_PID=$!
cd ..

function cleanup {
    echo ""
    echo "🛑 Shutting down OBS and TTS Backend..."
    kill $OBS_PID 2>/dev/null || true
    kill $TTS_PID 2>/dev/null || true
    echo "✅ Shutdown complete."
    exit 0
}

trap cleanup SIGINT SIGTERM

echo "✨ All systems go! (Press Ctrl+C to stop everything)"
wait
