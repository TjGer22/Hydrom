/**
 * @file NetworkManager.cpp
 * @author TjGer22
 * @brief Manages WiFi connections, AP/client mode switching and mDNS.
 * @date 2026
 *
 * @details
 * Implements the NetworkManager class. Handles WiFi client
 * reconnection logic, soft-AP setup, mDNS registration,
 * ArduinoOTA integration and reachability checks.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "NetworkManager.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP32Ping.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "Util.h"

#define DEBUG_PRINT(p, ...) Serial.print(p, ##__VA_ARGS__)
#define DEBUG_PRINTF(p, ...) Serial.printf(p, ##__VA_ARGS__)

/********************************************************************************
 *Local varaibles
 ********************************************************************************/
NetworkManager net;
WiFiClient client;
int8_t Current_Log_Level;

/**
 * @brief Default constructor. Initialises internal state to defaults.
 */
NetworkManager::NetworkManager(void) {
}
/**
 * @brief Updates the log verbosity level used for all serial output.
 *
 * @param l_current_Log_Level New log level (higher = more verbose).
 */
void NetworkManager::set_current_Log_Level(int8_t l_current_Log_Level) {
    Current_Log_Level=l_current_Log_Level;
}

/**
 * @brief Configures and starts the ArduinoOTA service.
 *        Sets the OTA port, hostname, password and event callbacks.
 *
 * @param name OTA hostname used to identify the device on the network.
 */
