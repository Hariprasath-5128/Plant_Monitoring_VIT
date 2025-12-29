# Plant Health Monitoring with ESP32-CAM and ViT

This project is a plant health monitoring system that uses an ESP32‑CAM to capture leaf images, sends them to a laptop, and classifies plant diseases with a fine‑tuned Vision Transformer (ViT) model. The final health status is displayed via a small Flask web server and, on the hardware side, on an LCD connected to the ESP32.

## Project structure

        Plant_Monitoring_VIT/
        ├── ESP32/
        │ ├── esp32_cam_plant_monitor/ # Arduino sketch + camera config
        │ ├── FLASK.py # Flask server that reads status.txt
        │ └── espcode.py # PC script: fetch images, run model, update status
        ├── input_images/ # Temporary images from ESP32 (ignored in git)
        ├── processed_images/ # Archived images (ignored in git)
        ├── model_training/
        │ ├── finetuned_vit/ # Final ViT model export (config only in git)
        │ ├── plantdisease/ # PlantVillage dataset (not in git)
        │ ├── train_vit_plant_disease.py # Training script
        │ ├── inference_vit_plant_disease.py # Inference script
        │ └── load_dataset.py # Example of loading dataset with datasets
        ├── espcode.py # (if kept at root) convenience runner
        ├── FLASK.py # (if kept at root) convenience Flask entry
        ├── requirements.txt
        └── README.md


## Hardware overview

- ESP32‑CAM module with OV2640 camera  
- LCD (I2C) connected to ESP32 for local status display  
- Laptop running Python and the ViT model  
- Wi‑Fi network connecting ESP32‑CAM and laptop

The ESP32‑CAM captures three leaf images on the `/capture` endpoint, embeds them as base64 JPEGs in an HTML response, and shows system status on the LCD. The Python `espcode.py` script requests this page, decodes the images, saves them, and runs the ViT model to classify plant health. It writes a summary (“Healthy” / “Disease Detected”) to `ESP32/status.txt`, which `FLASK.py` serves on `http://<PC_IP>:5000`. The ESP32 polls that URL and updates the LCD with the final health status.

## Software flow

1. **Image capture (ESP32‑CAM)**  
   - `esp32_cam_plant_monitor.ino` configures the camera pins, Wi‑Fi, flash LED, and LCD.  
   - When `/capture` is requested, it captures three JPEG frames, keeps them in memory, and serves an HTML page containing the images as base64 data URIs.  

2. **Image collection and inference (Laptop)**  
   - `espcode.py` calls the ESP32 `/capture` URL, extracts the base64 images from the HTML, and saves them into `input_images/`.  
   - For each image:
     - loads the fine‑tuned ViT model from `model_training/finetuned_vit/`,
     - preprocesses the image,
     - outputs top‑K predicted disease classes and probabilities.  
   - `espcode.py` aggregates predictions, computes an “infected percentage”, prints a report, moves images to `processed_images/`, and writes `status.txt` with either `Healthy` or `Disease Detected`.

3. **Status display (Flask + ESP32 LCD)**  
   - `ESP32/FLASK.py` runs a minimal Flask app that serves:
     - `/` → plain text status (for ESP32 LCD),
     - `/status_info` → JSON with status and last update time.  
   - The ESP32 periodically calls the Flask endpoint, updates the LCD with the latest status, and clears stored images after showing the result.

## Model training (Vision Transformer)

The ViT model is fine‑tuned on the **PlantVillage** dataset:

- `train_vit_plant_disease.py`  
  - loads images from `model_training/plantdisease/PlantVillage/<class_name>/*.jpg`,  
  - applies data augmentation and normalization,  
  - fine‑tunes `google/vit-base-patch16-224-in21k` for multi‑class disease classification,  
  - saves the trained model and feature extractor into `model_training/finetuned_vit/` using the Hugging Face `transformers` API (`save_pretrained`).  

- `inference_vit_plant_disease.py`  
  - loads the saved model and processor from `finetuned_vit/`,  
  - builds the label mapping from the dataset folder names,  
  - predicts the top‑K most likely disease classes for a given leaf image.

The dataset itself and large weight files (`model.safetensors`, checkpoints) are not stored in Git, only configuration files (`config.json`, `preprocessor_config.json`) needed to reconstruct the model.

## Running the system

1. **Setup environment**
#pip install -r requirements.txt


2. **Prepare dataset (for training)**  
   - Download the PlantVillage dataset and place it under `model_training/plantdisease/PlantVillage/` with one subfolder per class.  
   - Optionally run `load_dataset.py` to verify the structure using the `datasets` library.

3. **Train the ViT model (optional)**
        '''cd model_training
        python train_vit_plant_disease.py'''


This writes the fine‑tuned model into `model_training/finetuned_vit/`.

4. **Flash the ESP32‑CAM**

- Open `ESP32/esp32_cam_plant_monitor/esp32_cam_plant_monitor.ino` in Arduino IDE or PlatformIO.  
- Replace `ssid`, `password`, and `flask_info_url` with your own Wi‑Fi and laptop IP settings.  
- Select the correct ESP32‑CAM board and upload the sketch.

5. **Run Flask and PC controller**

From the project root or `ESP32/`:

start Flask status server
python ESP32/FLASK.py # listens on port 5000 by default

in another terminal, start the controller that fetches images and runs ViT
python ESP32/espcode.py


Open the ESP32 IP (shown in Serial Monitor) in a browser and trigger `/capture`. The script downloads the images, runs inference, and updates both the Flask server and the ESP32 LCD with the health status.

## Hugging Face model upload

To share the fine‑tuned ViT model:

1. Create a model repo on Hugging Face (e.g. `your-username/plant-health-vit`).  
2. From `model_training/finetuned_vit/`, log in with `huggingface-cli login` and run:

from transformers import ViTForImageClassification, ViTImageProcessor

model = ViTForImageClassification.from_pretrained("finetuned_vit")
processor = ViTImageProcessor.from_pretrained("finetuned_vit")

model.push_to_hub("your-username/plant-health-vit")
processor.push_to_hub("your-username/plant-health-vit")

3. Update `inference_vit_plant_disease.py` to load from the hub:

from transformers import ViTForImageClassification, ViTImageProcessor

MODEL_ID = "your-username/plant-health-vit"
model = ViTForImageClassification.from_pretrained(MODEL_ID)
processor = ViTImageProcessor.from_pretrained(MODEL_ID)

Now others only need your GitHub repo and the Hugging Face model name to run the system.
