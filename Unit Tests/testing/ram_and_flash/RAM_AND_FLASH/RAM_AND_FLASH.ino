#include "esp_system.h"
#include "esp_heap_caps.h"

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("===== ESP32 Memory Info =====");

  // Internal RAM
  Serial.print("Free internal heap: ");
  Serial.println(ESP.getFreeHeap());

  // Largest block (important for TFLite tensor arena)
  Serial.print("Largest free block: ");
  Serial.println(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

  // PSRAM
  if (psramFound()) {
    Serial.print("Total PSRAM: ");
    Serial.println(ESP.getPsramSize());

    Serial.print("Free PSRAM: ");
    Serial.println(ESP.getFreePsram());
  } else {
    Serial.println("PSRAM NOT FOUND");
  }

  // Flash
  Serial.print("Flash size: ");
  Serial.println(ESP.getFlashChipSize());

  Serial.print("Sketch size: ");
  Serial.println(ESP.getSketchSize());

  Serial.print("Free sketch space: ");
  Serial.println(ESP.getFreeSketchSpace());
}

void loop() {}
