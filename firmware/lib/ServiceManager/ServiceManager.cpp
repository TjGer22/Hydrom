/**
 * @file ServiceManager.cpp
 * @author TjGer22
 * @brief Transmits measurement data to external fermentation-tracking services.
 * @date 2026
 *
 * @details
 * Implements the ServiceManager class. Each send-method serialises
 * the current Hydrom measurement into the format expected by the
 * respective target service and dispatches the HTTP/MQTT request.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// The ServiceManager class provides functionality for interacting with MQTT and TCP services,
// as well as managing the data being sent to these services.

#include "ServiceManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

#include "Util.h"
#include "google_cert.h"

/**
 * @brief Default constructor.
 */
ServiceManager::ServiceManager() {
    // Default constructor. Nothing to do here.
}

/**
 * @brief Stores a float value in the internal JSON document under the given key.
 * @param id    JSON key name.
 * @param value Float value to store.
 */
void ServiceManager::add(String id, float value) {
    _doc[id] = value;
}
/**
 * @brief Stores a string value in the internal JSON document under the given key.
 * @param id    JSON key name.
 * @param value String value to store.
 */
void ServiceManager::add(String id, String value) {
    _doc[id] = value;
}
/**
 * @brief Stores an unsigned 32-bit integer in the internal JSON document under the given key.
 * @param id    JSON key name.
 * @param value Unsigned integer value to store.
 */
void ServiceManager::add(String id, uint32_t value) {
    _doc[id] = value;
}
/**
 * @brief Stores a signed 32-bit integer in the internal JSON document under the given key.
 * @param id    JSON key name.
 * @param value Signed integer value to store.
 */
void ServiceManager::add(String id, int32_t value) {
    _doc[id] = value;
}

/**
 * @brief Disconnects and releases both the plain and TLS WiFi client connections.
 */
void ServiceManager::stop() {
    Print_Info(5, current_Log_Level, "Stopping Networkclient");
    _client.stop();
    Print_Info(5, current_Log_Level, "Stopping secure Networkclient");
    _secureClient.stop();
    Print_Info(5, current_Log_Level, "Networkclients stopped");
}

/**
 * @brief Caches the latest sensor readings and device parameters for use by all send methods.
 *
 * @param l_Devicename        Device display name.
 * @param l_UUID_ID           BLE UUID colour index.
 * @param l_gravity           Raw tilt angle in degrees.
 * @param l_plato             Calculated wort density in degrees Plato.
 * @param l_specific_gravity  Calculated specific gravity.
 * @param l_temperature_c     Temperature in degrees Celsius.
 * @param l_temperature_f     Temperature in degrees Fahrenheit.
 * @param l_temperature_k     Temperature in Kelvin.
 * @param l_BatteryVoltage    Battery voltage in volts.
 * @param l_batteryPercentage Battery charge level in percent.
 * @param l_interval          Measurement interval in seconds.
 * @param l_RSSI              WiFi RSSI in dBm.
 * @param l_Conn_Timeout      TCP connection timeout in milliseconds.
 * @param l_current_Log_Level Active log verbosity level.
 */
void ServiceManager::update(String l_Devicename, int8_t l_UUID_ID, float l_gravity, float l_plato, float l_specific_gravity, float l_temperature_c, float l_temperature_f, float l_temperature_k, float l_BatteryVoltage, int8_t l_batteryPercentage, uint32_t l_interval, int8_t l_RSSI, int16_t l_Conn_Timeout, int8_t l_current_Log_Level) {
    current_Log_Level = l_current_Log_Level;
    Devicename        = l_Devicename;
    UUID_ID           = l_UUID_ID;
    gravity           = l_gravity;
    plato             = l_plato;
    specific_gravity  = l_specific_gravity;
    temperature_c     = l_temperature_c;
    temperature_f     = l_temperature_f;
    temperature_k     = l_temperature_k;
    BatteryVoltage    = l_BatteryVoltage;
    interval          = l_interval;
    RSSI              = l_RSSI;
    batteryPercentage = l_batteryPercentage;
    Conn_Timeout      = l_Conn_Timeout;

    Print_Info(3, current_Log_Level, String(Devicename) + " " + String(UUID_ID) + " " + String(gravity, 2) + "° " + String(plato) + "P " + String(specific_gravity, 3) + "SG " + String(temperature_c) + "°C " + String(temperature_f) + "°F " + String(temperature_k) + "K " + String(BatteryVoltage) + "V " + String(RSSI) + " " + String(batteryPercentage) + " " + String(Conn_Timeout));
}

/**
 * @brief Establishes a connection to an MQTT broker and registers the message callback.
 *
 * @param server     MQTT broker hostname or IP.
 * @param port       MQTT broker port.
 * @param name       MQTT client identifier.
 * @param username   MQTT username (empty string = no auth).
 * @param password   MQTT password.
 * @param secure     Reserved for future TLS support (currently unused).
 * @param CACert     Reserved for future TLS CA cert (currently unused).
 * @param deviceCert Reserved for future mTLS device cert (currently unused).
 * @param deviceKey  Reserved for future mTLS private key (currently unused).
 * @return true  Connected successfully.
 * @return false Connection failed; MQTT error code is logged.
 */
