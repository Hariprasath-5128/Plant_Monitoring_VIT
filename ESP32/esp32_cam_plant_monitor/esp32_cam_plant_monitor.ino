#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "board_config.h"

const char *ssid = "Hariprasath LAPTOP";
const char *password = "123456789";
const char *flask_info_url = "http://192.168.35.33:5000";

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiServer server(80);
#define FLASH_GPIO 4

// Global storage for captured images
struct CapturedImage {
  uint8_t *buffer;
  size_t length;
  bool valid;
};

CapturedImage capturedImages[3];
bool imagesReady = false;

// =============================
// Update LCD with status
// =============================
void updateLCD(const char *line1, const char *line2 = "") {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  if (strlen(line2) > 0) {
    lcd.setCursor(0, 1);
    lcd.print(line2);
  }
  Serial.print("[LCD] ");
  Serial.print(line1);
  if (strlen(line2) > 0) {
    Serial.print(" | ");
    Serial.println(line2);
  } else {
    Serial.println();
  }
}

// =============================
// Free stored images
// =============================
void freeStoredImages() {
  for (int i = 0; i < 3; i++) {
    if (capturedImages[i].valid && capturedImages[i].buffer != NULL) {
      free(capturedImages[i].buffer);
      capturedImages[i].buffer = NULL;
      capturedImages[i].valid = false;
      capturedImages[i].length = 0;
    }
  }
  imagesReady = false;
}

// =============================
// Capture 3 images and store in memory
// =============================
// =============================
// Capture 3 images and store in memory
// =============================
bool captureThreeImages() {
  updateLCD("Starting", "Capture...");
  delay(500);
  
  // Free any previously stored images
  freeStoredImages();
  
  // Turn flash ON at the start
  pinMode(FLASH_GPIO, OUTPUT);
  digitalWrite(FLASH_GPIO, HIGH);
  delay(300);  // Give flash time to reach full brightness
  
  bool allCaptured = true;
  
  for (int i = 0; i < 3; i++) {
    char msg1[17], msg2[17];
    sprintf(msg1, "Capturing %d/3", i + 1);
    updateLCD(msg1, "Please wait...");
    
    // Capture frame (flash stays on)
    camera_fb_t *fb = esp_camera_fb_get();
    
    if (!fb) {
      sprintf(msg2, "Image %d FAILED!", i + 1);
      updateLCD("Capture Error", msg2);
      Serial.printf("[ERROR] Failed to capture image %d\n", i + 1);
      allCaptured = false;
      delay(1000);
      continue;
    }
    
    // Allocate memory and copy image data
    capturedImages[i].buffer = (uint8_t *)malloc(fb->len);
    if (capturedImages[i].buffer == NULL) {
      Serial.printf("[ERROR] Memory allocation failed for image %d\n", i + 1);
      esp_camera_fb_return(fb);
      allCaptured = false;
      continue;
    }
    
    memcpy(capturedImages[i].buffer, fb->buf, fb->len);
    capturedImages[i].length = fb->len;
    capturedImages[i].valid = true;
    
    esp_camera_fb_return(fb);
    
    sprintf(msg2, "Image %d OK!", i + 1);
    updateLCD(msg1, msg2);
    Serial.printf("[SUCCESS] Captured image %d (%d bytes)\n", i + 1, capturedImages[i].length);
    
    delay(800);  // Delay between captures
  }
  
  // Turn flash OFF after all captures complete
  digitalWrite(FLASH_GPIO, LOW);
  
  if (allCaptured) {
    imagesReady = true;
    updateLCD("Capture Done!", "3 images ready");
    Serial.println("[SUCCESS] All 3 images captured successfully");
    delay(1500);
    return true;
  } else {
    updateLCD("Capture Failed", "Check serial");
    delay(2000);
    return false;
  }
}


// =============================
// Send HTML page with 3 images
// =============================
void sendImagesPage(WiFiClient &client) {
  if (!imagesReady) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain\r\n");
    client.println("Images not captured yet. Use /capture endpoint first.");
    return;
  }
  
  updateLCD("Sending images", "to Python...");
  
  // Send HTTP headers
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close\r\n");
  
  // Start HTML
  client.println("<!DOCTYPE html>");
  client.println("<html><head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<title>ESP32-CAM Leaf Images</title>");
  client.println("<style>");
  client.println("body { font-family: Arial; margin: 20px; background: #f0f0f0; }");
  client.println("h1 { color: #333; }");
  client.println(".container { max-width: 1200px; margin: 0 auto; }");
  client.println(".image-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }");
  client.println(".image-card { background: white; padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }");
  client.println("img { width: 100%; height: auto; border-radius: 4px; }");
  client.println(".timestamp { color: #666; font-size: 12px; margin-top: 10px; }");
  client.println("</style>");
  client.println("</head><body>");
  client.println("<div class='container'>");
  client.println("<h1>ESP32-CAM Leaf Health Analysis</h1>");
  client.println("<p>Captured 3 images for analysis</p>");
  client.println("<div class='image-grid'>");
  
  // Send each image as base64 embedded in HTML
  for (int i = 0; i < 3; i++) {
    if (capturedImages[i].valid) {
      client.println("<div class='image-card'>");
      client.printf("<h3>Leaf Image %d</h3>\n", i + 1);
      client.print("<img src='data:image/jpeg;base64,");
      
      // Send image data in base64
      sendBase64(client, capturedImages[i].buffer, capturedImages[i].length);
      
      client.println("' alt='Leaf Image'>");
      client.printf("<div class='timestamp'>Size: %d bytes</div>\n", capturedImages[i].length);
      client.println("</div>");
      
      Serial.printf("[SENT] Image %d (%d bytes)\n", i + 1, capturedImages[i].length);
    }
  }
  
  client.println("</div></div></body></html>");
  
  updateLCD("Images sent!", "Processing...");
  delay(1500);
  
  Serial.println("[COMPLETE] All images sent to client");
}

