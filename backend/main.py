import os
import re
import asyncio
import json
import numpy as np
import onnxruntime as ort
import requests
from fastapi import FastAPI, HTTPException, BackgroundTasks
from pydantic import BaseModel
import sounddevice as sd
from piper import PiperVoice

app = FastAPI(title="Omniversify TTS Backend")

# Configuration
BASE_DIR = os.path.dirname(__file__)
MODEL_DIR = os.path.join(BASE_DIR, "models")
os.makedirs(MODEL_DIR, exist_ok=True)

# Cache for loaded voices
loaded_voices = {}
# Progress tracker for downloads {voice_id: progress_percentage}
download_progress = {}

class TTSRequest(BaseModel):
    text: str
    engine: str = "Piper"
    backend: str = "ROCm"
    voice: str = "default"

# Constants
PIPER_VOICES_URL = "https://huggingface.co/rhasspy/piper-voices/resolve/main/voices.json"
VOICES_FILE = os.path.join(MODEL_DIR, "voices.json")

# Mapping display names to their Hugging Face relative paths
# This will be populated from voices.json
VOICE_DATA = {}

def update_voices_json():
    global VOICE_DATA
    try:
        print("Fetching latest Piper voices list...")
        r = requests.get(PIPER_VOICES_URL)
        if r.status_code == 200:
            with open(VOICES_FILE, "w") as f:
                f.write(r.text)
            VOICE_DATA = r.json()
            print(f"Loaded {len(VOICE_DATA)} voices.")
        else:
            print(f"Failed to fetch voices.json: {r.status_code}")
    except Exception as e:
        print(f"Error fetching voices: {e}")
        if os.path.exists(VOICES_FILE):
            with open(VOICES_FILE, "r") as f:
                VOICE_DATA = json.load(f)

# Initial load
if os.path.exists(VOICES_FILE):
    with open(VOICES_FILE, "r") as f:
        VOICE_DATA = json.load(f)
else:
    update_voices_json()

def strip_twitch_emotes(text):
    # Strip emojis (Unicode range for emojis)
    # This covers most common emojis while preserving international text
    emoji_pattern = re.compile("["
        u"\U0001f600-\U0001f64f"  # emoticons
        u"\U0001f300-\U0001f5ff"  # symbols & pictographs
        u"\U0001f680-\U0001f6ff"  # transport & map symbols
        u"\U0001f1e0-\U0001f1ff"  # flags (iOS)
        u"\U00002702-\U000027b0"
        u"\U000024c2-\U0001f251"
        "]+", flags=re.UNICODE)
    text = emoji_pattern.sub(r'', text)
    
    # Strip common Twitch Emotes
    emotes = ["LUL", "PogChamp", "Kappa", "ResidentSleeper", "BibleThump", "Pog", "OMEGALUL", "AYAYA", "Kreygasm"]
    for emote in emotes:
        text = re.sub(r'\b' + emote + r'\b', '', text)
    
    # Clean up multiple spaces
    text = re.sub(r'\s+', ' ', text).strip()
    return text

@app.get("/status")
def get_status():
    providers = ort.get_available_providers()
    return {
        "status": "online",
        "available_providers": providers,
        "rocm_available": "ROCMExecutionProvider" in providers,
        "vulkan_available": "VulkanExecutionProvider" in providers
    }

@app.get("/voices")
def list_voices():
    voices = []
    # Sort voices: downloaded first, then by language and name
    for voice_id, info in VOICE_DATA.items():
        onnx_filename = f"{voice_id}.onnx"
        onnx_path = os.path.join(MODEL_DIR, onnx_filename)
        
        lang_name = info.get("language", {}).get("name_english", "Unknown")
        quality = info.get("quality", "medium")
        display_name = f"[{lang_name}] {info.get('name', voice_id)} ({quality})"
        
        voices.append({
            "id": voice_id,
            "display_name": display_name,
            "downloaded": os.path.exists(onnx_path),
            "progress": download_progress.get(voice_id, 0)
        })
    
    # Sort to bring English voices to top for convenience
    voices.sort(key=lambda x: (not x["downloaded"], "English" not in x["display_name"], x["display_name"]))
    return voices

