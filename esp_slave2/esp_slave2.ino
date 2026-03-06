#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

// Pin definitions
#define TRIG_PIN_1 13
#define ECHO_PIN_1 12
#define TRIG_PIN_2 27
#define ECHO_PIN_2 26
#define TRIG_PIN_3 25
#define ECHO_PIN_4 33
#define SERVO_PIN 14
#define SIM800L_RX 16
#define SIM800L_TX 17

// Distance thresholds (in cm)
#define FULL_THRESHOLD 10  // Distance when trash is full (adjust based on your setup)
#define DOOR_OPEN_ANGLE 90   // Open position
#define DOOR_CLOSE_ANGLE 0   // Closed position

// Variables
// PHONE NUMBERS ARRAY - Add or remove numbers here as needed
const String PHONE_NUMBERS[] = {
  "+639683520050",  // Number 1
  "+639363639838", // Number 2
  "+639501050520", // Number 3
  "+639632195842" // Number 4

};

// Get the number of phone numbers in the array
const int PHONE_COUNT = sizeof(PHONE_NUMBERS) / sizeof(PHONE_NUMBERS[0]);

const int CHECK_INTERVAL = 5000; // Check every 5 seconds

Servo doorServo;
bool doorClosed = false;
unsigned long lastSMSTime = 0;
const unsigned long SMS_COOLDOWN = 30000; // 10 minutes between SMS
bool sim800lReady = false;

// Sensor data structure
struct TrashBin {
  String name;
  int trigPin;
  int echoPin;
  bool isFull;
  bool notified;
};

TrashBin bins[3] = {
  {"Bin 1", TRIG_PIN_1, ECHO_PIN_1, false, false},
  {"Bin 2", TRIG_PIN_2, ECHO_PIN_2, false, false},
  {"Bin 3", TRIG_PIN_3, ECHO_PIN_4, false, false}
};

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, SIM800L_RX, SIM800L_TX); // For SIM800L
  
  // Display phone numbers being used
  Serial.println("=================================");
  Serial.print("Sending alerts to ");
  Serial.print(PHONE_COUNT);
  Serial.println(" phone numbers:");
  for (int i = 0; i < PHONE_COUNT; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(PHONE_NUMBERS[i]);
  }
  Serial.println("=================================");
  
  // Allocate timers for ESP32 servo library
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Initialize ultrasonic sensor pins
  for (int i = 0; i < 3; i++) {
    pinMode(bins[i].trigPin, OUTPUT);
    pinMode(bins[i].echoPin, INPUT);
  }
  
  // Initialize servo with proper pulse widths
  doorServo.setPeriodHertz(50);
  doorServo.attach(SERVO_PIN, 500, 2400);
  
  // Start with door OPEN (90°)
  Serial.println("Initializing door to OPEN position...");
  doorServo.write(DOOR_OPEN_ANGLE);
  delay(1000);
  doorClosed = false;
  
  // Initialize SIM800L
  sim800lReady = setupSIM800L();
  
  if (sim800lReady) {
    Serial.println("✅ SIM800L initialized successfully!");
  } else {
    Serial.println("❌ SIM800L initialization failed! Check connections and power.");
  }
  
  Serial.println("System initialized. Monitoring trash bins...");
  Serial.print("Door OPEN angle: ");
  Serial.println(DOOR_OPEN_ANGLE);
  Serial.print("Door CLOSE angle: ");
  Serial.println(DOOR_CLOSE_ANGLE);
}