void NetworkManager::begin_OTA(String name) {
    // Port defaults to 3232
    ArduinoOTA.setPort(3232);

    // Hostname defaults to esp3232-[MAC]
    ArduinoOTA.setHostname(name.c_str());

    // No authentication by default
    ArduinoOTA.setPassword("uDVvmWYslcvl9XGD25sdRkSDUPB09oqAc00YSUMFt5eIMal");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    ArduinoOTA.setPasswordHash("b265fd52ec8d8c403c2bddb7e15a0a40");

    ArduinoOTA
        .onStart([]() {
            String type;
            if(ArduinoOTA.getCommand() == U_FLASH)
                type = "Firmware";
            else    // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Print_Info(4, Current_Log_Level, "Start uploading " + type);
        })
        .onEnd([]() {
            Print_Info(4, Current_Log_Level, "Uploading ended");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            Print_Info(4, Current_Log_Level, "Upload Progress: " + (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Print_Error( "[" + String(error) + "]:");
            if(error == OTA_AUTH_ERROR)
                Print_Error( "Auth Failed\n");
            else if(error == OTA_BEGIN_ERROR)
                Print_Error( "Begin Failed\n");
            else if(error == OTA_CONNECT_ERROR)
                Print_Error( "Connect Failed\n");
            else if(error == OTA_RECEIVE_ERROR)
                Print_Error( "Receive Failed\n");
            else if(error == OTA_END_ERROR)
                Print_Error( "End Failed\n");
        });
    ArduinoOTA.begin();
}

/**
 * @brief Connects to an existing WiFi network as a station (client).
 *        Retries up to 15 times unless skip_reconnect is set.
 *
 * @param Mode     ESP WiFi mode constant (WIFI_MODE_STA or WIFI_MODE_APSTA).
 * @param name     Hostname to set before connecting.
 * @param SSID     Target network SSID.
 * @param PASSWORD Target network password.
 * @param IP       Static IP (dotted string).
 * @param GATEWAY  Gateway address (dotted string).
 * @param SUBNET   Subnet mask (dotted string).
 * @return true  Connection established and an IP address was obtained.
 * @return false Connection failed after all retries.
 */
boolean NetworkManager::Client_Start(int Mode, String name, String SSID, String PASSWORD, const char * IP, const char * GATEWAY, const char * SUBNET) {
    int Circuit=0;
    do{
        Circuit++;
        Clientstatus=false;
        Set_Wifi_Mode(Mode);

        //Set Hostname; Important to set this before begin.
        WiFi.setHostname(name.c_str());
        WiFi.begin(SSID.c_str(), PASSWORD.c_str());

        // Wait for connection
        Print_Info(4, Current_Log_Level, "Waiting for WIFI Connection.");
        uint32_t start = millis();
        uint32_t timeout = 0;
        IPAddress zero(0, 0, 0, 0);

        while(WiFi.status() != WL_CONNECTED ||  WiFi.localIP()!=zero) {
            delay(100);

            if(WiFi.status() != WL_CONNECTED){
                DEBUG_PRINT("1");
            }else{
                if(WiFi.localIP()==zero){
                    DEBUG_PRINT("2");
                }else{
                    break;
                }
            }
            timeout = millis() - start;
            if(timeout > 10000) {    // wait a maximum 10s
                break;
            }
        }
        DEBUG_PRINT("\n");

        if(WiFi.status() != WL_CONNECTED ||WiFi.localIP()==zero) {
            Print_Error( "Failed to connected to WIFI: " + SSID+ " and Password "+PASSWORD + " Timeout: "+String(timeout));
            
            if(WiFi.status()==WL_NO_SSID_AVAIL){
                Print_Error( "No WIFI with SSID " + SSID+ " in range");
                SSIDinRange=false;
            }else{
                Print_Info(4, Current_Log_Level, "WIFI " + SSID+ " in range");
                SSIDinRange=true;
            }

            if(WiFi.status()!=WL_NO_SSID_AVAIL){
                Print_Error( "Fehlercode: "+String(WiFi.status()));
                SSIDinRange=false;
            }

            Clientstatus=false;
        }else{
            Print_Info(4, Current_Log_Level, "Connected to WIFI: " + SSID + "; Local IP: " + WiFi.localIP().toString()+ " and Password "+PASSWORD );
            Clientstatus=true;
        }

  
        if(Circuit>15){         // 10 times to try
            Print_Error( "That's it, I'm done!");
            break;
        }
        if(Circuit>1){ //
            Print_Error( "That was failed attempt number: " + String(Circuit));
        }
        if(g_skip_reconnect) {
            break;
        }
    }while(!Clientstatus);

    if(Clientstatus){
        return true;
    }else{
        return false;
    }
        
}
/**
 * @brief Starts both AP and client interfaces simultaneously (APSTA mode).
 *        Handles the case where one interface was already active.
 *
 * @param Mode         ESP WiFi mode constant (WIFI_MODE_APSTA).
 * @param name         Hostname.
 * @param AP_SSID      SSID to broadcast as soft-AP.
 * @param AP_PASSWORD  Password for the soft-AP.
 * @param STA_SSID     SSID to join as a client.
 * @param STA_PASSWORD Password for the client network.
 * @param IP           Static IP for the AP interface (dotted string).
 * @param GATEWAY      Gateway (dotted string).
 * @param SUBNET       Subnet mask (dotted string).
 * @return true  Both interfaces configured without error.
 * @return false STA SSID was empty or an interface could not be started.
 */
boolean NetworkManager::Server_Client_Start(int Mode, String name, String AP_SSID, String AP_PASSWORD,String STA_SSID, String STA_PASSWORD, const char * IP, const char * GATEWAY, const char * SUBNET) {
    Print_Info(4, Current_Log_Level, "Configure Wifi to STA AP Mode");      
    int Mode_old=0;
    if(WiFi.getMode()==WIFI_MODE_STA){
        Mode_old=1;
        Print_Info(4, Current_Log_Level, "Wifi-Mode was set to WiFi station + soft-AP mode");
    }else if(WiFi.getMode()==WIFI_MODE_AP){
        Mode_old=2;
        Print_Info(4, Current_Log_Level, "Wifi-Mode was set to WiFi station + soft-AP mode");
    }else if(WiFi.getMode()==WIFI_MODE_APSTA){
        Mode_old=3;
        Print_Info(4, Current_Log_Level, "Wifi-Mode was set to WiFi station + soft-AP mode");
    }else{
        Print_Info(4, Current_Log_Level, "No old Wifi mode fits");
    }

    
 
    if(strlen(STA_SSID.c_str()) == 0) {
        Print_Error("WIFI Station Settings not set!");
        return false;
    }

    
    WiFi.mode(WIFI_MODE_APSTA);
    if(WiFi.getMode()==WIFI_MODE_APSTA){
        Print_Info(4, Current_Log_Level, "Wifi-Mode was set to WiFi station + soft-AP mode");
    }else{
        Print_Error("Failed to set Mode to WiFi station + soft-AP");
    }

    char ip[20];
    StringCopyConst(ip, IP);
    IPAddress local_ip(StringToIp(ip));
    StringCopyConst(ip, GATEWAY);
    IPAddress gateway(StringToIp(ip));
    StringCopyConst(ip, SUBNET);
    IPAddress subnet(StringToIp(ip));

switch (Mode_old)
{
case 0:
    if(!WiFi.softAPConfig(local_ip, gateway, subnet)) {
        Print_Error( "could not configure IP: " + String(local_ip) + ", Gateway: " + String(gateway) + ", Subnet: " + String(subnet));
    }
    Print_Info(4, Current_Log_Level, "SoftAP was Configured");

    //Set Hostname; Important to set this before begin.
    WiFi.setHostname(name.c_str());
    if(!WiFi.begin(STA_SSID.c_str(), STA_PASSWORD.c_str())) {
        Print_Error( "Failed to start STA Wifi " + STA_SSID);
        return false;
    } else {
        Print_Info(4, Current_Log_Level, "Wifi STA_SSID: " + STA_SSID + "  IP: " + WiFi.localIP().toString());
        return true;
    }
    if(!WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD.c_str())) {
        Print_Error( "Failed to start AP Wifi " + AP_SSID);
        return false;
    } else {
        Print_Info(4, Current_Log_Level, "Wifi AP_SSID: " + AP_SSID + "  Local IP: " + WiFi.softAPIP().toString());
        return true;
    }
    break;
case 1://STA-Mode was old Mode so this doesn´t need to bei configured again.
    Print_Info(4, Current_Log_Level, "STA-Mode does not need to be set");
    if(!WiFi.softAPConfig(local_ip, gateway, subnet)) {
        Print_Error( "could not configure IP: " + String(local_ip) + ", Gateway: " + String(GATEWAY) + ", Subnet: " + String(SUBNET));
    }else{
        Print_Info(4, Current_Log_Level, "SoftAP was Configured");
    }

    if(!WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD.c_str())) {
        Print_Error( "Failed to start AP Wifi " + AP_SSID);
    } else {
        Print_Info(4, Current_Log_Level, "Wifi AP_SSID: " + AP_SSID + "  Local IP: " + WiFi.softAPIP().toString());
    }
    break;
case 2://SOFT AP-Mode was old Mode so this doesn´t need to bei configured again.
    Print_Info(4, Current_Log_Level, "SoftAP does not need to be set");

    //Set Hostname; Important to set this before begin.
    WiFi.setHostname(name.c_str());
    if(!WiFi.begin(STA_SSID.c_str(), STA_PASSWORD.c_str())) {
        Print_Error( "Failed to start STA Wifi " + STA_SSID);
    } else {
        Print_Info(4, Current_Log_Level, "Wifi STA_SSID: " + STA_SSID + "  IP: " + WiFi.localIP().toString());
    }
    break;
case 3:
    if(!WiFi.softAPConfig(local_ip, gateway, subnet)) {
        Print_Error( "could not configure IP: " + String(local_ip) + ", Gateway: " + String(GATEWAY) + ", Subnet: " + String(SUBNET));
    }
    Print_Info(4, Current_Log_Level, "SoftAP was Configured");

    //Set Hostname; Important to set this before begin. 
    WiFi.setHostname(name.c_str());
    if(!WiFi.begin(STA_SSID.c_str(), STA_PASSWORD.c_str())) {
        Print_Error( "Failed to start STA Wifi " + STA_SSID);
    } else {
        Print_Info(4, Current_Log_Level, "Wifi STA_SSID: " + STA_SSID + "  IP: " + WiFi.localIP().toString());
    }
    if(!WiFi.softAP(AP_SSID.c_str(), AP_PASSWORD.c_str())) {
        Print_Error( "Failed to start AP Wifi " + AP_SSID);
    } else {
        Print_Info(4, Current_Log_Level, "Wifi AP_SSID: " + AP_SSID + "  Local IP: " + WiFi.softAPIP().toString());
    }
    break;
default:
    break;
}

    // Wait for connection
    Print_Info(4, Current_Log_Level, "Waiting for WIFI Connection.");
    uint32_t start = millis();
    uint32_t timeout = 0;
    while(WiFi.status() != WL_CONNECTED) {
        delay(100);
        DEBUG_PRINT(".");
        timeout = millis() - start;
        if(timeout > 10000) {    // wait a maximum 10s
            break;
        }
    }
    DEBUG_PRINT("\n");

    if(WiFi.status() != WL_CONNECTED) {
        Print_Error( "Failed to connect to WIFI: " + STA_SSID);
        return false;
    }

    return true;
}


