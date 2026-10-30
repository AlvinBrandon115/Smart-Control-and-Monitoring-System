# Adding this to your Wokwi ESP-IDF (ESP32) project
1. Open your Wokwi project (diagram.json already pasted).
2. Replace main.c with main/main.c from this folder.
3. Add each of the other files from main/ (one new file per .c/.h) and edit main/CMakeLists.txt to match the one here.
4. Press Start. Open the Serial Monitor to use the menu (type: help).
Pin map = classic ESP32, matching diagram.json. MFRC522 driver is still a stub (no card scans yet).
