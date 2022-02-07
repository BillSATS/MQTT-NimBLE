	
#upload (after compiling) a script for the ESP32

compile:
	/opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:esp32 | tee output

upload:
	/opt/homebrew/bin/arduino-cli upload --port /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 | tee  output
	
clearFlash:
	/opt/homebrew/bin/esptool.py --chip esp32 --port /dev/cu.usbserial-0001 erase_flash
	
