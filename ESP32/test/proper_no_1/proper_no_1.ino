#include "esp_camera.h"
#include <WiFi.h>
#include "board_config.h"

// ===========================
// Wi-Fi credentials
// ===========================
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";


// We will store 3 images
#define IMAGE_COUNT 3
String imageBase64[IMAGE_COUNT];  

WiFiServer server(80);

void captureImages() {
  for (int i = 0; i < IMAGE_COUNT; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Convert to base64
    String image = "data:image/jpeg;base64,";
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    for (size_t j = 0; j < fbLen; j += 3) {
      int val = (fbBuf[j] << 16) + ((j + 1 < fbLen) ? (fbBuf[j + 1] << 8) : 0) + ((j + 2 < fbLen) ? fbBuf[j + 2] : 0);
      image += cb64[(val >> 18) & 0x3F];
      image += cb64[(val >> 12) & 0x3F];
      image += (j + 1 < fbLen) ? cb64[(val >> 6) & 0x3F] : '=';
      image += (j + 2 < fbLen) ? cb64[val & 0x3F] : '=';
    }

    imageBase64[i] = image;
    esp_camera_fb_return(fb);

    Serial.printf("Captured image %d\n", i + 1);

    if (i < IMAGE_COUNT - 1) {
      delay(4000); // wait 4 sec between captures
    }
  }
}

void handleClient(WiFiClient client) {
  String header = client.readStringUntil('\r');
  client.flush();

  String html = "<!DOCTYPE html><html><head><title>ESP32-CAM 3 Images</title></head><body>";
  html += "<h2>ESP32-CAM Captured Images (Auto-updating)</h2>";

  for (int i = 0; i < IMAGE_COUNT; i++) {
    html += "<p>Image " + String(i + 1) + ":</p>";
    html += "<img src='" + imageBase64[i] + "' width='320'><br>";
  }

  html += "</body></html>";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  client.println(html);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  server.begin();

  Serial.print("Camera Ready! Open http://");
  Serial.print(WiFi.localIP());
  Serial.println(" in your browser");
}

void loop() {
  // Always recapture images every 12 seconds
  captureImages();

  // Serve to any client that connects
  WiFiClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        handleClient(client);
        break;
      }
    }
    client.stop();
  }

  delay(12000);
}