bool ServiceManager::mqttConnect(const String & server, uint16_t port, const String & name, const String & username, const String & password, const bool secure, const char CACert[], const char deviceCert[], const char deviceKey[]) {
    /* Hier wird die MQTT-Verbindung hergestellt und bei Erfolg true zurückgegeben */
    /* Im Fehlerfall wird false zurückgegeben und die Fehlermeldung ausgegeben */

    // Allocate the noraml WiFi client to the PubSubClient
    _mqttClient.setClient(_client);

    _mqttClient.setServer(server.c_str(), port);
    _mqttClient.setCallback([this](char * topic, byte * payload, unsigned int length) { this->mqttCallback(topic, payload, length); });

    Print_Info(3, current_Log_Level, "Attempting MQTT connection");
    // Attempt to connect
    boolean ret;
    if(username[0] == '\0') {
        ret = _mqttClient.connect(name.c_str());
    } else {
        ret = _mqttClient.connect(name.c_str(), username.c_str(), password.c_str());
    }
    if(ret) {
        Print_Info(3, current_Log_Level, "Connected to MQTT");
        return true;
    } else {
        int Status = _mqttClient.state();

        switch(Status) {
            case -4:
                Print_Error("Failed MQTT connection, return code: Connection timeout");
                break;

            case -3:
                Print_Error("Failed MQTT connection, return code: Connection lost");
                break;

            case -2:
                Print_Error("Failed MQTT connection, return code: Connect failed");
                break;

            case -1:
                Print_Error("Failed MQTT connection, return code: Disconnected");
                break;

            case 1:
                Print_Error("Failed MQTT connection, return code: Bad protocol");
                break;

            case 2:
                Print_Error("Failed MQTT connection, return code: Bad client ID");
                break;

            case 3:
                Print_Error("Failed MQTT connection, return code: Unavailable");
                break;

            case 4:
                Print_Error("Failed MQTT connection, return code: Bad credentials");
                break;

            case 5:
                Print_Error("Failed MQTT connection, return code: Unauthorized");
                break;
        }
        return false;
    }
}

/**
 * @brief Publishes all sensor readings as individual topics to an MQTT broker.
 *
 * @param server     MQTT broker hostname or IP.
 * @param port       MQTT broker port.
 * @param username   MQTT username (empty string = no auth).
 * @param password   MQTT password.
 * @param TopicLevel Base topic path; values are published to TopicLevel/DeviceName/key.
 * @return true  All values published.
 * @return false MQTT connection failed.
 */
bool ServiceManager::sendMQTT(String server, uint16_t port, String username, String password, String TopicLevel) {
    /* Hier werden die Daten zum MQTT-Server gesendet und bei Erfolg true zurückgegeben */
    /* Im Fehlerfall wird false zurückgegeben und die Fehlermeldung ausgegeben */
    _doc.clear();
    add("tilt_G", String(gravity, 2));
    add("tilt_SG", String(specific_gravity, 3));
    add("tilt_P", String(plato, 2));
    add("temperature", String(temperature_c, 2));
    add("temp_units", "C");    //?
    add("battery", String(BatteryVoltage, 3));
    add("BatteryPercentage", String(batteryPercentage));
    add("gravity", String(plato, 2));
    add("interval", interval);
    add("RSSI", RSSI);
    Print_Info(3, current_Log_Level, "Sending Data to MQTT Server");

    bool response = mqttConnect(server, port, Devicename, username, password);
    if(response) {
        // MQTT publish values
        for(const auto & kv : _doc.as<JsonObject>()) {
            Print_Info(3, current_Log_Level, "MQTT publish: " + TopicLevel + "/" + Devicename + "/" + kv.key().c_str() + "/" + kv.value().as<String>());
            _mqttClient.publish((TopicLevel + "/" + Devicename + "/" + kv.key().c_str()).c_str(), kv.value().as<String>().c_str());
            _mqttClient.loop();
        }
    }

    Print_Info(3, current_Log_Level, "Closing MQTT connection after Sending");
    _mqttClient.disconnect();
    stop();
    Print_Info(3, current_Log_Level, "MQTT connection closed");
    return response;
}

/**
 * @brief Callback invoked by PubSubClient on receipt of an MQTT message.
 *
 * @param topic   Topic name the message arrived on.
 * @param payload Raw payload bytes.
 * @param length  Number of bytes in @p payload.
 */
void ServiceManager::mqttCallback(char * topic, byte * payload, unsigned int length) {
    /* Hier wird die ankommende MQTT-Nachricht behandelt */
    Print_Info(3, current_Log_Level, "MQTT message arrived [" + String(topic) + "] ");
    for(unsigned int i = 0; i < length; i++) {
        Print_Info(3, current_Log_Level, String((char)payload[i]));
    }
}

