#include <SegriGreen-project-1_inferencing.h>  // Edge Impulse library for machine learning model
#include <esp_now.h>
#include <WiFi.h>

// Define camera model for configuration
#define CAMERA_MODEL_AI_THINKER

// Required camera libraries/home/rhandon/Arduino/libraries/SegriGreen-project-1_inferencing
#include "img_converters.h"
#include "image_util.h"
#include "esp_camera.h"
#include "camera_pins.h"

// REPLACE WITH YOUR RECEIVER ESP32 MAC Address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Structure to send classification data
typedef struct struct_message {
  char trash_type[32];    // Classification label
  float confidence;       // Confidence score
  int timestamp;          // Optional: timestamp or sequence number
  bool classification_success; // Whether classification was successful
} struct_message;

// Create a struct_message for data transmission
struct_message classificationData;

esp_now_peer_info_t peerInfo;

// Global variables for image processing
dl_matrix3du_t *resized_matrix = NULL;
ei_impulse_result_t result = {0};

// Hardware Pin Definitions
#define IR_SENSOR_PIN 4           // IR sensor input pin

// ============== OPTIMIZED IR Sensor Logic ==============
// Variables for IR sensor state tracking
bool sensorCurrentlyTriggered = false;
unsigned long triggerStartTime = 0;
unsigned long triggerEndTime = 0;
bool trashDetected = false;

// Timing constants - REDUCED for faster response
const unsigned long MIN_TRIGGER_TIME = 50;     // Reduced from 100ms
const unsigned long STABILIZATION_DELAY = 200;  // Reduced from 500ms
const unsigned long DEBOUNCE_DELAY = 1000;      // Reduced from 2000ms
unsigned long lastDetectionTime = 0;

// Flag to control classification frequency
bool classificationInProgress = false;

// OPTIMIZATION: Flag to control debug printing
bool verboseDebug = false;  // Set to false to disable detailed debug messages

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (verboseDebug) {
    Serial.print("Last Packet Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  }
}

void setup() {
  // Initialize Serial communications
  Serial.begin(115200);
  
  // Configure IR sensor input
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  
  // Initialize ESP-NOW
  initializeESPNOW();
  
  // Camera configuration - OPTIMIZED for speed
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_240X240;
  config.jpeg_quality = 12;  // INCREASED from 10 to 12 (slightly lower quality = faster)
  config.fb_count = 1;

  // Initialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  // Additional camera settings
  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
  }

  s->set_hmirror(s, 1);
  s->set_vflip(s, 0);

  Serial.println("System Ready - Waiting for trash detection...");
  Serial.println("Optimized for speed!");
}

void loop() {
  // Check IR sensor
  checkIRSensor();
  
  // If trash is detected and no classification is in progress
  if (trashDetected && !classificationInProgress) {
    Serial.println("Trash detected! Capturing...");
    performClassification();
    trashDetected = false;
  }
  
  // Small delay - KEEP THIS LOW
  delay(10);  // Reduced from 50ms to 10ms for faster response
}

/**
 * OPTIMIZED IR Sensor Logic - Non-blocking
 */
void checkIRSensor() {
  int irState = digitalRead(IR_SENSOR_PIN);
  unsigned long currentTime = millis();
  
  // Case 1: Sensor just became triggered
  if (irState == LOW && !sensorCurrentlyTriggered) {
    sensorCurrentlyTriggered = true;
    triggerStartTime = currentTime;
    if (verboseDebug) Serial.println("→ Hand entered");
  }
  
  // Case 2: Sensor just became released
  if (irState == HIGH && sensorCurrentlyTriggered) {
    sensorCurrentlyTriggered = false;
    triggerEndTime = currentTime;
    
    // Calculate trigger duration
    unsigned long triggerDuration = triggerEndTime - triggerStartTime;
    
    if (verboseDebug) {
      Serial.print("← Hand withdrew after ");
      Serial.print(triggerDuration);
      Serial.println(" ms");
    }
    
    // Valid trash deposit?
    if (triggerDuration >= MIN_TRIGGER_TIME) {
      // OPTIMIZATION: Use non-blocking approach instead of delay()
      // Set a timer for stabilization instead of using delay()
      static unsigned long stabilizationStartTime = 0;
      stabilizationStartTime = currentTime;
      
      // Wait for stabilization WITHOUT blocking
      while (millis() - stabilizationStartTime < STABILIZATION_DELAY) {
        // Just wait - but we could do other things here if needed
        delay(1); // Small delay to prevent watchdog issues
      }
      
      // Debounce check
      if (currentTime - lastDetectionTime > DEBOUNCE_DELAY) {
        trashDetected = true;
        lastDetectionTime = currentTime;
        if (verboseDebug) Serial.println("  → Ready to capture");
      }
    }
  }
}

