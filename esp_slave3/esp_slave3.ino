/*
 * Segri Green - ESP32 Master with TFT Display
 * CENTERED TEXT - MAXIMUM FONT SIZE for 2" display
 * DISPLAY: 320 x 240 pixels (landscape)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>

// TFT Display
TFT_eSPI tft = TFT_eSPI();

// Structure to receive data from trash classifier
typedef struct struct_message {
    char trash_type[32];
    float confidence;
    int timestamp;
    bool classification_success;
} struct_message;

struct_message myData;

// Token storage
String currentToken = "";
String currentTrashType = "";
String currentDisplayTrashType = ""; // For displaying Biodegradable, etc.
int currentMinutes = 0;

// Helper function to center text horizontally (320px width)
int centerTextX(String text, int textSize) {
    tft.setTextSize(textSize);
    // Approximate width: each character ~ 6 pixels * textSize
    int textWidth = text.length() * (6 * textSize);
    return (320 - textWidth) / 2;
}

// Function to map trash type to display name and minutes
int getTimePrice(const char* trash_type, String &displayName) {
    if (strcmp(trash_type, "paper") == 0 || 
        strcmp(trash_type, "paper_cup") == 0 || 
        strcmp(trash_type, "cardboard") == 0) {
        displayName = "Biodegradable";
        return 15;
    }
    else if (strcmp(trash_type, "styrofoam") == 0 || 
             strcmp(trash_type, "plastic") == 0) {
        displayName = "Non-Biodegradable";
        return 20;
    }
    else if (strcmp(trash_type, "plastic_bottle") == 0) {
        displayName = "Recyclable";
        return 30;
    }
    else {
        displayName = "Unknown";
        return 0;
    }
}

// Function to generate a simple token
String generateToken(const char* trash_type, int minutes) {
    unsigned long now = millis();
    String token = String(trash_type[0]) + String(minutes) + String(now % 10000, HEX);
    token.toUpperCase();
    
    if (token.length() > 8) {
        token = token.substring(0,4) + "-" + token.substring(4,8);
    }
    return token;
}

// ============== CENTERED DISPLAY FUNCTIONS ==============

// Welcome screen - CENTERED for 320x240
void displayWelcome() {
    tft.fillScreen(TFT_BLACK);
    
    // "Segre Green" - VERY LARGE (size 4)
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(4);
    tft.setCursor(centerTextX("Segre", 4), 20);
    tft.println("Segre");
    tft.setCursor(centerTextX("Green", 4), 60);
    tft.println("Green");
    
    // Instructions - size 2
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX("Drop trash", 2), 110);
    tft.println("Drop trash");
    tft.setCursor(centerTextX("to earn WiFi!", 2), 135);
    tft.println("to earn WiFi!");
    
    // IP Address info - NEW LINE
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX("Go to:", 2), 165);
    tft.println("Go to:");
    tft.setCursor(centerTextX("192.168.4.1", 2), 190);
    tft.println("192.168.4.1");
    tft.setCursor(centerTextX("to enter token", 2), 215);
    tft.println("to enter token");
}

// Token screen - CENTERED for 320x240
void displayToken(String token, String displayType, int minutes) {
    tft.fillScreen(TFT_BLACK);
    
    // Header - size 3
    tft.fillRect(0, 0, 320, 40, TFT_DARKGREEN);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextSize(3);
    tft.setCursor(centerTextX("SegreGreen",3), 10);
    tft.println("SegreGreen");
    
    // TRASH label - size 2
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX("TRASH:", 2), 45);
    tft.println("TRASH:");
    
    // Display trash type (Biodegradable, etc.)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (displayType.length() <= 8) {
        tft.setTextSize(3);
        tft.setCursor(centerTextX(displayType, 3), 70);
        tft.println(displayType);
    } else if (displayType.length() <= 12) {
        tft.setTextSize(2);
        tft.setCursor(centerTextX(displayType, 2), 75);
        tft.println(displayType);
    } else {
        tft.setTextSize(2);
        String part1 = displayType.substring(0, 8);
        String part2 = displayType.substring(8);
        tft.setCursor(centerTextX(part1, 2), 70);
        tft.println(part1);
        tft.setCursor(centerTextX(part2, 2), 95);
        tft.println(part2);
    }
    

    
    // Time earned
    String timeStr = String(minutes) + " minutes";
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX("Time earned:", 2), 125);
    tft.println("Time earned:");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(centerTextX(timeStr, 3), 150);
    tft.println(timeStr);
    
    // Token box
    int boxWidth = 280;
    int boxX = (320 - boxWidth) / 2;
    tft.drawRect(boxX, 175, boxWidth, 40, TFT_YELLOW);
    
    // Token inside box
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(centerTextX(token, 3), 185);
    tft.println(token);
    
    // Instructions at bottom
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(centerTextX("Enter token at 192.168.4.1", 1), 225);
    tft.println("Enter token at 192.168.4.1");
}

// Confirmation screen - CENTERED for 320x240
void displayConfirmation() {
    tft.fillScreen(TFT_BLACK);
    
    // Large checkmark
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(8);
    tft.setCursor(120, 30);
    tft.println("✓");
    
    // SENT
    tft.setTextSize(5);
    tft.setCursor(centerTextX("SENT!", 5), 50);
    tft.println("SENT!");
    
    // Instructions
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX("Go to:", 2), 150);
    tft.println("Go to:");
    tft.setCursor(centerTextX("192.168.4.1", 2), 175);
    tft.println("192.168.4.1");
    tft.setCursor(centerTextX("to enter token", 2), 200);
    tft.println("to enter token");
    
    delay(3000);
    displayWelcome();
}

// Error screen - CENTERED for 320x240
void displayError(String error) {
    tft.fillScreen(TFT_BLACK);
    
    // Large X
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(8);
    tft.setCursor(120, 30);
    tft.println("X");
    
    // ERROR
    tft.setTextSize(5);
    tft.setCursor(centerTextX("ERROR", 5), 100);
    tft.println("ERROR");
    
    // Error message
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX(error, 2), 150);
    tft.println(error);
    
    // Try again
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX("Try again!", 2), 180);
    tft.println("Try again!");
    
    delay(3000);
    displayWelcome();
}

// Activation success screen
void displayActivated() {
    tft.fillScreen(TFT_BLACK);
    
    // WiFi ACTIVE
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(4);
    tft.setCursor(centerTextX("WiFi", 4), 50);
    tft.println("WiFi");
    tft.setCursor(centerTextX("ACTIVE!", 4), 100);
    tft.println("ACTIVE!");
    
    // Enjoy
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.setCursor(centerTextX("Enjoy!", 3), 160);
    tft.println("Enjoy!");
    
    delay(3000);
    displayWelcome();
}

// Startup screen
void displayStartup() {
    tft.fillScreen(TFT_BLACK);
    
    // Segre Green
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(4);
    tft.setCursor(centerTextX("Segre", 4), 40);
    tft.println("Segre");
    tft.setCursor(centerTextX("Green", 4), 85);
    tft.println("Green");
    
    // Starting...
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(centerTextX("Starting...", 2), 150);
    tft.println("Starting...");
    
    // IP Info on startup
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(centerTextX("192.168.4.1", 1), 200);
    tft.println("192.168.4.1");
}

// ESP-NOW Callback
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));
    
    Serial.print("Received: ");
    Serial.println(myData.trash_type);
    Serial.print("Confidence: ");
    Serial.println(myData.confidence);
    
    if (myData.classification_success) {
        String displayName;
        int minutes = getTimePrice(myData.trash_type, displayName);
        
        if (minutes > 0) {
            String token = generateToken(myData.trash_type, minutes);
            currentToken = token;
            currentTrashType = String(myData.trash_type);
            currentDisplayTrashType = displayName;
            currentMinutes = minutes;
            
            displayToken(token, displayName, minutes);
            
            Serial.print("TOKEN:");
            Serial.print(token);
            Serial.print(":");
            Serial.print(myData.trash_type);
            Serial.print(":");
            Serial.println(minutes);
            
            delay(8000);
            displayConfirmation();
        } else {
            displayError("Unknown trash");
        }
    } else {
        displayError("Low confidence");
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize TFT
    tft.init();
    tft.setRotation(1); // Landscape mode (320x240)
    tft.fillScreen(TFT_BLACK);
    
    // Show startup screen
    displayStartup();
    delay(2000);
    displayWelcome();
    
    // Setup ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        displayError("ESP-NOW failed");
        return;
    }
    
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
    Serial.println("ESP32 Ready - Waiting for trash...");
}

void loop() {
    if (Serial.available()) {
        String response = Serial.readStringUntil('\n');
        response.trim();
        
        if (response.startsWith("ACTIVATED:")) {
            String activatedToken = response.substring(10);
            if (activatedToken == currentToken) {
                displayActivated();
            }
        }
    }
    delay(100);
}