# Developer Notes - obs-omniversify-tts

## Project Architecture

The plugin is designed to be high-performance and modular. To ensure zero CPU hogging during gameplay, the heavy lifting (TTS inference) must be offloaded to the AMD GPU via ROCm.

### Components

1.  **OBS Plugin (Frontend)**: 
    - Written in **C++** using the OBS Studio API.
    - Utilizes **Qt** for the Dock UI.
    - Features: Chat preview window, Backend selection (ROCm/Vulkan), Voice selection dropdown, Simulation Mode toggle.
    - IPC: Communication with the TTS Backend (if external) via WebSockets or Local Sockets.

2.  **TTS Engine (Backend)**:
    - **Option A (ROCm)**: Python-based backend using `onnxruntime-rocm` or `torch-rocm`. Best for high-end AMD hardware.
    - **Option B (Vulkan)**: Python or C++ backend using `onnxruntime-vulkan` or `ncnn`. Offers broader compatibility across AMD and other GPUs.
    - **Models**: Focus on lightweight, fast models like **Piper** or **Silero**.
    - **Performance Goal**: Inference time < 100ms with negligible CPU impact.

### Technical Requirements (Arch Linux)

- **GPU Backends**: 
    - **ROCm**: `rocm-core`, `hip-runtime-amd`, `miopen-hip`.
    - **Vulkan**: `vulkan-icd-loader`, `vulkan-radeon` (for AMD), `vulkan-headers`.
- **OBS Development**: `obs-studio` (dev headers), `qt6-base`.
- **Python (Backend)**: Managed via `uv`.

## Roadmap

### Phase 1: Simulation & Mocking (Current Focus)
- Create the basic OBS Dock UI in C++/Qt.
- Implement "Simulation Mode" UI:
    - Text input area.
    - "Test TTS" button.
- Implement a backend-agnostic TTS handler that can route requests to either ROCm or Vulkan engines.
- Handle Emoji stripping:
    - Regex-based removal/replacement of emoji characters before sending to the TTS engine.
    - Ensure UI still displays them using a bundled font or system fallback.

### Phase 2: Local TTS Engine Integration
- Set up the Python environment using `uv`.
- Integrate a ROCm-accelerated TTS model.
- Establish IPC between the C++ plugin and the Python backend.

### Phase 3: Real Chat Integration
- Connect to streaming platform APIs (Twitch/YouTube) via OBS-provided hooks or direct WebSocket connections.
- Implement message queuing to handle high-traffic chat without overlapping audio.

## Design Decisions

- **Emoji Handling**: Emojis will be rendered in the preview dock but stripped from the text string sent to the TTS engine to avoid "pronouncing" hex codes or breaking the model.
- **AMD Focus**: We will use ROCm/HIP explicitly. Windows/Nvidia support will be abstracted for future implementation.
- **Language Choice**: C++ for the OBS-facing plugin for maximum stability; Python/`uv` for the TTS inference to leverage the rich AI ecosystem on Linux.

## Known Challenges
- **AMD/ROCm Setup**: Ensuring the user has the correct ROCm version compatible with their hardware.
- **Latency**: Minimizing the delay between a chat message appearing and the audio playing.
- **Audio Routing**: Ensuring the TTS audio is captured correctly by OBS or played through a specific monitor device.