/**
 * Performs classification and sends result
 */
void performClassification() {
  classificationInProgress = true;
  
  // OPTIMIZATION: Disable verbose debug during classification
  bool oldVerbose = verboseDebug;
  verboseDebug = false;
  
  // Perform classification
  unsigned long startTime = millis();
  String classificationResult = classify();
  unsigned long elapsed = millis() - startTime;
  
  Serial.print("Classification completed in ");
  Serial.print(elapsed);
  Serial.println(" ms");
  
  if (classificationResult.startsWith("[ERROR]")) {
    Serial.println("Classification failed: " + classificationResult);
    
    strcpy(classificationData.trash_type, "ERROR");
    classificationData.confidence = 0.0;
    classificationData.timestamp = millis();
    classificationData.classification_success = false;
  } else {
    Serial.println("Result: " + classificationResult);
    
    strncpy(classificationData.trash_type, classificationResult.c_str(), sizeof(classificationData.trash_type) - 1);
    classificationData.trash_type[sizeof(classificationData.trash_type) - 1] = '\0';
    
    // Get confidence score
    classificationData.confidence = 0;
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
      if (result.classification[i].value > classificationData.confidence) {
        classificationData.confidence = result.classification[i].value;
      }
    }
    classificationData.timestamp = millis();
    classificationData.classification_success = true;
  }
  
  // Send data
  sendClassificationData();
  
  // Restore verbose setting
  verboseDebug = oldVerbose;
  
  classificationInProgress = false;
  Serial.println("Ready for next detection");
}

/**
 * Sends classification data via ESP-NOW
 */
void sendClassificationData() {
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &classificationData, sizeof(classificationData));
  
  if (result == ESP_OK && verboseDebug) {
    Serial.println("Data sent");
  }
}

/**
 * Initializes ESP-NOW communication
 */
void initializeESPNOW() {
  WiFi.mode(WIFI_STA);
  
  Serial.print("ESP32 MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  
  Serial.println("ESP-NOW ready");
}

/**
 * OPTIMIZED classification function
 */
String classify() {
  capture_quick();

  if (!capture()) return "[ERROR] Capture failed";

  // Prepare signal
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_WIDTH;
  signal.get_data = &raw_feature_get_data;

  // Run classifier
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
  dl_matrix3du_free(resized_matrix);

  if (res != 0) {
    return "[ERROR] Classifier error";
  }

  // Find highest scoring classification
  int index = 0;
  float score = 0.0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    if (result.classification[ix].value > score) {
      score = result.classification[ix].value;
      index = ix;
    }
  }

  String label = String(result.classification[index].label);
  label.replace("\n", "");
  label.replace("\r", "");
  label.trim();

  return label;
}

/**
 * Quickly captures and releases a frame
 */
void capture_quick() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
}

/**
 * OPTIMIZED capture function
 */
bool capture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;

  // OPTIMIZATION: Use smaller buffer and simpler conversion
  dl_matrix3du_t *rgb888_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_matrix->item);

  resized_matrix = dl_matrix3du_alloc(1, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3);
  image_resize_linear(resized_matrix->item, rgb888_matrix->item, 
                      EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3, 
                      fb->width, fb->height);

  dl_matrix3du_free(rgb888_matrix);
  esp_camera_fb_return(fb);

  return true;
}

/**
 * Callback function for Edge Impulse
 */
int raw_feature_get_data(size_t offset, size_t out_len, float *signal_ptr) {
  size_t pixel_ix = offset * 3;
  
  for (size_t i = 0; i < out_len; i++) {
    uint8_t r = resized_matrix->item[pixel_ix];
    uint8_t g = resized_matrix->item[pixel_ix + 1];
    uint8_t b = resized_matrix->item[pixel_ix + 2];
    signal_ptr[i] = (r << 16) + (g << 8) + b;
    
    pixel_ix += 3;
  }

  return 0;
}