/**
 * @brief Sends sensor data as a JSON payload over a raw TCP connection.
 *
 * @param server Target server hostname or IP.
 * @param port   Target port (default: 80).
 * @return String Server response, or an empty string if the connection failed.
 */
String ServiceManager::sendTCP(String server, uint16_t port) {
    /* Hier werden die Daten zum TCP-Server gesendet */
    /* Bei Erfolg wird die Serverantwort zurückgegeben, im Fehlerfall ein leerer String */
    _doc.clear();
    add("name", Devicename);
    add("ID", UUID_ID);
    add("angle", String(gravity, 2));
    add("temperature", String(temperature_c, 2));
    add("temp_units", "C");
    add("battery", String(BatteryVoltage, 3));
    add("gravity", String(plato, 2));
    add("interval", interval);
    add("RSSI", RSSI);
    Print_Info(3, current_Log_Level, "Sending Data via TCP");
    int timeout = 0;

    serializeJson(_doc, Serial);

    if(_client.connect(server.c_str(), port)) {
        Print_Info(3, current_Log_Level, "Sender: TCP stream");
        serializeJson(_doc, _client);
        _client.println();
    } else {
        Print_Error("Sender: couldnt connect");
    }

    while(!_client.available() && timeout < Conn_Timeout) {
        timeout++;
        delay(1);
        yield();
    }
    String response;
    while(_client.available()) {
        Print_Info(5, current_Log_Level, _client.readString());
        yield();
    }
    stop();
    return response;
}

/**
 * @brief POSTs the current internal JSON document to an HTTP endpoint.
 *
 * @param server Target server hostname or IP.
 * @param uri    Request URI path.
 * @param port   Target port (default: 80).
 * @return true  Request dispatched (HTTP errors are logged, not returned).
 * @return false Unused.
 */
bool ServiceManager::sendGenericPost(String server, String uri, uint16_t port) {
    /* Hier wird ein HTTP-POST-Request gesendet und bei Erfolg true zurückgegeben */
    /* Im Fehlerfall wird false zurückgegeben und die Fehlermeldung ausgegeben */
    serializeJson(_doc, Serial);
    HTTPClient http;

    Print_Info(2, current_Log_Level, "HTTPAPI: posting");
    Print_Info(2, current_Log_Level, "Server: " + server);
    Print_Info(2, current_Log_Level, "Port: " + String(port));
    Print_Info(2, current_Log_Level, "URI: " + uri);

    // configure traged server and uri
    http.begin(_client, server, port, uri);
    http.addHeader("User-Agent", Devicename);
    http.addHeader("Connection", "close");
    http.addHeader("Content-Type", "application/json");

    String json;
    serializeJson(_doc, json);
    auto httpCode = http.POST(json);
    // Print_Info(5, current_Log_Level, "JSON: " + json);
    Print_Info(2, current_Log_Level, "code: " + httpCode);

    // httpCode will be negative on error
    if(httpCode > 0) {
        if(httpCode == HTTP_CODE_OK) {
            Print_Info(2, current_Log_Level, "[HTTP] POST... Success: " + http.getString());
        }
    } else {
        Print_Error("[HTTP] POST... failed, error: " + http.errorToString(httpCode));
    }
    http.end();
    stop();
    return true;
}

/**
 * @brief Writes measurement data to an InfluxDB line-protocol endpoint.
 *
 * @param server          InfluxDB server hostname or IP.
 * @param port            InfluxDB HTTP port.
 * @param db              Target database name.
 * @param username        InfluxDB username (empty = no auth).
 * @param password        InfluxDB password.
 * @param Measurementsname Source tag used as the measurement name prefix.
 * @return true  Data written (errors are logged, not returned).
 * @return false Unused.
 */
bool ServiceManager::sendInfluxDB(String server, uint16_t port, String db, String username, String password, String Measurementsname) {
    HTTPClient http;
    String uri = "/write?db=";
    String msg;
    _doc.clear();

    add("tilt_G", gravity);
    add("tilt_SG", specific_gravity);
    add("tilt_P", plato);
    add("temperature", temperature_c);
    add("temp_units", "C");    //?
    add("battery", BatteryVoltage);
    add("gravity", plato);
    add("interval", interval);
    add("RSSI", RSSI);
    Print_Info(3, current_Log_Level, String(F("Send Data to InfluxDB: ")) + db + String(F(" w/ Credentials: ")) + username + String(F(":")) + password);

    uri += db;

    Print_Info(5, current_Log_Level, String(F("INFLUXDB: post to DB: ")) + uri);

    http.begin(_client, server, port, uri);

    if(username.length() > 0) {
        http.setAuthorization(username.c_str(), password.c_str());
    }

    http.addHeader("User-Agent", Devicename);
    http.addHeader("Connection", "close");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    msg += Devicename;
    msg += ",source=";
    msg += Measurementsname;
    msg += " ";

    for(const auto & kv : _doc.as<JsonObject>()) {
        msg += kv.key().c_str();
        msg += "=";
        // check if value is of type char, if so set value in double quotes
        if(kv.value().is<const char *>()) {
            msg += '"' + kv.value().as<String>() + '"';
        } else {
            msg += kv.value().as<String>();
        }
        msg += ",";
    }
    msg.remove(msg.length() - 1);

    Print_Info(5, current_Log_Level, String(F("Post data: ")) + msg);

    auto httpCode = http.POST(msg);
    Print_Info(5, current_Log_Level, String(F("code: ")) + httpCode);

    // httpCode will be negative on error
    if(httpCode > 0) {
        if(httpCode == HTTP_CODE_OK) {
            Print_Info(5, current_Log_Level, http.getString());
        } else {
            Print_Error(http.getString());
        }
    } else {
        Print_Error("[HTTP] POST... failed, error: " + http.errorToString(httpCode));
    }

    http.end();
    stop();
    return true;
}

