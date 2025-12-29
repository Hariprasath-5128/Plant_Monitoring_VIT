"""
PC-side script to capture images from ESP32-CAM, run plant-health inference,
and expose the final status via a small Flask service.
"""

import base64
import os
import re
import shutil
import subprocess
import time
from datetime import datetime

import requests

# -------------------------
# CONFIGURATION
# -------------------------

# Project directories (relative to this file)
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ESP32_DIR = os.path.join(BASE_DIR, "ESP32")

FLASK_SCRIPT = os.path.join(ESP32_DIR, "FLASK.py")
CHECKING_SCRIPT = os.path.join(BASE_DIR, "checking.py")

# ESP32-CAM HTTP endpoint (adjust IP for your network)
ESP32_URL = "http://192.168.137.213/capture"

# Number of images to capture from ESP32-CAM
NUM_IMAGES = 3

# Flask status server (local)
FLASK_HOST = "127.0.0.1"
FLASK_PORT = 5000

# Image folders
SAVE_DIR = os.path.join(BASE_DIR, "input_images")
PROCESSED_DIR = os.path.join(BASE_DIR, "processed_images")

os.makedirs(SAVE_DIR, exist_ok=True)
os.makedirs(PROCESSED_DIR, exist_ok=True)


# -------------------------
# Utility: Check Flask Status
# -------------------------

def is_flask_running() -> bool:
    """Return True if the local Flask status server is responding."""
    try:
        url = f"http://{FLASK_HOST}:{FLASK_PORT}/status_info"
        response = requests.get(url, timeout=3)
        return response.status_code == 200
    except Exception:
        return False


# -------------------------
# Fetch and save images
# -------------------------

def fetch_and_save_images(num_images: int = NUM_IMAGES):
    """
    Request HTML page from ESP32-CAM, extract base64 JPEGs,
    and save them into SAVE_DIR.

    The ESP32 serves multiple <img> tags with data:image/jpeg;base64,... sources.
    """
    saved_files = []

    for attempt in range(3):  # retry up to 3 times
        try:
            print(f"[→] Fetching from ESP32 (attempt {attempt + 1})...")
            print(f"[i] Requesting: {ESP32_URL}")

            response = requests.get(ESP32_URL, timeout=60)
            response.raise_for_status()

            html = response.text

            # Save HTML response for debugging (optional)
            debug_file = os.path.join(BASE_DIR, "esp32_response.html")
            with open(debug_file, "w", encoding="utf-8") as f:
                f.write(html)
            print(f"[i] Saved response to {debug_file}")

            # Extract base64 images from HTML
            img_matches = re.findall(
                r"data:image/jpeg;base64,([A-Za-z0-9+/=\r\n]+)", html
            )
            print(f"[i] Found {len(img_matches)} images in response")

            if len(img_matches) < num_images:
                print(
                    f"[!] Expected {num_images} images, "
                    f"found {len(img_matches)} — retrying..."
                )
                time.sleep(3)
                continue  # retry

            # Process and save each image
            for i, img_b64 in enumerate(img_matches[:num_images], start=1):
                img_b64_clean = img_b64.replace("\r", "").replace("\n", "")

                # Fix base64 padding if needed
                missing_padding = len(img_b64_clean) % 4
                if missing_padding:
                    img_b64_clean += "=" * (4 - missing_padding)

                try:
                    img_data = base64.b64decode(img_b64_clean)
                    filename = os.path.join(SAVE_DIR, f"leaf{i}.jpg")
                    with open(filename, "wb") as f:
                        f.write(img_data)

                    print(f"[+] Saved {filename} ({len(img_data)} bytes)")
                    saved_files.append(filename)
                except Exception as e:
                    print(f"[!] Error decoding image {i}: {e}")
                    continue

                time.sleep(0.5)

            if len(saved_files) >= num_images:
                print(f"[✓] Successfully saved {len(saved_files)} images")
                return saved_files

        except requests.exceptions.Timeout:
            print(f"[!] Request timeout (attempt {attempt + 1}/3)")
            print("[i] ESP32 may be capturing images - waiting longer...")
            time.sleep(5)

        except requests.exceptions.RequestException as e:
            print(f"[!] Request error: {e}")
            time.sleep(3)

        except Exception as e:
            print(f"[!] Unexpected error: {e}")
            time.sleep(3)

    print("[x] Failed to get valid images after 3 attempts.")
    print("[i] Check esp32_response.html for details")
    return saved_files


# -------------------------
# Run checking.py for one image
# -------------------------

