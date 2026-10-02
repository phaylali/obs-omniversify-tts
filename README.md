# OBS Omniversify Multichat

An OBS Studio plugin providing a **multichat preview dock** (Twitch + Kick) with **local, GPU-accelerated Text-to-Speech**, optimized for AMD GPUs on Arch Linux.

**Package name:** `obs-omniversify-multichat-plugin`

## Overview

The dock shows incoming chat from your channels and speaks messages aloud using a fully local TTS engine — no cloud, no latency, no CPU hogging while you game.

## Features

- **Zero configuration**: install the package, start OBS — the TTS backend starts automatically with it. Type your streamer names, hit *Connect Streams*, done.
- **Real-time multichat preview**: Twitch and Kick side by side in one dock.
- **Local TTS**: Piper voices, offloaded to GPU (ROCm/Vulkan) via ONNX Runtime.
- **First-run self setup**: the default voice model downloads automatically on first launch into `~/.local/share/obs-omniversify-multichat/models`.
- **Configurable server port**: shown in the dock with a live status indicator (`● running` / `○ offline`); changing it restarts the backend instantly.
- **Simulation box**: test voices, volume and emoji handling before going live.
- **Crash-safe**: the backend dies with OBS even if OBS is killed (kernel-level parent-death signal), so no orphaned servers hold the port.

## Installation

### From the AUR

```bash
yay -S obs-omniversify-multichat-plugin
```

Then start OBS and enable the **Omniversify TTS** dock (Docks menu).

### Building from source

```bash
./build.sh          # builds and installs the plugin (sudo)
```

Requirements: `obs-studio`, Qt6, and the Python stack
(`python-fastapi`, `uvicorn`, `python-onnxruntime-rocm`, `piper-tts`, `python-sounddevice`, `python-curl_cffi`, …) — the PKGBUILD in `packaging/aur/` lists the full dependency set.

## How it works

```
OBS starts
  └─ plugin (obs-omniversify-multichat-plugin.so) loads
       ├─ spawns TTS backend: /usr/bin/python3 …/backend/main.py --port 6973
       │    └─ FastAPI on 127.0.0.1:<port> (status, voices, tts, kick-id)
       └─ registers the "Omniversify TTS" dock
              └─ dock talks to the backend over localhost
OBS exits (or crashes)
  └─ backend receives SIGTERM and shuts down
```

## Ports & settings

- Default port: `6973` (change it in the dock; stored in `~/.config/omniversify/obs-omniversify-multichat-plugin.conf`).
- Override the backend script location with `OMNIVERSIFY_BACKEND_DIR`, the model directory with `OMNIVERSIFY_MODEL_DIR`.

## License

MIT License
