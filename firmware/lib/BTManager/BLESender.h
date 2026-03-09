/**
 * @file BLESender.h
 * @author TjGer22
 * @brief Broadcasts sensor readings as BLE iBeacon advertisements.
 * @date 2026
 *
 * @details
 * Declares the BLESender class which configures the ESP32 BLE stack
 * and encodes gravity and temperature values into iBeacon major/
 * minor fields for reception by compatible mobile apps.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef BLESender_H
#define BLESender_H
#include <Arduino.h>

#include "BLEDevice.h"

#include "BLEBeacon.h"


class BLESender
{
public:
    /** @brief Default constructor. */
    BLESender();
    /**
     * @brief Initialises the ESP32 BLE stack and sets the transmission power.
     *
     * @param TransPower_ID Transmission power level 0–7
     *                      (0=−12 dBm … 7=+9 dBm).
     */
    void begin(int8_t TransPower_ID);
    /**
     * @brief Encodes gravity and temperature into an iBeacon advertisement
     *        and broadcasts it for 2 seconds.
     *
     * @param BLE_UUID             UUID colour index 1–8 (1=RED … 8=PINK),
     *                             used to identify the device in BLE scanner apps.
     * @param specific_gravity     Measured specific gravity (e.g. 1.050).
     * @param Temperature_Fahrenheit Wort temperature in degrees Fahrenheit.
     */
    void send_data(int8_t BLE_UUID, float specific_gravity,float Temperature_Fahrenheit);
    /**
     * @brief Stops BLE advertising and deinitialises the BLE stack
     *        to free memory for other tasks.
     */
    void stop(void);
    /**
     * @brief Selects the active BLE UUID colour profile.
     *
     * @param UUID index corresponding to a colour constant (1–8).
     */
    void setUUID(int);
    /**
     * @brief Sets the BLE radio transmission power.
     *
     * @param TransPower_ID Power level 0–7 (0=−12 dBm … 7=+9 dBm).
     *                      Values outside this range default to maximum power.
     */
    void setTransmittionPower(int);
    /**
     * @brief Converts a floating-point specific gravity to an integer
     *        suitable for the iBeacon minor field.
     *
     * @param SG Specific gravity (e.g. 1.050).
     * @return int16_t SG multiplied by 1000 and truncated (e.g. 1050).
     */
    int16_t calculate_BLE_Trans_Gravity(float);
    std::string BEACON_uuid;
private:
    byte btBuffer[256];
    byte btIndex;
    BLEAdvertising *pAdvertising;
    BLEAdvertisementData oAdvertisementData;
    BLEAdvertisementData oScanResponseData;
    BLEBeacon oBeacon;
    BLEUUID bleUUID;
    float get_Temperature_Fahrenheit(void);
};
#endif