/**
 * @brief Pushes sensor gauge metrics to a Prometheus Pushgateway.
 *
 * @param server   Pushgateway hostname or IP.
 * @param port     Pushgateway HTTP port.
 * @param job      Prometheus job label.
 * @param instance Prometheus instance label.
 * @return true  Metrics pushed (errors are logged).
 * @return false Unused.
 */
bool ServiceManager::sendPrometheus(String server, uint16_t port, String job, String instance) {
    _doc.clear();
    add("tilt", String(gravity, 2));
    add("temperature", String(temperature_c, 2));
    add("battery", String(BatteryVoltage, 3));
    add("gravity", String(plato, 2));
    add("interval", interval);
    add("RSSI", RSSI);
    Print_Info(3, current_Log_Level, "Sending Data to Prometheus");

    HTTPClient http;

    // the path looks like /metrics/job/<JOBNAME>[/instance/<INSTANCENAME>]
    String uri = "/metrics/job/";
    uri += job;
    uri += "/instance/";
    uri += instance;

    Print_Info(3, current_Log_Level, String("PROMETHEUS: posting to Prometheus Pushgateway: ") + uri);
    // configure traged server and uri
    http.begin(_client, server, port, uri);
    http.addHeader("User-Agent", Devicename);
    http.addHeader("Connection", "close");
    http.addHeader("Content-Type", "text/plain");

    String msg;

    // Build up the data for the Prometheus Pushgateway
    // A gauge is a metric that represents a single numerical value that can arbitrarily go up and down.
    for(const auto & kv : _doc.as<JsonObject>()) {
        msg += "# TYPE ";
        msg += kv.key().c_str();
        msg += " gauge\n";
        msg += "# HELP ";
        msg += kv.key().c_str();
        msg += " The approximate value of ";
        msg += kv.key().c_str();
        msg += ".\n";
        msg += kv.key().c_str();
        msg += " ";
        msg += kv.value().as<String>();
        msg += "\n";
    }

    Print_Info(3, current_Log_Level, String("POST data: ") + msg);
    auto httpCode = http.POST(msg);

    // httpCode will be 202 on success
    if(httpCode == 202) {
        Print_Info(3, current_Log_Level, String("code: ") + httpCode);
        Print_Info(3, current_Log_Level, http.getString());
    } else {
        Print_Error(String("code: ") + httpCode);

        Print_Error(http.errorToString(httpCode));

        Print_Error(http.getString());
    }
    http.end();
    stop();
    return true;
}
/**
 * @brief Send Data to Ubidots
 * ACHTUNG! UBIDOTS unterstützt nur 7 Variablen pro Device.
 * Wenn VAriablen hinzugefügt werden, müssen die Variablen in Ubidots gelöscht werden.
 * Das führt auf der Ubidots Seite zu einer Fehlermeldung und die Messwert werden nicht mehr gesendet.
 *
 * @param Unit_Temperature
 * @param Unit_Tilt
 * @param server
 * @param port
 * @param token
 * @return true if data was sent
 * @return false if data was not sent
 */