/**
 * @brief Sets the active WiFi radio mode.
 *
 * @param Mode 0=off, 1=STA (client), 2=AP (server), 3=APSTA (both).
 * @return true  Mode was applied successfully.
 * @return false Mode value was not recognised.
 */
boolean NetworkManager::Set_Wifi_Mode(int8_t Mode) {
    if(mode != Mode) {
        Print_Info(4, Current_Log_Level, "Wifi-Mode ID changed from " + String(mode) + " to " + String(Mode));
        mode = Mode;
    } else {
        Print_Info(6, Current_Log_Level, "Wifi-Mode ID is still " + String(Mode));
    }

    switch(mode) {
        case 0: /**< null mode */
            Print_Info(4, Current_Log_Level, "Wifi-Mode was set to NULL");
            WiFi.mode(WIFI_MODE_NULL);
            return true;
        case 1: /**< WiFi station mode */
            Print_Info(4, Current_Log_Level, "Wifi-Mode was set to WiFi station mode");
            WiFi.mode(WIFI_MODE_STA);
            return true;
        case 2: /**< WiFi soft-AP mode */
            Print_Info(4, Current_Log_Level, "Wifi-Mode was set to WiFi soft-AP mode");
            WiFi.mode(WIFI_MODE_AP);
            return true;
        case 3: /**< WiFi station + soft-AP mode */
            Print_Info(4, Current_Log_Level, "Wifi-Mode was set to WiFi station + soft-AP mode");
            WiFi.mode(WIFI_MODE_APSTA);
            return true;

        default:
            Print_Error( "UNDEFINDED Wifi Status");

            return false;
    }
}