@app.post("/download_voice/{voice_id}")
async def download_voice(voice_id: str, background_tasks: BackgroundTasks):
    if voice_id not in VOICE_DATA:
        raise HTTPException(status_code=404, detail="Voice not found in Piper database")
    
    # Find the ONNX file path in the files map
    files = VOICE_DATA[voice_id].get("files", {})
    onnx_rel_path = next((path for path in files if path.endswith(".onnx")), None)
    
    if not onnx_rel_path:
        raise HTTPException(status_code=404, detail="ONNX file path not found for this voice")
    
    background_tasks.add_task(perform_download, voice_id, onnx_rel_path)
    return {"message": f"Download started for {voice_id}"}

def perform_download(voice_id: str, rel_path: str):
    base_url = "https://huggingface.co/rhasspy/piper-voices/resolve/main"
    url = f"{base_url}/{rel_path}"
    
    path = os.path.join(MODEL_DIR, f"{voice_id}.onnx")
    json_url = url + ".json"
    json_path = path + ".json"

    try:
        download_file(url, path, voice_id)
        download_file(json_url, json_path, None)
        download_progress[voice_id] = 100
    except Exception as e:
        print(f"Download failed for {voice_id}: {e}")
        download_progress[voice_id] = -1

def download_file(url, path, voice_id):
    r = requests.get(url, stream=True)
    if r.status_code != 200:
        raise Exception(f"Failed to download: {r.status_code}")
        
    total_size = int(r.headers.get('content-length', 0))
    downloaded = 0
    with open(path, 'wb') as f:
        for chunk in r.iter_content(chunk_size=8192):
            if chunk:
                f.write(chunk)
                downloaded += len(chunk)
                if voice_id and total_size > 0:
                    download_progress[voice_id] = int((downloaded / total_size) * 100)

def load_voice(engine: str, voice_id: str, backend: str):
    # voice_id is the base filename (e.g., en_US-amy-low)
    model_path = os.path.join(MODEL_DIR, f"{voice_id}.onnx")
    config_path = model_path + ".json"
    
    if not os.path.exists(model_path):
        # If the requested voice_id is "default" or not found, fallback to Lessac
        model_path = os.path.join(MODEL_DIR, "en_US-lessac-low.onnx")
        config_path = model_path + ".json"

    cache_key = f"{engine}_{voice_id}_{backend}"
    if cache_key not in loaded_voices:
        print(f"Loading {engine} model: {voice_id} on {backend}...")
        voice = PiperVoice.load(model_path, config_path)
        loaded_voices[cache_key] = voice
    
    return loaded_voices[cache_key]

@app.post("/tts")
async def generate_tts(request: TTSRequest):
    try:
        clean_text = strip_twitch_emotes(request.text)
        if not clean_text.strip():
            return {"status": "skipped"}

        voice = load_voice(request.engine, request.voice, request.backend)

        def synthesize():
            all_audio = []
            for chunk in voice.synthesize(clean_text):
                if hasattr(chunk, 'audio_int16_array'):
                    all_audio.append(chunk.audio_int16_array)
                elif hasattr(chunk, 'audio_int16_bytes'):
                    all_audio.append(np.frombuffer(chunk.audio_int16_bytes, dtype=np.int16))
                else:
                    try:
                        all_audio.append(np.frombuffer(chunk, dtype=np.int16))
                    except:
                        pass
            return np.concatenate(all_audio) if all_audio else None

        audio_np = await asyncio.to_thread(synthesize)
        if audio_np is not None:
            sd.play(audio_np.astype(np.float32) / 32768.0, voice.config.sample_rate)
        
        return {"status": "success"}
    except Exception as e:
        print(f"Error: {e}")
        raise HTTPException(status_code=500, detail=str(e))

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=6973)
