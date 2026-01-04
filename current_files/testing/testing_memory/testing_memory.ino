#include <SPIFFS.h>
#include "esp_heap_caps.h"

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== ESP32 SYSTEM INFO ===");

  // --- RAM info ---
  size_t free_ram = esp_get_free_heap_size();
  size_t min_free_ram = esp_get_minimum_free_heap_size();
  Serial.printf("Free RAM: %u bytes\n", free_ram);
  Serial.printf("Minimum free RAM ever: %u bytes\n", min_free_ram);

  // --- SPIFFS info ---
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.println("SPIFFS Mounted Successfully");
   Serial.printf("SPIFFS Total Bytes: %u\n", (unsigned int)SPIFFS.totalBytes());
Serial.printf("SPIFFS Used Bytes: %u\n", (unsigned int)SPIFFS.usedBytes());
Serial.printf("SPIFFS Free Bytes: %u\n", (unsigned int)(SPIFFS.totalBytes() - SPIFFS.usedBytes()));

  }
  if (psramFound()) {
    Serial.println("PSRAM is available!");
    Serial.printf("Total PSRAM: %u bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  } else {
    Serial.println("No PSRAM detected.");
  }

  Serial.println("==========================");
}

void loop() {
  // nothing to do here
}