// =============================
// Send data as Base64
// =============================
void sendBase64(WiFiClient &client, uint8_t *data, size_t length) {
  static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  
  size_t i = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];
  
  while (length--) {
    char_array_3[i++] = *(data++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;
      
      for (i = 0; i < 4; i++)
        client.write(base64_chars[char_array_4[i]]);
      i = 0;
    }
  }
  
  if (i) {
    for (int j = i; j < 3; j++)
      char_array_3[j] = '\0';
    
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    
    for (int j = 0; j < i + 1; j++)
      client.write(base64_chars[char_array_4[j]]);
    
    while (i++ < 3)
      client.write('=');
  }
}

// =============================
// Handle client requests
// =============================
void handleClient(WiFiClient &client) {
  String request = client.readStringUntil('\r');
  client.flush();
  
  Serial.println("[REQUEST] " + request);
  
  if (request.indexOf("GET /capture") >= 0) {
    // Capture 3 images
    updateLCD("Request received", "Starting...");
    bool success = captureThreeImages();
    
    if (success) {
      sendImagesPage(client);
      updateLCD("Ready", "Awaiting results");
    } else {
      client.println("HTTP/1.1 500 Internal Server Error");
      client.println("Content-Type: text/plain\r\n");
      client.println("Failed to capture all images");
      updateLCD("Capture Failed", "Try again");
    }
    
  } else if (request.indexOf("GET /status") >= 0) {
    // Status endpoint
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain\r\n");
    client.printf("Images ready: %s\n", imagesReady ? "Yes" : "No");
    
  } else {
    // Default page
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html\r\n");
    client.println("<!DOCTYPE html><html><body>");
    client.println("<h2>ESP32-CAM Leaf Health Monitor</h2>");
    client.println("<h3>Available Endpoints:</h3>");
    client.println("<ul>");
    client.println("<li><a href='/capture'>/capture</a> - Capture 3 images and display</li>");
    client.println("<li><a href='/status'>/status</a> - Check capture status</li>");
    client.println("</ul>");
    client.printf("<p>Current status: %s</p>", imagesReady ? "Images ready" : "No images captured");
    client.println("</body></html>");
  }
  
  client.stop();
}

// =============================
// Fetch Flask status and show on LCD
// =============================
String lastStatus = "";

void checkFlaskForUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  http.begin(flask_info_url);
  http.setTimeout(3000);
  int code = http.GET();
  
  if (code == 200) {
    String payload = http.getString();
    payload.trim();
    if (payload.length() > 0 && payload != lastStatus) {
      lastStatus = payload;
      updateLCD("Health Status:", payload.c_str());
      
      // Free images after result is received
      if (imagesReady) {
        delay(5000);  // Show result for 5 seconds
        freeStoredImages();
        updateLCD("Ready", "for next scan");
      }
    }
  } else if (code > 0) {
    Serial.printf("[FLASK] HTTP error: %d\n", code);
  }
  http.end();
}

// =============================
// Setup
// =============================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Initialize LCD
  Wire.begin(14, 15);
  lcd.begin();
  lcd.backlight();
  updateLCD("ESP32-CAM", "Booting...");
  delay(1000);
  
  // Initialize image storage
  for (int i = 0; i < 3; i++) {
    capturedImages[i].buffer = NULL;
    capturedImages[i].length = 0;
    capturedImages[i].valid = false;
  }
  
  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;  // 320x240
  config.pixel_format = PIXFORMAT_JPEG;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;  // Lower = better quality (10-63)
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_LATEST;  // Always get latest frame
  
  updateLCD("Initializing", "Camera...");
  
  if (esp_camera_init(&config) != ESP_OK) {
    updateLCD("Camera FAILED!", "Check wiring");
    Serial.println("[ERROR] Camera init failed!");
    while (true) delay(1000);
  }
  
  Serial.println("[SUCCESS] Camera initialized");
  
  // Configure sensor settings
  sensor_t *s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled
  
  updateLCD("Connecting WiFi", ssid);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    updateLCD("WiFi FAILED!", "Check settings");
    Serial.println("\n[ERROR] WiFi connection failed");
    while (true) delay(1000);
  }
  
  Serial.println("\n[SUCCESS] WiFi connected!");
  Serial.print("[IP] ");
  Serial.println(WiFi.localIP());
  
  updateLCD("WiFi Connected!", WiFi.localIP().toString().c_str());
  delay(2000);
  
  server.begin();
  updateLCD("System Ready", "Waiting...");
  Serial.println("[READY] Server started. Waiting for requests...");
}

// =============================
// Loop
// =============================
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 3000;

void loop() {
  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }
  
  // Periodically check Flask for status updates
  if (millis() - lastCheck > CHECK_INTERVAL) {
    checkFlaskForUpdate();
    lastCheck = millis();
  }
}