/**
 * @brief Services the ArduinoOTA handler. Must be called in the main loop.
 */
void NetworkManager::loop(void) {
    ArduinoOTA.handle();
}

/**
 * @brief Checks whether the device has internet access via ICMP ping to google.com.
 *
 * @return true  Internet is reachable.
 * @return false WiFi not connected or ping failed.
 */
boolean NetworkManager::internetConnected(void) {
    if(!WiFi.isConnected()) {
        Print_Info(4, Current_Log_Level, "Internettest: Wifi SSID(" + String(WiFi.SSID()) + ") Password(" + String(WiFi.psk()) + ")not connected!");
        return false;
    } else {
        Print_Info(4, Current_Log_Level, "Internettest: Wifi SSID(" + String(WiFi.SSID()) + ") Password(" + String(WiFi.psk()) + ") connected!");
        if(Ping.ping("www.google.com", 3)) {
            Print_Info(4, Current_Log_Level, "Internettest: Internet connected!");
            return true;
        } else {
            Print_Info(4, Current_Log_Level, "Internettest: Internet not connected!");
            return false;
        }
    }
}
/**
 * @brief Initialises the WiFi stack and brings up the requested mode.
 *
 * @param l_skip_reconnect    If true, only one attempt is made per interface.
 * @param Mode                0=off, 1=client, 2=AP, 3=AP+client.
 * @param Name                mDNS hostname / OTA device name.
 * @param Client_SSID         SSID to join as a client.
 * @param Client_PASSWORD     Password for the client network.
 * @param Server_SSID         SSID to broadcast as soft-AP.
 * @param Server_PASSWORD     Password for the soft-AP (empty = open).
 * @param IP                  Static IP for the AP interface (dotted string).
 * @param GATEWAY             Gateway (dotted string).
 * @param SUBNET              Subnet mask (dotted string).
 * @param l_current_Log_Level Log verbosity level.
 * @return true  All requested interfaces were started.
 * @return false At least one interface could not be started.
 */