bool ServiceManager::sendUbidots(uint8_t Unit_Temperature, uint8_t Unit_Tilt, const char * server, uint16_t port, String token) {
    _doc.clear();
    add("angle_RAW", String(gravity, 2));
    add("battery_Level", batteryPercentage);
    add("temperature", String(temperature_c, 2));
    add("battery_Voltage", String(BatteryVoltage, 3));
    switch(Unit_Temperature) {
        case 0:
            add("temperature", String(temperature_c, 2));
            break;
        case 1:
            add("temperature", temperature_f);
            break;
        default:
            add("temperature", String(temperature_c, 2));
            break;
    }

    switch(Unit_Tilt) {
        case 0:
            add("Plato", String(plato, 2));
            break;
        case 1:
            add("Specific Gravity", String(specific_gravity, 3));
            break;
        default:
            add("Plato", String(plato, 2));
            break;
    }
    add("interval", interval);
    add("RSSI", RSSI);
    Print_Info(3, current_Log_Level, "Sending Data to Ubidots");

    serializeJson(_doc, Serial);

    if(_client.connect(server, port)) {
        Print_Info(3, current_Log_Level, "Sender: Ubidots posting");

        String msg = F("POST /api/v1.6/devices/");
        msg += Devicename;
        msg += "?token=";
        msg += token;
        msg += F(" HTTP/1.1\r\nHost: ");
        msg += server;
        msg += F("\r\nUser-Agent: ESP32\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: ");
        msg += measureJson(_doc);
        msg += "\r\n";

        _client.println(msg);
        Print_Info(3, current_Log_Level, msg);
        serializeJson(_doc, _client);
        _client.println();

    } else {
        Print_Error("Sender: couldnt connect");
    }

    int timeout = 0;
    while(!_client.available() && timeout < Conn_Timeout) {
        timeout++;
        delay(1);
        yield();
    }
    while(_client.available()) {
        char Response = _client.read();
        Serial.write(Response);
        yield();
    }
    // Print_Info(3, current_Log_Level, Response);
    //  currentValue = 0;
    _client.stop();
    stop();
    return true;
}

/**
 * @brief Sends measurement data to a Google Sheets Apps Script deployment.
 *
 * @param server Google Script server hostname.
 * @param port   HTTPS port (typically 443).
 * @param token  Google Apps Script deployment token.
 * @return true  Data POSTed and acknowledged.
 * @return false HTTPS connection or POST failed.
 */
bool ServiceManager::sendGoogleSheets(const char * server, uint16_t port, String token) {
    _doc.clear();
    String l_uri = "/macros/s/" + token + "/exec";

    add("name", Devicename);
    add("plato", String(plato, 2));
    add("SG", String(specific_gravity, 3));
    add("Temp_F", String(temperature_f, 2));
    add("Temp_C", String(temperature_c, 2));
    add("volt", String(BatteryVoltage, 3));
    add("battperc", String(batteryPercentage));
    add("tilt", String(gravity, 2));
    Print_Info(3, current_Log_Level, "Sending Data to Googlesheet Server:" + String(server) + " Port:" + String(port) + " uri:" + l_uri);

    return sendGenericSecurePost(server, l_uri, port, root_ca);
}

/**
 * @brief POSTs the current internal JSON document via HTTPS.
 *
 * @param server      Target server hostname.
 * @param l_uri       Full URI path.
 * @param port        HTTPS port.
 * @param certificate PEM CA certificate for TLS verification.
 * @return true  Request completed with HTTP 200.
 * @return false Connection failed or server returned an error.
 */
bool ServiceManager::sendGenericSecurePost(String server, String l_uri, uint16_t port, const char * certificate) {
    WiFiClientSecure client;
    client.setCACert(certificate);
    client.setTimeout(60000);

    delay(1000);
    if(!client.connect(server.c_str(), port)) {
        Serial.println("Connection failed google!");
        return false;
    } else {
        Serial.println("Secure Connection Established");
        HTTPClient https;

        String uri = "https://" + server + l_uri;    //
        if(https.begin(client, uri)) {
            Serial.println("HTTPS begined successfully; URI:" + uri);
        } else {
            Serial.println("HTTPS begin failed");
        }
        https.addHeader("User-Agent", Devicename);
        https.addHeader("Connection", "close");
        https.addHeader("Content-Type", "application/json");
        https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);    // This is absolutely necessary to get the response from the google Server

        String json;
        serializeJson(_doc, json);
        Print_Info(2, current_Log_Level, "json was built ready");

        int16_t httpCode = https.POST(json);

        // httpCode will be negative on error
        if(httpCode > 0) {
            if(httpCode == HTTP_CODE_OK) {
                Print_Info(2, current_Log_Level, "[HTTP] POST... Success: " + https.getString());
            } else {
                Print_Error("[HTTP] POST... failed, error: " + https.errorToString(httpCode));
                return false;
            }
        } else {
            Print_Error("[HTTP] POST... failed, error: " + https.errorToString(httpCode));
            return false;
        }
        https.end();
    }
    stop();
    return true;
}

/**
 * @brief Sends sensor readings to FHEM via an HTTP GET command string.
 *
 * @param server FHEM server hostname or IP.
 * @param port   FHEM HTTP port.
 * @return true  Data sent (errors are logged).
 * @return false Unused.
 */
