/**
 * @file ServiceManager.h
 * @author TjGer22
 * @brief Transmits measurement data to external fermentation-tracking services.
 * @date 2026
 *
 * @details
 * Declares the ServiceManager class which provides methods for
 * sending gravity and temperature readings to Brewfather, Brewblox,
 * iSpindel HTTP, Grainfather, MQTT, Telegram, Ubidots and more.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef SERVICEMANAGER_H
#define SERVICEMANAGER_H

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>


class ServiceManager
{
public:
  /** @brief Default constructor. Initialises the MQTT and WiFi client objects. */
  ServiceManager();
  /**
   * @brief Sends tilt and temperature data to a Brewblox MQTT endpoint.
   *
   * @param server   Hostname or IP of the Brewblox MQTT broker.
   * @param port     MQTT broker port.
   * @param topic    MQTT topic to publish to.
   * @param username MQTT username.
   * @param password MQTT password.
   * @return true  Data published successfully.
   * @return false MQTT connection failed.
   */
  bool sendBrewblox(String server, uint16_t port, String topic, String username, String password);

  /**
   * @brief Sends data to the Brewfather logging endpoint via HTTP POST.
   *
   * @param gravityUnit true = send gravity in SG, false = send in Plato.
   * @param server      Target server hostname or IP.
   * @param port        Target port.
   * @param uri         Request URI path.
   */
  void sendBrewfather(boolean gravityUnit, String server, uint16_t port, String uri);
  /**
   * @brief Sends measurement data to BierBot and returns the requested next polling interval.
   *
   * @param server           BierBot API server hostname.
   * @param Token            API token.
   * @param URI              API endpoint path.
   * @param ID               Chip/device ID string.
   * @param l_Firmwareversion Currently running firmware version string.
   * @return uint32_t Next request interval in milliseconds as specified by the server response.
   */
  uint32_t sendBierBot(String server, String Token, String URI, String ID, String l_Firmwareversion);
  /**
   * @brief Sends tilt and temperature data to CraftbeerPi via HTTP POST.
   *
   * @param server Target server hostname or IP.
   * @param port   Target port.
   * @param uri    Request URI path.
   */
  void sendCraftbeerpi(String server, uint16_t port, String uri);
  /**
   * @brief Sends specific gravity and temperature to Grainfather via HTTP POST.
   *
   * @param server Target server hostname or IP.
   * @param port   Target port.
   * @param uri    Request URI path.
   */
  void sendGrainfather(String server, uint16_t port, String uri);
  /**
   * @brief Sends sensor readings to FHEM via HTTP GET command.
   *
   * @param server FHEM server hostname or IP.
   * @param port   FHEM HTTP port.
   * @return true  Data sent successfully.
   * @return false Connection to FHEM failed.
   */
  bool sendFHEM(String server, uint16_t port);
  /**
   * @brief Sends the current internal JSON document as an HTTP POST request.
   *
   * @param server Target server hostname or IP.
   * @param uri    Request URI path.
   * @param port   Target port (default: 80).
   * @return true  Request dispatched (always true; HTTP errors are logged).
   * @return false Unused.
   */
  bool sendGenericPost(String server, String uri, uint16_t port = 80);
  /**
   * @brief Writes measurement data to an InfluxDB instance.
   *
   * @param server          InfluxDB server hostname or IP.
   * @param port            InfluxDB HTTP port.
   * @param db              Database name.
   * @param username        InfluxDB username (empty string = no auth).
   * @param password        InfluxDB password.
   * @param Measurementsname Source/measurement tag value.
   * @return true  Data written (always true; errors are logged).
   * @return false Unused.
   */
  bool sendInfluxDB(String server, uint16_t port, String db, String username, String password, String  Measurementsname);
  /**
   * @brief Publishes all sensor readings as individual key-value pairs to an MQTT broker.
   *
   * @param server     MQTT broker hostname or IP.
   * @param port       MQTT broker port.
   * @param username   MQTT username (empty string = no auth).
   * @param password   MQTT password.
   * @param TopicLevel Base topic path; data is published to TopicLevel/DeviceName/key.
   * @return true  All values published successfully.
   * @return false Connection to the broker failed.
   */
  bool sendMQTT(String server, uint16_t port, String username, String password, String TopicLevel);
  /**
   * @brief Pushes gauge metrics to a Prometheus Pushgateway.
   *
   * @param server   Pushgateway server hostname or IP.
   * @param port     Pushgateway HTTP port.
   * @param job      Prometheus job label.
   * @param instance Prometheus instance label.
   * @return true  Metrics pushed (always true; errors are logged).
   * @return false Unused.
   */
  bool sendPrometheus(String server, uint16_t port, String job, String instance);
  /**
   * @brief Sends sensor readings to TControl via TCP.
   *
   * @param server TControl server hostname or IP.
   * @param port   TControl TCP port.
   * @return true  Data sent (always true; errors are logged).
   * @return false Unused.
   */
  bool sendTCONTROL(String server, uint16_t port);
  /**
   * @brief Sends a formatted measurement message via Telegram Bot API.
   *
   * @param Unit_Temperature Temperature unit: 0 = Celsius, 1 = Fahrenheit.
   * @param Unit_Tilt        Tilt/gravity unit: 0 = Plato, 1 = SG.
   * @param Token            Telegram Bot API token.
   * @param Chat_ID          Target Telegram chat ID.
   * @return true  Message sent successfully.
   * @return false Telegram API reported an error.
   */
  bool sendTelegram(uint8_t Unit_Temperature,uint8_t Unit_Tilt, String Token, String Chat_ID);
  /**
   * @brief Sends sensor data as a JSON payload over a raw TCP connection.
   *
   * @param server Target server hostname or IP.
   * @param port   Target port (default: 80).
   * @return String Server response string, or an empty string on failure.
   */
  String sendTCP(String server, uint16_t port = 80);
  /**
   * @brief Sends sensor data to a custom HTTP endpoint, substituting URL placeholders.
   *
   * @param server Target server hostname or IP.
   * @param port   Target port.
   * @param uri    URI template with optional {VALUE_*} placeholders.
   */
  void sendHttp(String server, uint16_t port, char uri[265]);
  /**
   * @brief Sends sensor readings to a ThingSpeak channel.
   *
   * @param token   ThingSpeak write API key.
   * @param Channel ThingSpeak channel number.
   * @return true  Data written successfully.
   * @return false Transmission failed.
   */
  bool sendThingSpeak(String token, long Channel);
  /**
   * @brief Sends sensor readings to a Ubidots device via HTTP.
   *
   * @param Unit_Temperature Temperature unit: 0 = Celsius, 1 = Fahrenheit.
   * @param Unit_Tilt        Tilt unit: 0 = Plato, 1 = SG.
   * @param server           Ubidots API server hostname.
   * @param port             Ubidots API port.
   * @param token            Ubidots device token.
   * @return true  Data sent successfully.
   * @return false Transmission failed.
   */
  bool sendUbidots(uint8_t Unit_Temperature,uint8_t Unit_Tilt,const char *server, uint16_t port, String token);
  /**
   * @brief Sends sensor readings to a Google Sheets Apps Script endpoint.
   *
   * @param server Google Script server hostname.
   * @param port   HTTPS port (typically 443).
   * @param token  Google Apps Script deployment token.
   * @return true  Data POSTed successfully.
   * @return false HTTPS connection or POST failed.
   */
  bool sendGoogleSheets(const char *server, uint16_t port, String token);
  /**
   * @brief Sends the current internal JSON document via an HTTPS POST request.
   *
   * @param server      Target server hostname.
   * @param l_uri       Full URI path for the POST.
   * @param port        HTTPS port.
   * @param certificate PEM-encoded CA certificate for TLS verification.
   * @return true  Request completed with HTTP 200.
   * @return false Connection failed or server returned an error code.
   */
  bool sendGenericSecurePost(String server, String l_uri, uint16_t port, const char * certificate);

  /**
   * @brief Expands {VALUE_*} placeholders in a URL template with current sensor values.
   *
   * @param _url URL template string containing optional placeholders.
   * @return String Expanded URL with all recognised placeholders replaced.
   */
  String Replace_Placeholder(char _url[265]);
  /**
   * @brief Disconnects and releases both the plain and TLS WiFi client connections.
   */
  void stop(void);
  /**
   * @brief Caches the latest sensor readings and device parameters for use by all send methods.
   *
   * @param l_Devicename       Device display name.
   * @param l_UUID_ID          BLE UUID colour index.
   * @param l_gravity          Raw tilt angle in degrees.
   * @param l_plato            Calculated wort density in degrees Plato.
   * @param l_specific_gravity Calculated specific gravity.
   * @param l_temperature_c    Temperature in degrees Celsius.
   * @param l_temperature_f    Temperature in degrees Fahrenheit.
   * @param l_temperature_k    Temperature in Kelvin.
   * @param l_BatteryVoltage   Battery voltage in volts.
   * @param l_batteryPercentage Battery charge level in percent.
   * @param l_interval         Measurement interval in seconds.
   * @param l_RSSI             WiFi RSSI in dBm.
   * @param l_Conn_Timeout     TCP connection timeout in milliseconds.
   * @param l_current_Log_Level Active log verbosity level.
   */
  void update(String l_Devicename, int8_t l_UUID_ID, float l_gravity, float l_plato, float l_specific_gravity, float l_temperature_c, float l_temperature_f, float l_temperature_k, float l_BatteryVoltage,int8_t l_batteryPercentage, uint32_t l_interval, int8_t l_RSSI,int16_t l_Conn_Timeout,int8_t l_current_Log_Level);


  /**
   * @brief Adds a float field to the outgoing JSON document.
   * @param id    JSON key name.
   * @param value Float value to store.
   */
  void add(String id, float value);
  /**
   * @brief Adds a string field to the outgoing JSON document.
   * @param id    JSON key name.
   * @param value String value to store.
   */
  void add(String id, String value);
  /**
   * @brief Adds a signed 32-bit integer field to the outgoing JSON document.
   * @param id    JSON key name.
   * @param value Integer value to store.
   */
  void add(String id, int32_t value);
  void add(String id, uint32_t value);
  /**
   * @brief Callback invoked by PubSubClient when an MQTT message arrives on a subscribed topic.
   *
   * @param topic   C-string containing the topic name.
   * @param payload Pointer to the raw message payload bytes.
   * @param length  Number of bytes in @p payload.
   */
  void mqttCallback(char *topic, byte *payload, unsigned int length);
  /**
   * @brief Establishes a connection to an MQTT broker.
   *
   * @param server     MQTT broker hostname or IP.
   * @param port       MQTT broker port.
   * @param name       MQTT client identifier.
   * @param username   MQTT username (empty string = no auth).
   * @param password   MQTT password.
   * @param secure     If true use TLS (currently unused; kept for future use).
   * @param CACert     PEM CA certificate for TLS (currently unused).
   * @param deviceCert PEM device certificate for mTLS (currently unused).
   * @param deviceKey  PEM private key for mTLS (currently unused).
   * @return true  Connected successfully.
   * @return false Connection failed; error code is logged.
   */
  bool mqttConnect(const String &server, uint16_t port, const String &name, const String &username, const String &password, const bool secure = false, const char CACert[] = "", const char deviceCert[] = "", const char deviceKey[] = "");
  // ~ServiceManager();
int8_t current_Log_Level;
private:
  WiFiClient _client;
  PubSubClient _mqttClient;
  StaticJsonDocument<512> _doc;
  WiFiClientSecure _secureClient;
  /**
   * @brief Converts a temperature unit enum value to its abbreviation string.
   *
   * @param Temperature_Unit Unit index: 0 = "C", 1 = "F", 2 = "K".
   * @return String Abbreviation string, or "99" for unknown values.
   */
  String get_Temperature_Unit(int8_t Temperature_Unit);

String Devicename;
int8_t UUID_ID;
float gravity;
float plato;
float specific_gravity; 
float temperature_c;
float temperature_f; 
float temperature_k;
float BatteryVoltage; 
uint32_t interval; 
int8_t RSSI;
int8_t batteryPercentage;
int16_t Conn_Timeout;

};

#endif