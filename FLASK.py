from flask import Flask, jsonify
import os, time

app = Flask(__name__)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
STATUS_FILE = os.path.join(BASE_DIR, "status.txt")

def read_status():
    try:
        with open(STATUS_FILE, 'r', encoding='utf-8') as f:
            return f.read().strip()
    except FileNotFoundError:
        return "Status file not found"

def clear_status():
    # Clear the file after reading
    try:
        with open(STATUS_FILE, 'w', encoding='utf-8') as f:
            f.write("")
    except Exception as e:
        print(f"[DEBUG] Failed to clear status file: {e}")

@app.route('/')
def home():
    status = read_status()
    # ❌ Don't clear right away
    return f"{status}"

@app.route('/status_info')
def status_info():
    try:
        if not os.path.exists(STATUS_FILE):
            return jsonify({"status": "File not found", "updated": 0})

        with open(STATUS_FILE, 'r', encoding='utf-8') as f:
            status = f.read().strip()
        mtime = os.path.getmtime(STATUS_FILE)

        # ❌ Don't clear right away
        print(f"[DEBUG] File read: {STATUS_FILE} | Status: {status} | Modified: {mtime}")

    except FileNotFoundError:
        print("[DEBUG] File not found!")
        return jsonify({"status": "File not found", "updated": 0})
    
    return jsonify({
        "status": status,
        "updated": mtime
    })

if __name__ == '__main__':
    print("Flask server running!")
    print("STATUS_FILE is:", STATUS_FILE)
    print("Exists:", os.path.exists(STATUS_FILE))
    app.run(host='0.0.0.0', port=5000)
