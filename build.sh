#!/bin/bash
# Dev build script for obs-omniversify-multichat-plugin.
#
# The plugin now starts the TTS backend itself when OBS loads, so this script
# only builds and installs the plugin. Launch OBS normally afterwards
# (e.g. Super+O) — the dock and server come up automatically.

set -e
cd "$(dirname "$0")"

echo "🚀 Building obs-omniversify-multichat-plugin..."
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo "📦 Installing to /usr/lib/obs-plugins/ (needs sudo)..."
sudo cp build/obs-omniversify-multichat-plugin.so /usr/lib/obs-plugins/

echo "✅ Installed. Restart OBS to pick up the changes."
