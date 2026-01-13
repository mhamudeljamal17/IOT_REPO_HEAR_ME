# Audio Server Demo

This project demonstrates an audio server using an ESP32 microcontroller. The server allows users to upload WAV files and play them back using the I2S interface. **The code does not require any external audio library.**

## Features

- **Web interface**: Upload WAV files to the ESP32 using a web interface.
- **Local storage of files**: Uploaded WAV files are stored in ESP32 Flash using LittleFS.
- **WAV Header Parsing**: Read and parse the WAV file header to check the sample rate and bit depth and configure the I2S interface accordingly.
- **I2S Playback**: Play the uploaded WAV file using the I2S interface.

## Hardware Requirements

- ESP32 Development Board
- I2S DAC or amplifier such as max98357a

## Software Requirements

- This code uses ESPAsyncWebServer library by lacamera - [GitHub Repository](https://github.com/lacamera/ESPAsyncWebServer) - tested with version 3.1.

## Installation

1. **Install Dependencies**:
    - Install the ESP32 core for Arduino.
    - Install the required libraries using the Library Manager in the Arduino IDE or PlatformIO.

2. **Configure WiFi Credentials**:
    - Add your WiFi credentials to `secrets.h`:
      ```cpp
      #define WIFI_SSID "your-ssid"
      #define WIFI_PASSWORD "your-password"
      ```

3. **Upload the Code**:
    - Connect your ESP32 board to your computer.
    - Open `audio_server_demo.ino` in the Arduino IDE.
    - Select the correct board and port.
    - Upload the code to the ESP32.

4. **Upload the HTML File**:
   - The HTML file needs to be uploaded using the LittleFS uploader plugin - [follow instructions here](https://randomnerdtutorials.com/arduino-ide-2-install-esp8266-littlefs/).

## Usage

1. **Start the ESP32**:
    - After uploading the code, the ESP32 will start and connect to the configured WiFi network.

2. **Access the Web Interface**:
    - Open a web browser and navigate to the IP address of the ESP32 (check the serial monitor for the IP address).
    - Use the web interface to upload a WAV file.

3. **Playback**:
    - The uploaded WAV file will be played back using the I2S interface.
