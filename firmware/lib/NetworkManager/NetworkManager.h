/**
 * @file NetworkManager.h
 * @author TjGer22
 * @brief Manages WiFi connections, AP/client mode switching and mDNS.
 * @date 2026
 *
 * @details
 * Declares the NetworkManager class which abstracts all network
 * operations: connecting as a WiFi client, hosting a soft-AP,
 * mDNS advertisement, port probing and OTA-update support.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H
#include <Arduino.h>

class NetworkManager
{
public:
    NetworkManager(void);

    /**
     * @brief Initialises the WiFi stack and connects in the requested mode.
     *
     * @param l_skip_reconnect    If true, only one attempt is made instead of
     *                            retrying up to 15 times.
     * @param Mode                WiFi mode: 0=off, 1=client, 2=AP, 3=AP+client.
     * @param Name                mDNS hostname and OTA device name.
     * @param Client_SSID         SSID of the network to join as a client.
     * @param Client_PASSWORD     Password for the client network.
     * @param Server_SSID         SSID for the soft-AP to broadcast.
     * @param Server_PASSWORD     Password for the soft-AP (empty = open network).
     * @param IP                  Static IP for the AP interface (dotted string).
     * @param GATEWAY             Gateway for the AP interface (dotted string).
     * @param SUBNET              Subnet mask for the AP interface (dotted string).
     * @param l_current_Log_Level Log verbosity level (higher = more output).
     * @return true  Network established without errors.
     * @return false At least one mode could not be started.
     */
    boolean begin(boolean l_skip_reconnect, int8_t Mode, String Name,
                  String Client_SSID, String Client_PASSWORD,
                  String Server_SSID, String Server_PASSWORD,
                  const char *IP, const char *GATEWAY, const char *SUBNET,
                  int8_t l_current_Log_Level);

    /**
     * @brief Checks whether a specific TCP port is open on a remote host.
     *
     * @param URL  Hostname or IP address to probe (max 96 characters).
     * @param Port TCP port number to test.
     * @return true  Host reachable and port is open.
     * @return false WiFi not connected, host unreachable, or port closed.
     */
    boolean PortOpen(const char URL[96], uint16_t Port);

    /**
     * @brief Updates the log verbosity used for serial output.
     *
     * @param l_current_Log_Level New log level.
     */
    void set_current_Log_Level(int8_t l_current_Log_Level);

    /**
     * @brief Pings a remote host to check reachability.
     *
     * @param URL Hostname or IP address (max 96 characters).
     * @return true  Host responded to ICMP ping.
     * @return false WiFi not connected or host did not respond.
     */
    boolean ServerReachable(const char URL[96]);

    /**
     * @brief Returns the current IP address of the station interface.
     *
     * @return IPAddress IPv4 address assigned by DHCP or static config.
     */
    IPAddress getIPAdress(void);

    /**
     * @brief Returns whether the device is currently connected as a WiFi client.
     *
     * @return true  Station interface has an active association.
     * @return false Station interface is not connected.
     */
    boolean ClientModeConnected(void);

    /**
     * @brief Returns whether the configured client SSID was visible during
     *        the last connection attempt.
     *
     * @return true  Target SSID is (or was) in range.
     * @return false Target SSID was not visible.
     */
    boolean getInfoSSIDinRange();

    /**
     * @brief Must be called in the main loop to service the ArduinoOTA handler.
     */
    void loop(void);

private:
    void stop(void);
    void begin_OTA(String name);
    boolean internetConnected(void);
    boolean Server_Start(int Mode, String name, String SSID, String PASSWORD,
                         const char *IP, const char *GATEWAY, const char *SUBNET);
    boolean Set_Wifi_Mode(int8_t Mode);
    boolean Client_Start(int Mode, String name, String SSID, String PASSWORD,
                         const char *IP, const char *GATEWAY, const char *SUBNET);
    boolean Server_Client_Start(int Mode, String name,
                                String AP_SSID, String AP_PASSWORD,
                                String STA_SSID, String STA_PASSWORD,
                                const char *IP, const char *GATEWAY, const char *SUBNET);

    int8_t  mode;             ///< Currently active WiFi mode ID
    boolean Clientstatus;     ///< True when the station interface is connected
    boolean g_skip_reconnect; ///< When true, only one connection attempt is made
    boolean SSIDinRange;      ///< True when the target SSID was visible during last scan
};

extern NetworkManager net;
#endif
