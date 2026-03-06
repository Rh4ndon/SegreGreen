#include <HardwareSerial.h>

HardwareSerial sim800(2); // Use UART2 (RX=16, TX=17)

void setup() {
  Serial.begin(115200);
  sim800.begin(9600, SERIAL_8N1, 16, 17); // Check your module's baud rate
  
  delay(3000); // Wait for module to stabilize
  sendSMS("+639363639838", "Hello from ESP32!");
}

void loop() {}

void sendSMS(String number, String msg) {
  sim800.println("AT+CMGF=1"); // Set to text mode
  delay(500);
  sim800.print("AT+CMGS=\"");
  sim800.print(number);
  sim800.println("\"");
  delay(500);
  sim800.print(msg);
  delay(500);
  sim800.write(26); // Send Ctrl+Z to indicate end of message
  delay(5000); // Give it time to process
  Serial.println("SMS Sent Command Issued.");
}
