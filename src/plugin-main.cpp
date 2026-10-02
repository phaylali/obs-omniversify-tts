#include <obs-module.h>
#include "tts-dock.hpp"
#include "backend.hpp"

OBS_DECLARE_MODULE()

bool obs_module_load(void)
{
    blog(LOG_INFO, "Omniversify Multichat plugin loaded");

    // Bring the TTS backend up automatically; the dock is useless without it.
    StartBackend();

    // Register the Dock
    RegisterTTSDock();

    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "Omniversify Multichat plugin unloaded");
    StopBackend();
}
