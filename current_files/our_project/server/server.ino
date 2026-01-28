#include <WiFi.h>

#include <WebServer.h>

/* ===== WIFI CONFIG ===== */
const char* ssid = "Mahmuds_iphone";
const char* password = "mahmudja";

/* ===== WEB SERVER ===== */
WebServer server(80);

/* ===== ROOT PAGE (HTML FORM) ===== */
void handleRoot() {
  String html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>ESP32 Input</title>"
    "</head>"
    "<body>"
    "<h2>Send ID to ESP32</h2>"
    "<form action='/submit'>"
    "<input type='text' name='id' placeholder='Enter ID'>"
    "<br><br>"
    "<input type='submit' value='Send'>"
    "</form>"
    "</body>"
    "</html>";

  server.send(200, "text/html", html);
}

/* ===== HANDLE SUBMITTED DATA ===== */
void handleSubmit() {
  if (server.hasArg("id")) {
    String id = server.arg("id");

    Serial.println("===== DATA RECEIVED =====");
    Serial.print("ID: ");
    Serial.println(id);
    Serial.println("=========================");

    server.send(200, "text/plain", "ID received: " + id);
  } else {
    server.send(400, "text/plain", "Error: Missing id");
  }
}

/* ===== SETUP ===== */
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nStarting ESP32 Web Server...");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);

  server.begin();
  Serial.println("HTTP server started");
}

/* ===== LOOP ===== */
void loop() {
  server.handleClient();
}