bool ServiceManager::sendFHEM(String server, uint16_t port) {
    add("angle", String(gravity, 2));
    add("temperature", String(temperature_c, 2));
    add("temp_units", "C");
    add("battery", String(BatteryVoltage, 3));
    add("gravity", String(plato, 2));
    add("ID", UUID_ID);
    Print_Info(3, current_Log_Level, "Sending Data to FHEM");

    serializeJson(_doc, Serial);

    if(_client.connect(server.c_str(), port)) {
        Print_Info(3, current_Log_Level, "Sender: FHEM get");

        String msg = String("GET /fhem?cmd.Test=set%20");
        msg += Devicename;

        for(const auto & kv : _doc.as<JsonObject>()) {
            msg += "%20";
            msg += kv.value().as<String>();
        }

        msg += F("&XHR=1 HTTP/1.1\r\nHost: ");
        msg += server;
        msg += ":";
        msg += port;
        msg += "\r\nUser-Agent: ";
        msg += Devicename;
        msg += F("\r\nConnection: close\r\n");

        _client.println(msg);
    } else {
        Print_Error("Sender: couldnt connect");
    }

    int timeout = 0;
    while(!_client.available() && timeout < Conn_Timeout) {
        timeout++;
        delay(1);
        yield();
    }
    while(_client.available()) {
        char c = _client.read();
        Serial.write(c);
        yield();
    }
    // currentValue = 0;
    _client.stop();
    stop();
    return true;
}

/**
 * @brief Sends sensor data to TControl via a plain TCP connection.
 *
 * @param server TControl server hostname or IP.
 * @param port   TControl TCP port.
 * @return true  Data sent (errors are logged).
 * @return false Unused.
 */
bool ServiceManager::sendTCONTROL(String server, uint16_t port) {
    add("T", String(temperature_c, 2));
    add("D", String(gravity, 2));
    add("U", String(BatteryVoltage, 3));
    add("G", String(plato, 2));
    Print_Info(3, current_Log_Level, "Sending Data to TCONTROL");

    serializeJson(_doc, Serial);

    if(_client.connect(server.c_str(), port)) {
        Print_Info(3, current_Log_Level, "Sender: TCONTROL");
        String msg;

        for(const auto & kv : _doc.to<JsonObject>()) {
            msg += kv.key().c_str();
            msg += ": ";
            msg += kv.value().as<String>();
            msg += " ";
        }
        msg.remove(msg.length() - 1);

        _client.println(msg);

    } else {
        Print_Error("Sender: couldnt connect");
    }

    int timeout = 0;
    while(!_client.available() && timeout < Conn_Timeout) {
        timeout++;
        delay(1);
        yield();
    }
    while(_client.available()) {
        char c = _client.read();
        Serial.write(c);
        yield();
    }
    stop();
    return true;
}

/**
 * @brief Send data to Telgram
 *
 * @param Unit_Temperature 0 = Celsius, 1 = Fahrenheit
 * @param Unit_Tilt 0 = °P, 1 = SG
 * @param Token
 * @param Chat_ID
 * @return true if data was sent
 * @return false if data was not sent
 */
bool ServiceManager::sendTelegram(uint8_t Unit_Temperature, uint8_t Unit_Tilt, String Token, String Chat_ID) {
    Print_Info(3, current_Log_Level, "Start Telegram transmission");
    UniversalTelegramBot bot(Token, _secureClient);    // Create the Telegram bot
    String message_Temperature;                        // Message for Temperature
    String message_Tilt;                               // Message for Tilt
    time_t now = time(nullptr);                        // Get the current time

    configTime(0, 0, "pool.ntp.org");                  // get UTC time via NTP

    while(now < 24 * 3600) {                           // wait for time to be set
        Serial.print(".");
        delay(100);
        now = time(nullptr);
    }

    _secureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);    // Add root certificate for api.telegram.org
    _secureClient.setInsecure();                           // Set WiFiClientSecure class to insecure
    _secureClient.setTimeout(12000 / 1000);

    switch(Unit_Temperature) {
        case 0:
            message_Temperature = String(temperature_c,2) + "°C ";
            break;
        case 1:
            message_Temperature = String(temperature_f,2) + "°F ";
            break;
        default:
            message_Temperature = "";
            break;
    }

    switch(Unit_Tilt) {
        case 0:
            message_Tilt = String(plato, 3) + "°P ";
            break;
        case 1:
            message_Tilt = String(specific_gravity, 4) + "SG ";
            break;
        default:
            message_Tilt = "";
            break;
    }
    return bot.sendMessage(Chat_ID, Devicename + ": " + message_Tilt + message_Temperature + String(batteryPercentage) + "%  " + String(RSSI) + "RSSI");
}

/**
 * @brief Publishes tilt and temperature data to a Brewblox MQTT broker.
 *
 * @param server   MQTT broker hostname or IP.
 * @param port     MQTT broker port.
 * @param topic    MQTT topic to publish to.
 * @param username MQTT username.
 * @param password MQTT password.
 * @return true  Data published.
 * @return false MQTT connection failed.
 */
