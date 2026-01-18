#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SECRETS.h"  // Your WiFi credentials

// Backend server URL (Render deployment URL)
const char* backendUrl = "https://your-app.onrender.com";  // Replace with your Render URL

// Web server for receiving data
WiFiServer server(80);

// Recording info
String currentRecordingUrl = "";
String currentFileName = "";
String recordingId = "";

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\nHearMe ESP32 Starting...");
  
  // Connect to WiFi
  connectToWiFi();
  
  // Start web server to receive data from backend
  server.begin();
  Serial.println("HTTP server started on port 80");
  
  // Register this device with backend
  registerDevice();
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectToWiFi();
  }
  
  // Handle incoming HTTP requests from backend
  handleIncomingRequests();
  
  delay(100);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

void registerDevice() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot register - no WiFi");
    return;
  }
  
  HTTPClient http;
  String url = String(backendUrl) + "/api/register-device";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["menteeId"] = MENTEE_ID;  // Define this in SECRETS.h
  doc["ipAddress"] = WiFi.localIP().toString();
  
  String payload;
  serializeJson(doc, payload);
  
  Serial.println("Registering device with backend...");
  Serial.println("Payload: " + payload);
  
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    Serial.printf("Registration response code: %d\n", httpCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.printf("Registration failed: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

void handleIncomingRequests() {
  WiFiClient client = server.available();
  
  if (!client) {
    return;
  }
  
  Serial.println("New client connected");
  
  String request = "";
  String body = "";
  bool isBody = false;
  int contentLength = 0;
  
  // Read the request
  while (client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      
      if (line.length() == 0) {
        // Empty line indicates end of headers
        isBody = true;
        break;
      }
      
      if (request.length() == 0) {
        request = line;
      }
      
      // Check for Content-Length header
      if (line.startsWith("Content-Length:")) {
        contentLength = line.substring(15).toInt();
      }
    }
  }
  
  // Read body if POST request
  if (contentLength > 0) {
    body = client.readStringUntil('\0');
    body = body.substring(0, contentLength);
  }
  
  Serial.println("Request: " + request);
  Serial.println("Body: " + body);
  
  // Handle different endpoints
  if (request.indexOf("POST /api/recording") >= 0) {
    handleRecording(client, body);
  } else if (request.indexOf("GET /status") >= 0) {
    sendStatusResponse(client);
  } else {
    send404Response(client);
  }
  
  client.stop();
  Serial.println("Client disconnected");
}

void handleRecording(WiFiClient& client, String jsonBody) {
  // Parse JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonBody);
  
  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    sendErrorResponse(client, "Invalid JSON");
    return;
  }
  
  // Extract data
  recordingId = doc["recordingId"].as<String>();
  currentRecordingUrl = doc["downloadUrl"].as<String>();
  currentFileName = doc["fileName"].as<String>();
  String timestamp = doc["timestamp"].as<String>();
  
  Serial.println("=== New Recording Received ===");
  Serial.println("Recording ID: " + recordingId);
  Serial.println("File Name: " + currentFileName);
  Serial.println("Download URL: " + currentRecordingUrl);
  Serial.println("Timestamp: " + timestamp);
  Serial.println("=============================");
  
  // Here you can add code to:
  // 1. Download the audio file from downloadUrl
  // 2. Process it (emotion detection, etc.)
  // 3. Store results back to Firebase
  
  // For now, just acknowledge receipt
  processRecording();
  
  // Send success response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println("{\"success\":true,\"message\":\"Recording received\"}");
}

void processRecording() {
  // TODO: Implement your recording processing logic here
  // This is where you would:
  // 1. Download the audio file from currentRecordingUrl
  // 2. Run emotion detection
  // 3. Send results back to Firebase
  
  Serial.println("Processing recording...");
  
  // Example: Download audio file
  downloadAudioFile(currentRecordingUrl);
  
  // Example: Run emotion detection
  // String emotion = detectEmotion(audioData);
  
  // Example: Send results to backend
  // sendResults(recordingId, emotion);
}

void downloadAudioFile(String url) {
  if (url.length() == 0) {
    Serial.println("No URL to download");
    return;
  }
  
  Serial.println("Downloading audio from: " + url);
  
  HTTPClient http;
  http.begin(url);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    Serial.printf("File size: %d bytes\n", len);
    
    // Get the stream
    WiFiClient* stream = http.getStreamPtr();
    
    // Here you would save to SD card or process in chunks
    // For now, just read and discard
    uint8_t buff[128];
    int bytesRead = 0;
    
    while (http.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();
      if (size) {
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        bytesRead += c;
        
        if (len > 0) {
          len -= c;
        }
      }
      delay(1);
    }
    
    Serial.printf("Downloaded %d bytes\n", bytesRead);
  } else {
    Serial.printf("Download failed: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
  }
  
  http.end();
}

void sendStatusResponse(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  
  StaticJsonDocument<256> doc;
  doc["status"] = "online";
  doc["device"] = "ESP32 XIAO S3";
  doc["menteeId"] = MENTEE_ID;
  doc["ipAddress"] = WiFi.localIP().toString();
  doc["currentRecording"] = currentFileName;
  
  String response;
  serializeJson(doc, response);
  client.println(response);
}

void sendErrorResponse(WiFiClient& client, String message) {
  client.println("HTTP/1.1 400 Bad Request");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println("{\"error\":\"" + message + "\"}");
}

void send404Response(WiFiClient& client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("404 - Not Found");
}
