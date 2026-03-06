#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    char trash_type[32];
    float confidence;
    int timestamp;
    bool classification_success;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// Servo motor pins
#define SERVO1_PIN 13  // First drop servo
#define SERVO2_PIN 12  // Second sorting servo  
#define SERVO3_PIN 14  // Third sorting servo

// Servo positions
#define DROP_POS 180   // Position to drop trash
#define NEUTRAL_POS 90 // Neutral position
#define LEFT_POS 0     // Left position
#define RIGHT_POS 180  // Right position
#define WAIT_MS 1000   // Wait time between movements

// Create servo objects
Servo servo1;
Servo servo2;
Servo servo3;

// Function prototypes
void sortTrash(const char* trash_type);
void moveServoSequence(Servo& servo, int pos1, int delay1, int pos2 = -1, int delay2 = 0);

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Trash Type: ");
  Serial.println(myData.trash_type);
  Serial.print("Confidence: ");
  Serial.println(myData.confidence);
  Serial.print("Timestamp: ");
  Serial.println(myData.timestamp);
  Serial.print("Success: ");
  Serial.println(myData.classification_success);
  Serial.println();
  
  // Only sort if classification was successful
  if (myData.classification_success) {
    sortTrash(myData.trash_type);
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Attach servos to pins
  servo1.setPeriodHertz(50);    // Standard 50hz servo
  servo1.attach(SERVO1_PIN, 500, 2400);
  
  servo2.setPeriodHertz(50);
  servo2.attach(SERVO2_PIN, 500, 2400);
  
  servo3.setPeriodHertz(50);
  servo3.attach(SERVO3_PIN, 500, 2400);

  // Set servos to neutral position
  servo1.write(NEUTRAL_POS);
  servo2.write(NEUTRAL_POS);
  servo3.write(NEUTRAL_POS);
  delay(1000); // Give servos time to reach position

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  
  Serial.println("ESP-NOW Receiver with Servo Control Ready");
}

void loop() {
  // Empty loop - everything happens in callback
}

void sortTrash(const char* trash_type) {
  Serial.print("Sorting trash type: ");
  Serial.println(trash_type);
  
  // Styrofoam and plastic
  if (strcmp(trash_type, "styrofoam") == 0 || strcmp(trash_type, "plastic") == 0) {
    Serial.println("Sorting to Bin 1: Styrofoam/Plastic");

     // Servo Motor 1 Drops (flip right)
    servo1.write(RIGHT_POS);
    delay(WAIT_MS);
    servo1.write(NEUTRAL_POS);
    delay(WAIT_MS);
    
    // Servo Motor 2 flip left
    servo2.write(LEFT_POS);
    delay(WAIT_MS);
    servo2.write(NEUTRAL_POS);
    delay(WAIT_MS);
    
    // Servo Motor 3 flip right
    servo3.write(RIGHT_POS);
    delay(WAIT_MS);
    servo3.write(NEUTRAL_POS);
   
  
  }
  // Plastic bottle
  else if (strcmp(trash_type, "plastic_bottle") == 0) {
    Serial.println("Sorting to Bin 2: Plastic Bottle");

        // Servo Motor 1 Drops (flip right)
    servo1.write(RIGHT_POS);
    delay(WAIT_MS);
    servo1.write(NEUTRAL_POS);
    delay(WAIT_MS);
    
    // Servo Motor 2 flip right
    servo2.write(RIGHT_POS);
    delay(WAIT_MS);
    servo2.write(NEUTRAL_POS);
    delay(WAIT_MS);
    
    servo3.write(NEUTRAL_POS);
    
   
    
  }
  // Paper, paper_cup, cardboard
  else if (strcmp(trash_type, "paper") == 0 || 
           strcmp(trash_type, "paper_cup") == 0 || 
           strcmp(trash_type, "cardboard") == 0) {
    Serial.println("Sorting to Bin 3: Paper/Cardboard");

     
    // Servo Motor 1 Drops (flip right)
    servo1.write(RIGHT_POS);
    delay(WAIT_MS);
    servo1.write(NEUTRAL_POS);
    delay(WAIT_MS);
    
    // Servo Motor 2 flip left
    servo2.write(LEFT_POS);
    delay(WAIT_MS);
    servo2.write(NEUTRAL_POS);
    delay(WAIT_MS);
    
    // Servo Motor 3 flip left
    servo3.write(LEFT_POS);
    delay(WAIT_MS);
    servo3.write(NEUTRAL_POS);

    
    

  }
  else {
    Serial.println("Unknown trash type - No sorting action");
  }
  
  delay(500); // Small delay before next operation 
}

// Helper function for smoother servo movements
void moveServoSequence(Servo& servo, int pos1, int delay1, int pos2, int delay2) {
  servo.write(pos1);
  delay(delay1);
  if (pos2 != -1) {
    servo.write(pos2);
    delay(delay2);
  }
}