bool ServiceManager::sendBrewblox(String server, uint16_t port, String topic, String username, String password) {
    add("Tilt[deg]", String(gravity, 2));
    add("Temperature[deg]", String(temperature_c, 2));
    add("Battery[V]", String(BatteryVoltage, 3));
    add("gravity", String(plato, 2));
    add("specificgravity", specific_gravity);
    add("Rssi[dBm]", RSSI);
    Print_Info(3, current_Log_Level, "Sending Data to BREWBLOX");

    bool response = mqttConnect(server, port, Devicename, username, password);
    if(response) {
        String json;
        serializeJson(_doc, json);
        Print_Info(3, current_Log_Level, "Brewblox MQTT publish: " + topic);
        _mqttClient.publish(topic.c_str(), ("{\"key\":\"" + Devicename + "\",\"data\":" + json + "}").c_str());
    }
    Print_Info(3, current_Log_Level, "Closing MQTT connection after Sending");
    _mqttClient.disconnect();
    stop();
    Print_Info(3, current_Log_Level, "MQTT connection closed");
    return response;
}

/**
 * @brief Sends measurement data to the Brewfather logging endpoint.
 *
 * @param gravityUnit true = send gravity in SG, false = Plato.
 * @param server      Target server hostname or IP.
 * @param port        Target port.
 * @param uri         Request URI path.
 */
void ServiceManager::sendBrewfather(boolean gravityUnit, String server, uint16_t port, String uri) {
    _doc.clear();
    add("name", Devicename);
    add("temp", String(temperature_c, 2));
    add("temp_unit", "C");
    add("gravity", String(plato, 2));
    add("gravity_unit", "P");
    add("battery", String(BatteryVoltage));
    Print_Info(3, current_Log_Level, "Sending Data to Brewfather Server:" + server + " Port:" + String(port) + " uri:" + uri);

    sendGenericPost(server, uri, port);
}

/**
 * @brief Sends measurement data to BierBot and returns the next polling interval.
 *
 * @param server           BierBot API server hostname.
 * @param Token            API key.
 * @param URI              API endpoint path.
 * @param ID               Device chip ID.
 * @param l_Firmwareversion Firmware version string.
 * @return uint32_t Next request interval in milliseconds as instructed by the server.
 */
uint32_t ServiceManager::sendBierBot(String server, String Token, String URI, String ID, String l_Firmwareversion) {
    WiFiClientSecure * _Client = new WiFiClientSecure;         // new instance of WiFiClientSecure
    uint32_t next_request_ms   = 60 * 1000;                    // set default timeout for HTTP connection
    String uri                 = "https://" + server + URI;    // start to assemble the URL
    HTTPClient http;                                           // create HTTPClient object
    String json;

    _doc.clear();    // clear JSON document if it was ued before

    add("apikey", Token);
    add("type", "hydrom");                                          // As long as I have no other products this is equal to the brand
    add("brand", "hydrom");                                         // Brand of the device
    add("version", l_Firmwareversion);                                // Version of the firmware will be replaced by the real value
    add("chipid", ID);                                              // ChipID of the device will be replaced by the real value
    add("s_number_wort_0", String(plato, 2));                       // gravity can be in SG or °P, depending on user setting
    add("s_number_temp_0", String(temperature_c, 2));               // always transmit °C
    add("s_number_voltage_0", String(BatteryVoltage, 2));           // voltage of the battery
    add("s_number_batterypercent_0", String(batteryPercentage));    // battery percentage
    add("s_number_wifi_0", WiFi.RSSI());                            // WiFi signal strength
    add("s_number_tilt_0", String(gravity, 2));                     // RAW Tilt

    _Client->setInsecure();                                         // set SecureClient to unsecure mode

    Print_Info(3, current_Log_Level, "Sending Data to BierBot Server: " + uri);

    // serializeJson(_doc, Serial); // create JSON string

    http.begin(*_Client, uri);    // send POST request
    http.addHeader("User-Agent", "ispindel");
    http.addHeader("Connection", "close");
    http.addHeader("Content-Type", "application/json");

    serializeJson(_doc, json);                                                // create JSON string

    auto httpCode = http.POST(json);                                          // receive response

    Print_Info(3, current_Log_Level, "JSON: " + json);                        // print response
    Print_Info(3, current_Log_Level, "code: " + httpCode);                    // print response

    if(httpCode > 0) {                                                        // parse response
        if(httpCode == HTTP_CODE_OK) {                                        // If the server's response is ok
            String payload      = http.getString();                           // Get the request response payload
            uint8_t startIdx    = payload.indexOf("next_request_ms") + 17;    // find the next request time
            String nrmSubstring = payload.substring(startIdx);                // get the substring from the next request time
            uint8_t endIdx      = -1;

            if(nrmSubstring.indexOf("}") != -1) {                                                         // set the end index to -1 to check if it was set later
                endIdx = nrmSubstring.indexOf("}") + startIdx;                                            // end based on bracket
            } else if(nrmSubstring.indexOf(",") != -1) {                                                  // if there is no bracket, check for a comma
                endIdx = nrmSubstring.indexOf(",") + startIdx;                                            // set the end index to the comma
            }
            if(startIdx > 0 && endIdx > startIdx) {                                                       // if the start and end index are valid
                String next_request_str = payload.substring(startIdx, endIdx);                            // get the next request time
                Print_Info(3, current_Log_Level, "next request string in " + next_request_str + "ms");    // print the next request time
                next_request_ms = next_request_str.toInt();                                               // convert the next request time to an integer
            }
        }
    } else {
        Print_Info(3, current_Log_Level, "[HTTP] POST... failed, error: " + http.errorToString(httpCode));
    }
    http.end();                // end HTTPClient
    return next_request_ms;    // return the next request time
}

