# HearMe - IoT Sound Detection System

## Project Description

An IoT system for detecting anger using ESP32 microcontroller, machine learning, and mobile notifications. The system captures audio through a microphone, processes it using a TensorFlow Lite model, and sends alerts to a Flutter mobile application via Firebase.
For wider view of the app read MENTEE_NUMBER_SYSTEM.md, where it is explained about the unique numbers generated.
For further explanation on how to set up the complete notification system with ESP32 integration read NOTIFICATION_SETUP_GUIDE.md

## Team Members

- mahmud omar
- rula younis
- shaden abo raya

## Libraries

### ESP32 Libraries
- **Arduino ESP32**
- **Firebase ESP Client** 
- **Adafruit SSD1306**
- **Adafruit GFX Library**
- **TensorFlowLite_ESP32**

### Flutter Libraries
- **Flutter SDK**
- **firebase_core**
- **firebase_auth**
- **cloud_firestore**
- **firebase_storage**
- **firebase_messaging**

### Python Libraries (Model Training)
- **TensorFlow**
- **NumPy**
- **Librosa**
- **Scikit-learn**

## Hardware Components Required

- ESP32-CAM module
- INMP441 I2S Digital Microphone
- MAX98357A I2S DAC Amplifier
- Speaker (4-8 Ohm, 3W)
- OLED Display 128x64 (I2C)
- 4x3 Matrix Keypad
- Jumper wires and breadboard
- USB cable for programming
- 5V 2A Power supply



### Setup Steps
1. Clone this repository
2. Configure `ESP32/SECRETS.h` with your WiFi and Firebase credentials (copy from `SECRETS_EXAMPLE.h`)
3. Review `ESP32/parameters.h` and adjust GPIO pins if needed
4. Install ESP32 libraries listed above
5. Upload `ESP32/esp32_receiver.ino` to your board
6. Set up Firebase project and add configuration files to Flutter app
7. Run `flutter pub get` in the flutter_app folder
8. Build and run the Flutter application

### Configuration Files
- **SECRETS.h**: WiFi credentials and Firebase tokens (see ESP32/SECRETS.h)
- **parameters.h**: Hardware GPIO pin configuration (see ESP32/PARAMETERS.md)



## Project Structure

