#pragma once

#include <QString>

// Shared TTS-backend management between plugin-main.cpp (lifecycle) and
// tts-dock.cpp (URL building / settings UI).
//
// Settings live in ~/.config/omniversify/obs-omniversify-multichat-plugin.conf

int BackendPort();
void SetBackendPort(int port);
QString BackendBaseUrl();

// True if something is already listening on the configured port.
bool BackendIsRunning(int port = -1);

// Spawn the backend python process (no-op if one is already listening).
// The child gets PR_SET_PDEATHSIG so it dies even if OBS crashes.
void StartBackend();

// Ask the backend to exit (SIGTERM, then SIGKILL as last resort).
void StopBackend();

// Stop + start again — used after the port setting changes.
void RestartBackend();
