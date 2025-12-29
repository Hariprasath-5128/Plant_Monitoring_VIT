"""
Run inference with the fine-tuned ViT plant disease model.

Usage:
    python inference_vit_plant_disease.py <path_to_image>

Expected structure (relative to this file):
- finetuned_vit/ (contains config + weights saved by training script)
- plantdisease/PlantVillage/<class_name>/*.jpg (for class names)
"""

import os
import sys
import time

import torch
from PIL import Image
from transformers import ViTForImageClassification, ViTImageProcessor


sys.stdout.reconfigure(encoding="utf-8")

# -------------------------
# Paths / config
# -------------------------

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

MODEL_DIR = os.path.join(BASE_DIR, "finetuned_vit")
DATASET_DIR = os.path.join(BASE_DIR, "plantdisease", "PlantVillage")

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
TOP_K = 5


# -------------------------
# Load model + processor
# -------------------------

def load_model_and_processor(model_dir, device):
    try:
        processor = ViTImageProcessor.from_pretrained(model_dir)
    except Exception:
        from transformers import ViTFeatureExtractor

        processor = ViTFeatureExtractor.from_pretrained(model_dir)

    model = ViTForImageClassification.from_pretrained(model_dir)
    model.to(device).eval()
    return model, processor


# -------------------------
# Prediction
# -------------------------

def predict(image_path, model, processor, device, id2label, top_k=5):
    image = Image.open(image_path).convert("RGB")
    inputs = processor(images=image, return_tensors="pt").to(device)

    with torch.no_grad():
        outputs = model(**inputs)
        probs = torch.softmax(outputs.logits, dim=-1)[0].cpu().numpy()

    top_indices = probs.argsort()[::-1][:top_k]
    results = []
    for idx in top_indices:
        label = id2label.get(str(idx), f"Class_{idx}")
        score = float(probs[idx])
        results.append({"label": label, "score": score})

    best = results[0]
    print(f"\nPredicted: {best['label']} ({best['score'] * 100:.1f}% confidence)")
    if len(results) > 1:
        print("Other possibilities:")
        for r in results[1:]:
            print(f" - {r['label']} ({r['score'] * 100:.1f}%)")

    return results


# -------------------------
# Main
# -------------------------

def main():
    if len(sys.argv) > 1:
        image_path = sys.argv[1]
    else:
        # fallback for debugging
        image_path = os.path.join(BASE_DIR, "sample", "leaf1.jpg")

    while not os.path.exists(image_path):
        print(f"[waiting] {image_path} not found yet...")
        time.sleep(1)

    model, processor = load_model_and_processor(MODEL_DIR, DEVICE)

    # Build id2label mapping from dataset folder names
    classes = sorted(os.listdir(DATASET_DIR))
    id2label = {str(i): cls_name for i, cls_name in enumerate(classes)}

    predictions = predict(image_path, model, processor, DEVICE, id2label, TOP_K)

    print("\nPredictions:")
    for pred in predictions:
        print(f"{pred['label']}: {pred['score']:.4f}")


if __name__ == "__main__":
    main()
