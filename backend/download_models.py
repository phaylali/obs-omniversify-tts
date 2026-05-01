import os
import requests

def download_piper_model(model_dir):
    # Correct Hugging Face URLs for Piper voices
    model_name = "en_US-lessac-low"
    base_url = "https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/lessac/low"
    
    onnx_url = f"{base_url}/{model_name}.onnx"
    json_url = f"{base_url}/{model_name}.onnx.json"
    
    os.makedirs(model_dir, exist_ok=True)
    
    onnx_path = os.path.join(model_dir, f"{model_name}.onnx")
    json_path = os.path.join(model_dir, f"{model_name}.onnx.json")
    
    # Force re-download if file is too small (meaning it was a "Not Found" error previously)
    def should_download(path):
        if not os.path.exists(path):
            return True
        return os.path.getsize(path) < 100 # Real models/configs are much larger
    
    if should_download(onnx_path):
        print(f"Downloading {model_name} ONNX model from Hugging Face...")
        r = requests.get(onnx_url, allow_redirects=True)
        if r.status_code == 200:
            with open(onnx_path, "wb") as f:
                f.write(r.content)
        else:
            print(f"Failed to download ONNX: {r.status_code}")
            
    if should_download(json_path):
        print(f"Downloading {model_name} config from Hugging Face...")
        r = requests.get(json_url, allow_redirects=True)
        if r.status_code == 200:
            with open(json_path, "wb") as f:
                f.write(r.content)
        else:
            print(f"Failed to download JSON: {r.status_code}")
            
    return onnx_path

if __name__ == "__main__":
    MODEL_DIR = os.path.join(os.path.dirname(__file__), "models")
    download_piper_model(MODEL_DIR)
