/**
 * @file BLESender.cpp
 * @author TjGer22
 * @brief Broadcasts sensor readings as BLE iBeacon advertisements.
 * @date 2026
 *
 * @details
 * Implements the BLESender class. Sets up a BLE advertiser, encodes
 * the current measurement into iBeacon format and controls
 * transmit power.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <Arduino.h>
#include "BLESender.h"
#include "../FileManager/Globals.h"

// Define UUIDs for different colors (used to identify different devices)
#define RED    "A495BB10-C5B1-4B44-B512-1370F02D74DE"
#define GREEN  "A495BB20-C5B1-4B44-B512-1370F02D74DE"
#define BLACK  "A495BB30-C5B1-4B44-B512-1370F02D74DE"
#define PURPLE "A495BB40-C5B1-4B44-B512-1370F02D74DE"
#define ORANGE "A495BB50-C5B1-4B44-B512-1370F02D74DE"
#define BLUE   "A495BB60-C5B1-4B44-B512-1370F02D74DE"
#define YELLOW "A495BB70-C5B1-4B44-B512-1370F02D74DE"
#define PINK   "A495BB80-C5B1-4B44-B512-1370F02D74DE"

/**
 * @brief Default constructor for the BLESender class.
 */
BLESender::BLESender(void) {}

/**
 * @brief Initializes BLE advertising with the specified transmission power.
 * 
 * @param TransPower_ID Transmission power level (0-7).
 */
void BLESender::begin(int8_t TransPower_ID) {
    BLEDevice::init("iBeacon"); // Initialize BLE with the name "iBeacon"
    setTransmittionPower(TransPower_ID); // Set BLE transmission power
}

/**
 * @brief Sends hydrometer data via BLE advertisement.
 * 
 * @param BLE_UUID Identifier to determine which UUID to use (1-8).
 * @param specific_gravity The measured specific gravity of the liquid.
 * @param Temperature_Fahrenheit The temperature in Fahrenheit.
 */
void BLESender::send_data(int8_t BLE_UUID, float specific_gravity, float Temperature_Fahrenheit) {
    int8_t l_BLE_UUID = BLE_UUID;
    oBeacon = BLEBeacon();
    bleUUID = BLEUUID(GREEN); // Default UUID

    // Assign the correct UUID based on BLE_UUID
    switch(l_BLE_UUID) {
        case 1: bleUUID = BLEUUID(RED); break;
        case 2: bleUUID = BLEUUID(GREEN); break;
        case 3: bleUUID = BLEUUID(BLACK); break;
        case 4: bleUUID = BLEUUID(PURPLE); break;
        case 5: bleUUID = BLEUUID(ORANGE); break;
        case 6: bleUUID = BLEUUID(BLUE); break;
        case 7: bleUUID = BLEUUID(YELLOW); break;
        case 8: bleUUID = BLEUUID(PINK); break;
        default: bleUUID = BLEUUID(RED); break;
    }

    // Configure BLE beacon settings
    oBeacon.setManufacturerId(APPLE_IBEACON_MANUFACTURER_ID); // Manufacturer ID for Apple iBeacon
    bleUUID = bleUUID.to128();
    oBeacon.setProximityUUID(BLEUUID(bleUUID.getNative()->uuid.uuid128, 16, true));
    oBeacon.setMajor(Temperature_Fahrenheit); // Store temperature as the major value
    oBeacon.setMinor(calculate_BLE_Trans_Gravity(specific_gravity)); // Store specific gravity as minor value
    oBeacon.setSignalPower(oBeacon.getSignalPower());

    // Prepare BLE advertisement data
    oAdvertisementData = BLEAdvertisementData();
    oScanResponseData = BLEAdvertisementData();
    oAdvertisementData.setFlags(0x04); // Set flag for BLE advertisement

    std::string strServiceData = "";
    strServiceData += (char)26;     // Length
    strServiceData += (char)0xFF;   // Type
    strServiceData += oBeacon.getData(); // Append beacon data

    oAdvertisementData.addData(strServiceData);
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setAdvertisementData(oAdvertisementData);
    pAdvertising->setScanResponseData(oScanResponseData);
    pAdvertising->start(); // Start BLE advertising

    delay(2000); // Wait before stopping advertisement
}

/**
 * @brief Stops BLE advertising.
 */
void BLESender::stop(void) {
    pAdvertising->stop(); // Stop BLE advertisement
    BLEDevice::deinit(true); // Deinitialize BLE to free resources
}

/**
 * @brief Sets the transmission power level of the BLE device.
 * 
 * @param TransPower_ID Transmission power level (0-7).
 * - 0: -12 dBm
 * - 1:  -9 dBm
 * - 2:  -6 dBm
 * - 3:  -3 dBm
 * - 4:   0 dBm
 * - 5:  +3 dBm
 * - 6:  +6 dBm
 * - 7:  +9 dBm
 */
void BLESender::setTransmittionPower(int TransPower_ID) {
    switch(TransPower_ID) {
        case 0: BLEDevice::setPower(ESP_PWR_LVL_N12); break;
        case 1: BLEDevice::setPower(ESP_PWR_LVL_N9); break;
        case 2: BLEDevice::setPower(ESP_PWR_LVL_N6); break;
        case 3: BLEDevice::setPower(ESP_PWR_LVL_N3); break;
        case 4: BLEDevice::setPower(ESP_PWR_LVL_N0); break;
        case 5: BLEDevice::setPower(ESP_PWR_LVL_P3); break;
        case 6: BLEDevice::setPower(ESP_PWR_LVL_P6); break;
        case 7: BLEDevice::setPower(ESP_PWR_LVL_P9); break;
        default: BLEDevice::setPower(ESP_PWR_LVL_P9); break; // Default to max power if invalid input
    }
}

/**
 * @brief Converts specific gravity into an integer format for BLE transmission.
 * 
 * @param l_SG Specific gravity value.
 * @return int16_t Specific gravity multiplied by 1000 for better precision.
 */
int16_t BLESender::calculate_BLE_Trans_Gravity(float l_SG) {
    float l_SG_1000 = l_SG * 1000;
    return static_cast<int>(l_SG_1000);
}