def run_inference_on_image(image_path: str) -> str:
    """Run ML inference on a single image using checking.py."""
    print(f"\n[→] Processing {os.path.basename(image_path)} ...")
    try:
        result = subprocess.run(
            ["python", CHECKING_SCRIPT, image_path],
            capture_output=True,
            text=True,
            timeout=60,
        )
        output = result.stdout.strip()
        if result.stderr.strip():
            print("[!] stderr:", result.stderr.strip())

        print(f"[✓] Result: {output}\n")
        return output if output else "No output"

    except subprocess.TimeoutExpired:
        print(f"[!] Inference timeout for {os.path.basename(image_path)}")
        return "Timeout"

    except Exception as e:
        print(f"[!] Failed to run checking.py: {e}")
        return "Error"


# -------------------------
# Move processed image
# -------------------------

def move_to_processed(image_path: str) -> None:
    """Move processed image to an archive folder with timestamp prefix."""
    try:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = os.path.basename(image_path)
        dest_path = os.path.join(PROCESSED_DIR, f"{timestamp}_{filename}")
        shutil.move(image_path, dest_path)
        print(f"[→] Moved {filename} → processed_images/\n")
    except Exception as e:
        print(f"[!] Failed to move {image_path}: {e}")


# -------------------------
# Analyze Results
# -------------------------

def analyze_results(results):
    """
    Analyze text results from checking.py and compute infection statistics.

    Returns:
        status: "Healthy" or "Infected" (or "Unknown" if no results)
        percentage: infection percentage as float
    """
    total = len(results)
    if total == 0:
        return "Unknown", 0.0

    infected = sum(
        ("infect" in res.lower()) or ("disease" in res.lower())
        for res in results
    )
    percentage = (infected / total) * 100 if total else 0.0
    status = "Infected" if infected > 0 else "Healthy"
    return status, round(percentage, 2)


# -------------------------
# Print Final Summary
# -------------------------

def print_summary(results, status: str, percentage: float) -> None:
    """Pretty-print a table with per-image results and overall diagnosis."""
    print("\n" + "=" * 80)
    print(f"{'FINAL LEAF HEALTH REPORT':^80}")
    print("=" * 80)
    print(f"{'S.No.':<8}{'Image Name':<25}{'Detected Result':<45}")
    print("-" * 80)

    for i, (img, res) in enumerate(results, start=1):
        print(f"{i:<8}{os.path.basename(img):<25}{res:<45}")

    print("-" * 80)
    print(f"{'Overall Leaf Condition:':<33}{status:<15}")
    print(f"{'Infection Percentage:':<33}{percentage}%")
    print("=" * 80 + "\n")


# -------------------------
# MAIN FLOW
# -------------------------

def main() -> None:
    """End-to-end pipeline: capture, infer, archive, summarize, start Flask."""
    print("\n" + "=" * 80)
    print(f"{'ESP32-CAM LEAF HEALTH ANALYSIS SYSTEM':^80}")
    print("=" * 80 + "\n")

    # Step 1: Fetch images from ESP32-CAM
    print("[STEP 1] Fetching images from ESP32-CAM...")
    image_files = fetch_and_save_images(num_images=NUM_IMAGES)

    if not image_files:
        print("\n[x] No images captured. Exiting...")
        print("[i] Troubleshooting:")
        print("    1. Check ESP32 IP address is correct")
        print("    2. Verify ESP32 is powered and connected to WiFi")
        print("    3. Review esp32_response.html for error details")
        return

    # Step 2: Run inference on each image
    print(f"\n[STEP 2] Running ML inference on {len(image_files)} images...")
    results = []
    for img in image_files:
        result = run_inference_on_image(img)
        results.append((img, result))
        time.sleep(1)

    # Step 3: Move images to processed folder
    print("\n[STEP 3] Archiving processed images...")
    for img, _ in results:
        move_to_processed(img)
        time.sleep(0.5)

    # Step 4: Analyze and display results
    print("\n[STEP 4] Analyzing results...")
    raw_results = [r[1] for r in results]
    status, percentage = analyze_results(raw_results)
    print_summary(results, status, percentage)

    # Step 5: Write status file for Flask
    status_file = os.path.join(ESP32_DIR, "status.txt")
    flask_status = "Healthy" if status == "Healthy" else "Disease Detected"

    try:
        with open(status_file, "w", encoding="utf-8") as f:
            f.write(flask_status)
        print(f"[✓] Status file updated: {flask_status}")
    except Exception as e:
        print(f"[!] Failed to write status file: {e}")

    # Step 6: Start Flask server (if not running)
    if not is_flask_running():
        print("\n[STEP 5] Launching Flask server to display status...")
        try:
            subprocess.Popen(
                ["python", FLASK_SCRIPT],
                creationflags=(
                    subprocess.CREATE_NEW_CONSOLE if os.name == "nt" else 0
                ),
            )
            print("[✓] Flask server started")
        except Exception as e:
            print(f"[!] Failed to start Flask: {e}")
    else:
        print("\n[i] Flask server already running")

    print("\n" + "=" * 80)
    print(f"{'PROCESS COMPLETE — FLASK SERVER ACTIVE':^80}")
    print("=" * 80 + "\n")


if __name__ == "__main__":
    main()