/**
 * @brief Sends tilt and temperature data to CraftbeerPi via HTTP POST.
 *
 * @param server Target server hostname or IP.
 * @param port   Target port.
 * @param uri    Request URI path.
 */
void ServiceManager::sendCraftbeerpi(String server, uint16_t port, String uri) {
    _doc.clear();
    add("name", Devicename);
    add("ID", UUID_ID);
    add("angle", String(plato, 2));
    add("temperature", String(temperature_c, 2));
    add("temp_units", "C");
    add("battery", String(BatteryVoltage, 3));
    add("RSSI", RSSI);

    Print_Info(3, current_Log_Level, "Sending Data to CraftbeerPi");
    sendGenericPost(server, uri, port);
}

/**
 * @brief Sends sensor data to a configurable HTTP endpoint, resolving {VALUE_*} placeholders in the URI.
 *
 * @param server Target server hostname or IP.
 * @param port   Target port.
 * @param uri    URI template (up to 265 chars) with optional placeholders.
 */
void ServiceManager::sendHttp(String server, uint16_t port, char uri[265]) {
    _doc.clear();
    add("name", Devicename);
    add("ID", UUID_ID);
    add("angle", String(gravity, 2));
    add("temperature", String(temperature_c, 2));
    add("temp_units", "C");
    add("battery", String(BatteryVoltage, 3));
    add("BatteryCapacity", batteryPercentage);
    add("gravity", String(plato, 2));
    add("interval", interval);
    add("RSSI", RSSI);
    add("tilt", String(gravity, 2));

    Print_Info(3, current_Log_Level, "Sending Data to HTTP-Server");
    sendGenericPost(server, Replace_Placeholder(uri), port);
}

/**
 * @brief Sends specific gravity and temperature to a Grainfather endpoint via HTTP POST.
 *
 * @param server Target server hostname or IP.
 * @param port   Target port.
 * @param uri    Request URI path.
 */
void ServiceManager::sendGrainfather(String server, uint16_t port, String uri) {
    _doc.clear();
    add("specific_gravity", String(specific_gravity, 3));
    add("temperature", String(temperature_c, 2));
    add("battery", String(BatteryVoltage, 3));
    add("unit", "celsius");

    Print_Info(3, current_Log_Level, "Sending Data to Grainfather");
    sendGenericPost(server, uri, port);
}

/**
 * @brief Converts a temperature unit index to its abbreviation string.
 *
 * @param Temperature_Unit 0 = Celsius ("C"), 1 = Fahrenheit ("F"), 2 = Kelvin ("K").
 * @return String Unit abbreviation, or "99" for unrecognised values.
 */
String ServiceManager::get_Temperature_Unit(int8_t Temperature_Unit) {
    switch(Temperature_Unit) {
        case 0:
            return "C";
        case 1:
            return "F";
        case 2:
            return "K";
        default:
            Print_Error("The Temperature Unit was not defined: " + String(Temperature_Unit));
            return "99";
            break;
    }
}

/**
 * @brief Das Hydrom erlaubt es Platzhalter in der URL zu verwenden.
 * Diese Platzhalter werden hier ausgetauscht.
 *
 *
 * @param _url
 * @return String
 */
String ServiceManager::Replace_Placeholder(char _url[265]) {
    String URL = _url;
    URL.replace("{NAME_DEVICE}", Devicename);
    URL.replace("{VALUE_TEMPERATURE_C}", String(temperature_c, 1));
    //    URL.replace("{VALUE_TEMPERATURE_K}", String(temperature_c.Kelvin, 1));
    //    URL.replace("{VALUE_TEMPERATURE_F}", String(temperature_c.Fahrenheit, 1));
    URL.replace("{VALUE_TILT_P}", String(plato, 3));
    URL.replace("{VALUE_TILT_SG}", String(specific_gravity, 3));
    URL.replace("{VALUE_TILT_G}", String(gravity, 3));
    URL.replace("{VALUE_RSSI}", String(RSSI));

    // URL.replace("{TILT_COMMA}}", String(temperature_c,1));
    URL.replace("{VALUE_BATTERY_VOLTAGE}", String(BatteryVoltage, 3));
    // URL.replace("{BATTERY_COMMA}", String(temperature_c,1));
    URL.replace("{VALUE_BATTERY_PERCENTAGE}", String(batteryPercentage));
    // URL.replace("{TEMPERATURE_DOT}", String(temperature_c, 1));
    return URL;
}