boolean NetworkManager::begin(boolean l_skip_reconnect,int8_t Mode,String Name,String Client_SSID, String Client_PASSWORD,String Server_SSID, String Server_PASSWORD, const char * IP, const char * GATEWAY, const char * SUBNET,int8_t l_current_Log_Level) {
    boolean err = 0;
    g_skip_reconnect=l_skip_reconnect;

    int Mode_old=0;
    if(WiFi.getMode()==WIFI_MODE_STA){
        Mode_old=1;
        Print_Info(4, Current_Log_Level, "WIFI_MODE_STA ist OLD Wifi Mode");
    }else if(WiFi.getMode()==WIFI_MODE_AP){
        Mode_old=2;
        Print_Info(4, Current_Log_Level, "WIFI_MODE_AP ist OLD Wifi Mode");
    }else if(WiFi.getMode()==WIFI_MODE_APSTA){
        Mode_old=3;
        Print_Info(4, Current_Log_Level, "WIFI_MODE_APSTA ist OLD Wifi Mode");
    }else{
        Print_Info(4, Current_Log_Level, "No old Wifi mode fits");
    }
    Current_Log_Level= l_current_Log_Level;
    switch(Mode) {
        case 0: /**< null mode */
                Print_Info(4, Current_Log_Level, "Wifi-Mode was set to NULL");
                WiFi.mode(WIFI_MODE_NULL);
            break;
        case 1: /**< WiFi station mode */
                if(!Client_Start(WIFI_MODE_STA, Name, Client_SSID, Client_PASSWORD, IP, GATEWAY, SUBNET))
                    err++;
            break;
        case 2: /**< WiFi soft-AP mode */
                if(!Server_Start(WIFI_MODE_AP, Name, Server_SSID, Server_PASSWORD, IP, GATEWAY, SUBNET))
                    err++;
            break;
        case 3: /**< WiFi station + soft-AP mode */
                //if(!Server_Client_Start(WIFI_MODE_APSTA, Name, Server_SSID, Server_PASSWORD,Client_SSID, Client_PASSWORD, IP, GATEWAY, SUBNET))
                switch (Mode_old)
                {
                case 0:
                    if(!Client_Start(WIFI_MODE_APSTA, Name, Client_SSID, Client_PASSWORD, IP, GATEWAY, SUBNET))
                        err++;
                    if(!Server_Start(WIFI_MODE_APSTA, Name, Server_SSID, Server_PASSWORD, IP, GATEWAY, SUBNET))
                        err++;
                    break;
                case 1:
                    if(!Server_Start(WIFI_MODE_APSTA, Name, Server_SSID, Server_PASSWORD, IP, GATEWAY, SUBNET))
                        err++;
                    break;
                case 2:
                    if(!Client_Start(WIFI_MODE_APSTA, Name, Client_SSID, Client_PASSWORD, IP, GATEWAY, SUBNET))
                        err++;
                    break;
                case 3:
                    if(!Client_Start(WIFI_MODE_APSTA, Name, Client_SSID, Client_PASSWORD, IP, GATEWAY, SUBNET))
                        err++;
                    
                    if(!Server_Start(WIFI_MODE_APSTA, Name, Server_SSID, Server_PASSWORD, IP, GATEWAY, SUBNET))
                        err++;
                    break;                
                default:
                    break;
                }
            break;
        default:
                Print_Error( "UNDEFINDED Wifi Status");
                err++;
            break;
    }
    //Print_Info(6, Current_Log_Level, "Irgendwas geht hier immer kaputt: "+ Name);
    //begin_OTA(Name);

    if(err == 0) {
        Print_Info(6, Current_Log_Level, "Network was successfully Established.");
        return true;
    }
    Print_Error( "Something went wrong when setting up the network connection.");
    return false;
}
/**
 * @brief Starts a WiFi soft-AP with the given credentials and static IP.
 *
 * @param Mode     ESP WiFi mode constant (WIFI_MODE_AP or WIFI_MODE_APSTA).
 * @param name     Hostname to advertise.
 * @param SSID     SSID to broadcast.
 * @param PASSWORD AP password (empty string = open network).
 * @param IP       Static IP for the AP interface (dotted string).
 * @param GATEWAY  Gateway (dotted string).
 * @param SUBNET   Subnet mask (dotted string).
 * @return true  Access point started successfully.
 * @return false Failed to configure or start the soft-AP.
 */
