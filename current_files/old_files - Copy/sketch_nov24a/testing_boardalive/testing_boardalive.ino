
void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("Board is alive!");
}

void loop() {
  delay(1000);
}