void loop() {
  bool anyBinFull = false;
  int fullBinIndex = -1; // Track which bin is full
  
  // Check all sensors first to see if any bin is full
  for (int i = 0; i < 3; i++) {
    float distance = measureDistance(i);
    bins[i].isFull = (distance <= FULL_THRESHOLD && distance > 0);
    
    Serial.print(bins[i].name);
    Serial.print(": ");
    Serial.print(distance);
    Serial.print(" cm - ");
    Serial.println(bins[i].isFull ? "FULL" : "OK");
    
    if (bins[i].isFull && !bins[i].notified) {
      anyBinFull = true;
      fullBinIndex = i; // Remember which bin triggered the alert
    }
    
    // Reset notification if bin is no longer full
    if (!bins[i].isFull) {
      bins[i].notified = false;
    }
  }
  
  // If a bin is full and we haven't sent notification yet
  if (anyBinFull && fullBinIndex != -1) {
    
    // STEP 1: Close the door first
    if (!doorClosed) {
      Serial.println("🔒 Closing door before sending SMS alert...");
      closeDoor();
      delay(1000); // Give door time to fully close
    }
    
    // STEP 2: Send SMS alert (door is now closed)
    if (sim800lReady) {
      Serial.println("📱 Door closed, sending SMS alert...");
      sendSMSAlert(fullBinIndex);
      bins[fullBinIndex].notified = true;
    } else {
      Serial.println("⚠️ Cannot send SMS - SIM800L not initialized");
      bins[fullBinIndex].notified = true; // Mark as notified to prevent repeated messages
    }
    
    // STEP 3: Wait a moment before opening door again
    delay(2000);
    
    // STEP 4: Open the door again
    if (doorClosed) {
      Serial.println("🔓 SMS sent, opening door again...");
      openDoor();
    }
  }
  
  // If no bins are full but door is closed, open it
  if (!anyBinFull && doorClosed) {
    openDoor();
  }
  
  delay(CHECK_INTERVAL);
}

