/*
  ESP32-CAM Trash Image Capture Station
  Creates WiFi Access Point for direct image download
  Board: AI Thinker ESP32-CAM
  
  Features:
  1. Creates WiFi AP "Trash-Capture-AP"
  2. Web interface to view stream and capture images
  3. Manual capture button - downloads directly to phone
  4. No storage on ESP32 (saves phone storage space)
  
  Usage:
  1. Upload this code using ESP32-as-Programmer method
  2. Connect phone to WiFi: "Trash-Capture-AP" (password: 12345678)
  3. Open browser, go to: http://192.168.4.1
  4. Capture trash images that download directly to phone
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ===================
// CAMERA PINS - AI Thinker ESP32-CAM
// ===================
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ===================
// WIFI CONFIGURATION
// ===================
const char* ssid = "Trash-Capture-AP";
const char* password = "12345678";

WebServer server(80);

// ===================
// IMAGE CAPTURE SETTINGS
// ===================
bool flashEnabled = false;
unsigned long lastCaptureTime = 0;
const unsigned long captureCooldown = 3000; // 3 seconds between captures

// ===================
// WEB PAGE HTML
// ===================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Trash Image Capture</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin: 20px;
            background: #f5f5f5;
        }
        .container {
            max-width: 800px;
            margin: auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            margin-bottom: 20px;
        }
        h3 {
            color: #444;
        }
        button {
            padding: 12px 24px;
            margin: 10px;
            background: #4CAF50;
            color: white;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            cursor: pointer;
            font-weight: bold;
        }
        button:hover {
            background: #45a049;
        }
        .live-view {
            margin: 20px 0;
            border: 2px solid #ddd;
            border-radius: 5px;
            padding: 10px;
            background: #000;
        }
        #stream {
            width: 240px;
            height: 240px;
            object-fit: cover;
            border: 2px solid #fff;
        }
        .status {
            background: #e8f5e9;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-size: 14px;
        }
        .warning {
            background: #fff3e0;
            color: #ef6c00;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-size: 14px;
        }
        .info-box {
            background: #e3f2fd;
            padding: 15px;
            border-radius: 5px;
            margin: 15px 0;
            text-align: left;
            font-size: 14px;
        }
        #captureStatus {
            margin: 10px;
            padding: 10px;
            border-radius: 5px;
            display: none;
            font-size: 14px;
        }
        .success {
            background: #c8e6c9;
            color: #2e7d32;
        }
        .error {
            background: #ffcdd2;
            color: #c62828;
        }
        .stats {
            background: #f5f5f5;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
            font-size: 13px;
        }
        ul {
            margin: 10px 0;
            padding-left: 20px;
        }
        li {
            margin-bottom: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Trash Image Capture for AI Training</h1>
        
        <div class="status">
            <strong>INSTRUCTIONS:</strong><br>
            1. Connect to WiFi: Trash-Capture-AP<br>
            2. Images download directly to your phone<br>
            3. Images are NOT saved on ESP32-CAM<br>
            4. Use images to train your AI model
        </div>
        
        <div class="stats">
            <strong>Camera Settings:</strong><br>
            Resolution: 240x240 pixels | Format: JPEG | Quality: 10 (High)
        </div>
        
        <div class="live-view">
            <h3>Live Camera View</h3>
            <img id="stream" src="/stream">
            <br><br>
            <button onclick="capturePhoto()">CAPTURE TRASH IMAGE</button>
            <button onclick="toggleFlash()">TOGGLE FLASH</button>
            <button onclick="location.reload()">REFRESH PAGE</button>
            <div id="captureStatus"></div>
        </div>
        
        <div class="info-box">
            <strong>TIPS FOR AI TRAINING:</strong>
            <ul>
                <li>Capture different trash types: plastic, paper, metal, organic</li>
                <li>Vary lighting conditions: daylight, indoor, shadow</li>
                <li>Different angles and distances from trash</li>
                <li>At least 50 images per class for good results</li>
                <li>Label images clearly on your phone after download</li>
                <li>Include empty background images (no trash) for negative training</li>
            </ul>
        </div>
        
        <div class="warning">
            <strong>NOTE:</strong> Images download directly to your phone. 
            Make sure to allow downloads in your browser settings.
            Each image will be named with timestamp: trash_YYYYMMDD_HHMMSS.jpg
        </div>
    </div>
    
    <script>
        // Auto-refresh stream every 1 second
        setInterval(() => {
            document.getElementById('stream').src = '/stream?t=' + new Date().getTime();
        }, 1000);
        
        // Format timestamp for filename
        function formatDateForFilename(date) {
            const year = date.getFullYear();
            const month = String(date.getMonth() + 1).padStart(2, '0');
            const day = String(date.getDate()).padStart(2, '0');
            const hours = String(date.getHours()).padStart(2, '0');
            const minutes = String(date.getMinutes()).padStart(2, '0');
            const seconds = String(date.getSeconds()).padStart(2, '0');
            return `trash_${year}${month}${day}_${hours}${minutes}${seconds}.jpg`;
        }
        
        // Capture photo and download
        function capturePhoto() {
            // Show loading status
            var statusDiv = document.getElementById('captureStatus');
            statusDiv.innerHTML = "Capturing image... Please wait.";
            statusDiv.className = "status";
            statusDiv.style.display = "block";
            
            // Create a timestamp-based filename
            var now = new Date();
            var filename = formatDateForFilename(now);
            
            // Fetch and download the image
            fetch('/capture')
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Capture failed - ' + response.statusText);
                    }
                    return response.blob();
                })
                .then(blob => {
                    // Create download link
                    var url = window.URL.createObjectURL(blob);
                    var a = document.createElement('a');
                    a.style.display = 'none';
                    a.href = url;
                    a.download = filename;
                    document.body.appendChild(a);
                    
                    // Trigger download
                    a.click();
                    
                    // Clean up
                    window.URL.revokeObjectURL(url);
                    document.body.removeChild(a);
                    
                    // Show success message
                    statusDiv.innerHTML = "SUCCESS: Image downloaded as " + filename;
                    statusDiv.className = "success";
                    
                    // Hide message after 5 seconds
                    setTimeout(() => {
                        statusDiv.style.display = "none";
                    }, 5000);
                })
                .catch(error => {
                    statusDiv.innerHTML = "ERROR: " + error.message + ". Try again.";
                    statusDiv.className = "error";
                    console.error('Error:', error);
                });
        }
        
        // Toggle flash
        function toggleFlash() {
            fetch('/flash');
        }
    </script>
</body>
</html>
)rawliteral";

// ===================
// CAMERA INITIALIZATION
// ===================
bool startCamera() {
    // Camera configuration structure
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
    config.xclk_freq_hz = 20000000;    // 20MHz clock frequency
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_240X240;
    config.jpeg_quality = 10;          // Lower value = higher quality
    config.fb_count = 1;               // Number of frame buffers
    
    // Check if PSRAM is available
    if(psramFound()){
        Serial.println("PSRAM found - using PSRAM for frame buffers");
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    } else {
        Serial.println("No PSRAM - using DRAM for frame buffers");
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }
    
    // Initialize the camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }
    
    // Additional camera settings for OV3660 sensor
    sensor_t *s = esp_camera_sensor_get();
    if (s->id.PID == OV3660_PID) {
        Serial.println("OV3660 sensor detected - applying optimized settings");
        s->set_vflip(s, 1);        // Flip image vertically
        s->set_brightness(s, 1);   // Increase brightness
        s->set_saturation(s, 0);   // Reduce saturation
        
        // Additional optimization for trash detection
        s->set_contrast(s, 1);     // Increase contrast
        s->set_whitebal(s, 1);     // Auto white balance
        s->set_exposure_ctrl(s, 1); // Auto exposure
        s->set_gain_ctrl(s, 1);    // Auto gain
    } else {
        Serial.println("Using default camera settings");
    }
    
    s->set_hmirror(s, 1);
    s->set_vflip(s, 0);
    
    Serial.println("Camera initialized successfully");
    Serial.println("Resolution: 240x240 | JPEG Quality: 10 | Clock: 20MHz");
    return true;
}

// ===================
// WEB SERVER HANDLERS
// ===================
void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleStream() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        server.send(500, "text/plain", "Camera error");
        return;
    }
    
    server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void handleCapture() {
    unsigned long now = millis();
    if (now - lastCaptureTime < captureCooldown) {
        server.send(429, "text/plain", "Please wait " + String((captureCooldown - (now - lastCaptureTime)) / 1000) + " seconds");
        return;
    }
    
    // Enable flash for better image
    if (flashEnabled) {
        digitalWrite(4, HIGH);
        delay(100);
    }
    
    // Capture image
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        if (flashEnabled) digitalWrite(4, LOW);
        server.send(500, "text/plain", "Camera capture failed");
        return;
    }
    
    // Disable flash
    if (flashEnabled) {
        delay(50);
        digitalWrite(4, LOW);
    }
    
    // Get current timestamp for filename
    time_t nowTime;
    struct tm timeinfo;
    char timestamp[20];
    
    if (getLocalTime(&timeinfo)) {
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &timeinfo);
    } else {
        sprintf(timestamp, "%lu", millis());
    }
    
    // Send image directly to client for download
    String filename = "trash_" + String(timestamp) + ".jpg";
    server.sendHeader("Content-Type", "image/jpeg");
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
    
    esp_camera_fb_return(fb);
    lastCaptureTime = millis();
    
    Serial.printf("Image captured and sent: %s (%d bytes)\n", filename.c_str(), fb->len);
}

void handleFlash() {
    flashEnabled = !flashEnabled;
    if (flashEnabled) {
        digitalWrite(4, HIGH);
        Serial.println("Flash turned ON");
    } else {
        digitalWrite(4, LOW);
        Serial.println("Flash turned OFF");
    }
    server.send(200, "text/plain", flashEnabled ? "Flash ON" : "Flash OFF");
}

// ===================
// SETUP FUNCTION
// ===================
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=========================================");
    Serial.println("TRASH IMAGE CAPTURE STATION");
    Serial.println("For AI/ML Training Dataset Collection");
    Serial.println("=========================================\n");
    
    // Setup flash LED
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    
    // Initialize camera
    Serial.println("Initializing camera with 240x240 resolution...");
    if (!startCamera()) {
        Serial.println("Camera initialization failed. Please check connections.");
        Serial.println("Restarting in 3 seconds...");
        delay(3000);
        ESP.restart();
    }
    
    // Start WiFi Access Point
    Serial.println("\nStarting WiFi Access Point...");
    WiFi.softAP(ssid, password);
    delay(100);
    
    Serial.println("\n=== SYSTEM READY ===");
    Serial.print("WiFi SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("Connect your phone to this WiFi network");
    Serial.println("Open browser and go to: http://192.168.4.1");
    Serial.println("=========================================\n");
    
    // Setup web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/stream", HTTP_GET, handleStream);
    server.on("/capture", HTTP_GET, handleCapture);
    server.on("/flash", HTTP_GET, handleFlash);
    
    server.begin();
    Serial.println("Web server started on port 80");
}

// ===================
// LOOP FUNCTION
// ===================
void loop() {
    server.handleClient();
    delay(1);
}