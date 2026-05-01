#include <obs-module.h>
#include "tts-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-omniversify-tts", "en-US")

bool obs_module_load(void)
{
    blog(LOG_INFO, "Omniversify TTS Plugin loaded");
    
    // Register the Dock
    RegisterTTSDock();
    
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "Omniversify TTS Plugin unloaded");
}
