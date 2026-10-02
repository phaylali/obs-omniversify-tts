# Developer Notes - obs-omniversify-multichat-plugin

## Project Architecture

```
src/
  plugin-main.cpp   OBS module entry: obs_module_load/unload → backend lifecycle + dock registration
  backend.cpp/.hpp  Backend manager: port config, QProcess spawn/kill, TCP liveness probe
  tts-dock.cpp/.hpp The dock UI (chat preview, voices, settings, port spinbox, status label)
backend/
  main.py           FastAPI TTS server: /status /voices /tts /kick/id/{ch} /download_voice/{id}
packaging/aur/
  PKGBUILD          Arch package (builds with -DCMAKE_INSTALL_PREFIX=/usr)
```

### Runtime flow

1. OBS loads `/usr/lib/obs-plugins/obs-omniversify-multichat-plugin.so`.
2. `obs_module_load` calls `StartBackend()`:
   - TCP-probe the configured port first — if something already listens, don't spawn a duplicate.
   - Spawn `/usr/bin/python3 <backend>/main.py --port <port>` via `QProcess`.
   - **`PR_SET_PDEATHSIG(SIGTERM)`** in the child → backend dies even on `kill -9` of OBS.
   - stdout/stderr are piped into the OBS log (`[omniversify-backend]` lines) — keep this, it's the only way to debug backend crashes.
3. Dock polls `/voices` every 2 s (drives voice list + server status indicator).
4. `obs_module_unload` → `StopBackend()` (SIGTERM, 3 s grace, then SIGKILL).

### Backend script resolution order (`BackendScriptPath()`)

1. `$OMNIVERSIFY_BACKEND_DIR/main.py`
2. `BACKEND_INSTALL_DIR` (CMake: `${CMAKE_INSTALL_PREFIX}/share/…/backend`) — what the AUR package uses
3. `BACKEND_DEV_DIR` (CMake: source tree) — what a local dev build uses

## Design Decisions

- **Auto-start with OBS**: user requirement — zero config. Install package, start OBS, backend is up. No systemd unit, no manual `build.sh` run.
- **System Python + pacman deps (the "Arch way")**: no `uv` at runtime. Deps come from `extra` (`python-onnxruntime-rocm`, `uvicorn`, `python-fastapi`, `python-curl_cffi`, …) plus AUR (`piper-tts`, `python-sounddevice`). `piper-tts`'s `python-onnxruntime` dep is satisfied by `python-onnxruntime-rocm` (it `Provides: python-onnxruntime`).
- **`/usr/bin/python3` absolute path**: OBS inherits the shell `PATH`, where `python3` may be a uv shim without system site-packages → `ModuleNotFoundError`. Never spawn bare `python3`.
- **Models in XDG data dir**: `/usr/share` is read-only, so `resolve_model_dir()` falls back to `~/.local/share/obs-omniversify-multichat/models`. Dev checkouts with a writable `backend/models` keep using it.
- **First-run voice download**: if the model dir has no `.onnx`, the default voice (`en_US-lessac-low`, same one `load_voice()` falls back to) downloads in a background thread → fresh install TTS-ready with no interaction.
- **Port as a setting** (`~/.config/omniversify/obs-omniversify-multichat-plugin.conf`), editable in the dock; changing it triggers `RestartBackend()`.
- **Emoji/emote handling**: emotes rendered as images in the preview, stripped (incl. `channelEmote` pattern) before TTS.
- **Packaging name**: `obs-omniversify-multichat-plugin` (was `obs-omniversify-tts`). The dock id `OmniversifyTTS` is unchanged so existing OBS layouts keep working.

## Build & Test

```bash
./build.sh                      # dev build → /usr/lib/obs-plugins (sudo)
cd packaging/aur && makepkg -f  # produce the AUR package (PREFIX=/usr)
sudo pacman -U obs-omniversify-multichat-plugin-*.pkg.tar.zst
```

Quick checks after launching OBS:

```bash
curl -s 127.0.0.1:6973/status                      # {"status":"online",…}
grep omniversify ~/.config/obs-studio/logs/*.log    # plugin + backend lines
kill -9 $(pgrep -x obs)                             # backend must die too (PDEATHSIG)
```

**Gotcha**: when building AUR deps manually (`makepkg`), strip uv from `PATH` — otherwise `python`/`python3` resolve to uv's interpreter and setuptools/pacman modules are invisible:

```bash
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin
```

## Known Challenges

- **ROCm availability**: `/status` reports `rocm_available`; the system `python-onnxruntime-rocm` currently exposes MIGraphX/DNNL/CPU providers — Vulkan/CPU fallback is what the dock's backend selector maps to.
- **Latency**: message → audio should stay well under 1 s; model stays resident in `loaded_voices` cache.
- **Audio routing**: `sd.play` outputs to the default device; OBS capture of TTS depends on the user's audio setup.
