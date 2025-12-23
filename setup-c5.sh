#!/bin/bash
# ESP32-C5 Development Setup Script (IDF v5.5)
# This script sets up the environment for ESP32-C5 development using ESP-IDF v5.5

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Setting up ESP32-C5 development environment (IDF v5.5)...${NC}"

# Check if ESP-IDF v5.5 is installed
if [ ! -d "$HOME/esp/esp-idf-v5.5" ]; then
    echo -e "${RED}ESP-IDF v5.5 not found at $HOME/esp/esp-idf-v5.5${NC}"
    echo -e "${YELLOW}Please wait for the installation to complete or run:${NC}"
    echo "cd ~/esp && git clone --recursive --branch v5.3.2 https://github.com/espressif/esp-idf.git esp-idf-v5.3"
    echo "cd ~/esp && git clone --recursive --branch v5.5.1 https://github.com/espressif/esp-idf.git esp-idf-v5.5"
    echo "cd ~/esp/esp-idf-v5.5 && ./install.sh esp32c5"
    exit 1
fi

# Source ESP-IDF
echo -e "${GREEN}Sourcing ESP-IDF v5.5 environment...${NC}"
source ~/esp/esp-idf-v5.5/export.sh

# Set target to ESP32-C5
echo -e "${GREEN}Setting target to ESP32-C5...${NC}"
idf.py set-target esp32c5

echo -e "${GREEN}Setup complete!${NC}"
echo -e "${YELLOW}You can now use the following commands:${NC}"
echo "  idf.py build          - Build the project"
echo "  idf.py flash          - Flash to device"
echo "  idf.py monitor        - Monitor serial output"
echo "  idf.py flash monitor  - Flash and monitor"
echo ""
echo -e "${YELLOW}Note: You need to run 'source ~/esp/esp-idf-v5.5/export.sh' in each new terminal${NC}"