boolean NetworkManager::Server_Start(int Mode, String name, String SSID, String PASSWORD, const char * IP, const char * GATEWAY, const char * SUBNET) {
    Print_Info(4, Current_Log_Level, "Network Mac-Adress: " + WiFi.macAddress());

    char ip[20];
        StringCopyConst(ip, IP);
        IPAddress local_ip(StringToIp(ip));
        StringCopyConst(ip, GATEWAY);
        IPAddress gateway(StringToIp(ip));
        StringCopyConst(ip, SUBNET);
        IPAddress subnet(StringToIp(ip));

    // set the rest of the wifi and IP settings
    Print_Info(4, Current_Log_Level, "Starting WIFI Access Point: " + SSID);

    if(!WiFi.softAPConfig(local_ip, gateway, subnet)) {
        Print_Error( "could not configure IP: " + String(local_ip) + ", Gateway: " + String(gateway) + ", Subnet: " + String(subnet));
    }
    Print_Info(4, Current_Log_Level, "SoftAP was Configured");
    Set_Wifi_Mode(Mode);
    WiFi.setHostname(name.c_str());
    if(PASSWORD==""){
            if(!WiFi.softAP(SSID.c_str())) {
        Print_Error( "Failed to start AP Wifi " + SSID+" without Password");
        return false;
    } else {
        Print_Info(4, Current_Log_Level, "Wifi SSID: " + SSID + "  Local IP: " + WiFi.softAPIP().toString()+" was created without Password");
        return true;
    }
    }else{
    if(!WiFi.softAP(SSID.c_str(), PASSWORD.c_str())) {
        Print_Error( "Failed to start AP Wifi " + SSID+" with Password "+PASSWORD);
        return false;
    } else {
        Print_Info(4, Current_Log_Level, "Wifi SSID: " + SSID + "  Local IP: " + WiFi.softAPIP().toString()+" with Password "+PASSWORD);
        return true;
    }
    }
}
/**
 * @brief Pings a remote host to check reachability.
 *
 * @param URL Hostname or IP address to ping (max 96 characters).
 * @return true  Host responded to the ICMP ping.
 * @return false WiFi is not connected or the host did not respond.
 */
boolean NetworkManager::ServerReachable(const char URL[96]) {
    if(!WiFi.isConnected()) {
        Print_Info(4, Current_Log_Level, "Reachabletest: Wifi SSID("+String(WiFi.SSID())+") Password("+String(WiFi.psk())+") not connected!");
        return false;
    } else {
        Print_Info(4, Current_Log_Level, "Reachabletest: Wifi SSID("+String(WiFi.SSID())+") Password("+String(WiFi.psk())+") connected!");
        if(Ping.ping(URL, 3)) {
            Print_Info(4, Current_Log_Level, "Reachabletest: Server "+String(URL)+" reachable!");
            return true;
        } else {
            Print_Info(4, Current_Log_Level, "Reachabletest: Server "+String(URL)+" not reachable!");
            return false;
        }
    }
}


/**
 * @brief Checks whether a specific TCP port is open on a remote host.
 *
 * @param URL  Hostname or IP address to probe.
 * @param Port TCP port number to test.
 * @return true  Host is reachable and the port is open.
 * @return false WiFi not connected, host unreachable, or port closed.
 */
boolean NetworkManager::PortOpen(const char URL[15], uint16_t Port) {
    if(!WiFi.isConnected()) {
        Print_Info(4, Current_Log_Level, "Porttest: Wifi not connected!");
        return false;
    } else {
        if(!ServerReachable(URL)) {
            return false;
        } else {
            Print_Info(4, Current_Log_Level, "Porttest: Wifi connected!");
            if(client.connect(URL, Port)) {
                Print_Info(4, Current_Log_Level, "Porttest: Port open!");
                return true;
            } else {
                Print_Info(4, Current_Log_Level, "Porttest: Port closed!");
                return false;
            }
        }
    }
}

/**
 * @brief Returns the current IP address of the station interface.
 *
 * @return IPAddress IPv4 address of the active station connection.
 */
IPAddress NetworkManager::getIPAdress(void){
    return WiFi.localIP();
}

/**
 * @brief Returns whether the device is currently connected as a WiFi client.
 *
 * @return true  Station interface has an active association.
 * @return false Station interface is not connected.
 */
boolean NetworkManager::ClientModeConnected(void){
return Clientstatus;
}

/**
 * @brief Returns whether the configured client SSID was visible during
 *        the last connection attempt.
 *
 * @return true  Target SSID was in range.
 * @return false Target SSID was not visible.
 */
boolean NetworkManager::getInfoSSIDinRange(void){
 return SSIDinRange;
}