float measureDistance(int binIndex) {
  digitalWrite(bins[binIndex].trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(bins[binIndex].trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(bins[binIndex].trigPin, LOW);
  
  long duration = pulseIn(bins[binIndex].echoPin, HIGH, 30000);
  float distance = duration * 0.034 / 2;
  
  if (duration == 0) {
    return 999;
  }
  
  return distance;
}

void closeDoor() {
  Serial.println("=================================");
  Serial.println("Closing door (moving to 0°)...");
  
  if (doorClosed) {
    Serial.println("Door already closed");
    return;
  }
  
  for (int angle = DOOR_OPEN_ANGLE; angle >= DOOR_CLOSE_ANGLE; angle -= 5) {
    doorServo.write(angle);
    Serial.print("Moving to: ");
    Serial.print(angle);
    Serial.println("°");
    delay(50);
  }
  
  doorServo.write(DOOR_CLOSE_ANGLE);
  delay(200);
  
  doorClosed = true;
  Serial.println("✓ Door CLOSED at 0°");
  Serial.println("=================================");
}

void openDoor() {
  Serial.println("=================================");
  Serial.println("Opening door (moving to 90°)...");
  
  if (!doorClosed) {
    Serial.println("Door already open");
    return;
  }
  
  for (int angle = DOOR_CLOSE_ANGLE; angle <= DOOR_OPEN_ANGLE; angle += 5) {
    doorServo.write(angle);
    Serial.print("Moving to: ");
    Serial.print(angle);
    Serial.println("°");
    delay(50);
  }
  
  doorServo.write(DOOR_OPEN_ANGLE);
  delay(200);
  
  doorClosed = false;
  Serial.println("✓ Door OPEN at 90°");
  Serial.println("=================================");
}

bool setupSIM800L() {
  delay(3000); // Wait for SIM800L to power up
  
  Serial.println("Initializing SIM800L...");
  
  // Try multiple times to establish communication
  for (int attempt = 1; attempt <= 3; attempt++) {
    Serial.print("Attempt ");
    Serial.print(attempt);
    Serial.println(" to initialize SIM800L...");
    
    // Test basic communication
    if (sendATCommand("AT", "OK", 2000)) {
      Serial.println("✅ SIM800L responded to AT command");
      
      // Check SIM card
      if (sendATCommand("AT+CPIN?", "READY", 2000)) {
        Serial.println("✅ SIM card ready");
        
        // Set SMS to text mode
        if (sendATCommand("AT+CMGF=1", "OK", 2000)) {
          Serial.println("✅ SMS text mode set");
          
          // Set character set
          sendATCommand("AT+CSCS=\"GSM\"", "OK", 2000);
          
          // Check signal quality
          sendATCommand("AT+CSQ", "", 1000);
          
          // Check network registration
          sendATCommand("AT+CREG?", "", 1000);
          
          Serial.println("✅ SIM800L fully initialized");
          return true;
        }
      }
    }
    
    Serial.println("❌ Attempt failed, retrying in 2 seconds...");
    delay(2000);
  }
  
  Serial.println("❌ SIM800L initialization failed after 3 attempts");
  return false;
}

bool sendATCommand(String command, String expectedResponse, int timeout) {
  Serial2.println(command);
  
  String response = "";
  unsigned long startTime = millis();
  
  while (millis() - startTime < timeout) {
    if (Serial2.available()) {
      char c = Serial2.read();
      response += c;
      Serial.print(c); // Echo the response
    }
  }
  
  Serial.println();
  
  if (expectedResponse != "" && response.indexOf(expectedResponse) != -1) {
    return true;
  } else if (expectedResponse == "") {
    return true; // No expected response to check
  }
  
  return false;
}

void sendSMSAlert(int binIndex) {
  unsigned long currentTime = millis();
  
  // Check SMS cooldown
  if (currentTime - lastSMSTime < SMS_COOLDOWN) {
    int minutesLeft = (SMS_COOLDOWN - (currentTime - lastSMSTime)) / 60000;
    int secondsLeft = ((SMS_COOLDOWN - (currentTime - lastSMSTime)) % 60000) / 1000;
    Serial.print("⏱️ SMS cooldown: ");
    Serial.print(minutesLeft);
    Serial.print(" minutes ");
    Serial.print(secondsLeft);
    Serial.println(" seconds remaining");
    return;
  }
  
  String message = "ALERT: " + bins[binIndex].name + " is FULL! Please empty the trash bin.";
  
  Serial.println("=================================");
  Serial.println("📱 Sending SMS alerts to all recipients:");
  Serial.println("Message: " + message);
  Serial.println("=================================");
  
  // Send to all phone numbers
  int successCount = 0;
  for (int i = 0; i < PHONE_COUNT; i++) {
    Serial.print("📲 Sending to ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(PHONE_COUNT);
    Serial.print(": ");
    Serial.println(PHONE_NUMBERS[i]);
    
    // Clear any pending data
    while (Serial2.available()) {
      Serial2.read();
    }
    
    // Set recipient number - use a simpler approach
    Serial2.print("AT+CMGS=\"");
    Serial2.print(PHONE_NUMBERS[i]);
    Serial2.println("\"");
    delay(1000); // Wait for prompt
    
    // Check for '>' prompt
    String response = "";
    unsigned long timeout = millis() + 5000;
    bool promptReceived = false;
    
    while (millis() < timeout) {
      if (Serial2.available()) {
        char c = Serial2.read();
        response += c;
        Serial.print(c);
        if (c == '>') {
          promptReceived = true;
          break;
        }
      }
    }
    
    if (promptReceived) {
      Serial.println();
      Serial.println("✓ Prompt received, sending message...");
      
      // Send message content
      Serial2.print(message);
      delay(200);
      
      // Send Ctrl+Z (ASCII 26)
      Serial2.write(26);
      delay(5000); // Wait for sending to complete
      
      // Check response
      response = "";
      timeout = millis() + 10000;
      while (millis() < timeout) {
        if (Serial2.available()) {
          char c = Serial2.read();
          response += c;
          Serial.print(c);
        }
      }
      Serial.println();
      
      if (response.indexOf("OK") > 0 || response.indexOf("+CMGS") > 0) {
        successCount++;
        Serial.println("  ✅ Sent successfully");
      } else {
        Serial.println("  ❌ Failed to send");
      }
    } else {
      Serial.println("  ❌ No prompt received from SIM800L");
    }
    
    delay(2000); // Delay between sends
  }
  
  lastSMSTime = currentTime;
  Serial.println("=================================");
  Serial.print("📊 SMS alerts completed: ");
  Serial.print(successCount);
  Serial.print("/");
  Serial.print(PHONE_COUNT);
  Serial.println(" sent successfully");
  Serial.println("=================================");
}

// Function to manually test SIM800L from Serial Monitor
void testSIM800L() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();
    if (command.startsWith("AT")) {
      Serial.println("Sending to SIM800L: " + command);
      Serial2.println(command);
      
      delay(1000);
      while (Serial2.available()) {
        String response = Serial2.readString();
        Serial.println("Response: " + response);
      }
    }
  }
}