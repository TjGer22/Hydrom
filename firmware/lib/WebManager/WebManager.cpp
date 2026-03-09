/**
 * @file WebManager.cpp
 * @author TjGer22
 * @brief Embedded HTTP and WebSocket server for the device configuration UI.
 * @date 2026
 *
 * @details
 * Implements the WebManager class. Serves static HTML/JS/CSS assets,
 * processes configuration form submissions, triggers calibration
 * and firmware-update flows, and streams live data over WebSockets.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "WebManager.h"

#include <Arduino.h>
#include <SPIFFS.h>
#include <Update.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "Calibration_MPU.h"
#include "Calibrator.h"
#include "DeviceManager.h"
#include "HydromConfiguration.h"
#include "SensorManager.h"
#include "Util.h"
#include "WebPages.h"
#include "TemperatureCompensation.h"
#include "ArduAutoUpdater.h"
#include "Globals.h"
// Secrets.h removed — no longer needed (cloud app removed)

#define DEBUG_PRINT(p, ...) Serial.print(p, ##__VA_ARGS__)
#define DEBUG_PRINTF(p, ...) Serial.printf(p, ##__VA_ARGS__)

/*********************************************************************
 * LOCAL VARIABLES
 **********************************************************************/
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81, "/read");
String page;
uint32_t WebsocketStart = 0;
boolean g_once = true;
uint8_t clientId;
TemperatureCompensation tempComp;

boolean show_Calibration_Result=false; // If true, the feedback will be shown on the Calibrationpage



float last_gravity;
float last_specific_gravity;
float last_plato;

float last_temperature_celsius;
float last_temperature_Fahrenheit;
float last_temperature_Kelvin;

boolean postStatus;

boolean Connection_Failed;
boolean Calibration_Failed;

File uploadFile;
/*********************************************************************
 * LOCAL FUNCTIONS
 **********************************************************************/
void Choose_Landingpage(void);
void Page_Home(void);
void Page_NotFound(void);
void Page_Calibration(void);
void Page_Settings_WIFI(void);
void Page_Information(void);
void Page_MPU_Calibration(void);
void doMPUcalibration(void);
void doPlainWaterMeasurement(void);
void Page_Settings_Other(void);
void Page_Settings_Services(void);
void addHead(const char * title);
void addHeader();
void addPanel();
void addAction(const char * title);
void addMessage(WebMessageType_t type, String message);
void replace_Language_File(void);

void Page_Reset(void);
void Page_Restart(void);
void Page1_Landingpage(void);
void Page2_Landingpage(void);
void Page3_Landingpage(void);
void Page4_Landingpage(void);
void Page_Calibration(void);
void Calibration_Step(int8_t Step);
void Page_Calibration_Step1(void);
void Page_Calibration_Step2(void);
void Page_Calibration_Step3(void);
void Page_Calibration_Step4(void);
void Page_Calibration_Step5(void);
void Page_Calibration_Step6(void);
void Page_Calibration_Step7(void);
void Page_Calibration_Conclusion();
void Calibration_Conclusion(int8_t l_CalibrationSteps);
void Page_Calibration_Restart(void);
void prepare_next_step(int8_t step);
void save_collected_data(int8_t step);
void onUpdatePageUpload(void);
void onUpdatePage(void);
void writeValues(void);
void checkPostContent(void);
void gotoDeepSleep(void);
void gotoDebugbode(void);
int CheckForArg(int l_Target, String l_name);
double CheckForArg(double l_Target, String l_name);
double CheckForArg(double l_Target, String l_name);
float CheckForArg(float l_Target, String l_name);
void Prepare_Page(const char * l_titel, const char * l_headline, const char * l_backlink, const char * HTML_CONTENT);
float Calculate_Measurement_Value(float gradient, float y_low, float Measurement);
float Calculate_Offset(float gradient, float y_low, float Measurement);
float Calculate_gradient(float y_low, float y_high);
void force_Measurement();
boolean load_English(void);
boolean load_German(void);
boolean load_Italien(void);
boolean load_Espaniol(void);
boolean load_French(void);
boolean load_Dutch(void);
boolean load_Portuguese(void);
boolean load_Swedish(void);
boolean load_Finnish(void);
boolean load_Emty(void);

// void clear_calibration(void);
boolean updateValid;
Calibration_MPU calibration_mpu;
boolean TestMessage_state;

/**
 * @brief WebSocket event callback dispatched by the WebSockets library.
 *
 * @param num     Client connection number.
 * @param type    Event type (connected, disconnected, text, binary, etc.).
 * @param payload Pointer to the event payload data.
 * @param length  Length of @p payload in bytes.
 */
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Print_Info(4, Hydrom.current_Log_Level, "Web Socket disconnection: [" + String(num) + " Disconnected!");
            break;

        case WStype_CONNECTED:
            Print_Info(4, Hydrom.current_Log_Level, "Web Socket connection: [" + String(num) + "] Connected from " + String(webSocket.remoteIP(num)[0]) + "." + String(webSocket.remoteIP(num)[1]) + "." + String(webSocket.remoteIP(num)[2]) + "." + String(webSocket.remoteIP(num)[3]));
            clientId = num;
            break;

        case WStype_TEXT:
            Print_Info(4, Hydrom.current_Log_Level, "Local IP: ");
            DEBUG_PRINT(WiFi.softAPIP().toString().c_str());
            if(strcmp((char *)payload, "store") == 0)
                break;
        case WStype_BIN:
            break;
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
        default:
            break;
    }
}

/**
 * @brief Default constructor.
 */
WebManager::WebManager(void) {}

/**
 * @brief Initialises internal WebManager state before the server is started.
 */
void WebManager::begin(void) {
    TestMessage_state=false;
    updateValid = false;
}
/**
 * @brief Registers all HTTP route handlers and static-file mappings,
 *        then starts the HTTP and WebSocket servers.
 */
void WebManager::start(void) {
    // create the HTTP request handlers for pages
    server.on("/", Choose_Landingpage);
    server.on("/land1", Page1_Landingpage);
    server.on("/land2", Page2_Landingpage);
    server.on("/land3", Page3_Landingpage);
    server.on("/land4", Page4_Landingpage);
    server.on("/plainwatermeasurement", doPlainWaterMeasurement);
    server.on("/calibrate", Page_Calibration);    // Page_Calibration);
    server.on("/settings", Page_Settings_Other);
    server.on("/net", Page_Settings_WIFI);
    server.on("/information", Page_Information);
    server.on("/mpuinformation", Page_MPU_Calibration);
    server.on("/mpucalibration", doMPUcalibration);
    server.on("/update", onUpdatePage);
    server.on(
        "/firmware_update", HTTP_POST, []() {
            server.sendHeader("Connection", "close");
            config.saveFS_Firmware();
            onUpdatePage();
            ESP.restart();
        },
        onUpdatePageUpload);
    server.on("/reset", Page_Reset);
    server.on("/restart", Page_Restart);
    server.on("/firmware_update", onUpdatePage);
    server.on("/services", Page_Settings_Services);
    server.on("/Step1", Page_Calibration_Step1);
    server.on("/Step2", Page_Calibration_Step2);
    server.on("/Step3", Page_Calibration_Step3);
    server.on("/Step4", Page_Calibration_Step4);
    server.on("/Step5", Page_Calibration_Step5);
    server.on("/Step6", Page_Calibration_Step6);
    server.on("/Step7", Page_Calibration_Step7);
    server.on("/conclusion", Page_Calibration_Conclusion);
    server.on("/restart_calibration", Page_Calibration_Restart);
    server.on("/sleepdeep", gotoDeepSleep);
    server.on("/debugmode", gotoDebugbode);

    // server.on("/language", onLanguagePage);
    // server.on("/language_update", HTTP_POST, []() { onLanguagePage(); }, onLanguagePageUpload);
    // server.on("/backup", onBackupPage);
    // server.onNotFound(onNotFound);
    // Request handlders for static downloads
    //server.serveStatic("/js", SPIFFS, "/js.js");
    server.serveStatic("/img-mono-logo", SPIFFS, "/mono_logo.svg");
    server.serveStatic("/img-main-logo", SPIFFS, "/main_logo.svg");
    server.serveStatic("/favicon.ico", SPIFFS, "/favicon.png");
    //server.serveStatic("/css-style", SPIFFS, "/style.css");
    //server.serveStatic("/language.json", SPIFFS, "/language.json");
    server.serveStatic(DEF_FILE_SETTINGS, SPIFFS, DEF_FILE_SETTINGS);
    server.serveStatic(DEF_FILE_SETTINGS_, SPIFFS, DEF_FILE_SETTINGS_);
    server.serveStatic(DEF_FILE_OFFSET, SPIFFS, DEF_FILE_OFFSET);
    server.serveStatic(DEF_FILE_SERVICES1, SPIFFS, DEF_FILE_SERVICES1);
    server.serveStatic(DEF_FILE_SERVICES2, SPIFFS, DEF_FILE_SERVICES2);
    server.serveStatic(DEF_FILE_DEVICEINFO, SPIFFS, DEF_FILE_DEVICEINFO);
    server.serveStatic(DEF_FILE_FIRMWAREVERSION, SPIFFS, DEF_FILE_FIRMWAREVERSION);
    server.serveStatic(DEF_FILE_SERVICERESPONSE, SPIFFS, DEF_FILE_SERVICERESPONSE);
    server.serveStatic(DEF_FILE_FONT_WOFF, SPIFFS, DEF_FILE_FONT_WOFF);
    // start the HTTP web server
    server.begin();
    Print_Info(4, Hydrom.current_Log_Level, "HTTP server started");
    // create a socket for sending and receiving data
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
}
/**
 * @brief Stops the HTTP server and closes the WebSocket server.
 */
void WebManager::stop(void) {
    server.close();
    server.stop();
    webSocket.close();
}
/**
 * @brief Processes one round of pending HTTP client requests.
 *        Must be called repeatedly from the main loop while the server is running.
 */
void WebManager::loop(void) {
    server.handleClient();
}

WebManager web;
/**
 * @brief Initialises the HTML page buffer with DOCTYPE, head section and CSS,
 *        replacing the title placeholder.
 *
 * @param title Page title shown in the browser tab.
 */
void addHead(const char * title) {
    page = "<!DOCTYPE html>\n";
    page += "<html>";
    page += HTML_HEAD;
    page.replace("{CSS_Placeholder}", CSS);
    page += "<body>";
    page.replace("{Name_Title}", String(title));
}

/**
 * @brief Appends the site header bar (including battery percentage) to the page buffer.
 */
void addHeader() {
    page += HTML_HEADER;
    page.replace("{Percentage}", String(configSensor.batteryPercentage));
}
/**
 * @brief Appends the navigation panel to the page buffer.
 *        Shows the debug panel when the log level exceeds 10.
 */
void addPanel() {
    if(Hydrom.current_Log_Level <= 10) {
        Print_Info(4, Hydrom.current_Log_Level, "Because the debug level is less than 10, the normal panel is shown.");
        page += HTML_PANEL;
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "Because the debug level is greater than 10, the DEBUG panel is shown.");
        page += HTML_PANEL_DEBUG;
    }
}
/**
 * @brief Appends the action bar (title + back button) to the page buffer.
 *
 * @param title      Text displayed as the page action title.
 * @param backtolink URL for the back navigation button.
 */
void addAction(const char * title, const char * backtolink) {
    page += HTML_ACTION;
    page.replace("{BacktoLink}", String(backtolink));
    page.replace("{titel_actionbar}", String(title));
}

/**
 * @brief Appends a styled message banner to the page buffer.
 *
 * @param type    Visual style of the banner (success, info, error, warning).
 * @param message Message text to display.
 */
void addMessage(WebMessageType_t type, String message) {
    String mclass, mtype;
    switch(type) {
        case MSG_SUCCESS:
            Device.ConstantGREEN();
            mclass = "success";
            mtype  = "[Headline_SUCCESS]";
            break;
        case MSG_INFO:
            mclass = "info";
            mtype  = "[Headline_INFORMATION]";
            break;
        case MSG_ERROR:
            Device.ConstantRED();
            mclass = "error";
            mtype  = "[Headline_ERROR]";
            break;
        case MSG_WARNING:
            mclass = "warning";
            mtype  = "[Headline_WARNING]";
            break;
    }
    page.replace("{web_message}", HTML_MESSAGE);
    page.replace("{message_class}", mclass);
    page.replace("{message_type}", mtype);
    page.replace("{message_content}", message);
}

/**
 * @brief
 *
 */
/**
 * @brief Selects and renders the configured landing page (1–4) or the default home page.
 */
void Choose_Landingpage(void) {
    checkPostContent();
    switch(Hydrom.Landingpage) {
        case 3:
            Page_Home();
            break;
        case 4:
            Page4_Landingpage();
            Hydrom.Landingpage -= 1;
                config.saveFS();
            break;
        case 5:
            Page3_Landingpage();
            Hydrom.Landingpage -= 1;
                config.saveFS();
            break;
        case 6://Networksettings
                Page2_Landingpage();
            Hydrom.Landingpage -= 1;
                config.saveFS();
            break;
        case 7:
            Page1_Landingpage();
            Hydrom.Landingpage -= 1;
                config.saveFS();
            break;
        default:
            Page_Home();
            break;
    }
    
}
/**
 * @brief Renders landing page variant 1 (gravity + temperature overview).
 */
void Page1_Landingpage(void) {

    // Clear Page
    page.clear();
    page = HTML_LAND1_CONTENT;
    page.replace("{CSS_Placeholder}", CSS);
    page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
    writeValues();
    // send the response last
    server.send(200, "text/html", page);
}
/**
 * @brief Renders landing page variant 2 (compact numeric display).
 */
void Page2_Landingpage(void) {
    // Clear Page
    page.clear();
    page = HTML_LAND2_CONTENT;
    if(!Connection_Failed){
        Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page2_Landingpage; Wifi Established");
        page.replace("{TEXT}", "[Text_Wizard_Wifi]");
    }else{
        Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page2_Landingpage; Wifi Connection failed");
        page.replace("{TEXT}", "");

        if(net.getInfoSSIDinRange()){
            addMessage(MSG_SUCCESS,HTML_SUCCESS_WIFI_FOUND);
        }else{
            addMessage(MSG_ERROR,HTML_ERROR_WIFI_NOT_FOUND);
        }  

        addMessage(MSG_ERROR,HTML_ERROR_WIFI);
    }

    page.replace("{CSS_Placeholder}", CSS);
        page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
    writeValues();
    // send the response last
    server.send(200, "text/html", page);
}
/**
 * @brief Renders landing page variant 3 (large-font minimalist display).
 */
void Page3_Landingpage(void) {
    // Clear Page
    page.clear();
    page = HTML_LAND3_CONTENT;
    if(net.ClientModeConnected()){
        Print_Info(4, Hydrom.current_Log_Level, "WebManager; Pag32_Landingpage; Wifi Established");
        addMessage(MSG_SUCCESS,HTML_SUCCESS_WIFI);
        Device.ConstantGREEN();
    }
    
    page.replace("{CSS_Placeholder}", CSS);
        page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
    writeValues();
    // send the response last
    server.send(200, "text/html", page);
}
/**
 * @brief Renders landing page variant 4 (battery + RSSI focused display).
 */
void Page4_Landingpage(void) {
    // Clear Page
    page.clear();
    page = HTML_LAND4_CONTENT;
    page.replace("{CSS_Placeholder}", CSS);
    page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
    writeValues();
    // send the response last
    server.send(200, "text/html", page);
}

/*
 * HTTP request handler for root page
 */
/**
 * @brief Renders the home dashboard page with current sensor readings.
 */
void Page_Home(void) {
    char l_titel[] = "[Titel_Home]";

    Print_Info(4, Hydrom.current_Log_Level, "Webpage " + String(l_titel) + " was choosen");
    // Clear Page
    page.clear();
    // Add_Head
    addHead(l_titel);
    // AddHeader
    addHeader();
    // ACTIONbar with connectionstatus without backbutton
    page += HTML_ACTIONHOME;
    page += "<main>";
    // Add Panel
    addPanel();
    page += HTML_HOME_CONTENT;
    page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
    //
    if(millis()>800000)
        addMessage(MSG_WARNING,HTML_WARN_HOME);
    // Overwrite the placeholders
    writeValues();
    // send the response last
    server.send(200, "text/html", page);
}
/*
 * HTTP 404 request handler
 */
/**
 * @brief Sends a 404 Not Found response for unknown routes.
 */
void Page_NotFound(void) {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for(uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

/**
 * @brief Appends the last calibration step result summary to the page buffer.
 */
void Show_result() {
        // Check for Content
    checkPostContent();
    Prepare_Page("[Titel_Calibration]", "[Headine_Conclusion]", "/Step7", HTML_CONTENT_CALIBRATION_CONCLUSION);
        // send the response last
    writeValues();
    server.send(200, "text/html", page);
}

/**
 * @brief Assembles a complete HTML page from the given parameters and sends it to the client.
 *
 * @param l_titel        Browser tab title.
 * @param l_headline     Page headline / action bar title.
 * @param l_backlink     URL for the back navigation button.
 * @param HTML_CONTENT   Main body HTML template string.
 */
void Prepare_Page(const char * l_titel, const char * l_headline, const char * l_backlink, const char * HTML_CONTENT) {
    Print_Info(4, Hydrom.current_Log_Level, "Prepare Webpage " + String(l_titel) + "");
    // Clear Page
    page.clear();
    // Add_Head
    addHead(l_titel);
    // AddHeader
    addHeader();
    // Actionbar
    addAction(l_headline, l_backlink);
    page += "<main>";
    // Add Panel
    addPanel();
    // Content
    page += HTML_CONTENT;
    page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
}

/**
 * @brief Start calibration with choosing the Methode
 *
 */
/**
 * @brief Renders the gravity calibration overview page.
 */
void Page_Calibration(void) {
        // Check for Content
    checkPostContent();
    Prepare_Page("[Titel_Calibration]", "[Titel_Calibration]", "/", HTML_CONTENT_CALIBRATION);
    if(show_Calibration_Result){
        show_Calibration_Result=false;
    //Check if Calibration was successfull
    if((configSensor.plato>=DEF_LIMIT_LOW_TOLLERANCE_PLATO&&configSensor.plato<=DEF_LIMIT_HIGH_TOLLERANCE_PLATO)&&(configSensor.gravity_PWC>=DEF_LIMIT_LOW_TOLLERANCE_GRAVITY&&configSensor.gravity_PWC<=DEF_LIMIT_HIGH_TOLLERANCE_GRAVITY)){//Check if Calibration was successfull
        Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page_Calibration; Calibration was successfull");
        const  char HTML_SUCCESS_PWC1[] = R"rawliteral(Plato before PWC: {plato_before_PWC}<br><br>YAR: {PlainWaterYar}<br>PITCH {PlainWaterPitch}<br>ROLL {PlainWaterRoll}<br>
    Gravity: {gravity_PWC}<br>
    Temperature: {temperature_PWC}<br>
    Temperature Drift: {temperature_drift}<br>
    <br>
    Plato after PWC: {plato_after_PWC})rawliteral";
        addMessage(MSG_INFO,HTML_SUCCESS_PWC1);
        const  char HTML_SUCCESS_PWC[] = R"rawliteral([Message_Calibration_Success])rawliteral";
        addMessage(MSG_SUCCESS,HTML_SUCCESS_PWC);

    }else{ 
    /**
     * @brief 
     * 
     */
        const  char HTML_ERROR_PWC[] = R"rawliteral([Message_Calibration_Error]<br>
        Plato before PWC: {plato_before_PWC}<br>
    <br>
    YAR: {PlainWaterYar}<br>
    PITCH {PlainWaterPitch}<br>
    ROLL {PlainWaterRoll}<br>
    Gravity: {gravity_PWC}<br>
    Temperature: {temperature_PWC}<br>
    Temperature Drift: {temperature_drift}<br>
    <br>
    Plato after PWC: {plato_after_PWC})rawliteral";
        addMessage(MSG_INFO,HTML_ERROR_PWC);
        Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page_Calibration; Calibration failed");


        if(configSensor.gravity_PWC<=DEF_LIMIT_LOW_TOLLERANCE_GRAVITY||configSensor.gravity_PWC>=DEF_LIMIT_HIGH_TOLLERANCE_GRAVITY){//Check if Gravity is in Tollerance
            Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page_Calibration; Gravity is not in Tollerance");
                    const  char HTML_ERROR_PWC[] = R"rawliteral(
                Value outside of tollerance<br>
                Gravity PWC: {gravity_PWC})rawliteral";
            addMessage(MSG_ERROR,HTML_ERROR_PWC);
        }

        if(configSensor.temperature_PWC<=DEF_LIMIT_LOW_TOLLERANCE_TEMPERATURE||configSensor.temperature_PWC>=DEF_LIMIT_HIGH_TOLLERANCE_TEMPERATURE){//Check if Temperature is in Tollerance
            Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page_Calibration; Temperature is not in Tollerance");
                    const  char HTML_ERROR_PWC[] = R"rawliteral(
                Value outside of tollerance<br>
                Temperature PWC: {temperature_PWC})rawliteral";
            addMessage(MSG_ERROR,HTML_ERROR_PWC);
        }
        
        if(configSensor.temperature_drift<=DEF_LIMIT_LOW_TOLLERANCE_TEMPERATURE_DRIFT||configSensor.temperature_drift>=DEF_LIMIT_HIGH_TOLLERANCE_TEMPERATURE_DRIFT){//Check if Temperature Drift is in Tollerance
            Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page_Calibration; Temperature Drift is not in Tollerance");
                    const  char HTML_ERROR_PWC[] = R"rawliteral(
                Value outside of tollerance<br>
                Temperature Drift PWC: {temperature_drift})rawliteral";
            addMessage(MSG_ERROR,HTML_ERROR_PWC);
        }

        if(configSensor.plato_after_PWC<=DEF_LIMIT_LOW_TOLLERANCE_PLATO||configSensor.plato_after_PWC>=DEF_LIMIT_HIGH_TOLLERANCE_PLATO){//Check if Plato after PWC is in Tollerance
            Print_Info(4, Hydrom.current_Log_Level, "WebManager; Page_Calibration; Plato after PWC is not in Tollerance");
            const  char HTML_ERROR_PWC[] = R"rawliteral(
                Value outside of tollerance<br>
                Plato after PWC: {plato_after_PWC})rawliteral";
            addMessage(MSG_ERROR,HTML_ERROR_PWC);
        }
    
}
}
    // send the response last
    writeValues();
    server.send(200, "text/html", page);
}
/**
 * @brief Renders the miscellaneous device settings page (name, language, sleep time, etc.).
 */
void Page_Settings_Other(void) {
        // Check for Content
    checkPostContent();
    Prepare_Page("[Titel_Settings]", "[Titel_Settings]", "/", HTML_CONTENT_SETTINGS);
    // send the response last
    writeValues();
    server.send(200, "text/html", page);
}
/**
 * @brief Renders the services configuration page (Brewfather, MQTT, InfluxDB, etc.).
 */
void Page_Settings_Services(void) {
        // Check for Content
    checkPostContent();
    Prepare_Page("[Titel_Service]", "[Titel_Service]", "/", HTML_SERVICE_CONTENT);
    if(TestMessage_state){
        while(Device.getTestMessage_state()){
            delay(500);
            Print_Info(3, Hydrom.current_Log_Level, ".");
        }
            
        if(!Brewblox.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Brewblox<br>"+Brewblox.Response_Text);
       if(!Brewfather.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Brewfather<br>"+Brewfather.Response_Text);
        if(!BierBot.Response_Text.isEmpty())
           addMessage(MSG_INFO, "BierBot<br>"+BierBot.Response_Text);
        if(!Craftbeerpi.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Craftbeerpi<br>"+Craftbeerpi.Response_Text);
        if(!Fhem.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Fhem<br>"+Fhem.Response_Text);
        if(!Grainfather.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Grainfather<br>"+Grainfather.Response_Text);
        if(!Http.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Http<br>"+Http.Response_Text);
        if(!InfluxDB.Response_Text.isEmpty())
            addMessage(MSG_INFO, "InfluxDB<br>"+InfluxDB.Response_Text);
        if(!Mqtt.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Mqtt<br>"+Mqtt.Response_Text);
        if(!Prometheus.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Prometheus<br>"+Prometheus.Response_Text);
        if(!Tcontrol.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Tcontrol<br>"+Tcontrol.Response_Text);
        if(!Tcp.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Tcp<br>"+Tcp.Response_Text);
        if(!Telegram.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Telegram<br>"+Telegram.Response_Text);
        if(!ThingSpeak.Response_Text.isEmpty())
            addMessage(MSG_INFO, "ThingSpeak<br>"+ThingSpeak.Response_Text);
        if(!Ubidots.Response_Text.isEmpty())
            addMessage(MSG_INFO, "Ubidots<br>"+Ubidots.Response_Text);
        if(!GoogleSheets.Response_Text.isEmpty())
            addMessage(MSG_INFO, "GoogleSheets<br>"+GoogleSheets.Response_Text);
    }

    // send the response last
    writeValues();
    server.send(200, "text/html", page);
}
/**
 * @brief Renders the WiFi / network settings page.
 */
void Page_Settings_WIFI(void) {
        // Check for Content
    checkPostContent();
    Prepare_Page("[Titel_Wifi_Settings]", "[Titel_Wifi_Settings]", "/", HTML_CONTENT_WIFI);
    if(!Wifi.mode==MODE_SERVER){
        if(Connection_Failed){
            Print_Error("Page Settings Wifi Connection failed");
                    if(net.getInfoSSIDinRange()){
            addMessage(MSG_SUCCESS,HTML_SUCCESS_WIFI_FOUND);
            addMessage(MSG_ERROR,HTML_ERROR_WIFI);
        }else{
            addMessage(MSG_ERROR,HTML_ERROR_WIFI_NOT_FOUND);
        }  
        }else{
            if(net.ClientModeConnected()){
                Print_Info(4, Hydrom.current_Log_Level, "Page Settings Wifi ClientModeConnected() was positiv");
                addMessage(MSG_SUCCESS,HTML_SUCCESS_WIFI);
            }else{
                        if(net.getInfoSSIDinRange()){
            addMessage(MSG_SUCCESS,HTML_SUCCESS_WIFI_FOUND);
        }else{
            addMessage(MSG_ERROR,HTML_ERROR_WIFI_NOT_FOUND);
        }  

        addMessage(MSG_ERROR,HTML_ERROR_WIFI);
                Print_Error("Page Settings Wifi ClientModeConnected() was negativ");
            }
        }
    }else{
        addMessage(MSG_INFO,HTML_INFO_WIFI);
    }
    // send the response last
    writeValues();
    server.send(200, "text/html", page);
}
/**
 * @brief Renders the device information page (firmware version, chip ID, IP, etc.).
 */
void Page_Information(void) {
        // Check for Content
    checkPostContent();
    Prepare_Page("[Titel_Information]", "[Titel_Information]", "/", HTML_CONTENT_INFORMATION);
        // send the response last
    writeValues();
    server.send(200, "text/html", page);
}

/**
 * @brief Toggles the log-level boost that activates the debug navigation panel.
 */
void gotoDebugbode(void) {
        // Check for Content
    checkPostContent();
    Prepare_Page("DEBUGMODE", "DEBUGMODE", "/", HTML_CONTENT_DEBUGMODE);
        // send the response last
    writeValues();
    server.send(200, "text/html", page);
}


/**
 * @brief Renders the MPU6050 accelerometer/gyroscope calibration information page.
 */
void Page_MPU_Calibration(void) {
        // Check for Content
    checkPostContent();
    Prepare_Page("[Titel_MPU_Calibration]", "[Titel_MPU_Calibration]", "/", HTML_CONTENT_MPU);
        // send the response last
    writeValues();
    server.send(200, "text/html", page);
}

/**
 * @brief Renders calibration wizard step 1 (plain-water reference measurement).
 */
void Page_Calibration_Step1(void) {
    for(int i = 0; i < 10; i++) {
        configSensor.Step[i].AmountWater           = 0;
        configSensor.Step[i].AmountSugar           = 0;
        configSensor.Step[i].MeasuredTemperature   = 0.0;
        configSensor.Step[i].MeasuredGravity       = 0.0;
        configSensor.Step[i].isStable              = false;
        configSensor.Step[i].deviation             = 0.0;
        configSensor.Step[i].Plato                 = 0;
        configSensor.Step[i].Last_Calibration_Step = true;
    }

    prepare_next_step(1);
}
/**
 * @brief Renders calibration wizard step 2 (first wort sample measurement).
 */
void Page_Calibration_Step2(void) {
    Calibration_Step(2);
}
/**
 * @brief Renders calibration wizard step 3 (second wort sample measurement).
 */
void Page_Calibration_Step3(void) {
    Calibration_Step(3);
}
/**
 * @brief Renders calibration wizard step 4 (third wort sample measurement).
 */
void Page_Calibration_Step4(void) {
    Calibration_Step(4);
}
/**
 * @brief Renders calibration wizard step 5 (fourth wort sample measurement).
 */
void Page_Calibration_Step5(void) {
    Calibration_Step(5);
}
/**
 * @brief Renders calibration wizard step 6 (fifth wort sample measurement).
 */
void Page_Calibration_Step6(void) {
    Calibration_Step(6);
}
/**
 * @brief Renders calibration wizard step 7 (sixth wort sample measurement).
 */
void Page_Calibration_Step7(void) {
    Calibration_Step(7);
}

/**
 * @brief Renders a single calibration wizard data-entry step page.
 *
 * @param Step Step number (1–7); determines page content and navigation links.
 */
void Calibration_Step(int8_t Step) {
    Print_Info(4, Hydrom.current_Log_Level, "The calibration step " + String(Step) + " was triggered.");
    save_collected_data(Step - 1);
    // Here it was decided that the minimum number of measurement points to calculate a function is 3.
    if(configSensor.Step[2].Last_Calibration_Step || configSensor.Step[3].Last_Calibration_Step || configSensor.Step[4].Last_Calibration_Step || configSensor.Step[5].Last_Calibration_Step || configSensor.Step[6].Last_Calibration_Step) {
        Print_Info(4, Hydrom.current_Log_Level, "In the calibration step " + String(Step) + " the last step " + String(Step - 1) + " was identified.");

        for(int i = Step - 2; i <= 6; i++) {
            configSensor.Step[i].Last_Calibration_Step = true;
            Print_Info(4, Hydrom.current_Log_Level, "Step " + String(i) + " was reseted");
        }

        Calibration_Conclusion(Step - 1);
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "The last step was obviously not the last. Tested was step " + String(Step - 2));
        prepare_next_step(Step);
    }
}

/**
 * @brief Renders the calibration conclusion page after all steps are complete.
 */
void Page_Calibration_Conclusion(void) {
    save_collected_data(7);
    calibrator.calculate(7);
    postStatus = config.saveFS_Settings();
    Show_result();
}

/**
 * @brief Runs the polynomial curve-fit on the collected calibration data and displays the result.
 *
 * @param l_CalibrationSteps Number of calibration steps completed (must be > 2 for a valid fit).
 */
void Calibration_Conclusion(int8_t l_CalibrationSteps) {
    save_collected_data(7);

    calibrator.calculate(l_CalibrationSteps);
    postStatus = config.saveFS_Settings();
    Show_result();
}
/**
 * @brief Clears all recorded calibration data and returns the user to step 1.
 */
void Page_Calibration_Restart(void) {
    // All old values are overwritten with zeros.
    calibrator.clear();
    Page_Calibration();
}

/**
 * @brief
 *
 * @param method
 * @param step 1/Wasser 2/Zucker 3/Wasser 4/Wasser 5/Wasser 6/Wasser 7/Wasser
 */
/**
 * @brief Reads the form POST data submitted for a calibration step and advances to the next step.
 *
 * @param step Current step number (1–7); determines which form fields are read.
 */
void prepare_next_step(int8_t step) {
    char l_titel[]    = "Calibration";
    char l_Headline[] = "{Step_Headline}";
    char l_backlink[] = "/";

    // Clear Page
    page.clear();
    // Add_Head
    addHead(l_titel);
    // AddHeader
    addHeader();
    // Actionbar
    addAction(l_Headline, l_backlink);    //(char *)language.get(lang.CLOUD_SERVICES));
    page += "<main>";
    // Add Panel
    addPanel();
    page += HTML_CONTENT_CALIBRATION_;

    Print_Info(4, Hydrom.current_Log_Level, "We are currently in step " + String(step) + " in Reference Method");
    page.replace("{SUGAR_Methode}", "");
    page.replace("{CheckedReferencePlato}", "notchecked");
    page.replace("{ReferencePlatoVisibility}", "none");
    page.replace("{ReferencePlatoValue}", "0.000");

    page.replace("{CheckedReferenceSG}", "notchecked");
    page.replace("{ReferenceSGVisibility}", "none");
    page.replace("{ReferenceSGValue}", "1000");

    switch(step) {
        case 1:
            page.replace("{ReferencePlatoValue}", "0.00");
            page.replace("{SUGAR_Methode}", HTML_CONTENT_Step0_Sugar);
            page.replace("{REFERENCE_Methode}", "[Text_Step1_Reference_Methode]");
            break;
        case 2:
            page.replace("{ReferencePlatoValue}", "20.00");
            page.replace("{SUGAR_Methode}", "[Text_Step2_Sugar_Methode]");
            page.replace("{REFERENCE_Methode}", "[Text_Reference_Methode]");
            replace_Language_File();
            page.replace("amountofsugar", String((uint16_t)configSensor.Step[1].AmountSugar));
            break;
        case 3:
            page.replace("{ReferencePlatoValue}", "15.00");
            page.replace("{SUGAR_Methode}", "[Text_Step3_Sugar_Methode]");
            page.replace("{REFERENCE_Methode}", "[Text_Reference_Methode]");
            replace_Language_File();
            page.replace("xyx", String((uint16_t)configSensor.Step[2].AmountWater - configSensor.Step[1].AmountWater));
            break;
        case 4:
            page.replace("{ReferencePlatoValue}", "10.00");
             page.replace("{SUGAR_Methode}", "[Text_Step4_Sugar_Methode]");
            page.replace("{REFERENCE_Methode}", "[Text_Reference_Methode]");
            replace_Language_File();
            page.replace("xyx", String((uint16_t)configSensor.Step[3].AmountWater - configSensor.Step[2].AmountWater));
            break;
        case 5:
            page.replace("{ReferencePlatoValue}", "7.50");
            page.replace("{SUGAR_Methode}", "[Text_Step5_Sugar_Methode]");
            page.replace("{REFERENCE_Methode}", "[Text_Reference_Methode]");
            replace_Language_File();
            page.replace("xyx", String((uint16_t)configSensor.Step[4].AmountWater - configSensor.Step[3].AmountWater));
            break;
        case 6:
            page.replace("{ReferencePlatoValue}", "5.00");
             page.replace("{SUGAR_Methode}", "[Text_Step6_Sugar_Methode]");
            page.replace("{REFERENCE_Methode}", "[Text_Reference_Methode]");
            replace_Language_File();
            page.replace("xyx", String((uint16_t)configSensor.Step[5].AmountWater - configSensor.Step[4].AmountWater));
            break;
        case 7:
            page.replace("{ReferencePlatoValue}", "2.50");
            page.replace("{SUGAR_Methode}", "[Text_Step7_Sugar_Methode]");
            page.replace("{REFERENCE_Methode}", "[Text_Reference_Methode]");
            replace_Language_File();
            page.replace("xyx", String((uint16_t)configSensor.Step[6].AmountWater - configSensor.Step[5].AmountWater));
            page.replace("{next}", "/conclusion");
            page.replace("{STEP}", "/conclusion");
            break;
        default:
            Print_Error("Step not found!");

            break;
    }
    page.replace("{Step_Headline}", "[Headline_Schritt] {0}/7");
    page.replace("{STEP}", "/Step{1}");
    page.replace("{next}", "/Step{1}");
    page.replace("{0}", String(step));
    page.replace("{1}", String(step + 1));
    page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
    replace_Language_File();
    // send the response last
    server.send(200, "text/html", page);
}
/**
 * @brief Converts a specific gravity value (×1000) to degrees Plato.
 *
 * @param SG Specific gravity multiplied by 1000 (e.g. 1050 for SG 1.050).
 * @return double Approximate degrees Plato using the formula 259 - (259 / SG).
 */
double SG2Plato(int16_t SG) {
    Print_Info(5, Hydrom.current_Log_Level, "SG (" + String(SG) + ") will be converted to Plato (" + String(259 - (259 / SG), 3) + ")");
    return 259 - (259 / SG);
}
/**
 * @brief Stores the sensor readings captured during a calibration step into the configuration.
 *
 * @param step Calibration step index (0-based) used to address the step array.
 */
void save_collected_data(int8_t step) {
    int8_t index = step - 1;
    if(server.method() == HTTP_POST) {
        Print_Info(4, Hydrom.current_Log_Level, "HTTP Post was found");
        if(server.hasArg("ElementCalibration")) {
            Print_Info(4, Hydrom.current_Log_Level, "ElementCalibration was found");
            if((server.arg("ArgumentEnableReferencePlato").equals("on"))) {
                Print_Info(4, Hydrom.current_Log_Level, "ElementReferencePlato value was found");
                if(server.hasArg("ReferencePlatoValue"))
                    configSensor.Step[index].Plato = CheckForArg(configSensor.Step[index].Plato, "ReferencePlatoValue");
                configSensor.Step[index].Last_Calibration_Step = false;
            } else {
                if((server.arg("ArgumentEnableReferenceSG").equals("on"))) {
                    Print_Info(4, Hydrom.current_Log_Level, "ElementReferenceSG value was found");
                    if(server.hasArg("ReferenceSGValue"))
                        configSensor.Step[index].Plato = SG2Plato(CheckForArg(configSensor.Step[index].Plato, "ReferenceSGValue"));
                    configSensor.Step[index].Last_Calibration_Step = false;
                } else {
                    Print_Info(4, Hydrom.current_Log_Level, "No reference value was found. This means that a calibration is performed with the values collected so far.");
                    configSensor.Step[index - 1].Last_Calibration_Step = true;
                    Print_Info(4, Hydrom.current_Log_Level, String(configSensor.Step[0].Last_Calibration_Step) + String(configSensor.Step[1].Last_Calibration_Step) + String(configSensor.Step[2].Last_Calibration_Step) + String(configSensor.Step[3].Last_Calibration_Step) + String(configSensor.Step[4].Last_Calibration_Step) + String(configSensor.Step[5].Last_Calibration_Step) + String(configSensor.Step[6].Last_Calibration_Step));
                    return;
                }
            }
        }
        if(server.hasArg("AmountWater")) {
            Print_Info(4, Hydrom.current_Log_Level, "AmountWater value was found");
            configSensor.Step[0].AmountWater = CheckForArg(configSensor.Step[0].AmountWater, "AmountWater");
            float normal                     = configSensor.Step[0].AmountWater / 400;
            Print_Info(4, Hydrom.current_Log_Level, "The ratio of water to sugar is: " + String(normal, 4));
            configSensor.Step[1].AmountSugar = normal * 100;
            Print_Info(4, Hydrom.current_Log_Level, "It will take a total of " + String(configSensor.Step[1].AmountSugar, 4) + "g sugar");
            configSensor.Step[2].AmountSugar = normal * 100;
            configSensor.Step[3].AmountSugar = normal * 100;
            configSensor.Step[4].AmountSugar = normal * 100;
            configSensor.Step[5].AmountSugar = normal * 100;
            configSensor.Step[6].AmountSugar = normal * 100;
            configSensor.Step[1].AmountWater = configSensor.Step[0].AmountWater;
            configSensor.Step[2].AmountWater = configSensor.Step[1].AmountWater + normal * 166;
            configSensor.Step[3].AmountWater = configSensor.Step[2].AmountWater + normal * 333;
            configSensor.Step[4].AmountWater = configSensor.Step[3].AmountWater + normal * 333;
            configSensor.Step[5].AmountWater = configSensor.Step[4].AmountWater + normal * 666;
            configSensor.Step[6].AmountWater = configSensor.Step[5].AmountWater + normal * 2000;
        }
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "No http post was found in Step " + String(step));
    }

    if(index <= sizeof(configSensor.Step)) {
//        while(!sensormanager.getStableState())
//            sensormanager.resetStableState();
//        Print_Info(4, Hydrom.current_Log_Level, "stable Gravity Value was found!");
        configSensor.Step[index].MeasuredGravity = configSensor.gravity;
        Print_Info(4, Hydrom.current_Log_Level, "The Measured Gravity is: " + String(configSensor.Step[index].MeasuredGravity) + "°G");
        configSensor.Step[index].MeasuredTemperature = configSensor.temperature;
        configSensor.Step[index].isStable            = true;
    } else {
        Print_Error("requested index is higer then the Array!");
        Device.ConstantRED();
    }
}

/**
 * HTTP POST request handler for a factory reset
 */
/**
 * @brief Handles the /reset route: performs a factory reset and restarts the device.
 */
void Page_Reset(void) {
    config.factoryReset();
}
/*
 * HTTP 404 request handler
 */
/**
 * @brief Sends a 404 Not Found response for unregistered routes.
 */
void onNotFound(void) {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for(uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

/*
 * HTTP Request handler for firmware update page
 */
/**
 * @brief Renders the OTA firmware update upload page.
 */
void onUpdatePage(void) {
        // Check for Content
    //checkPostContent();
    Prepare_Page("[Titel_Update]", "[Titel_Update]", "/", HTML_CONTENT_UPDATE);
    if(server.method() == HTTP_POST) {
        if(updateValid) {
            addMessage(MSG_SUCCESS,HTML_SUCCESS_UPDATE);
        } else {
            addMessage(MSG_ERROR,HTML_ERROR_UPDATE);
        }
    } else {
    }
    // send the response last
    writeValues();
    server.send(200, "text/html", page);
}
/*
 * HTTP Request handler for firmware update upload page
 */

/**
 * @brief Handles the streaming firmware binary upload for OTA updates.
 *        Writes incoming chunks to the Update library and finalises on completion.
 */
void onUpdatePageUpload(void) {
    HTTPUpload & upload = server.upload();
    if(configSensor.batteryPercentage >= 90||true) {
        if(upload.status == UPLOAD_FILE_START) {
            Print_Info(4, Hydrom.current_Log_Level, "STARTING FIRMWARE UPDATE: ");
            DEBUG_PRINT(upload.filename.c_str());
            Print_Info(4, Hydrom.current_Log_Level, "");

            updateValid = false;
            // check if its settings or firmware update
            if(upload.filename.indexOf(".bin") != -1) {
                Print_Info(4, Hydrom.current_Log_Level, "Update is Firmware update");
                // start with max available size
                if(!Update.begin()) {    // start with max available size
                    Update.printError(Serial);
                    Device.ConstantRED();
                }else{
                    Print_Info(4, Hydrom.current_Log_Level, "Update.begin = true");
                }
                updateValid = true;
            } else {
                Print_Error("Invalid update file! Aborting");
                updateValid = false;
                onUpdatePage();
            }
        } else if(upload.status == UPLOAD_FILE_WRITE) {
            if(updateValid) {
                if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                    Device.ConstantRED();
                    Print_Error("Failed to write Upload");
                }
                DEBUG_PRINT(".");
                //Update.size()/100
                //Update.
                //Print_Info(4, Hydrom.current_Log_Level, String(Update.size()-Update.progress()));
            }else{
                Print_Info(4, Hydrom.current_Log_Level, "Status=File.Write;updateValid = false");
            }
            
        } else if(upload.status == UPLOAD_FILE_END) {
            Device.ConstantGREEN();
            if(updateValid) {
                Print_Info(4, Hydrom.current_Log_Level, "File was Uploaded");
                if(Update.end(true)) {    // true to set the size to the current progress
                    Device.setrebootstart();
                    onUpdatePage();
                    Print_Info(4, Hydrom.current_Log_Level, "Update Success: %u\nRebooting...\n" + upload.totalSize);
                } else {
                    Print_Error("Termination failed");
                    updateValid=false;
                    onUpdatePage();
                    Update.printError(Serial);
                    Device.ConstantRED();
                }
                Serial.setDebugOutput(false);
            }
            postStatus = true;
            Print_Info(4, Hydrom.current_Log_Level, "Upload: END, Size: ");
            DEBUG_PRINT(upload.totalSize);
        } else if(upload.status == UPLOAD_FILE_ABORTED) {
            if(updateValid)
                Update.end();
            Print_Error("Update was aborted");
            postStatus = false;
        }
    } else {
        Print_Error("Hydrom is not Charging");
    }
}
/**
 * @brief Reads all submitted form values from the HTTP POST request and persists them to SPIFFS.
 */
void writeValues() {


    Print_Info(4, Hydrom.current_Log_Level, "Start Replacing Placeholder");
    
    String l_formular = "P={FormularXXXXXX}{OperatorXXXXXX}{FormularXXXXX}{OperatorXXXXX}{FormularXXXX}{OperatorXXXX}{FormularXXX}{OperatorXXX}{FormularXX}{OperatorXX}{FormularX}{OperatorXD}{FormularD}";
    if(Hydrom.Coefficients[0] > 0) {
        l_formular.replace("{OperatorXD}", "+");
    } else {
        l_formular.replace("{OperatorXD}", "");
    }
    if(Hydrom.Coefficients[1] > 0) {
        l_formular.replace("{OperatorX}", "+");
    } else {
        l_formular.replace("{OperatorX}", "");
    }
    if(Hydrom.Coefficients[2] > 0) {
        l_formular.replace("{OperatorXX}", "+");
    } else {
        l_formular.replace("{OperatorXX}", "");
    }
    if(Hydrom.Coefficients[3] > 0) {
        l_formular.replace("{OperatorXXX}", "+");
    } else {
        l_formular.replace("{OperatorXXX}", "");
    }
    if(Hydrom.Coefficients[4] > 0) {
        l_formular.replace("{OperatorXXXX}", "+");
    } else {
        l_formular.replace("{OperatorXXXX}", "");
    }
    if(Hydrom.Coefficients[5] > 0) {
        l_formular.replace("{OperatorXXXXX}", "+");
    } else {
        l_formular.replace("{OperatorXXXXX}", "");
    }
    if(Hydrom.Coefficients[6] > 0) {
        l_formular.replace("{OperatorXXXXXX}", "+");
    } else {
        l_formular.replace("{OperatorXXXXXX}", "");
    }
    l_formular.replace("{FormularXXXXXX}", String(config.check_Plausibility("Hydrom.Coefficient 6", Hydrom.Coefficients[6], -10, 10), 15));
    l_formular.replace("{FormularXXXXX}", String(config.check_Plausibility("Hydrom.Coefficient 5", Hydrom.Coefficients[5], -10, 10), 15));
    l_formular.replace("{FormularXXXX}", String(config.check_Plausibility("Hydrom.Coefficient 4", Hydrom.Coefficients[4], -10, 10), 15));
    l_formular.replace("{FormularXXX}", String(config.check_Plausibility("Hydrom.Coefficient 3", Hydrom.Coefficients[3], -10, 10), 15));
    l_formular.replace("{FormularXX}", String(config.check_Plausibility("Hydrom.Coefficient 2", Hydrom.Coefficients[2], -100, 100), 15));
    l_formular.replace("{FormularX}", String(config.check_Plausibility("Hydrom.Coefficient 1", Hydrom.Coefficients[1], -1500, 1500), 15));
    l_formular.replace("{FormularD}", String(config.check_Plausibility("Hydrom.Coefficient 0", Hydrom.Coefficients[0], -30000, 30000), 6));

    page.replace("{FormularXXXXXX}", String(config.check_Plausibility("Hydrom.Coefficient 6", Hydrom.Coefficients[6], -10, 10), 15));
    page.replace("{FormularXXXXX}", String(config.check_Plausibility("Hydrom.Coefficient 5", Hydrom.Coefficients[5], -10, 10), 15));
    page.replace("{FormularXXXX}", String(config.check_Plausibility("Hydrom.Coefficient 4", Hydrom.Coefficients[4], -10, 10), 15));
    page.replace("{FormularXXX}", String(config.check_Plausibility("Hydrom.Coefficient 3", Hydrom.Coefficients[3], -10, 10), 15));
    page.replace("{FormularXX}", String(config.check_Plausibility("Hydrom.Coefficient 2", Hydrom.Coefficients[2], -100, 100), 15));
    page.replace("{FormularX}", String(config.check_Plausibility("Hydrom.Coefficient 1", Hydrom.Coefficients[1], -1500, 1500), 15));
    page.replace("{FormularD}", String(config.check_Plausibility("Hydrom.Coefficient 0", Hydrom.Coefficients[0], -30000, 30000), 6));
    page.replace("{Firmwareversion_current}", Firmwareversion);

    

    page.replace("{Firmwareversion_new}", updater.getLatestVersion(DEF_GITHUB_REPO));

    
    float l_diff_Temperature = 0;
    Print_Info(5, Hydrom.current_Log_Level, "Replacing Placeholder Temperature Unit: " + String(configSensor.temperature_unit));
    switch(configSensor.temperature_unit) {
        case C:
            Print_Info(5, Hydrom.current_Log_Level, "Temperature Unit Placeholder was replaced by Celsius");
            l_diff_Temperature = configSensor.temperature - last_temperature_celsius;
            page.replace("{Display_TemperatureUnit}", "°C");
            page.replace("{Temp_selected_1}", "selected");
            last_temperature_celsius = configSensor.temperature;
            page.replace("{Temperature}", String(configSensor.temperature));
            break;
        case F:
            Print_Info(5, Hydrom.current_Log_Level, "Temperature Unit Placeholder was replaced by Kelvin");
            l_diff_Temperature = sensormanager.get_converted_temperature(1) - last_temperature_Fahrenheit;
            page.replace("{Display_TemperatureUnit}", "°F");
            page.replace("{Temp_selected_2}", "selected");
            last_temperature_Fahrenheit = sensormanager.get_converted_temperature(1);
            page.replace("{Temperature}", String(sensormanager.get_converted_temperature(1)));
            break;
        case K:
            Print_Info(5, Hydrom.current_Log_Level, "Temperature Unit Placeholder was replaced by Kelvin");
            l_diff_Temperature = sensormanager.get_converted_temperature(2) - last_temperature_Kelvin;
            page.replace("{Display_TemperatureUnit}", "K");
            page.replace("{Temp_selected_3}", "selected");
            last_temperature_Kelvin = sensormanager.get_converted_temperature(2);
            page.replace("{Temperature}", String(sensormanager.get_converted_temperature(2)));
            break;
    }


    
    switch (Hydrom.current_Language)
    {
        case 0:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by German");
            page.replace("{Language_selected_0}", "selected");
        break;
        case 1:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by English");
            page.replace("{Language_selected_1}", "selected");
        break;
        case 2:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by Itlian");
            page.replace("{Language_selected_2}", "selected");
        break;
        case 3:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by Espaniol");
            page.replace("{Language_selected_3}", "selected");
        break;
        case 4:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by Frensh");
            page.replace("{Language_selected_4}", "selected");
        break;
        case 5:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by Dutch");
            page.replace("{Language_selected_5}", "selected");
        break;
        case 6:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by Portogisian");
            page.replace("{Language_selected_6}", "selected");
        break;
        case 7:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by Swedish");
            page.replace("{Language_selected_7}", "selected");
        break;
        case 8:
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by Finnland");
            page.replace("{Language_selected_8}", "selected");
        break;
            Print_Info(5, Hydrom.current_Log_Level, "Language Placeholder was replaced by English caused by default");
            page.replace("{Language_selected_1}", "selected");
    default:

        break;
}


page.replace("{Language_selected_0}", "");
page.replace("{Language_selected_1}", "");
page.replace("{Language_selected_2}", "");
page.replace("{Language_selected_3}", "");
page.replace("{Language_selected_4}", "");
page.replace("{Language_selected_5}", "");
page.replace("{Language_selected_6}", "");
page.replace("{Language_selected_7}", "");
page.replace("{Language_selected_8}", "");

switch (Hydrom.current_Log_Level)
    {
        case 0:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by German");
            page.replace("{debugmode_selected_0}", "selected");
        break;
        case 1:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by English");
            page.replace("{debugmode_selected_1}", "selected");
        break;
        case 2:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by Itlian");
            page.replace("{debugmode_selected_2}", "selected");
        break;
        case 3:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by Espaniol");
            page.replace("{debugmode_selected_3}", "selected");
        break;
        case 4:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by Frensh");
            page.replace("{debugmode_selected_4}", "selected");
        break;
        case 5:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by Dutch");
            page.replace("{debugmode_selected_5}", "selected");
        break;
        case 6:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by Portogisian");
            page.replace("{debugmode_selected_6}", "selected");
        break;
        case 7:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by Swedish");
            page.replace("{debugmode_selected_7}", "selected");
        break;
        case 8:
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by Finnland");
            page.replace("{debugmode_selected_8}", "selected");
        break;
            Print_Info(5, Hydrom.current_Log_Level, "debugmode Placeholder was replaced by English caused by default");
            page.replace("{debugmode_selected_1}", "selected");
    default:

        break;
}
page.replace("{debugmode_selected_0}", "");
page.replace("{debugmode_selected_1}", "");
page.replace("{debugmode_selected_2}", "");
page.replace("{debugmode_selected_3}", "");
page.replace("{debugmode_selected_4}", "");
page.replace("{debugmode_selected_5}", "");
page.replace("{debugmode_selected_6}", "");
page.replace("{debugmode_selected_7}", "");
page.replace("{debugmode_selected_8}", "");


    page.replace("{diff_Temperature}", String(l_diff_Temperature));
    if(l_diff_Temperature > 0) {
        page.replace("{sign_diff_Temperature}", "+");
    } else {
        page.replace("{sign_diff_Temperature}", "");
    }
    page.replace("{Temp_selected_1}", "");
    page.replace("{Temp_selected_2}", "");
    page.replace("{Temp_selected_3}", "");

    float l_diff_Tilt = 0;
    Print_Info(5, Hydrom.current_Log_Level, "Replacing Placeholder Tilt Unit: " + String(configSensor.tilt_unit));
    if(!configSensor.Enable_TemperatureCompensation)
        page.replace("({sign_Plato_Comp}{diff_PLato_Comp}{Display_TiltUnit})","");

    switch(configSensor.tilt_unit) {
        case P:
            Print_Info(5, Hydrom.current_Log_Level, "Tilt Unit Placeholder was replaced by Plato");
            l_diff_Tilt = configSensor.plato - last_plato;
            page.replace("{Plato}", String(configSensor.plato));
            page.replace("{Display_TiltUnit}", "°P");

            if(configSensor.Correction_plato > 0) {
                page.replace("{sign_Plato_Comp}", "+");
            } else {
                page.replace("{sign_Plato_Comp}", "");
            }
            page.replace("{diff_PLato_Comp}",String(configSensor.Correction_plato));

            page.replace("{TiltName}", "[Label_Tilt_Option_1]");
            page.replace("{Tilt_selected_1}", "selected");
            page.replace("{diff_Tilt}", String(l_diff_Tilt, 2));
            last_plato = configSensor.plato;
            break;
        case SG:
            Print_Info(5, Hydrom.current_Log_Level, "Tilt Unit Placeholder was replaced by SG");
            l_diff_Tilt = configSensor.specific_gravity - last_specific_gravity;
            page.replace("{Plato}", String(configSensor.specific_gravity, 3));
            page.replace("{Display_TiltUnit}", "SG");

            if(configSensor.Correction_specific_gravity > 0) {
                page.replace("{sign_Plato_Comp}", "+");
            } else {
                page.replace("{sign_Plato_Comp}", "");
            }
            page.replace("{diff_PLato_Comp}",String(configSensor.Correction_specific_gravity));

            page.replace("{TiltName}", "[Label_Tilt_Option_2]");
            page.replace("{Tilt_selected_2}", "selected");
            page.replace("{diff_Tilt}", String(l_diff_Tilt, 3));
            last_specific_gravity = configSensor.specific_gravity;
            break;
        case G:
            Print_Info(5, Hydrom.current_Log_Level, "Tilt Unit Placeholder was replaced by Gravity");
            l_diff_Tilt = configSensor.gravity - last_gravity;
            page.replace("{Plato}", String(configSensor.gravity));
            page.replace("({sign_Plato_Comp}{diff_PLato_Comp}{Display_TiltUnit})","");
            page.replace("{Display_TiltUnit}", "°");
            page.replace("{TiltName}", "[Label_Tilt_Option_3]");
            page.replace("{Tilt_selected_3}", "selected");
            page.replace("{diff_Tilt}", String(l_diff_Tilt, 2));
            last_gravity = configSensor.gravity;
            break;
    }
    //here the Language Placeholder will be replaced
    replace_Language_File();

    page.replace("{Plato_before_PWC}", String(configSensor.plato_before_PWC, 2));
    page.replace("{Plato_after_PWC}", String(configSensor.plato_after_PWC, 2));
    page.replace("{Gravity_PWC}", String(configSensor.gravity_PWC, 2));
    page.replace("{Temperature_PWC}", String(configSensor.temperature_PWC, 2));
    page.replace("{Temperature_Drift}", String(configSensor.temperature_drift, 5));



    page.replace("{Devicename}", Hydrom.name);
    page.replace("{IPAdresse}", net.getIPAdress().toString());
    if(l_diff_Tilt > 0) {
        page.replace("{sign_Tilt}", "+");
    } else {
        page.replace("{sign_Tilt}", "");
    }

    page.replace("{Tilt_selected_1}", "");
    page.replace("{Tilt_selected_2}", "");
    page.replace("{Tilt_selected_3}", "");

    page.replace("{MPUAccelx}", String(Hydrom.accelOffset[0]));
    page.replace("{MPUAccely}", String(Hydrom.accelOffset[1]));
    page.replace("{MPUAccelz}", String(Hydrom.accelOffset[2]));
    page.replace("{MPUGyrox}", String(Hydrom.gyroOffset[0]));
    page.replace("{MPUGyroy}", String(Hydrom.gyroOffset[1]));
    page.replace("{MPUGyroz}", String(Hydrom.gyroOffset[2]));

    page.replace("{ClientSSID}", Wifi.client_ssid);
    page.replace("{ClientPassword}", Wifi.client_password);
    page.replace("{ServerSSID}", Hydrom.name);
    page.replace("{ServerPassword}", Wifi.server_password);

    switch(Bluetooth.UUID_ID) {
        case 1:
            page.replace("{UUID_selected_1}", "selected");
            break;
        case 2:
            page.replace("{UUID_selected_2}", "selected");
            break;
        case 3:
            page.replace("{UUID_selected_3}", "selected");
            break;
        case 4:
            page.replace("{UUID_selected_4}", "selected");
            break;
        case 5:
            page.replace("{UUID_selected_5}", "selected");
            break;
        case 6:
            page.replace("{UUID_selected_6}", "selected");
            break;
        case 7:
            page.replace("{UUID_selected_7}", "selected");
            break;
        default:
            break;
    }

    switch(Bluetooth.BLETransPower) {
        case 0:
            page.replace("{PWR_selected_0}", "selected");
            break;  
        case 1:
            page.replace("{PWR_selected_1}", "selected");
            break;
        case 2:
            page.replace("{PWR_selected_2}", "selected");
            break;
        case 3:
            page.replace("{PWR_selected_3}", "selected");
            break;
        case 4:
            page.replace("{PWR_selected_4}", "selected");
            break;
        case 5:
            page.replace("{PWR_selected_5}", "selected");
            break;
        case 6:
            page.replace("{PWR_selected_6}", "selected");
            break;
        case 7:
            page.replace("{PWR_selected_7}", "selected");
            break;
        default:
            break;
    }

    page.replace("{UUID_selected_1}", "");
    page.replace("{UUID_selected_2}", "");
    page.replace("{UUID_selected_3}", "");
    page.replace("{UUID_selected_4}", "");
    page.replace("{UUID_selected_5}", "");
    page.replace("{UUID_selected_6}", "");
    page.replace("{UUID_selected_7}", "");
     page.replace("{PWR_selected_0}", "");   
    page.replace("{PWR_selected_1}", "");
    page.replace("{PWR_selected_2}", "");
    page.replace("{PWR_selected_3}", "");
    page.replace("{PWR_selected_4}", "");
    page.replace("{PWR_selected_5}", "");
    page.replace("{PWR_selected_6}", "");
    page.replace("{PWR_selected_7}", "");

    if(Wifi.mode == MODE_CLIENT||Wifi.mode == MODE_SERVER_CLIENT) {
        page.replace("{CheckedClient}", "checked");
        page.replace("{CheckedServer}", "notchecked");
        page.replace("{ClientVisibility}", "inline");
        page.replace("{ServerVisibility}", "none");
    } else {
        page.replace("{CheckedClient}", "notchecked");
        page.replace("{CheckedServer}", "checked");
        page.replace("{ClientVisibility}", "none");
        page.replace("{ServerVisibility}", "inline");
    }
    if(Bluetooth.Enabled) {
        page.replace("{CheckedBluetooth}", "checked");
        page.replace("{BluetoothVisibility}", "flex");
    } else {
        page.replace("{CheckedBluetooth}", "notchecked");
        page.replace("{BluetoothVisibility}", "none");
    }

    if(Mqtt.Enabled) {
        page.replace("{CheckedMqtt}", "checked");
        page.replace("{MqttVisibility}", "flex");
    } else {
        page.replace("{CheckedMqtt}", "notchecked");
        page.replace("{MqttVisibility}", "none");
    }
    page.replace("{MqttServer}", String(Mqtt.TargetServer));
    page.replace("{MqttPort}", String(Mqtt.TargetPort));
    page.replace("{MqttUsername}", String(Mqtt.user));
    page.replace("{MqttPassword}", String(Mqtt.password));
    page.replace("{MqttTopicLevel}", String(Mqtt.TopicLevel));

    
    page.replace("{CheckedDeepSleep}", "checked");
    page.replace("{DeepSleepVisibility}", "clumn");
    page.replace("{DeepSleepHour}", String(DeepSleep.hours));
    page.replace("{DeepSleepMinute}", String(DeepSleep.minutes));
    page.replace("{DeepSleepSecond}", String(DeepSleep.seconds));

    if(configSensor.Enable_TemperatureCompensation) {
        page.replace("{CheckedTempComp}", "checked");
        page.replace("{TempCompVisibility}", "flex");
    } else {
        page.replace("{CheckedTempComp}", "notchecked");
        page.replace("{TempCompVisibility}", "none");
    }
    
    if(configSensor.Enable_RecHeadstand) {
        page.replace("{CheckedRecHeadstand}", "checked");
        page.replace("{RecHeadstandVisibility}", "flex");
    } else {
        page.replace("{CheckedRecHeadstand}", "notchecked");
        page.replace("{RecHeadstandVisibility}", "none");
    }

    if(Brewblox.Enabled) {
        page.replace("{CheckedBrewblox}", "checked");
        page.replace("{BrewbloxVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(Brewblox.TargetServer)) {
                page.replace("{BrewbloxServerReachable}", "#99c03c");
            } else {
                page.replace("{BrewbloxServerReachable}", "#FF0000");
            }
            if(net.PortOpen(Brewblox.TargetServer, Brewblox.TargetPort)) {
                page.replace("{BrewbloxPortOpen}", "#99c03c");
            } else {
                page.replace("{BrewbloxPortOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedBrewblox}", "notchecked");
        page.replace("{BrewbloxVisibility}", "none");
    }
    page.replace("{BrewbloxServer}", String(Brewblox.TargetServer));
    page.replace("{BrewbloxPort}", String(Brewblox.TargetPort));
    page.replace("{BrewbloxTopic}", String(Brewblox.TopicLevel));

    if(Brewfather.Enabled) {
        page.replace("{CheckedBrewfather}", "checked");
        page.replace("{BrewfatherVisibility}", "flex");
    } else {
        page.replace("{CheckedBrewfather}", "notchecked");
        page.replace("{BrewfatherVisibility}", "none");
    }
    page.replace("{BrewfatherURI}", String(Brewfather._url));

    if(BierBot.Enabled) {
        page.replace("{CheckedBierBot}", "checked");
        page.replace("{BierBotVisibility}", "flex");
    } else {
        page.replace("{CheckedBierBot}", "notchecked");
        page.replace("{BierBotVisibility}", "none");
    }
    page.replace("{BierBotToken}", String(BierBot.Token));

    if(Craftbeerpi.Enabled) {
        page.replace("{CheckedCraftbeerpi}", "checked");
        page.replace("{CraftbeerpiVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(Craftbeerpi.TargetServer)) {
                page.replace("{CraftbeerpiServerReachable}", "#99c03c");
            } else {
                page.replace("{CraftbeerpiServerReachable}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedCraftbeerpi}", "notchecked");
        page.replace("{CraftbeerpiVisibility}", "none");
    }
    page.replace("{CraftbeerpiServer}", String(Craftbeerpi.TargetServer));

    if(Fhem.Enabled) {
        page.replace("{CheckedFhem}", "checked");
        page.replace("{FhemVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(Fhem.TargetServer)) {
                page.replace("{FhemServerReachable}", "#99c03c");
            } else {
                page.replace("{FhemServerReachable}", "#FF0000");
            }
            if(net.PortOpen(Fhem.TargetServer, Fhem.TargetPort)) {
                page.replace("{FhemPortOpen}", "#99c03c");
            } else {
                page.replace("{FhemPortOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedFhem}", "notchecked");
        page.replace("{FhemVisibility}", "none");
    }
    page.replace("{FhemServer}", String(Fhem.TargetServer));
    page.replace("{FhemPort}", String(Fhem.TargetPort));

    if(Grainfather.Enabled) {
        page.replace("{CheckedGrainfather}", "checked");
        page.replace("{GrainfatherVisibility}", "flex");
    } else {
        page.replace("{CheckedGrainfather}", "notchecked");
        page.replace("{GrainfatherVisibility}", "none");
    }
    page.replace("{GrainfatherURI}", String(Grainfather._url));
    if(Http.Enabled) {
        page.replace("{CheckedHttp}", "checked");
        page.replace("{HttpVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(Http.TargetServer)) {
                page.replace("{HttpServerReachable}", "#99c03c");
            } else {
                page.replace("{HttpServerReachable}", "#FF0000");
            }
            if(net.PortOpen(Http.TargetServer, Http.TargetPort)) {
                page.replace("{HttpPortOpen}", "#99c03c");
            } else {
                page.replace("{HttpPortOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedHttp}", "notchecked");
        page.replace("{HttpVisibility}", "none");
    }
    page.replace("{HttpServer}", String(Http.TargetServer));
    page.replace("{HttpPort}", String(Http.TargetPort));
    page.replace("{HttpURI}", String(Http._url));

    if(Mqtt.Enabled) {
        page.replace("{CheckedMqtt}", "checked");
        page.replace("{MqttVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(Mqtt.TargetServer)) {
                page.replace("{MqttServerReachable}", "#99c03c");
            } else {
                page.replace("{MqttServerReachable}", "#FF0000");
            }
            if(net.PortOpen(Mqtt.TargetServer, Mqtt.TargetPort)) {
                page.replace("{MqttPortOpen}", "#99c03c");
            } else {
                page.replace("{MqttPortOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedMqtt}", "notchecked");
        page.replace("{MqttVisibility}", "none");
    }
    page.replace("{MqttServer}", String(Mqtt.TargetServer));
    page.replace("{MqttPort}", String(Mqtt.TargetPort));
    page.replace("{MqttUsername}", String(Mqtt.user));
    page.replace("{MqttPassword}", String(Mqtt.password));
    page.replace("{MqttTopicLevel}", String(Mqtt.TopicLevel));

    if(InfluxDB.Enabled) {
        page.replace("{CheckedInfluxDB}", "checked");
        page.replace("{InfluxDBVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(InfluxDB.TargetServer)) {
                page.replace("{InfluxDBServerReachable}", "#99c03c");
            } else {
                page.replace("{InfluxDBServerReachable}", "#FF0000");
            }
            if(net.PortOpen(InfluxDB.TargetServer, InfluxDB.TargetPort)) {
                page.replace("{InfluxDBPortOpen}", "#99c03c");
            } else {
                page.replace("{InfluxDBPortOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedInfluxDB}", "notchecked");
        page.replace("{InfluxDBVisibility}", "none");
    }
    page.replace("{InfluxDBServer}", String(InfluxDB.TargetServer));
    page.replace("{InfluxDBPort}", String(InfluxDB.TargetPort));
    page.replace("{InfluxDBDB}", String(InfluxDB.db));
    page.replace("{InfluxDBUsername}", String(InfluxDB.user));
    page.replace("{InfluxDBPassword}", String(InfluxDB.password));
    page.replace("{InfluxDBMeasurementsname}", String(InfluxDB.Measurementsname));

    if(Prometheus.Enabled) {
        page.replace("{CheckedPrometheus}", "checked");
        page.replace("{PrometheusVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(Prometheus.TargetServer)) {
                page.replace("{PrometheusServerReachable}", "#99c03c");
            } else {
                page.replace("{PrometheusServerReachable}", "#FF0000");
            }
            if(net.PortOpen(Prometheus.TargetServer, Prometheus.TargetPort)) {
                page.replace("{PrometheusPortOpen}", "#99c03c");
            } else {
                page.replace("{PrometheusPortOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedPrometheus}", "notchecked");
        page.replace("{PrometheusVisibility}", "none");
    }
    page.replace("{PrometheusServer}", String(Prometheus.TargetServer));
    page.replace("{PrometheusPort}", String(Prometheus.TargetPort));
    page.replace("{PrometheusJob}", String(Prometheus.job));
    page.replace("{PrometheusInstance}", String(Prometheus.instance));

    if(Tcontrol.Enabled) {
        page.replace("{CheckedTcontrol}", "checked");
        page.replace("{TcontrolVisibility}", "flex");
        if(TestMessage_state) {
            if(net.ServerReachable(Tcontrol.TargetServer)) {
                page.replace("{TcontrolServerReachable}", "#99c03c");
            } else {
                page.replace("{TcontrolServerReachable}", "#FF0000");
            }
            if(net.PortOpen(Tcontrol.TargetServer, Tcontrol.TargetPort)) {
                page.replace("{TcontrolPortOpen}", "#99c03c");
            } else {
                page.replace("{TcontrolPortOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedTcontrol}", "notchecked");
        page.replace("{TcontrolVisibility}", "none");
    }
    page.replace("{TcontrolServer}", String(Tcontrol.TargetServer));
    page.replace("{TcontrolPort}", String(Tcontrol.TargetPort));

    if(Telegram.Enabled) {
        page.replace("{CheckedTelegram}", "checked");
        page.replace("{TelegramVisibility}", "flex");
        if(TestMessage_state) {
            if(false) {
                page.replace("{TelegramTokenReachable}", "#99c03c");
            } else {
                page.replace("{TelegramTokenReachable}", "#FF0000");
            }
            if(false) {
                page.replace("{TelegramChatIDOpen}", "#99c03c");
            } else {
                page.replace("{TelegramChatIDOpen}", "#FF0000");
            }
        }
    } else {
        page.replace("{CheckedTelegram}", "notchecked");
        page.replace("{TelegramVisibility}", "none");
    }
    page.replace("{TelegramToken}", String(Telegram.Token));
    page.replace("{TelegramChatID}", String(Telegram.chatID));

    if(Tcp.Enabled) {
        page.replace("{CheckedTcp}", "checked");
        page.replace("{TcpVisibility}", "flex");
            if(TestMessage_state) {
                if(net.ServerReachable(Tcp.TargetServer)) {
                    page.replace("{TcpServerReachable}", "#99c03c");
                } else {
                    page.replace("{TcpServerReachable}", "#FF0000");
                }
                if(net.PortOpen(Tcp.TargetServer, Tcp.TargetPort)) {
                    page.replace("{TcpPortOpen}", "#99c03c");
                } else {
                    page.replace("{TcpPortOpen}", "#FF0000");
                }
            }
    } else {
        page.replace("{CheckedTcp}", "notchecked");
        page.replace("{TcpVisibility}", "none");
    }
    page.replace("{TcpServer}", String(Tcp.TargetServer));
    page.replace("{TcpPort}", String(Tcp.TargetPort));

    if(ThingSpeak.Enabled) {
        page.replace("{CheckedThingSpeak}", "checked");
        page.replace("{ThingSpeakVisibility}", "flex");
    } else {
        page.replace("{CheckedThingSpeak}", "notchecked");
        page.replace("{ThingSpeakVisibility}", "none");
    }
    page.replace("{ThingSpeakToken}", String(ThingSpeak.Token));
    page.replace("{ThingSpeakChannel}", String(ThingSpeak.Channel));

    if(Ubidots.Enabled) {
        page.replace("{CheckedUbidots}", "checked");
        page.replace("{UbidotsVisibility}", "flex");
    } else {
        page.replace("{CheckedUbidots}", "notchecked");
        page.replace("{UbidotsVisibility}", "none");
    }
    page.replace("{UbidotsToken}", String(Ubidots.Token));

    if(GoogleSheets.Enabled) {
        page.replace("{CheckedGoogleSheets}", "checked");
        page.replace("{GoogleSheetsVisibility}", "flex");
    } else {
        page.replace("{CheckedGoogleSheets}", "notchecked");
        page.replace("{GoogleSheetsVisibility}", "none");
    }
    page.replace("{GoogleSheetsToken}", String(GoogleSheets.Token));

    page.replace("{Formular}", l_formular);

    if(configSensor.Step[2].Last_Calibration_Step)
        page.replace("<tr><td>Step 4</td><td>cell2_4</td><td>cell3_4</td><td>cell4_4</td><td>cell5_4</td></tr>", "");
    if(configSensor.Step[3].Last_Calibration_Step)
        page.replace("<tr><td>Step 5</td><td>cell2_5</td><td>cell3_5</td><td>cell4_5</td><td>cell5_5</td></tr>", "");
    if(configSensor.Step[4].Last_Calibration_Step)
        page.replace("<tr><td>Step 6</td><td>cell2_6</td><td>cell3_6</td><td>cell4_6</td><td>cell5_6</td></tr>", "");
    if(configSensor.Step[5].Last_Calibration_Step)
        page.replace("<tr><td>Step 7</td><td>cell2_7</td><td>cell3_7</td><td>cell4_7</td><td>cell5_7</td></tr>", "");

    if(true) {
        page.replace("cell2_1", String(configSensor.Step[0].Plato, 1) + "°P");
        page.replace("cell2_2", String(configSensor.Step[1].Plato, 1) + "°P");
        page.replace("cell2_3", String(configSensor.Step[2].Plato, 1) + "°P");
        page.replace("cell2_4", String(configSensor.Step[3].Plato, 1) + "°P");
        page.replace("cell2_5", String(configSensor.Step[4].Plato, 1) + "°P");
        page.replace("cell2_6", String(configSensor.Step[5].Plato, 1) + "°P");
        page.replace("cell2_7", String(configSensor.Step[6].Plato, 1) + "°P");

        page.replace("cell3_1", String(configSensor.Step[0].MeasuredGravity, 3) + "°");
        page.replace("cell3_2", String(configSensor.Step[1].MeasuredGravity, 3) + "°");
        page.replace("cell3_3", String(configSensor.Step[2].MeasuredGravity, 3) + "°");
        page.replace("cell3_4", String(configSensor.Step[3].MeasuredGravity, 3) + "°");
        page.replace("cell3_5", String(configSensor.Step[4].MeasuredGravity, 3) + "°");
        page.replace("cell3_6", String(configSensor.Step[5].MeasuredGravity, 3) + "°");
        page.replace("cell3_7", String(configSensor.Step[6].MeasuredGravity, 3) + "°");

        page.replace("cell4_1", String(configSensor.Step[0].MeasuredTemperature, 1) + "°C");
        page.replace("cell4_2", String(configSensor.Step[1].MeasuredTemperature, 1) + "°C");
        page.replace("cell4_3", String(configSensor.Step[2].MeasuredTemperature, 1) + "°C");
        page.replace("cell4_4", String(configSensor.Step[3].MeasuredTemperature, 1) + "°C");
        page.replace("cell4_5", String(configSensor.Step[4].MeasuredTemperature, 1) + "°C");
        page.replace("cell4_6", String(configSensor.Step[5].MeasuredTemperature, 1) + "°C");
        page.replace("cell4_7", String(configSensor.Step[6].MeasuredTemperature, 1) + "°C");

        page.replace("cell5_1", String(configSensor.Step[0].deviation, 3) + "°P");
        page.replace("cell5_2", String(configSensor.Step[1].deviation, 3) + "°P");
        page.replace("cell5_3", String(configSensor.Step[2].deviation, 3) + "°P");
        page.replace("cell5_4", String(configSensor.Step[3].deviation, 3) + "°P");
        page.replace("cell5_5", String(configSensor.Step[4].deviation, 3) + "°P");
        page.replace("cell5_6", String(configSensor.Step[5].deviation, 3) + "°P");
        page.replace("cell5_7", String(configSensor.Step[6].deviation, 3) + "°P");
    }
    page.replace("{PlainWaterYar}", String(configSensor.PlainWater_ypr[0], 3));
    page.replace("{PlainWaterPitch}", String(configSensor.PlainWater_ypr[1], 3));
    page.replace("{PlainWaterRoll}", String(configSensor.PlainWater_ypr[2], 3));

    page.replace("{plato_before_PWC}", String(configSensor.plato_before_PWC, 3));
    page.replace("{plato_after_PWC}", String(configSensor.plato_after_PWC, 3));
    page.replace("{gravity_PWC}", String(configSensor.gravity_PWC, 3)); 
    page.replace("{temperature_PWC}", String(configSensor.temperature_PWC, 3)); 
    page.replace("{temperature_drift}", String(configSensor.temperature_drift, 3)); 

    page.replace("{Landingpage}", String(Hydrom.Landingpage));

    page.replace("{mac_adddress}", String(Wifi.MacAdress));
    
    page.replace("{web_message}", "");
    page.replace("https_", "https://");
    page.replace("http_", "http://");
    page.replace("§", AnfUEHRUNGSZEICHEN);
        page.replace("{TempCompFactor}", String(configSensor.TemperatureCompensation_Factor,7));
TestMessage_state=false;
    Print_Info(4, Hydrom.current_Log_Level, "End Replacing Placeholder");
}
/**
 * @brief Validates and applies submitted POST parameters from the settings pages.
 *        Reads form arguments and updates the corresponding configuration variables.
 */
void checkPostContent() {
    Print_Info(4, Hydrom.current_Log_Level, "Check the Post Content");
    if(server.method() == HTTP_POST) {
        Print_Info(4, Hydrom.current_Log_Level, "HTTP Post was found");
        if(server.hasArg("ElementClient")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableClient was found");
            if((server.arg("ArgumentEnableClient").equals("on"))) {
                Print_Info(4, Hydrom.current_Log_Level, "Clientmode is active");
                if(server.hasArg("ServerSSID"))
                    StringCopyConst(Hydrom.name, server.arg("ServerSSID").c_str());
                if(server.hasArg("ServerPassword"))
                    StringCopyConst(Wifi.server_password, server.arg("ServerPassword").c_str());
                if(server.hasArg("ClientSSID"))
                    StringCopyConst(Wifi.client_ssid, server.arg("ClientSSID").c_str());
                if(server.hasArg("ClientPassword"))
                    StringCopyConst(Wifi.client_password, server.arg("ClientPassword").c_str());
                Wifi.mode = MODE_SERVER_CLIENT;
                if(server.hasArg("Element_Wizard_Page_2")){
                        Print_Info(4, Hydrom.current_Log_Level, ">Wizard< Element was found");
                        if(net.begin(true, 3, Hydrom.name, Wifi.client_ssid, Wifi.client_password, Hydrom.name, Wifi.server_password, DEF_IP, DEF_GATEWAY, DEF_SUBNET,Hydrom.current_Log_Level)){
                            Print_Info(4, Hydrom.current_Log_Level, "Wifimode Server_Client was successfuly activated via Wizard");
                            Connection_Failed=false;
                        }else{
                            Print_Error("Wifimode Server_Client failed");
                            Connection_Failed=true;
                            Hydrom.Landingpage=Hydrom.Landingpage+1;
                        }
                    }else{
                        Print_Info(4, Hydrom.current_Log_Level, "no >Wizard< Element was found");
                        if(net.begin(true, 3, Hydrom.name, Wifi.client_ssid, Wifi.client_password, Hydrom.name, Wifi.server_password, DEF_IP, DEF_GATEWAY, DEF_SUBNET,Hydrom.current_Log_Level)){
                            Print_Info(4, Hydrom.current_Log_Level, "Wifimode Server_Client was successfuly activated");
                            Connection_Failed=false;
                        }else{
                            Print_Error("Wifimode Server_Client failed without Wizard");
                            Connection_Failed=true;
                        }
                }


            } else {
                Wifi.mode = MODE_SERVER;
                Print_Info(4, Hydrom.current_Log_Level, "Servermode is active");
                if(server.hasArg("ServerSSID"))
                    StringCopyConst(Hydrom.name, server.arg("ServerSSID").c_str());
                if(server.hasArg("ServerPassword"))
                    StringCopyConst(Wifi.server_password, server.arg("ServerPassword").c_str());
            }
        }
        if(server.hasArg("Element_Wizard_Page_2")){
        if(server.hasArg("ElementSkip")){
            Hydrom.Landingpage=3;
        }
        }


        if(server.hasArg("ElementBluetooth")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableBluetooth was found");
            if((server.arg("ArgumentEnableBluetooth").equals("on"))) {
                Bluetooth.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Bluetooth is active");
            } else {
                Bluetooth.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Bluetooth is deactive");
            }
        }
        if(server.hasArg("ElementBrewblox")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableBrewblox was found");
            if((server.arg("ArgumentEnableBrewblox").equals("on"))) {
                Brewblox.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Brewblox is active");
                if(server.hasArg("BrewbloxServer"))
                    StringCopyConst(Brewblox.TargetServer, server.arg("BrewbloxServer").c_str());
                Brewblox.TargetPort = CheckForArg(Brewblox.TargetPort, "BrewbloxPort");
                if(server.hasArg("BrewbloxTopicLevel"))
                    StringCopyConst(Brewblox.TopicLevel, server.arg("BrewbloxTopic").c_str());
            } else {
                Brewblox.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Brewblox is deactive");
            }
        }
        if(server.hasArg("ElementBrewfather")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableBrewfather was found");
            if((server.arg("ArgumentEnableBrewfather").equals("on"))) {
                Brewfather.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Brewfather is active");
                if(server.hasArg("BrewfatherURI"))
                    StringCopyConst(Brewfather._url, server.arg("BrewfatherURI").c_str());
            } else {
                Brewfather.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Brewfather is deactive");
            }
        }

        if(server.hasArg("ElementBierBot")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableBierBot was found");
            if((server.arg("ArgumentEnableBierBot").equals("on"))) {
                BierBot.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "BierBot is active");
                if(server.hasArg("BierBotToken"))
                    StringCopyConst(BierBot.Token, server.arg("BierBotToken").c_str());
            } else {
                BierBot.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "BierBot is deactive");
            }
        }

        if(server.hasArg("ElementCraftbeerpi")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableCraftbeerpi was found");
            if((server.arg("ArgumentEnableCraftbeerpi").equals("on"))) {
                Craftbeerpi.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Craftbeerpi is active");
                if(server.hasArg("CraftbeerpiServer"))
                    StringCopyConst(Craftbeerpi.TargetServer, server.arg("CraftbeerpiServer").c_str());
            } else {
                Craftbeerpi.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Craftbeerpi is deactive");
            }
        }
        if(server.hasArg("ElementFhem")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableFhem was found");
            if((server.arg("ArgumentEnableFhem").equals("on"))) {
                Fhem.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Fhem is active");
                if(server.hasArg("FhemServer"))
                    StringCopyConst(Fhem.TargetServer, server.arg("FhemServer").c_str());
                Fhem.TargetPort = CheckForArg(Fhem.TargetPort, "FhemPort");
                if(server.hasArg("FhemName"))
                    StringCopyConst(Fhem.TopicLevel, server.arg("FhemName").c_str());
            } else {
                Fhem.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Fhem is deactive");
            }
        }
        if(server.hasArg("ElementGrainfather")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableGrainfather was found");
            if((server.arg("ArgumentEnableGrainfather").equals("on"))) {
                Grainfather.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Grainfather is active");
                if(server.hasArg("GrainfatherURI"))
                    StringCopyConst(Grainfather._url, server.arg("GrainfatherURI").c_str());
            } else {
                Grainfather.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Grainfather is deactive");
            }
        }
        if(server.hasArg("ElementHttp")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableHttp was found");
            if((server.arg("ArgumentEnableHttp").equals("on"))) {
                Http.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Http is active");
                if(server.hasArg("HttpServer"))
                    StringCopyConst(Http.TargetServer, server.arg("HttpServer").c_str());
                Http.TargetPort = CheckForArg(Http.TargetPort, "HttpPort");
                if(server.hasArg("HttpURI"))
                    StringCopyConst(Http._url, server.arg("HttpURI").c_str());
            } else {
                Http.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Http is deactive");
            }
        }
        if(server.hasArg("ElementInfluxDB")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableInfluxDB was found");
            if((server.arg("ArgumentEnableInfluxDB").equals("on"))) {
                InfluxDB.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "InfluxDB is active");
                if(server.hasArg("InfluxDBServer"))
                    StringCopyConst(InfluxDB.TargetServer, server.arg("InfluxDBServer").c_str());
                InfluxDB.TargetPort = CheckForArg(InfluxDB.TargetPort, "InfluxDBPort");
                if(server.hasArg("InfluxDBDB"))
                    StringCopyConst(InfluxDB.db, server.arg("InfluxDBDB").c_str());
                if(server.hasArg("InfluxDBUsername"))
                    StringCopyConst(InfluxDB.user, server.arg("InfluxDBUsername").c_str());
                if(server.hasArg("InfluxDBPassword"))
                    StringCopyConst(InfluxDB.password, server.arg("InfluxDBPassword").c_str());
                if(server.hasArg("InfluxDBMeasurementsname"))
                    StringCopyConst(InfluxDB.Measurementsname, server.arg("InfluxDBMeasurementsname").c_str()); 
            } else {
                InfluxDB.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "InfluxDB is deactive");
            }
        }
        if(server.hasArg("ElementMqtt")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableMqtt was found");
            if((server.arg("ArgumentEnableMqtt").equals("on"))) {
                Mqtt.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Mqtt is active");
                if(server.hasArg("MqttServer"))
                    StringCopyConst(Mqtt.TargetServer, server.arg("MqttServer").c_str());
                Mqtt.TargetPort = CheckForArg(Mqtt.TargetPort, "MqttPort");
                if(server.hasArg("MqttUsername"))
                    StringCopyConst(Mqtt.user, server.arg("MqttUsername").c_str());
                if(server.hasArg("MqttPassword"))
                    StringCopyConst(Mqtt.password, server.arg("MqttPassword").c_str());
                if(server.hasArg("MqttTopicLevel"))
                    StringCopyConst(Mqtt.TopicLevel, server.arg("MqttTopicLevel").c_str());
            } else {
                Mqtt.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Mqtt is deactive");
            }
        }
        if(server.hasArg("ElementPrometheus")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnablePrometheus was found");
            if((server.arg("ArgumentEnablePrometheus").equals("on"))) {
                Prometheus.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Prometheus is active");
                if(server.hasArg("PrometheusServer"))
                    StringCopyConst(Prometheus.TargetServer, server.arg("PrometheusServer").c_str());
                Prometheus.TargetPort = CheckForArg(Prometheus.TargetPort, "PrometheusPort");
                if(server.hasArg("PrometheusJob"))
                    StringCopyConst(Prometheus.job, server.arg("PrometheusJob").c_str());
                if(server.hasArg("PrometheusInstance"))
                    StringCopyConst(Prometheus.instance, server.arg("PrometheusInstance").c_str());
            } else {
                Prometheus.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Prometheus is deactive");
            }
        }
        if(server.hasArg("ElementTcontrol")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableTcontrol was found");
            if((server.arg("ArgumentEnableTcontrol").equals("on"))) {
                Tcontrol.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Tcontrol is active");
                if(server.hasArg("TcontrolServer"))
                    StringCopyConst(Tcontrol.TargetServer, server.arg("TcontrolServer").c_str());
                Tcontrol.TargetPort = CheckForArg(Tcontrol.TargetPort, "TcontrolPort");
            } else {
                Tcontrol.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Tcontrol is deactive");
            }
        }
        if(server.hasArg("ElementTcp")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableTcp was found");
            if((server.arg("ArgumentEnableTcp").equals("on"))) {
                Tcp.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Tcp is active");
                if(server.hasArg("TcpServer")) {
                    StringCopyConst(Tcp.TargetServer, server.arg("TcpServer").c_str());
                    Print_Info(4, Hydrom.current_Log_Level, " INFO:TCP Server  Value was saved");
                }
                Tcp.TargetPort = CheckForArg(Tcp.TargetPort, "TcpPort");
            } else {
                Tcp.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Tcp is deactive");
            }
        }
        if(server.hasArg("ElementThingSpeak")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableThingSpeak was found");
            if((server.arg("ArgumentEnableThingSpeak").equals("on"))) {
                ThingSpeak.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "ThingSpeak is active");
                if(server.hasArg("ThingSpeakToken"))
                    StringCopyConst(ThingSpeak.Token, server.arg("ThingSpeakToken").c_str());
                ThingSpeak.Channel = CheckForArg(ThingSpeak.Channel, "ThingSpeakChannel");
            } else {
                ThingSpeak.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "ThingSpeak is deactive");
            }
        }
        if(server.hasArg("ElementUbidots")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableUbidots was found");
            if((server.arg("ArgumentEnableUbidots").equals("on"))) {
                Ubidots.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Ubidots is active");
                if(server.hasArg("UbidotsToken"))
                    StringCopyConst(Ubidots.Token, server.arg("UbidotsToken").c_str());
            } else {
                Ubidots.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Ubidots is deactive");
            }
        }

        if(server.hasArg("ElementGoogleSheets")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableGoogleSheets was found");
            if((server.arg("ArgumentEnableGoogleSheets").equals("on"))) {
                GoogleSheets.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "GoogleSheets is active");
                if(server.hasArg("GoogleSheetsToken"))
                    StringCopyConst(GoogleSheets.Token, server.arg("GoogleSheetsToken").c_str());
            } else {
                GoogleSheets.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "GoogleSheets is deactive");
            }
        }

        if(server.hasArg("ElementTelegram")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableTelegram was found");
            if((server.arg("ArgumentEnableTelegram").equals("on"))) {
                Telegram.Enabled = true;
                Print_Info(4, Hydrom.current_Log_Level, "Telegram is active");
                if(server.hasArg("TelegramToken"))
                    StringCopyConst(Telegram.Token, server.arg("TelegramToken").c_str());
                if(server.hasArg("TelegramChatID"))
                    StringCopyConst(Telegram.chatID, server.arg("TelegramChatID").c_str());
            } else {
                Telegram.Enabled = false;
                Print_Info(4, Hydrom.current_Log_Level, "Telegram is deactive");
            }
        }

        if(server.hasArg("DeepSleepHour")) DeepSleep.hours = CheckForArg(DeepSleep.hours, "DeepSleepHour");
        if(server.hasArg("DeepSleepMinute")) DeepSleep.minutes = CheckForArg(DeepSleep.minutes, "DeepSleepMinute");
        if(server.hasArg("DeepSleepSecond")) DeepSleep.seconds = CheckForArg(DeepSleep.seconds, "DeepSleepSecond");

        if(server.hasArg("ElementTempComp")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableTempComp was found");
            if((server.arg("ArgumentEnableTempComp").equals("on"))) {
                configSensor.Enable_TemperatureCompensation = true;
                Print_Info(4, Hydrom.current_Log_Level, "Temperature Compensation is active");
                configSensor.TemperatureCompensation_Factor   = CheckForArg(configSensor.TemperatureCompensation_Factor, "TempCompFactor");
            } else {
                configSensor.Enable_TemperatureCompensation = false;
                Print_Info(4, Hydrom.current_Log_Level, "Temperature Compensation is deactive");
            }
        }

        if(server.hasArg("ElementRecHeadstand")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableRecHeadstand was found");
            if((server.arg("ArgumentEnableRecHeadstand").equals("on"))) {
                configSensor.Enable_RecHeadstand = true;
                Print_Info(4, Hydrom.current_Log_Level, "RecHeadstand is active");
            } else {
                configSensor.Enable_RecHeadstand = false;
                Print_Info(4, Hydrom.current_Log_Level, "RecHeadstand is deactive");
            }
        }

        if(server.hasArg("ArgumentEnableManuellAdjustment")) {
            Print_Info(4, Hydrom.current_Log_Level, "Manuel Adjustment Element was found");
            if((server.arg("ArgumentEnableManuellAdjustment").equals("on"))) {
                Print_Info(4, Hydrom.current_Log_Level, "Manuell Adjustment of the Formular started");
                Hydrom.Coefficients[0] = config.check_Plausibility("Hydrom.Coefficient 0", CheckForArg(Hydrom.Coefficients[0], "FormularD"), -30000, 30000);
                Hydrom.Coefficients[1] = config.check_Plausibility("Hydrom.Coefficient 1", CheckForArg(Hydrom.Coefficients[1], "FormularX"), -1500, 1500);
                Hydrom.Coefficients[2] = config.check_Plausibility("Hydrom.Coefficient 2", CheckForArg(Hydrom.Coefficients[2], "FormularXX"), -100, 100);
                Hydrom.Coefficients[3] = config.check_Plausibility("Hydrom.Coefficient 3", CheckForArg(Hydrom.Coefficients[3], "FormularXXX"), -10, 10);
                Hydrom.Coefficients[4] = config.check_Plausibility("Hydrom.Coefficient 4", CheckForArg(Hydrom.Coefficients[4], "FormularXXXX"), -10, 10);
                Hydrom.Coefficients[5] = config.check_Plausibility("Hydrom.Coefficient 5", CheckForArg(Hydrom.Coefficients[5], "FormularXXXXX"), -10, 10);
                Hydrom.Coefficients[6] = config.check_Plausibility("Hydrom.Coefficient 6", CheckForArg(Hydrom.Coefficients[6], "FormularXXXXXX"), -10, 10);
            }
        }

        if(server.hasArg("ArgumentEnableCalibration")) {
            Print_Info(4, Hydrom.current_Log_Level, "Calibrate Element was found");
            if((server.arg("ArgumentEnableCalibration").equals("on"))) {
                Print_Info(4, Hydrom.current_Log_Level, "Calibration choosen");
                prepare_next_step(1);
            }
        }
        if(server.hasArg("ArgumentEnablePlainWaterMeasurement")) {
            Print_Info(4, Hydrom.current_Log_Level, "Plainwater Measurement Element was found");
            if(server.arg("ArgumentEnablePlainWaterMeasurement").equals("on")) {
                Print_Info(4, Hydrom.current_Log_Level, "Do Plainwater Measurement");
                doPlainWaterMeasurement();
            }
        }
        Hydrom.Landingpage      = CheckForArg(Hydrom.Landingpage, "Landingpage");
        Bluetooth.UUID_ID       = CheckForArg(Bluetooth.UUID_ID, "UUID");
        Bluetooth.BLETransPower = CheckForArg(Bluetooth.BLETransPower, "BLETransPower");
        if(server.hasArg("Devicename"))
            StringCopyConst(Hydrom.name, server.arg("Devicename").c_str());

        if(server.hasArg("ElementSkip")&&server.hasArg("Element_Wizard_Page_1")){
            Hydrom.Landingpage=3;
        }
        if(server.hasArg("ElementOnlyBluetooth")){
            Wifi.mode = MODE_SERVER;
            Print_Info(4, Hydrom.current_Log_Level, "Servermode is active, caused Only Bluetooth");
            Bluetooth.Enabled = true;
            Print_Info(4, Hydrom.current_Log_Level, "Only Bluetooth is activ");
        }

        if(server.hasArg("ElementOneStepBack"))
            Hydrom.Landingpage=Hydrom.Landingpage+2;

        if(server.hasArg("TiltUnit")) {
            Print_Info(4, Hydrom.current_Log_Level, "Argument Tilt Unit was found");
            switch(server.arg("TiltUnit").toInt()) {
                case 1:
                    configSensor.tilt_unit = P;
                    Print_Info(4, Hydrom.current_Log_Level, "Plato was choosen as Tilt Unit");
                    break;
                case 2:
                    configSensor.tilt_unit = SG;
                    Print_Info(4, Hydrom.current_Log_Level, "SG was choosen as Tilt Unit");
                    break;
                case 3:
                    configSensor.tilt_unit = G;
                    Print_Info(4, Hydrom.current_Log_Level, "G was choosen as Tilt Unit");
                    break;
            }
        }
        if(server.hasArg("ChoosenLanguage")) {
            Print_Info(4, Hydrom.current_Log_Level, "Argument Language was found");
            Hydrom.current_Language=server.arg("ChoosenLanguage").toInt();
        }
        if(server.hasArg("DoUpdate")) {
            Print_Info(4, Hydrom.current_Log_Level, "Update will be done");
            updater.set_current_Log_Level(Hydrom.current_Log_Level);
            //Show "Wait for Update Page"
            String firmwareUrl = updater.getFirmwareBinaryUrl(DEF_GITHUB_REPO);
            if (firmwareUrl.length() > 0) {
                updater.firmwareUpdate(firmwareUrl);
            }

        }

        if(server.hasArg("Choosendebugmode")) {
            Print_Info(4, Hydrom.current_Log_Level, "Argument debugmode was found");
            Hydrom.current_Log_Level=server.arg("Choosendebugmode").toInt();
        }

        if(server.hasArg("TemperatureUnit")) {
            Print_Info(4, Hydrom.current_Log_Level, "Argument TemperatureUnit was found");
            switch(server.arg("TemperatureUnit").toInt()) {
                case 1:
                    configSensor.temperature_unit = C;
                    Print_Info(4, Hydrom.current_Log_Level, "Celsius was choosen as Temperature Unit");
                    break;
                case 2:
                    configSensor.temperature_unit = F;
                    Print_Info(4, Hydrom.current_Log_Level, "Fahrenheit was choosen as Temperature Unit");
                    break;
                case 3:
                    configSensor.temperature_unit = K;
                    Print_Info(4, Hydrom.current_Log_Level, "Kelvin was choosen as Temperature Unit");
                    break;
            }
        }
        if(server.hasArg("ElementTest")||server.hasArg("TestMessage_Wizard")) {
            Print_Info(4, Hydrom.current_Log_Level, "EnableTest was found");
            if(server.hasArg("TestMessage_Wizard")) {
                Hydrom.Landingpage=Hydrom.Landingpage+1;
            }
            if((server.arg("ArgumentEnableTest").equals("on")||server.hasArg("TestMessage_Wizard"))) {
                Device.setTestMessage_state(true);
                Print_Info(4, Hydrom.current_Log_Level, "Testmessage will be sent");
                
                TestMessage_state= true;
            }else{
                TestMessage_state= false;
            }
        }
        config.saveFS();
        if(server.hasArg("SleepDeep_Wizard")){
            gotoDeepSleep();
        }
    } else {
        Print_Info(4, Hydrom.current_Log_Level, "HTTP Post was not found");
    }
}

/**
 * @brief Handles the /sleepdeep route: saves configuration and enters deep sleep immediately.
 */
void gotoDeepSleep() {
    if(g_once){
    config.saveFS();
    Print_Info(4, Hydrom.current_Log_Level, "The Hydrom shall shut down now");
    Device.setshutdown_event(true);
    g_once=false;
    }

}
/**
 * @brief Handles the /mpucalibration route: triggers an MPU6050 offset calibration sequence.
 */
void doMPUcalibration() {
    Hydrom.MPU_is_Calibrated=false;
    while(!Hydrom.MPU_is_Calibrated){
        //Print_Info(2, Hydrom.current_Log_Level, "Calibration of MPU6050 is running");
        Device.LED_FlipFlop();
        delay(1000);
    }
    if(config.saveFS_Offsets()) {
        Print_Info(1, Hydrom.current_Log_Level, "Settings were Successfully written to the file system.");
        Calibration_Failed=false;
    } else {
        Print_Error("Something went wrong while writing the settings to the file system.");
        Device.setREDLEDstate(true);
        Device.setGREENLEDstate(false);
        Calibration_Failed=true;
    }
    char l_titel[] = "[Titel_Home]";

    Print_Info(4, Hydrom.current_Log_Level, "Webpage " + String(l_titel) + " was choosen");
    // Clear Page
    page.clear();
    // Add_Head
    addHead(l_titel);
    // AddHeader
    addHeader();
    // ACTIONbar with connectionstatus without backbutton
    page += HTML_ACTIONHOME;
    page += "<main>";
    // Add Panel
    addPanel();
    page += HTML_HOME_CONTENT;
    page += "</main>";
    page += SCRIPT;
    page += "</body></html>";
    //
    if(millis()>800000)
        addMessage(MSG_WARNING,HTML_WARN_HOME);

    if(Calibration_Failed){
        addMessage(MSG_ERROR,HTML_ERROR_MPU_CALIBRATION);
    }else{
        addMessage(MSG_SUCCESS,HTML_SUCCESS_MPU_CALIBRATION);
    }

    // Overwrite the placeholders
    writeValues();
    // send the response last
    server.send(200, "text/html", page);
}

/**
 * @brief This function is used to measure the plain water
 * 
 * Test ob die Kaliereung funktioniert
 * Vergleich alter Plato Wert mit neuem Platowert 
 * Bewerte die Güte des Winkel
 * Bewerte die Güte des neuen Plato Wertes
 * 
 */
/**
 * @brief Handles the /plainwatermeasurement route: captures the plain-water tilt reference angle.
 */
void doPlainWaterMeasurement() {
    show_Calibration_Result=true;
    Print_Info(2, Hydrom.current_Log_Level, "Plain Water Measurement was started Type:"+String(Hydrom.type));
    force_Measurement();
    float l_gradient_plain_water_lower =71.575;
    float l_gradient_plain_water_upper =74.823;
    float l_gradient_Step1_lower =66.116;
    float l_gradient_Step1_upper =72.176;
    float l_gradient_Step2_lower =56.291;
    float l_gradient_Step2_upper =67;
    float l_gradient_Step3_lower =48.134;
    float l_gradient_Step3_upper =57.653;
    float l_gradient_Step4_lower =36.8;
    float l_gradient_Step4_upper =43.287;
    float l_gradient_Step5_lower =29.346;
    float l_gradient_Step5_upper =34.445;
    float l_gradient_Step6_lower =25.251;
    float l_gradient_Step6_upper =28.992;

    configSensor.Step[0].Plato = 0;
    configSensor.Step[1].Plato = 3.5;
    configSensor.Step[2].Plato = 6.5;
    configSensor.Step[3].Plato = 9.3;
    configSensor.Step[4].Plato = 13.5;
    configSensor.Step[5].Plato = 17;
    configSensor.Step[6].Plato = 20;
if(Hydrom.type==HYDROM2109){
    Print_Info(2, Hydrom.current_Log_Level, "PWC HYDROM2109 Values saved");
    l_gradient_plain_water_lower =71.575;
    l_gradient_plain_water_upper =74.823;
    l_gradient_Step1_lower =66.116;
    l_gradient_Step1_upper =72.176;
    l_gradient_Step2_lower =56.291;
    l_gradient_Step2_upper =67;
    l_gradient_Step3_lower =48.134;
    l_gradient_Step3_upper =57.653;
    l_gradient_Step4_lower =36.8;
    l_gradient_Step4_upper =43.287;
    l_gradient_Step5_lower =29.346;
    l_gradient_Step5_upper =34.445;
    l_gradient_Step6_lower =25.251;
    l_gradient_Step6_upper =28.992;

    configSensor.Step[0].Plato = 0;
    configSensor.Step[1].Plato = 3.5;
    configSensor.Step[2].Plato = 6.5;
    configSensor.Step[3].Plato = 9.3;
    configSensor.Step[4].Plato = 13.5;
    configSensor.Step[5].Plato = 17;
    configSensor.Step[6].Plato = 20;
} else if (Hydrom.type==HYDROM2207) {
    Print_Info(2, Hydrom.current_Log_Level, "PWC HYDROM2207 Values saved");
    l_gradient_plain_water_lower =69.06;
    l_gradient_plain_water_upper =73.75;
    l_gradient_Step1_lower =64.35;
    l_gradient_Step1_upper =68.81;
    l_gradient_Step2_lower =57.51;
    l_gradient_Step2_upper =63.68;
    l_gradient_Step3_lower =53.92;
    l_gradient_Step3_upper =60.76;
    l_gradient_Step4_lower =51.54;
    l_gradient_Step4_upper =58.23;
    l_gradient_Step5_lower =45.83;
    l_gradient_Step5_upper =53.81;
    l_gradient_Step6_lower =36.06;
    l_gradient_Step6_upper =41.92;

    configSensor.Step[0].Plato = 0;
    configSensor.Step[1].Plato = 5;
    configSensor.Step[2].Plato = 9;
    configSensor.Step[3].Plato = 11;
    configSensor.Step[4].Plato = 12;
    configSensor.Step[5].Plato = 14.5;
    configSensor.Step[6].Plato = 20;
} else if (Hydrom.type==HYDROM2303) {
    Print_Info(2, Hydrom.current_Log_Level, "PWC HYDROM2303 Values saved");
    l_gradient_plain_water_lower =66.27;
    l_gradient_plain_water_upper =69.7;
    l_gradient_Step1_lower =62.5;
    l_gradient_Step1_upper = 66.76;
    l_gradient_Step2_lower =57.44;
    l_gradient_Step2_upper =63.36;
    l_gradient_Step3_lower =52.37;
    l_gradient_Step3_upper =58.33;
    l_gradient_Step4_lower =46.7;
    l_gradient_Step4_upper =53.32;
    l_gradient_Step5_lower =37.43;
    l_gradient_Step5_upper =42.34;
    l_gradient_Step6_lower =30.49;
    l_gradient_Step6_upper =33.5;

    configSensor.Step[0].Plato = 0;
    configSensor.Step[1].Plato = 2.5;
    configSensor.Step[2].Plato = 5;
    configSensor.Step[3].Plato = 7.5;
    configSensor.Step[4].Plato = 10;
    configSensor.Step[5].Plato = 15;
    configSensor.Step[6].Plato = 20;
}
    /**
     * @brief For the Status-Message the current values are saved
     * 
     */
    configSensor.plato_before_PWC=configSensor.plato;
    configSensor.gravity_PWC=configSensor.gravity;
    configSensor.temperature_PWC=configSensor.temperature;
    configSensor.temperature_drift=configSensor.std_temperature;


    /**
     * @brief The values for the compensation are calculated
     * 
     */
    float l_offset_plain_water           = Calculate_Offset(Calculate_gradient(l_gradient_plain_water_lower, l_gradient_plain_water_upper), l_gradient_plain_water_lower, tempComp.Compensate_Temperature(configSensor.temperature_PWC, configSensor.gravity_PWC,configSensor.TemperatureCompensation_Factor));    // in %
    configSensor.Step[0].MeasuredGravity =configSensor.gravity_PWC;//tempComp.Compensate_Temperature(configSensor.temperature, configSensor.gravity,configSensor.TemperatureCompensation_Factor);
    configSensor.Step[1].MeasuredGravity = Calculate_Measurement_Value(Calculate_gradient(l_gradient_Step1_lower, l_gradient_Step1_upper), l_gradient_Step1_lower, l_offset_plain_water);
    configSensor.Step[2].MeasuredGravity = Calculate_Measurement_Value(Calculate_gradient(l_gradient_Step2_lower, l_gradient_Step2_upper), l_gradient_Step2_lower, l_offset_plain_water);
    configSensor.Step[3].MeasuredGravity = Calculate_Measurement_Value(Calculate_gradient(l_gradient_Step3_lower, l_gradient_Step3_upper), l_gradient_Step3_lower, l_offset_plain_water);
    configSensor.Step[4].MeasuredGravity = Calculate_Measurement_Value(Calculate_gradient(l_gradient_Step4_lower, l_gradient_Step4_upper), l_gradient_Step4_lower, l_offset_plain_water);
    configSensor.Step[5].MeasuredGravity = Calculate_Measurement_Value(Calculate_gradient(l_gradient_Step5_lower, l_gradient_Step5_upper), l_gradient_Step5_lower, l_offset_plain_water);
    configSensor.Step[6].MeasuredGravity = Calculate_Measurement_Value(Calculate_gradient(l_gradient_Step6_lower, l_gradient_Step6_upper), l_gradient_Step6_lower, l_offset_plain_water);

    Print_Info(3, Hydrom.current_Log_Level, "Plato: " + String(configSensor.Step[0].Plato, 3) + " Measured Value: " + String(configSensor.Step[0].MeasuredGravity, 3));
    Print_Info(3, Hydrom.current_Log_Level, "Plato: " + String(configSensor.Step[1].Plato, 3) + " Measured Value: " + String(configSensor.Step[1].MeasuredGravity, 3));
    Print_Info(3, Hydrom.current_Log_Level, "Plato: " + String(configSensor.Step[2].Plato, 3) + " Measured Value: " + String(configSensor.Step[2].MeasuredGravity, 3));
    Print_Info(3, Hydrom.current_Log_Level, "Plato: " + String(configSensor.Step[3].Plato, 3) + " Measured Value: " + String(configSensor.Step[3].MeasuredGravity, 3));
    Print_Info(3, Hydrom.current_Log_Level, "Plato: " + String(configSensor.Step[4].Plato, 3) + " Measured Value: " + String(configSensor.Step[4].MeasuredGravity, 3));
    Print_Info(3, Hydrom.current_Log_Level, "Plato: " + String(configSensor.Step[5].Plato, 3) + " Measured Value: " + String(configSensor.Step[5].MeasuredGravity, 3));
    Print_Info(3, Hydrom.current_Log_Level, "Plato: " + String(configSensor.Step[6].Plato, 3) + " Measured Value: " + String(configSensor.Step[6].MeasuredGravity, 3));
    calibrator.calculate(7);

    postStatus = config.saveFS_Settings();

    force_Measurement();
    configSensor.plato_after_PWC=configSensor.plato;

}
/**
 * @brief Calculates a linear interpolation value from gradient and baseline.
 *
 * @param gradient  Slope of the linear segment.
 * @param y_low     Baseline (lower) value.
 * @param Offset    Input measurement to scale.
 * @return float    Interpolated output value.
 */
float Calculate_Measurement_Value(float gradient, float y_low, float Offset) {
    Print_Info(3, Hydrom.current_Log_Level, "The calculated Measurement result is the following for the gradient" + String(gradient, 3) + " and the under device " + String(y_low, 3) + " and the Offeset " + String(Offset, 3) + ": " + String(gradient * Offset + y_low, 3));
    return gradient * Offset + y_low;
}
/**
 * @brief Computes the x-axis offset for a given measurement on a linear segment.
 *
 * @param gradient    Slope of the linear segment.
 * @param y_low       Baseline (lower) value.
 * @param Measurement Target measurement value.
 * @return float      Offset along the x-axis.
 */
float Calculate_Offset(float gradient, float y_low, float Measurement) {
    Print_Info(3, Hydrom.current_Log_Level, "The calculated Offeset result is the following for the gradient" + String(gradient, 3) + " and the under device " + String(y_low, 3) + ": " + String(((Measurement - y_low) / gradient), 3));
    return (Measurement - y_low) / gradient;
}
/**
 * @brief Computes the gradient (slope) between two y-values over a fixed unit x-interval.
 *
 * @param y_low  Lower y-value.
 * @param y_high Upper y-value.
 * @return float Gradient (y_high - y_low).
 */
float Calculate_gradient(float y_low, float y_high) {
    Print_Info(3, Hydrom.current_Log_Level, "The gradient of " + String(y_low, 3) + " and " + String(y_high, 3) + " is " + String(((y_high - y_low) / 100), 5));
    return (y_high - y_low) / 100;
}

/**
 * @brief Handles the /restart route: saves configuration and reboots the ESP32.
 */
void Page_Restart() {
    config.saveFS();
    // Page_Home();
    ESP.restart();
}

/**
 * @brief Sets a flag that causes the main loop to perform an immediate measurement cycle.
 */
void force_Measurement(){
    Print_Info(3, Hydrom.current_Log_Level, "Force Measurement");
    Hydrom.force_Measurement=true;
    while(Hydrom.force_Measurement)
        delay(100);
    Print_Info(3, Hydrom.current_Log_Level, "Measurement done");
}

/**
 * @brief This function checks if there is a new value for the variable. If so, it will be written into the variable.
 *
 *
 * @param l_Target The variable with which the new value is compared
 * @param l_name The string to search for in the HTML post
 * @return int
 */
/**
 * @brief Returns the integer value of an HTTP argument if present, or the current target value.
 *
 * @param l_Target Default/current value returned when the argument is absent.
 * @param l_name   Name of the HTTP argument to look up.
 * @return int     Argument value if present; @p l_Target otherwise.
 */
int CheckForArg(int l_Target, String l_name) {
    if(server.hasArg(l_name)) {
        Print_Info(4, Hydrom.current_Log_Level, "Element " + l_name + " was found.");
        if(server.arg(l_name).toInt() != l_Target) {
            Print_Info(4, Hydrom.current_Log_Level, "" + String(server.arg(l_name).toInt()) + " was stored as new Value for " + l_name + ".");
            return server.arg(l_name).toInt();
        } else {
            return l_Target;
        }
    } else {
        return l_Target;
    }
}
/**
 * @brief This function checks if there is a new value for the variable. If so, it will be written into the variable.
 *
 *
 * @param l_Target The variable with which the new value is compared
 * @param l_name The string to search for in the HTML post
 * @return double
 */
double CheckForArg(double l_Target, String l_name) {
    if(server.hasArg(l_name)) {
        Print_Info(4, Hydrom.current_Log_Level, "double Element " + l_name + " was found.");
        if(server.arg(l_name).toDouble() != l_Target) {
            Print_Info(4, Hydrom.current_Log_Level, "" + String(server.arg(l_name).toDouble(), 3) + " was stored as new Value for " + l_name + ".");
            return server.arg(l_name).toDouble();
        } else {
            return l_Target;
        }
    } else {
        return l_Target;
    }
}

/**
 * @brief This function checks if there is a new value for the variable. If so, it will be written into the variable.
 *
 *
 * @param l_Target The variable with which the new value is compared
 * @param l_name The string to search for in the HTML post
 * @return float
 */
/**
 * @brief Returns the float value of an HTTP argument if present, or the current target value.
 *
 * @param l_Target Default/current value returned when the argument is absent.
 * @param l_name   Name of the HTTP argument to look up.
 * @return float   Argument value if present; @p l_Target otherwise.
 */
float CheckForArg(float l_Target, String l_name) {
    if(server.hasArg(l_name)) {
        Print_Info(4, Hydrom.current_Log_Level, "Element " + l_name + " was found.");
        if(server.arg(l_name).toFloat() != l_Target) {
            Print_Info(4, Hydrom.current_Log_Level, "" + String(server.arg(l_name).toFloat(), 3) + " was stored as new Value for " + l_name + ".");
            return server.arg(l_name).toFloat();
        } else {
            return l_Target;
        }
    } else {
        return l_Target;
    }
}

/**
 * @brief Applies the selected UI language by replacing language-string variables
 *        with the translations loaded from the active language module.
 */
void replace_Language_File(){
        Print_Info(4, Hydrom.current_Log_Level, "Start Replacing Language Files");
    switch (Hydrom.current_Language)
    {
    case 0:
        load_German();
        break;
    case 1:
        load_English();
        break;
    case 2:
        load_Italien();
        break;
    case 3:
        load_Espaniol();
        break;
    case 4:
        load_French();
        break;
    case 5:
        load_Dutch();
        break;
    case 6:
        load_Portuguese();
        break;
    case 7:
        load_Swedish();
        break;
    case 8:
        load_Finnish();
        break;
    case 9:
        load_Emty();
        break;        
    default:
        load_English();
        break;
    }
    Print_Info(4, Hydrom.current_Log_Level, "End Replacing Language Files");
}


/**
 * 4825bytes/Language
 * 
 */

/**
 * @brief Loads German UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_German() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading German Language Pack");
    page.replace("[Brewblox_Expl]","Sie haben Brewblox aktiviert.<br>Eine detaillierte Anleitung finden Sie unter <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>");
    page.replace("[Brewfather_Expl]","Sie haben Brewfather aktiviert.<br>Bitte ersetzen Sie XXXXX durch die ID der Brewfather APP.<br>Eine detaillierte Anleitung finden Sie unter <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","Sie haben BierBot aktiviert.<br>Bitte ersetzen Sie den Token  durch den API Key der BierBot APP.<br>Eine detaillierte Anleitung finden Sie unter <a href=§https_anleitung.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Zurück");
    page.replace("[Button_SAVE]","sichern");
    page.replace("[Button_Skip]","Überspringen");
    page.replace("[Button_Sleep_Deep]","Schlafen legen");
    page.replace("[Button_Start_PWM]","Klarwasser Messung durchführen");
    page.replace("[Button_Start_Wizard]","Assistent starten");
    page.replace("[Button_TestMessage]","Testnachricht");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Referenzkalibrierung");
    page.replace("[Craftbeerpi_Expl]","Sie haben CraftbeerPi aktiviert.");
    page.replace("[DeepSleep_Expl]","Damit das Hydrom funktioniert, muss die Zeit eingestellt werden die das Gerät schlafen soll.<br>Das Hydrom schläft ein und wacht zur eingestellten Zeit auf und sendet die Messwerte an den Dienst.");
    page.replace("[TempComp_Expl]","Das Hydrom unterliegt den Naturgesetzen. Die Temperatur des Sud wirkt sich auf die Dichte aus, um das herauszurechen brauchen wir die Kompensation.");
    page.replace("[Message_Warn_HOME]","Sie befinden sich noch immer im Konfigurationsmodus.<br>So hält der Akku nur max. 10h. Um das Gerät wie vorgesehen zu verwenden wählen Sie ein Clouddienst und versetzen Sie das Hydrom in den Tiefschlaf.<br> <a href=§https_anleitung.hydrom.io/§ target=§_blank§>Anleitung</a>");
    page.replace("[Grainfather_Expl]","Sie haben Grainfather aktiviert.<br>Eine detaillierte Anleitung finden Sie unter <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>");
    page.replace("[Headine_Conclusion]","Fazit");
    page.replace("[Headline_Activ_Client_Mode]","bestehendes WLAN");
    page.replace("[Headline_AP_Mode]","KonfigurationsWlan");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Formular anpassen");
    page.replace("[Headline_DeepSleep]","Intervall");
    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Headline_RecHeadstand]","Erkenne<br>Kopfstand");
    page.replace("[Headline_PWM]","Klarwasser Messung");
    page.replace("[Headline_Schritt]","Schritt");
    page.replace("[Headline_Testmessage]","Testnachricht");
    page.replace("[Home_Last_Update]","vom letzten Update");
    page.replace("[Home_Temperature]","Temperatur");
    page.replace("[Http_Expl]","Sie haben Http aktiviert.<br>Eine ausführliche Anleitung finden Sie unter <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>");
    page.replace("[InfluxDB_Expl]","Sie haben InfluxDB aktiviert.");
    page.replace("[Label_Database]","Datenbank");
    page.replace("[Label_Instance]","Instanz");
    page.replace("[Label_Job]","Auftrag");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Sprache");
    page.replace("[Label_Measurementsname]","Messungsname");
    page.replace("[Label_Methode]","Methode");
    page.replace("[Label_Methode_Reference]","Referenz");
    page.replace("[Label_Methode_Sugar]","Zucker");
    page.replace("[Label_Offset]","Offset");
    page.replace("[Label_Password]","Passwort");
    page.replace("[Label_Port]","Port");
    page.replace("[Label_Server]","Server");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Einheit Temperatur");
    page.replace("[Label_Tilt_Option_1]","Plato");
    page.replace("[Label_Tilt_Option_2]","spezifische Dichte");
    page.replace("[Label_Tilt_Option_3]","Grad");
    page.replace("[Label_Tilt_Unit]","Einheit Neigung");
    page.replace("[Label_Token]","Token");
    page.replace("[Label_Topic_Level]","Topic Ebene");
    page.replace("[Label_Trans_PWR]","BLE-Sendeleistung");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Benutzername");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","Update fehlgeschlagen.<br>Der Hydrom startet neu.<br>Dann versuchen Sie es erneut<br>");
    page.replace("[Message_UPLOAD_Error]","Das Hochladen der Datei ist fehlgeschlagen.<br>Die Datei ist beschädigt.");
    page.replace("[Message_UPLOAD_Success]","Upload war erfolgreich.<br>Das Hydrom schläft jetzt.<br>Bitte starten Sie das Hydrom neu");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"Die Verbindung konnte nicht hergestellt werden. Bitte versuchen Sie es erneut. <br>Das Hydrom hat die folgenden Bedingungen zum Netzwerk: <br>Verschlüsselung: WEP oder WPA2 (WPA3 ist in Arbeit) <br>Frequenz: 2.4ghz");
    page.replace("[Message_WiFi_INFO]","Das Hydrom befindet sich derzeit im Access Point Modus.<br>Das bedeutet, dass Dienste wie Brewfather, Grainfather und Brewblox nicht genutzt werden können, da das Gerät als Client mit einem Netzwerk verbunden sein muss.<br>Geben Sie einfach die untenstehenden WLAN-Daten ein und drücken Sie §save§");
    page.replace("[Message_WiFi_Success]","Das Hydrom ist mit dem bestehenden Netzwerk verbunden!<br>Im bestehenden Netzwerk ist das Hydrom entweder über den Hostnamen <a href=§http_{Devicename}§>{Devicename}</a> oder über die IP-Adresse <a href=§http_{IPAdresse}§>{IPAdresse}</a> erreichbar");
    page.replace("[Message_Calibration_Success]","Das Hydrom wurde erfolgreich kalibriert!");
    page.replace("[Message_Calibration_Error]","Kalibriervorgang fehlgeschlagen!");
    page.replace("[Mqtt_Expl]","Sie haben MQTT aktiviert.<br>Eine ausführliche Anleitung finden Sie unter <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>");
    page.replace("[Network]","Netzwerk");
    page.replace("[Panel_Text_calibration]","Kalibrierung");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","WLAN");
    page.replace("[Panel_Text_services]","Dienste");
    page.replace("[Panel_Text_settings]","Einstellungen");
    page.replace("[Panel_Text_update]","Aktualisieren");
    page.replace("[Password]","Kennwort");
    page.replace("[Prometheus_Expl]","Sie haben Prometheus aktiviert.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","Sie haben Tcontrol aktiviert.");
    page.replace("[Tcp_Expl]","Sie haben Tcp aktiviert.");
    page.replace("[Text_Reference_Methode]","Stellen Sie das Aräometer in die Flüssigkeit, deren Plato- oder SG-Wert Sie kennen, und notieren Sie die Messwerte unten.<br>Wenn Sie diesen Punkt nicht mehr aufzeichnen möchten, deaktivieren Sie einfach beide Schalter, und die nächste Seite ist die Zusammenfassung.");
    page.replace("[Text_Step1_Reference_Methode]","Fügen Sie das Hydrom zu Ihrer Referenzflüssigkeit hinzu.<br>Das kann auch das Bier sein, das Sie gerade brauen.<br>Dann wird Ihr nächstes Bier intelligent gebraut.");
    page.replace("[Text_Step2_Sugar_Methode]","Stelle eine gesättigte Zuckerlösung her, indem du die Zuckermenge zum Wasser aus Schritt 1 hinzugibst.");
    page.replace("[Text_Step3_Sugar_Methode]","Um das Zucker-Wasser-Gemisch zu verdünnen, fügen Sie xyxml Wasser hinzu.<br> Wenn Sie nicht nachgemessen haben und daher einen anderen Referenzwert kennen, können Sie den Plato-Wert so lassen, wie er voreingestellt war.");
    page.replace("[Text_Step4_Sugar_Methode]","Zum Verdünnen der Zucker-Wasser-Mischung fügen Sie xyxml-Wasser hinzu.<br> Wenn Sie nicht nachgemessen haben und daher einen anderen Referenzwert kennen, können Sie den Plato-Wert so belassen, wie er voreingestellt war.");
    page.replace("[Text_Step5_Sugar_Methode]","Fügen Sie erneut xyxml Wasser hinzu<br> Wenn Sie nicht nachgemessen haben und somit einen anderen Referenzwert kennen, können Sie den Plato-Wert so belassen, wie er voreingestellt war.");
    page.replace("[Text_Step6_Sugar_Methode]","Verdünnen Sie nun das vorletzte Mal die Mischung mit xyxml Wasser.<br> Wenn Sie nicht nachgemessen haben und daher einen anderen Referenzwert kennen, können Sie den Plato-Wert so belassen, wie er vorgegeben wurde.");
    page.replace("[Text_Step7_Sugar_Methode]","Bitte geben Sie zum letzten Mal xyxml Wasser hinzu.<br> Wenn Sie nicht nachgemessen haben und daher einen anderen Referenzwert kennen, können Sie den Plato-Wert so belassen, wie er voreingestellt wurde.");
    page.replace("[Text_Wizard_Wifi]","Hier können Sie die Zugangsdaten Ihres Netzwerks eingeben, um das Hydrom mit dem Internet oder einem lokalen Server zu verbinden.<br><br>Sie brauchen das nicht?<br> Dann gehen Sie einfach auf den Button >>[Only_Bluetooth]<< und benutzen das Hydrom mit Bluetooth.");
    page.replace("[Titel_Calibration]","Kalibrierung");
    page.replace("[Titel_Home]","Startseite");
    page.replace("[Titel_Information]","Informationen");
    page.replace("[Titel_MPU_Calibration]","MPU-Kalibrierung");
    page.replace("[Titel_Settings]","Einstellungen");
    page.replace("[Titel_Support]","Unterstützung");
    page.replace("[Titel_Wifi_Settings]","WLAN-Einstellungen");
    page.replace("[Ubidots_Expl]","Sie haben Ubidots aktiviert. <br>Eine detaillierte Anleitung finden Sie unter <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>");
    page.replace("[GoogleSheets_Expl]","Sie haben GoogleSheets aktiviert. <br>Eine detaillierte Anleitung finden Sie unter <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","In diesem Modus verbindet sich das Hydrom mit dem unten angegebenen WLAN und ist somit ein Client und kann bei Bedarf auf das Internet zugreifen.");
    page.replace("[WiFi_Name]","WLAN-Name");
    page.replace("[Wizard_Connect_Wifi]","Verbinden mit bestehendem Netzwerk");
    page.replace("[Wizard_Procedure_1]","<h4> Ablauf der Installation:</h4><ul><li>Netzwerkeinstellungen vornehmen</li><li>Hydrom kalibrieren</li><li>Dienst hinzufügen</li><li>Hydrom in den Ruhezustand versetzen</li></ul>");
    page.replace("[Wizard_text_1]","Das Hydrom ist nur dann wirklich brauchbar, wenn es in ein bestehendes WLAN integriert ist.");
    page.replace("[Wizard_text_2]","Mit viel Hingabe haben mein Team und ich dieses Produkt für Sie entwickelt!");
    page.replace("[Wizard_text_3]","Das Hydrom wird unkalibriert ausgeliefert.<br>Zur Kalibrierung wird ein Topf mit klarem Wasser (20°) benötigt.<br>Setzen Sie das Hydrometer in das Wasser und aktivieren Sie die Messung.<br>Wenn Sie diesen Schritt überspringen wollen, wählen Sie §Skip§.");
    page.replace("[Wizard_text_4]","<br>Der letzte Schritt ist das Einrichten eines Dienstes.<br>Dieser Dienst dient dazu, die Daten zu visualisieren und nutzbar zu machen.<br>Drei verschiedene Dienste werden unterstützt:<br>1.Bluetooth<br>2. Eigener Server (Lokal)<br>3. Cloud-Dienst (Internet)<br>");
    page.replace("[Wizard_Thank_You_1]","Vielen Dank, dass Sie sich für Hydrom entschieden haben!");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Netzwerk");
    page.replace("[Link_Manuel]","https_instruction.hydrom.io/");
    page.replace("[Text_Wifi_AP_Mode]","Jedes Mal, wenn das Hydrom im Konfigurationsmodus gestartet wird, richtet das Hydrom ein WLAN ein.");
    page.replace("[Label_Devicename]","Gerätename");
    page.replace("[Headline_SUCCESS]","ERFOLG!");
    page.replace("[Headline_INFORMATION]","INFORMATION:");
    page.replace("[Headline_ERROR]","ERROR!");
    page.replace("[Headline_WARNING]","WARNUNG!");
    page.replace("[Battery]","Bat:");
    page.replace("[Button_Start]","start");
    page.replace("[Text_2_Step0_Sugar_Methode]","Stellen Sie eine Schale bereit, die mindestens 2100ml fasst.<br>Auch bei 400ml Wasser sollte das Hydrom bereits frei schwimmen.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Geben Sie hier die anfängliche Wassermenge in ml ein:");
    page.replace("[Text_4_Step0_Sugar_Methode]","Wenn Sie mit der Eingabe fertig sind, bestätigen Sie mit $FERTIG$.<br>Das Hydrom misst den aktuellen Winkel und speichert ihn zur Kalibrierung.");
    page.replace("[Button_Done]","DONE");
    page.replace("[Headline_Plato]","Plato");
    page.replace("[Headline_Gravity]","Winkel");
    page.replace("[Headline_Temperature]","Temperatur");
    page.replace("[Headline_Deviation]","Abweichung");
    page.replace("[Text_Update]","<h1>Firmware laden</h1><p>Stellen Sie unbedingt sicher, dass Sie nur die originale Hydrom-Firmware herunterladen. Geht der Upload schief, dann erlischt die Garantie und die Firmware muss mühsam mit spezieller Hardware neu installiert werden.</p><p>Aktuelle Firmware: ");
    page.replace("[Text_Update_New]","<p>Verfügbare Firmware: ");  
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Update");
    page.replace("[Titel_Service]","Service");
    page.replace("[Only_Bluetooth]","Nur Bluetooth");
    page.replace("[Telegram_Expl]","Sie haben Telegram aktiviert.<br>Eine detaillierte Anleitung finden Sie unter <a href=§https_anleitung.hydrom.io/connect-services-1/das-hydrom-mit-telegram-verbinden§ target=§_blank§>Link</a>");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading German Language Pack"); 
    return true;
}

/**
 * @brief Loads English UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_English() {
    page.replace("[Brewblox_Expl]","You have Brewblox enabled.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>");
    page.replace("[Brewfather_Expl]","You have Brewfather enabled.<br>Please replace the XXXXX with the ID from the Brewfather APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Back");
    page.replace("[Button_SAVE]","SAVE");
    page.replace("[Button_Skip]","Skip");
    page.replace("[Button_Sleep_Deep]","Sleep Deep");
    page.replace("[Button_Start_PWM]","Do PlainwaterMeasurement");
    page.replace("[Button_Start_Wizard]","Start Wizard");
    page.replace("[Button_TestMessage]","Testmessage");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Reference calibration");
    page.replace("[Craftbeerpi_Expl]","You have CraftbeerPi enabled.");
    page.replace("[DeepSleep_Expl]","In order for the hydrom to function, the sleep time must be set.<br>The Hydrom falls asleep and wakes up at the defined time and sends the measured values to the service.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","You have Grainfather enabled.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>");
    page.replace("[Headine_Conclusion]","Conclusion");
    page.replace("[Headline_Activ_Client_Mode]","Activate Client-Mode");
    page.replace("[Headline_AP_Mode]","Configuration-WiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Costumize Formular");
    page.replace("[Headline_DeepSleep]","DeepSleep");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");
    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","PlainWater-Measurement");
    page.replace("[Headline_Schritt]","Step");
    page.replace("[Headline_Testmessage]","Test message");
    page.replace("[Home_Last_Update]","from last update");
    page.replace("[Home_Temperature]","Temperature");
    page.replace("[Http_Expl]","You have Http enabled.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>");
    page.replace("[InfluxDB_Expl]","You have InfluxDB enabled.");
    page.replace("[Label_Database]","Database");
    page.replace("[Label_Instance]","Instance");
    page.replace("[Label_Job]","Job");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Language");
    page.replace("[Label_Measurementsname]","Measurementsname");
    page.replace("[Label_Methode]","Methode");
    page.replace("[Label_Methode_Reference]","Reference");
    page.replace("[Label_Methode_Sugar]","Sugar");
    page.replace("[Label_Offset]","Offset");
    page.replace("[Label_Password]","Password");
    page.replace("[Label_Port]","Port");
    page.replace("[Label_Server]","Server");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Temperature Unit");
    page.replace("[Label_Tilt_Option_1]","Plato");
    page.replace("[Label_Tilt_Option_2]","Specific Gravity");
    page.replace("[Label_Tilt_Option_3]","Degree");
    page.replace("[Label_Tilt_Unit]","Tilt Unit");
    page.replace("[Label_Token]","Token");
    page.replace("[Label_Topic_Level]","Topic Level");
    page.replace("[Label_Trans_PWR]","BLE Transmition Power");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Username");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","Update failed.<br>The Hydrom restarts.<br>Then try again<br>");
    page.replace("[Message_UPLOAD_Error]","The upload of the file failed.<br>File corrupted");
    page.replace("[Message_UPLOAD_Success]","Upload was successful.<br>The hydrom is now asleep.<br>Please restart the Hydrom");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");
    page.replace("[Message_WiFi_INFO]","The Hydrom is currently in Access Point mode.<br>This means that services such as Brewfather, Grainfather and Brewblox cannot be used, as the Device must be connected as a client to a Network.<br>Simply enter the WLAN data below and press §save§");
    page.replace("[Message_WiFi_Success]","The Hydrom is connected to the existing network!<br> In the existing network, the Hydrom is accessible either via the host name <a href=§http_{Devicename}§>{Devicename}</a> or via the IP address <a href=§http_{IPAdresse}§>{IPAdresse}</a>");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","You have MQTT enabled.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>");
    page.replace("[Network]","Network");
    page.replace("[Panel_Text_calibration]","Calibration");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Services");
    page.replace("[Panel_Text_settings]","Settings");
    page.replace("[Panel_Text_update]","Update");
    page.replace("[Password]","Password");
    page.replace("[Prometheus_Expl]","You have Prometheus enabled.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","You have Tcontrol enabled.");
    page.replace("[Tcp_Expl]","You have Tcp enabled.");
    page.replace("[Text_Reference_Methode]","Place the hydrometer in the liquid for which you know the Plato or SG value and record the readings below.<br>If you no longer wish to record this item, simply uncheck both switches and the next page is the summary.");
    page.replace("[Text_Step1_Reference_Methode]","Add the hydrom to your reference liquid.<br>This can also be the beer you are currently brewing.<br>Then your next beer will be brewed smart.");
    page.replace("[Text_Step2_Sugar_Methode]","Make a saturated sugar solution by adding amountofsugarg of sugar to the water from Step 1.");
    page.replace("[Text_Step3_Sugar_Methode]","To dilute the sugar water mixture add xyxml water.<br> If you have not remeasured and therefore know another reference value, you can leave the Plato value as it was preset.");
    page.replace("[Text_Step4_Sugar_Methode]","To dilute the sugar water mixture add xyxml water.<br> If you have not remeasured and therefore know another reference value, you can leave the Plato value as it was preset.");
    page.replace("[Text_Step5_Sugar_Methode]","Add xyxml water again<br> If you have not remeasured and therefore know another reference value, you can leave the Plato value as it was preset.");
    page.replace("[Text_Step6_Sugar_Methode]","So now the second to last time dilute the mixture with xyxml water.<br> If you have not remeasured and therefore know another reference value, you can leave the Plato value as it was preset.");
    page.replace("[Text_Step7_Sugar_Methode]","Please add xyxml of water for the last time.<br> If you have not remeasured and therefore know another reference value, you can leave the Plato value as it was preset.");
    page.replace("[Text_Wizard_Wifi]","Here you can enter the access data of your network to connect the Hydrom to the Internet or a local server.<br><br>You don't need that?<br>Well then just go with the button >>[Only_Bluetooth]<< and use the Hydrom with Bluetooth.");
    page.replace("[Titel_Calibration]","Calibration");
    page.replace("[Titel_Home]","Home");
    page.replace("[Titel_Information]","Information");
    page.replace("[Titel_MPU_Calibration]","MPU Calibration");
    page.replace("[Titel_Settings]","Settings");
    page.replace("[Titel_Support]","Support");
    page.replace("[Titel_Wifi_Settings]","Wifi Settings");
    page.replace("[Ubidots_Expl]","You have Ubidots enabled.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","This mode connects the Hydrom to the Wifi entered below and is thus a client and can access the Internet if necessary.");
    page.replace("[WiFi_Name]","WiFi-Name");
    page.replace("[Wizard_Connect_Wifi]","Connect to existing Network");
    page.replace("[Wizard_Procedure_1]","<h4> Procedure of the installation:</h4><ul><li>set the network settings</li><li>Calibrate your Hydrom</li><li>Add Service</li><li>Put the Hydrom to sleep</li></ul>");
    page.replace("[Wizard_text_1]","The Hydrom is only really usable when it is integrated into an existing Wifi.");
    page.replace("[Wizard_text_2]","With much dedication, my team and I have developed this product for you!");
    page.replace("[Wizard_text_3]","The Hydrom is delivered uncalibrated.<br>A pot of clear water (20°) is required for calibration.<br>Place the hydrometer in the water and activate the Measurement.<br>If you want to skip this step, select §Skip§.");
    page.replace("[Wizard_text_4]","<br>The last step is to set up a service.<br>This service serves to visualise the data and make it usable.<br>Three different services are supported:<br>1.Bluetooth<br>2. Own server (Local)<br>3. cloud service (internet)<br>");
    page.replace("[Wizard_Thank_You_1]","Thank you for choosing Hydrom!");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Network");
    page.replace("[Link_Manuel]","https_instruction.hydrom.io/");
    page.replace("[Text_Wifi_AP_Mode]","Every time the hydrom is started in configuration mode, then the hydrom sets up a wifi.");
    page.replace("[Label_Devicename]","Devicename");
    page.replace("[Headline_SUCCESS]","SUCCESS!");
    page.replace("[Headline_INFORMATION]","INFORMATION:");
    page.replace("[Headline_ERROR]","ERROR!");
    page.replace("[Headline_WARNING]","WARNING!");
    page.replace("[Battery]","Bat:");
    page.replace("[Button_Start]","start");
    page.replace("[Text_2_Step0_Sugar_Methode]","Provide a bowl that holds at least 2100ml.<br>Also, at 400ml of water, the hydrom should already be floating freely.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Enter the initial water volume in ml here:");
    page.replace("[Text_4_Step0_Sugar_Methode]","When you are finished with your input, confirm with $DONE$.<br>The Hydrom will measure the current angle and save it for calibration.");
    page.replace("[Button_Done]","DONE");
    page.replace("[Headline_Plato]","Plato");
    page.replace("[Headline_Gravity]","Gravity");
    page.replace("[Headline_Temperature]","Temperature");
    page.replace("[Headline_Deviation]","Deviation");
    page.replace("[Text_Update]","<h1>Load Firmware</h1><p>Make absolutely sure that you download only the original Hydrom firmware. If the upload goes wrong, then the warranty is voided and the firmware must be laboriously reinstalled with special hardware.</p><p> Current Firmware: ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Update");
    page.replace("[Titel_Service]","Service");   
    page.replace("[Only_Bluetooth]","Only Bluetooth");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading English Language Pack");
    return true;
}

/**
 * @brief Loads Italian UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_Italien() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading Italien Language Pack");
    page.replace("[Brewblox_Expl]","Avete abilitato Brewblox.<br>Per istruzioni dettagliate, vedere <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>");
    page.replace("[Brewfather_Expl]","Si è abilitato Brewfather.<br>Sostituire XXXXX con l'ID dell'APP Brewfather.<br>Per istruzioni dettagliate, vedere <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Indietro");
    page.replace("[Button_SAVE]","SALVARE");
    page.replace("[Button_Skip]","Saltare");
    page.replace("[Button_Sleep_Deep]","Dormire profondamente");
    page.replace("[Button_Start_PWM]","Eseguire la misurazione dell'acqua di pianura");
    page.replace("[Button_Start_Wizard]","Avvia procedura guidata");
    page.replace("[Button_TestMessage]","Messaggio di prova");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Calibrazione di riferimento");
    page.replace("[Craftbeerpi_Expl]","È stato attivato CraftbeerPi.");
    page.replace("[DeepSleep_Expl]","Affinché l'Hydrom funzioni, è necessario impostare il tempo di sospensione.<br>L'Hydrom si addormenta e si sveglia all'ora definita e invia i valori misurati al servizio.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","Si è abilitato Grainfather.<br>Per istruzioni dettagliate, vedere <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>");
    page.replace("[Headine_Conclusion]","Conclusione");
    page.replace("[Headline_Activ_Client_Mode]","Attivare la modalità client");
    page.replace("[Headline_AP_Mode]","ConfigurationWiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Costumizzare il modulo");
    page.replace("[Headline_DeepSleep]","Dormire profondamente");
    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","Misurazione dell'acqua piatta");
    page.replace("[Headline_Schritt]","Passo");
    page.replace("[Headline_Testmessage]","Messaggio di prova");
    page.replace("[Home_Last_Update]","dall'ultimo aggiornamento");
    page.replace("[Home_Temperature]","Temperatura");
    page.replace("[Http_Expl]","L'Http è abilitato.<br>Per istruzioni dettagliate, vedere <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>");
    page.replace("[InfluxDB_Expl]","InfluxDB è abilitato.");
    page.replace("[Label_Database]","Database");
    page.replace("[Label_Instance]","Istanza");
    page.replace("[Label_Job]","Lavoro");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Lingua");
    page.replace("[Label_Measurementsname]","Nome della misura");
    page.replace("[Label_Methode]","Metodo");
    page.replace("[Label_Methode_Reference]","Riferimento");
    page.replace("[Label_Methode_Sugar]","Zucchero");
    page.replace("[Label_Offset]","Offset");
    page.replace("[Label_Password]","Password");
    page.replace("[Label_Port]","Porta");
    page.replace("[Label_Server]","Server");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Unità di temperatura");
    page.replace("[Label_Tilt_Option_1]","Platone");
    page.replace("[Label_Tilt_Option_2]","Gravità specifica");
    page.replace("[Label_Tilt_Option_3]","Grado");
    page.replace("[Label_Tilt_Unit]","Unità di inclinazione");
    page.replace("[Label_Token]","Gettone");
    page.replace("[Label_Topic_Level]","Livello dell'argomento");
    page.replace("[Label_Trans_PWR]","Potenza di trasmissione BLE");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Nome utente");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","Aggiornamento fallito.<br>L'Hydrom si riavvia.<br>Poi riprovare<br>");
    page.replace("[Message_UPLOAD_Error]","Il caricamento del file non è riuscito.<br>File danneggiato");
    page.replace("[Message_UPLOAD_Success]","Il caricamento è riuscito.<br>L'idrom è ora addormentato.<br>Riavviare l'idrom.");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"Non è stato possibile stabilire la connessione. Si prega di riprovare. <br>L'Hydrom ha le seguenti condizioni di connessione alla rete: <br>Crittografia: WEP o WPA2 (WPA3 è in corso) <br>Frequenza: 2,4ghz");
    page.replace("[Message_WiFi_INFO]","L'Hydrom è attualmente in modalità Access Point.<br>Questo significa che non è possibile utilizzare servizi come Brewfather, Grainfather e Brewblox, in quanto il dispositivo deve essere collegato come client a una rete.<br>Semplicemente inserire i dati WLAN qui sotto e premere §save§");
    page.replace("[Message_WiFi_Success]","Hydrom è connesso alla rete esistente!<br>Nella rete esistente, Hydrom è accessibile tramite il nome host <a href=§http_{Devicename}§>{Devicename}</a> o tramite l'indirizzo IP <a href=§http_{IPAdresse}§>{IPAdresse}</a>.");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","Si è abilitato MQTT.<br>Per istruzioni dettagliate, vedere <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>");
    page.replace("[Network]","Rete");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");
    page.replace("[Panel_Text_calibration]","Calibrazione");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Servizi");
    page.replace("[Panel_Text_settings]","Impostazioni");
    page.replace("[Panel_Text_update]","Aggiornamento");
    page.replace("[Password]","Password");
    page.replace("[Prometheus_Expl]","È stato attivato Prometheus.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","È abilitato Tcontrol.");
    page.replace("[Tcp_Expl]","È stato attivato Tcp.");
    page.replace("[Text_Reference_Methode]","Posizionare l'idrometro nel liquido di cui si conosce il valore Plato o SG e registrare le letture qui sotto.<br>Se non si desidera più registrare questa voce, è sufficiente deselezionare entrambi gli interruttori e la pagina successiva è il riepilogo.");
    page.replace("[Text_Step1_Reference_Methode]","Aggiungere l'idrometro al liquido di riferimento.<br>Può essere anche la birra che si sta producendo al momento.<br>La prossima birra sarà prodotta in modo intelligente.");
    page.replace("[Text_Step2_Sugar_Methode]","Preparare una soluzione satura di zucchero aggiungendo una quantità di zucchero all'acqua del punto 1. Per diluire la miscela di zucchero e acqua, aggiungere un po' di zucchero.");
    page.replace("[Text_Step3_Sugar_Methode]","Per diluire la miscela di acqua e zucchero aggiungere xyxml di acqua.<br>Se non avete rimisurato e quindi conoscete un altro valore di riferimento, potete lasciare il valore Plato come era stato preimpostato.");
    page.replace("[Text_Step4_Sugar_Methode]","Per diluire la miscela di acqua e zucchero aggiungere acqua xyxml.<br> Se non si è rimisurato e quindi si conosce un altro valore di riferimento, si può lasciare il valore di Plato come era stato preimpostato.");
    page.replace("[Text_Step5_Sugar_Methode]","Aggiungere nuovamente acqua xyxml<br> Se non si è rimisurato e quindi si conosce un altro valore di riferimento, si può lasciare il valore di Plato come era stato impostato.");
    page.replace("[Text_Step6_Sugar_Methode]","Quindi, per la penultima volta, diluire la miscela con xyxml di acqua.<br> Se non si è rimisurato e quindi si conosce un altro valore di riferimento, si può lasciare il valore di Platone come era stato preimpostato.");
    page.replace("[Text_Step7_Sugar_Methode]","Aggiungere xyxml di acqua per l'ultima volta.<br> Se non è stata effettuata una nuova misurazione e quindi si conosce un altro valore di riferimento, è possibile lasciare il valore di Platone come era stato preimpostato.");
    page.replace("[Text_Wizard_Wifi]","Here you can enter the access data of your network to connect the Hydrom to the Internet or a local server.<br><br>You don't need that?<br>Well then just go with the button >>[Only_Bluetooth]<< and use the Hydrom with Bluetooth.");
    page.replace("[Titel_Calibration]","Calibrazione");
    page.replace("[Titel_Home]","Casa");
    page.replace("[Titel_Information]","Informazioni");
    page.replace("[Titel_MPU_Calibration]","Calibrazione MPU");
    page.replace("[Titel_Settings]","Impostazioni");
    page.replace("[Titel_Support]","Supporto");
    page.replace("[Titel_Wifi_Settings]","Impostazioni Wifi");
    page.replace("[Ubidots_Expl]","Avete attivato Ubidots.<br>Per istruzioni dettagliate, vedere <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","Questa modalità collega l'Hydrom al Wifi inserito di seguito");
    page.replace("[WiFi_Name]","Nome WiFi");
    page.replace("[Wizard_Connect_Wifi]","Connettiti alla rete esistente");
    page.replace("[Wizard_Procedure_1]","<h4> Procedura di installazione:</h4><ul><li>impostare le impostazioni di rete</li><li>calibrare l'Hydrom</li><li>aggiungere il servizio</li><li> mettere l'Hydrom a riposo</li></ul>");
    page.replace("[Wizard_text_1]","Hydrom è realmente utilizzabile solo quando è integrato in una rete Wifi esistente.");
    page.replace("[Wizard_text_2]","Con grande impegno, il mio team e io abbiamo sviluppato questo prodotto per voi!");
    page.replace("[Wizard_text_3]","L'Hydrom viene consegnato non calibrato.<br>Per la calibrazione è necessario un vaso di acqua limpida (20°).<br>Posizionare l'idrometro nell'acqua e attivare la misurazione.<br>Se si desidera saltare questo passaggio, selezionare §Skip§.");
    page.replace("[Wizard_text_4]","<br>L'ultimo passo consiste nell'impostare un servizio.<br>Questo servizio serve a visualizzare i dati e a renderli utilizzabili.<br>Sono supportati tre diversi servizi:<br>1.Bluetooth<br>2. Server proprio (locale)<br>3. servizio cloud (internet)<br>");
    page.replace("[Wizard_Thank_You_1]","Grazie per aver scelto Hydrom!");
    page.replace("[Wizard_titel_1]","Mago");
    page.replace("[Wizard_titel_2]","Rete");
    page.replace("[Link_Manuel]","https_instruction.hydrom.io/");
    page.replace("[Text_Wifi_AP_Mode]","Ogni volta che Hydrom viene avviato in modalità di configurazione, imposta una rete wifi.");
    page.replace("[Label_Devicename]","Nome dispositivo");
    page.replace("[Headline_SUCCESS]","SUCCESSO!");
    page.replace("[Headline_INFORMATION]","INFORMAZIONI:");
    page.replace("[Headline_ERROR]","ERRORE!");
    page.replace("[Headline_WARNING]","ATTENZIONE!");
    page.replace("[Battery]","Bat:");
    page.replace("[Button_Start]","avvio");
    page.replace("[Text_2_Step0_Sugar_Methode]","Predisporre una bacinella di almeno 2100 ml.<br>Inoltre, a 400 ml di acqua, l'idrometro dovrebbe già galleggiare liberamente.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Inserire qui il volume iniziale dell'acqua in ml:");
    page.replace("[Text_4_Step0_Sugar_Methode]","Una volta terminato l'inserimento, confermare con $DONE$.<br>L'Hydrom misurerà l'angolo corrente e lo salverà per la calibrazione.");
    page.replace("[Button_Done]","FATTO");
    page.replace("[Headline_Plato]","Platone");
    page.replace("[Headline_Gravity]","Gravità");
    page.replace("[Headline_Temperature]","Temperatura");
    page.replace("[Headline_Deviation]","Deviazione");
    page.replace("[Text_Update]","<h1>Carica il firmware</h1><p> Assicuratevi assolutamente di scaricare solo il firmware originale Hydrom. Se il caricamento va male, la garanzia viene annullata e il firmware deve essere faticosamente reinstallato con un hardware speciale.</p><p>Versione attuale del firmware: ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","CARICAMENTO");
    page.replace("[Titel_Update]","Aggiornamento");
    page.replace("[Titel_Service]","Servizio");
    page.replace("[Only_Bluetooth]","Solo Bluetooth");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading Italien Language Pack");
    return true;
}

/**
 * @brief Loads Spanish UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_Espaniol() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading Espaniol Language Pack");
    page.replace("[Brewblox_Expl]","Tiene Brewblox habilitado.<br>Para obtener instrucciones detalladas, consulte <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>.");
    page.replace("[Brewfather_Expl]","Tiene activado Brewfather.<br>Sustituya el XXXXX por el ID de la APP Brewfather.<br>Para obtener instrucciones detalladas, consulte <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Volver");
    page.replace("[Button_SAVE]","GUARDAR");
    page.replace("[Button_Skip]","Saltar");
    page.replace("[Button_Sleep_Deep]","Dormir profundo");
    page.replace("[Button_Start_PWM]","Hacer Medición de Agua Plana");
    page.replace("[Button_Start_Wizard]","Iniciar el Asistente");
    page.replace("[Button_TestMessage]","Mensaje de prueba");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Calibración");
    page.replace("[Craftbeerpi_Expl]","Tiene activado el CraftbeerPi.");
    page.replace("[DeepSleep_Expl]","Para que el hidrom funcione, se debe establecer el tiempo de sueño.<br>El hidrom se duerme y se despierta a la hora definida y envía los valores medidos al servicio.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","Tiene Grainfather habilitado.<br>Para obtener instrucciones detalladas, consulte <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>.");
    page.replace("[Headine_Conclusion]","Conclusión");
    page.replace("[Headline_Activ_Client_Mode]","Activar el modo cliente");
    page.replace("[Headline_AP_Mode]","ConfigurationWiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Costumize Formular");
    page.replace("[Headline_DeepSleep]","DeepSleep");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");

    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","Medición de agua plana");
    page.replace("[Headline_Schritt]","Paso");
    page.replace("[Headline_Testmessage]","Mensaje de prueba");
    page.replace("[Home_Last_Update]","de la última actualización");
    page.replace("[Home_Temperature]","Temperatura");
    page.replace("[Http_Expl]","Tiene Http habilitado.<br>Para obtener instrucciones detalladas, consulte <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>.");
    page.replace("[InfluxDB_Expl]","Tiene InfluxDB habilitado.");
    page.replace("[Label_Database]","Base de datos");
    page.replace("[Label_Instance]","Instancia");
    page.replace("[Label_Job]","Trabajo");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Idioma");
    page.replace("[Label_Measurementsname]","Nombre de la medida");
    page.replace("[Label_Methode]","Método");
    page.replace("[Label_Methode_Reference]","Referencia");
    page.replace("[Label_Methode_Sugar]","Azúcar");
    page.replace("[Label_Offset]","Desplazamiento");
    page.replace("[Label_Password]","Contraseña");
    page.replace("[Label_Port]","Puerto");
    page.replace("[Label_Server]","Servidor");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Unidad de temperatura");
    page.replace("[Label_Tilt_Option_1]","Plato");
    page.replace("[Label_Tilt_Option_2]","Gravedad específica");
    page.replace("[Label_Tilt_Option_3]","Grado");
    page.replace("[Label_Tilt_Unit]","Unidad de inclinación");
    page.replace("[Label_Token]","Ficha");
    page.replace("[Label_Topic_Level]","Nivel del tema");
    page.replace("[Label_Trans_PWR]","Potencia de transmisión BLE");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Nombre de usuario");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","La actualización ha fallado.<br>El Hydrom se reinicia.<br>Entonces inténtelo de nuevo<br>");
    page.replace("[Message_UPLOAD_Error]","La carga del archivo falló.<br>Archivo corrupto");
    page.replace("[Message_UPLOAD_Success]","La carga fue exitosa.<br>La hidrom está ahora dormida.<br>Por favor, reinicie la hidrom");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"No se ha podido establecer la conexión. Por favor, inténtelo de nuevo. <br>La Hydrom tiene las siguientes condiciones a la red: <br>Encriptación: WEP o WPA2 (WPA3 está en proceso) <br>Frecuencia: 2.4ghz");
    page.replace("[Message_WiFi_INFO]","El Hydrom está actualmente en modo Punto de Acceso.<br>Esto significa que no se pueden utilizar servicios como Brewfather, Grainfather y Brewblox, ya que el Dispositivo debe estar conectado como cliente a una Red.<br>Simplemente introduzca los datos de la WLAN a continuación y pulse §guardar§");
    page.replace("[Message_WiFi_Success]","El Hydrom está conectado a la red existente.<br>En la red existente, el Hydrom es accesible a través del nombre de host <a href=§http_{Devicename}§>{Devicename}</a> o a través de la dirección IP <a href=§http_{IPAdresse}§>{IPAdresse}</a>.");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","Tiene MQTT habilitado.<br> Para obtener instrucciones detalladas, consulte <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>.");
    page.replace("[Network]","Red");
    page.replace("[Panel_Text_calibration]","Calibración");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Servicios");
    page.replace("[Panel_Text_settings]","Ajustes");
    page.replace("[Panel_Text_update]","Actualizar");
    page.replace("[Password]","Contraseña");
    page.replace("[Prometheus_Expl]","Tiene activado Prometheus.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","Tiene Tcontrol activado.");
    page.replace("[Tcp_Expl]","Tiene Tcp habilitado.");
    page.replace("[Text_Reference_Methode]","Coloque el hidrómetro en el líquido del que conoce el valor de Plato o SG y registre las lecturas que aparecen a continuación.<br>Si ya no desea registrar este elemento, simplemente desmarque ambos interruptores y la siguiente página será el resumen.");
    page.replace("[Text_Step1_Reference_Methode]","Añada la hidromina a su líquido de referencia.<br>También puede ser la cerveza que esté elaborando en ese momento.<br>Entonces su próxima cerveza será elaborada de forma inteligente.");
    page.replace("[Text_Step2_Sugar_Methode]","Haga una solución saturada de azúcar añadiendo la cantidad de azúcar al agua del paso 1.");
    page.replace("[Text_Step3_Sugar_Methode]","Para diluir la mezcla de agua azucarada añada xyxml agua.<br> Si no ha vuelto a medir y, por lo tanto, conoce otro valor de referencia, puede dejar el valor de Plato como estaba preestablecido.");
    page.replace("[Text_Step4_Sugar_Methode]","Para diluir la mezcla de agua azucarada añada agua xyxml.<br> Si no ha vuelto a medir y, por tanto, conoce otro valor de referencia, puede dejar el valor de Plato como estaba preestablecido.");
    page.replace("[Text_Step5_Sugar_Methode]","Añada agua xyxml de nuevo<br> Si no ha vuelto a medir y por lo tanto conoce otro valor de referencia, puede dejar el valor de Plato como estaba preestablecido.");
    page.replace("[Text_Step6_Sugar_Methode]","Entonces ahora la penúltima vez diluya la mezcla con xyxml de agua.<br> Si no ha vuelto a medir y por lo tanto conoce otro valor de referencia, puede dejar el valor de Plato como estaba preestablecido.");
    page.replace("[Text_Step7_Sugar_Methode]","Por favor, añada xyxml de agua por última vez.<br> Si no ha vuelto a medir y, por tanto, conoce otro valor de referencia, puede dejar el valor de Plato tal y como estaba preajustado.");
    page.replace("[Text_Wizard_Wifi]","Zum Verdünnen der Zucker-Wasser-Mischung fügen Sie xyxml-Wasser hinzu.<br> Wenn Sie nachgemessen haben und daher einen anderen Referenzwert kennen, können Sie den Plato-Wert so belassen, wie er voreingestellt war.");
    page.replace("[Titel_Calibration]","Calibración");
    page.replace("[Titel_Home]","Inicio");
    page.replace("[Titel_Information]","Información");
    page.replace("[Titel_MPU_Calibration]","Calibración de la MPU");
    page.replace("[Titel_Settings]","Ajustes");
    page.replace("[Titel_Support]","Soporte");
    page.replace("[Titel_Wifi_Settings]","Ajustes de Wifi");
    page.replace("[Ubidots_Expl]","Tiene Ubidots habilitado.<br> Para obtener instrucciones detalladas, consulte <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>.");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","Este modo conecta el Hydrom a la Wifi introducida a continuación y por lo tanto es un cliente y puede acceder a Internet si es necesario.");
    page.replace("[WiFi_Name]","Nombre del WiFi");
    page.replace("[Wizard_Connect_Wifi]","Conectar a la red existente");
    page.replace("[Wizard_Procedure_1]","<h4> Procedimiento de la instalación:</h4><ul><li>Configurar la red</li><li>Calibrar su Hydrom</li><li>Añadir servicio</li><li>Poner el Hydrom en reposo</li></ul>.");
    page.replace("[Wizard_text_1]","El Hydrom sólo es realmente utilizable cuando se integra en un Wifi existente.");
    page.replace("[Wizard_text_2]","Con mucha dedicación, mi equipo y yo hemos desarrollado este producto para usted.");
    page.replace("[Wizard_text_3]","El Hydrom se entrega sin calibrar.<br>Para la calibración se necesita un recipiente con agua clara (20°).<br>Coloque el hidrómetro en el agua y active la Medición.<br>Si quiere saltarse este paso, seleccione §Skip§.");
    page.replace("[Wizard_text_4]","<br>El último paso es configurar un servicio.<br>Este servicio sirve para visualizar los datos y hacerlos utilizables.<br>Se admiten tres servicios diferentes:<br>1.Bluetooth<br>2. Servidor propio (Local)<br>3. Servicio en la nube (Internet)<br>");
    page.replace("[Wizard_Thank_You_1]","¡Gracias por elegir Hydrom!");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Red");
    page.replace("[Link_Manuel]","https_instruction.hydrom.io/");
    page.replace("[Text_Wifi_AP_Mode]","Cada vez que se inicia la hidrom en modo de configuración, entonces la hidrom establece un wifi.");
    page.replace("[Label_Devicename]","Nombre del dispositivo");
    page.replace("[Headline_SUCCESS]","ÉXITO");
    page.replace("[Headline_INFORMATION]","INFORMACIÓN:");
    page.replace("[Headline_ERROR]","¡ERROR!");
    page.replace("[Headline_WARNING]","¡ADVERTENCIA!");
    page.replace("[Battery]","Bat:");
    page.replace("[Button_Start]","inicie");
    page.replace("[Text_2_Step0_Sugar_Methode]","Disponga de un recipiente con una capacidad mínima de 2100ml.<br>Además, a los 400ml de agua, el hidrometro ya debe flotar libremente.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Introduzca aquí el volumen de agua inicial en ml:");
    page.replace("[Text_4_Step0_Sugar_Methode]","Cuando haya terminado de introducirlo, confirme con $DONE$.<br>El hidrometro medirá el ángulo actual y lo guardará para su calibración.");
    page.replace("[Button_Done]","DONE");
    page.replace("[Headline_Plato]","Plato");
    page.replace("[Headline_Gravity]","Gravedad");
    page.replace("[Headline_Temperature]","Temperatura");
    page.replace("[Headline_Deviation]","Desviación");
    page.replace("[Text_Update]","<h1>Cargar Firmware</h1><p>Asegúrese de que descarga sólo el firmware original de Hydrom. Si la carga va mal, entonces la garantía se anula y el firmware debe ser laboriosamente reinstalado con hardware especial.</p><p>Versión actual del firmware: ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Actualizar");
    page.replace("[Titel_Service]","Servicio");
    page.replace("[Only_Bluetooth]","Sólo Bluetooth");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading Espaniol Language Pack");
    return true;
}

/**
 * @brief Loads French UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_French() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading French Language Pack");
    page.replace("[Brewblox_Expl]","Vous avez activé Brewblox.<br>Pour des instructions détaillées, voir <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>.");
    page.replace("[Brewfather_Expl]","Vous avez activé Brewfather.<br>Veuillez remplacer le XXXXX par l'ID de l'APP Brewfather.<br>Pour des instructions détaillées, voir <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Retour");
    page.replace("[Button_SAVE]","SAUVEGARDER");
    page.replace("[Button_Skip]","Sauter");
    page.replace("[Button_Sleep_Deep]","Dormir profondément");
    page.replace("[Button_Start_PWM]","Faire PlainwaterMeasurement");
    page.replace("[Button_Start_Wizard]","Démarrer l'assistant");
    page.replace("[Button_TestMessage]","Testmessage");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Calibrage");
    page.replace("[Craftbeerpi_Expl]","Vous avez activé CraftbeerPi.");
    page.replace("[DeepSleep_Expl]","Pour que l'hydrom fonctionne, le temps de sommeil doit être défini.<br>L'hydrom s'endort et se réveille à l'heure définie et envoie les valeurs mesurées au service.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","Vous avez activé Grainfather.<br>Pour des instructions détaillées, voir <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>.");
    page.replace("[Headine_Conclusion]","Conclusion");
    page.replace("[Headline_Activ_Client_Mode]","Activer le mode client");
    page.replace("[Headline_AP_Mode]","ConfigurationWiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Costumiser le formulaire");
    page.replace("[Headline_DeepSleep]","DeepSleep");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");

    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","PlainWater-Measurement");
    page.replace("[Headline_Schritt]","Étape");
    page.replace("[Headline_Testmessage]","Test du message");
    page.replace("[Home_Last_Update]","de la dernière mise à jour");
    page.replace("[Home_Temperature]","Température");
    page.replace("[Http_Expl]","Vous avez activé Http.<br>Pour des instructions détaillées, voir <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>.");
    page.replace("[InfluxDB_Expl]","Vous avez activé InfluxDB.");
    page.replace("[Label_Database]","Base de données");
    page.replace("[Label_Instance]","Instance");
    page.replace("[Label_Job]","Travail");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Langue");
    page.replace("[Label_Measurementsname]","Nom des mesures");
    page.replace("[Label_Methode]","Méthode");
    page.replace("[Label_Methode_Reference]","Référence");
    page.replace("[Label_Methode_Sugar]","Sucre");
    page.replace("[Label_Offset]","Décalage");
    page.replace("[Label_Password]","Mot de passe");
    page.replace("[Label_Port]","Port");
    page.replace("[Label_Server]","Serveur");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Unité de température");
    page.replace("[Label_Tilt_Option_1]","Platon");
    page.replace("[Label_Tilt_Option_2]","Gravité spécifique");
    page.replace("[Label_Tilt_Option_3]","Degré");
    page.replace("[Label_Tilt_Unit]","Unité d'inclinaison");
    page.replace("[Label_Token]","Token");
    page.replace("[Label_Topic_Level]","Niveau du sujet");
    page.replace("[Label_Trans_PWR]","Puissance de transmission BLE");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Nom d'utilisateur");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","La mise à jour a échoué.<br>L'Hydrom redémarre.<br>Puis réessayez<br>.");
    page.replace("[Message_UPLOAD_Error]","Le téléchargement du fichier a échoué.<br>Fichier corrompu.");
    page.replace("[Message_UPLOAD_Success]","Le téléchargement a réussi.<br>L'hydrom est maintenant endormi.<br>Veuillez redémarrer l'hydrom.");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"La connexion n'a pas pu être établie. Veuillez réessayer. <br>L'Hydrom a les conditions suivantes au réseau : <br>Encryptage : WEP ou WPA2 (WPA3 est en cours) <br>Fréquence : 2,4ghz <br>.");
    page.replace("[Message_WiFi_INFO]","L'Hydrom est actuellement en mode point d'accès.<br>Cela signifie que des services tels que Brewfather, Grainfather et Brewblox ne peuvent pas être utilisés, car l'appareil doit être connecté en tant que client à un réseau.<br>Il suffit de saisir les données WLAN ci-dessous et d'appuyer sur §save§.");
    page.replace("[Message_WiFi_Success]","L'Hydrom est connecté au réseau existant!<br>Dans le réseau existant, l'Hydrom est accessible soit via le nom d'hôte <a href=§http_{Devicename}§>{Devicename}</a> ou via l'adresse IP <a href=§http_{IPAdresse}§>{IPAdresse}</a>.");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","Vous avez activé MQTT.<br>Pour des instructions détaillées, voir <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>.");
    page.replace("[Network]","Réseau");
    page.replace("[Only_Bluetooth]","Bluetooth uniquement");
    page.replace("[Panel_Text_calibration]","Calibrage");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Services");
    page.replace("[Panel_Text_settings]","Paramètres");
    page.replace("[Panel_Text_update]","Mise à jour");
    page.replace("[Password]","Mot de passe");
    page.replace("[Prometheus_Expl]","Vous avez activé Prometheus.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","Tcontrol est activé.");
    page.replace("[Tcp_Expl]","Tcp est activé.");
    page.replace("[Text_Reference_Methode]","Placez l'hydromètre dans le liquide dont vous connaissez la valeur Plato ou SG et enregistrez les lectures ci-dessous.<br>Si vous ne souhaitez plus enregistrer cet élément, il suffit de décocher les deux interrupteurs et la page suivante est le résumé.");
    page.replace("[Text_Step1_Reference_Methode]","Ajoutez l'hydrom à votre liquide de référence.<br>Cela peut aussi être la bière que vous êtes en train de brasser.<br>Puis votre prochaine bière sera brassée intelligemment.");
    page.replace("[Text_Step2_Sugar_Methode]","Faites une solution de sucre saturée en ajoutant amountofsugarg de sucre à l'eau de l'étape 1.");
    page.replace("[Text_Step3_Sugar_Methode]","Pour diluer le mélange sucre-eau, ajoutez xyxml d'eau.<br>Si vous n'avez pas refait les mesures et que vous connaissez donc une autre valeur de référence, vous pouvez laisser la valeur Plato telle qu'elle a été préréglée.");
    page.replace("[Text_Step4_Sugar_Methode]","Pour diluer le mélange d'eau sucrée, ajoutez de l'eau xyxml.<br>Si vous n'avez pas remesuré et donc connu une autre valeur de référence, vous pouvez laisser la valeur Plato telle qu'elle était prédéfinie.");
    page.replace("[Text_Step5_Sugar_Methode]","Ajouter à nouveau xyxml d'eau<br> Si vous n'avez pas remesuré et donc connu une autre valeur de référence, vous pouvez laisser la valeur Plato telle qu'elle a été prédéfinie.");
    page.replace("[Text_Step6_Sugar_Methode]","Donc maintenant la deuxième et dernière fois, diluez le mélange avec xyxml d'eau<br> Si vous n'avez pas remesuré et donc connaissez une autre valeur de référence, vous pouvez laisser la valeur Plato telle qu'elle était prédéfinie.");
    page.replace("[Text_Step7_Sugar_Methode]","Veuillez ajouter xyxml d'eau pour la dernière fois.<br>Si vous n'avez pas remesuré et que vous connaissez donc une autre valeur de référence, vous pouvez laisser la valeur Plato telle qu'elle était prédéfinie.");
    page.replace("[Text_Wizard_Wifi]","Zum Verdünnen der Zucker-Wasser-Mischung fügen Sie xyxml-Wasser hinzu.<br>Wenn Sie nicht nachgemessen haben und daher einen anderen Referenzwert kennen, können Sie den Plato-Wert so belassen, wie er voreingestellt war.");
    page.replace("[Titel_Calibration]","Calibrage");
    page.replace("[Titel_Home]","Accueil");
    page.replace("[Titel_Information]","Informations");
    page.replace("[Titel_MPU_Calibration]","Calibrage MPU");
    page.replace("[Titel_Settings]","Paramètres");
    page.replace("[Titel_Support]","Support");
    page.replace("[Titel_Wifi_Settings]","Paramètres Wifi");
    page.replace("[Ubidots_Expl]","Vous avez activé Ubidots.<br>Pour des instructions détaillées, voir <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>.");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","Ce mode connecte l'Hydrom au Wifi saisi ci-dessous, il est donc client et peut accéder à Internet si nécessaire.");
    page.replace("[WiFi_Name]","Nom du Wifi");
    page.replace("[Wizard_Connect_Wifi]","Se connecter à un réseau existant");
    page.replace("[Wizard_Procedure_1]","<h4>Procédure de l'installation:</h4><ul><li>réglage des paramètres réseau</li><li>Calibrage de votre Hydrom</li><li>Ajout de service</li><li>Mise en veille de l'Hydrom</li></ul>.");
    page.replace("[Wizard_text_1]","L'Hydrom n'est vraiment utilisable que lorsqu'il est intégré à un Wifi existant.");
    page.replace("[Wizard_text_2]","Avec beaucoup de dévouement, mon équipe et moi avons développé ce produit pour vous !");
    page.replace("[Wizard_text_3]","L'Hydrom est livré non calibré.<br>Un pot d'eau claire (20°) est nécessaire pour le calibrage.<br>Placez l'hydromètre dans l'eau et activez la Mesure.<br>Si vous voulez sauter cette étape, sélectionnez §Skip§.");
    page.replace("[Wizard_text_4]","<br>La dernière étape consiste à configurer un service.<br>Ce service sert à visualiser les données et à les rendre utilisables.<br>Trois services différents sont pris en charge :<br>1.Bluetooth<br>2. propre serveur (Local)<br>3. service cloud (internet)<br>.");
    page.replace("[Wizard_Thank_You_1]","Merci d'avoir choisi Hydrom !");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Réseau");
    page.replace("[Link_Manuel]","§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt.");
    page.replace("[Text_Wifi_AP_Mode]","Chaque fois que l'hydrom est démarré en mode configuration, alors l'hydrom met en place un wifi.");
    page.replace("[Label_Devicename]","Nom du dispositif");
    page.replace("[Headline_SUCCESS]","SUCCESS !");
    page.replace("[Headline_INFORMATION]","INFORMATION :");
    page.replace("[Headline_ERROR]","ERREUR !");
    page.replace("[Headline_WARNING]","WARNING !");
    page.replace("[Battery]","Bat :");
    page.replace("[Button_Start]","commencer");
    page.replace("[Text_2_Step0_Sugar_Methode]","Prévoyez un bol d'une contenance d'au moins 2100ml.<br>Aussi, à 400ml d'eau, l'hydrom doit déjà flotter librement.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Saisissez ici le volume d'eau initial en ml :");
    page.replace("[Text_4_Step0_Sugar_Methode]","Lorsque vous avez terminé votre saisie, confirmez avec $DONE$.<br>L'hydrom va mesurer l'angle actuel et l'enregistrer pour le calibrage.");
    page.replace("[Button_Done]","DONE");
    page.replace("[Headline_Plato]","Platon");
    page.replace("[Headline_Gravity]","Gravité");
    page.replace("[Headline_Temperature]","Température");
    page.replace("[Headline_Deviation]","Déviation");
    page.replace("[Text_Update]","<h1>Chargez le micrologiciel</h1><p>Veuillez absolument vous assurer que vous téléchargez uniquement le micrologiciel original d'Hydrom. Si le téléchargement se passe mal, alors la garantie est annulée et le micrologiciel doit être laborieusement réinstallé avec du matériel spécial.</p><p>Version actuelle du micrologiciel : ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Mise à jour");
    page.replace("[Titel_Service]","Service");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading French Language Pack");
    return true;
}



/**
 * @brief Loads Dutch UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_Dutch() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading Dutch Language Pack");
    page.replace("[Brewblox_Expl]","U heeft Brewblox ingeschakeld.<br>Voor gedetailleerde instructies, zie <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>");
    page.replace("[Brewfather_Expl]","U heeft Brewfather ingeschakeld.<br>Vervang de XXXXX door de ID van de Brewfather APP.<br>Voor gedetailleerde instructies, zie <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Terug");
    page.replace("[Button_SAVE]","OPSLAAN");
    page.replace("[Button_Skip]","Overslaan");
    page.replace("[Button_Sleep_Deep]","Diep slapen");
    page.replace("[Button_Start_PWM]","PlainwaterMeting doen");
    page.replace("[Button_Start_Wizard]","Wizard starten");
    page.replace("[Button_TestMessage]","Testbericht");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Kalibratie");
    page.replace("[Craftbeerpi_Expl]","U heeft CraftbeerPi ingeschakeld.");
    page.replace("[DeepSleep_Expl]","Om de hydrom te laten functioneren, moet de slaaptijd worden ingesteld.<br>De hydrom valt in slaap en wordt op de ingestelde tijd wakker en stuurt de gemeten waarden naar de service.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","U heeft Grainfather ingeschakeld.<br>Voor gedetailleerde instructies, zie <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>");
    page.replace("[Headine_Conclusion]","Conclusie");
    page.replace("[Headline_Activ_Client_Mode]","Client-Modus activeren");
    page.replace("[Headline_AP_Mode]","ConfigurationWiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Formulier kosten");
    page.replace("[Headline_DeepSleep]","DiepSlaap");
    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","PlainWater-Meeting");
    page.replace("[Headline_Schritt]","Stap");
    page.replace("[Headline_Testmessage]","Test bericht");
    page.replace("[Home_Last_Update]","van laatste update");
    page.replace("[Home_Temperature]","Temperatuur");
    page.replace("[Http_Expl]","U heeft Http ingeschakeld.<br>Voor gedetailleerde instructies, zie <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>");
    page.replace("[InfluxDB_Expl]","U hebt InfluxDB ingeschakeld.");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");
    page.replace("[Label_Database]","Database");
    page.replace("[Label_Instance]","Instantie");
    page.replace("[Label_Job]","Job");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Taal");
    page.replace("[Label_Measurementsname]","Metingsnaam");
    page.replace("[Label_Methode]","Methode");
    page.replace("[Label_Methode_Reference]","Referentie");
    page.replace("[Label_Methode_Sugar]","Suiker");
    page.replace("[Label_Offset]","Offset");
    page.replace("[Label_Password]","Wachtwoord");
    page.replace("[Label_Port]","Poort");
    page.replace("[Label_Server]","Server");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Temperatuur Eenheid");
    page.replace("[Label_Tilt_Option_1]","Plato");
    page.replace("[Label_Tilt_Option_2]","SG");
    page.replace("[Label_Tilt_Option_3]","Graad");
    page.replace("[Label_Tilt_Unit]","Kanteleenheid");
    page.replace("[Label_Token]","Penning");
    page.replace("[Label_Topic_Level]","Onderwerp Niveau");
    page.replace("[Label_Trans_PWR]","BLE Zendvermogen");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Gebruikersnaam");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","Update mislukt.<br>De Hydrom herstart.<br>Probeer dan opnieuw<br>");
    page.replace("[Message_UPLOAD_Error]","Het uploaden van het bestand is mislukt.<br>Bestand beschadigd");
    page.replace("[Message_UPLOAD_Success]","Uploaden is gelukt.<br>De Hydrom is nu in slaapstand.<br>Herstart de Hydrom a.u.b.");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"De verbinding kon niet tot stand worden gebracht. Probeer het opnieuw. <br>De Hydrom heeft de volgende voorwaarden aan het netwerk: <br>Encryptie: WEP of WPA2 (WPA3 is in uitvoering) <br>Frequentie: 2.4ghz");
    page.replace("[Message_WiFi_INFO]","De Hydrom staat momenteel in Access Point mode.<br>Dit betekent dat diensten als Brewfather, Grainfather en Brewblox niet gebruikt kunnen worden, omdat het apparaat als client verbonden moet zijn met een Netwerk.<br>Voer de WLAN gegevens hieronder in en druk op §save§");
    page.replace("[Message_WiFi_Success]","De Hydrom is verbonden met het bestaande netwerk!<br>In het bestaande netwerk is de Hydrom bereikbaar via de hostnaam <a href=§http_{Devicename}§>{Devicename}</a> of via het IP adres <a href=§http_{IPAdresse}§>{IPAdresse}</a>");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","U heeft MQTT ingeschakeld.<br>Voor gedetailleerde instructies, zie <a href=§https_instructie.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>");
    page.replace("[Network]","Netwerk");

    page.replace("[Panel_Text_calibration]","Kalibratie");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Diensten");
    page.replace("[Panel_Text_settings]","Instellingen");
    page.replace("[Panel_Text_update]","Updaten");
    page.replace("[Password]","Wachtwoord");
    page.replace("[Prometheus_Expl]","U hebt Prometheus ingeschakeld.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","Tcontrol is ingeschakeld.");
    page.replace("[Tcp_Expl]","U hebt Tcp ingeschakeld.");
    page.replace("[Text_Reference_Methode]","Plaats de hydrometer in de vloeistof waarvan u de Plato of SG waarde weet en noteer de aflezingen hieronder.<br>Als u dit niet meer wilt noteren, verwijder dan het vinkje bij beide schakelaars en de volgende pagina is de samenvatting.");
    page.replace("[Text_Step1_Reference_Methode]","Voeg de hydrom toe aan je referentievloeistof.<br>Dit kan ook het bier zijn dat je op dit moment aan het brouwen bent.<br>Dan wordt je volgende bier slim gebrouwen.");
    page.replace("[Text_Step2_Sugar_Methode]","Maak een verzadigde suikeroplossing door hoeveelheidsugarg suiker toe te voegen aan het water uit stap 1.");
    page.replace("[Text_Step3_Sugar_Methode]","Om het suikerwatermengsel te verdunnen, voegt u xyxml water toe.<br> Als u niet opnieuw hebt gemeten en dus een andere referentiewaarde kent, kunt u de Plato waarde laten staan zoals deze was ingesteld.");
    page.replace("[Text_Step4_Sugar_Methode]","Om het suikerwater mengsel te verdunnen voeg xyxml water toe.<br> Als u niet opnieuw heeft gemeten en dus een andere referentiewaarde weet, kunt u de Plato waarde laten staan zoals deze was ingesteld.");
    page.replace("[Text_Step5_Sugar_Methode]","Voeg nogmaals xyxml water toe<br> Als u niet opnieuw heeft gemeten en dus een andere referentiewaarde weet, kunt u de Plato waarde laten staan zoals deze was vooringesteld.");
    page.replace("[Text_Step6_Sugar_Methode]","Verdun nu voor de een na laatste keer het mengsel met xyxml water<br> Als je niet opnieuw hebt gemeten en dus een andere referentiewaarde weet, kun je de Plato waarde laten staan zoals die vooraf was ingesteld.");
    page.replace("[Text_Step7_Sugar_Methode]","Voeg voor de laatste keer xyxml water toe.<br> Als u niet opnieuw heeft gemeten en daarom een andere referentiewaarde weet, kunt u de Plato waarde laten zoals deze was vooringesteld.");
    page.replace("[Text_Wizard_Wifi]","Here you can enter the access data of your network to connect the Hydrom to the Internet or a local server.<br><br>You don't need that?<br>Well then just go with the button >>[Only_Bluetooth]<< and use the Hydrom with Bluetooth.");
    page.replace("[Titel_Calibration]","Kalibratie");
    page.replace("[Titel_Home]","Home");
    page.replace("[Titel_Information]","Informatie");
    page.replace("[Titel_MPU_Calibration]","MPU-kalibratie");
    page.replace("[Titel_Settings]","Instellingen");
    page.replace("[Titel_Support]","Ondersteuning");
    page.replace("[Titel_Wifi_Settings]","Wifi Instellingen");
    page.replace("[Ubidots_Expl]","U heeft Ubidots ingeschakeld.<br>Voor gedetailleerde instructies, zie <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","Deze modus verbindt de Hydrom met de hieronder ingevoerde Wifi en is dus een client en kan toegang krijgen tot het internet indien nodig.");
    page.replace("[WiFi_Name]","WiFi-Naam");
    page.replace("[Wizard_Connect_Wifi]","Verbinden met bestaand netwerk");
    page.replace("[Wizard_Procedure_1]","<h4>De procedure voor de installatie:</h4><ul><li>de netwerkinstellingen instellen</li><li>uw Hydrom kalibreren</li><li>service toevoegen</li><li>de Hydrom in slaapstand zetten</li></ul>");
    page.replace("[Wizard_text_1]","De Hydrom is pas echt bruikbaar wanneer hij is geïntegreerd in een bestaande Wifi.");
    page.replace("[Wizard_text_2]","Met veel toewijding hebben mijn team en ik dit product voor u ontwikkeld!");
    page.replace("[Wizard_text_3]","De Hydrom wordt ongecalibreerd geleverd.<br>Een pot met helder water (20°) is nodig voor calibratie.<br>Plaats de hydrometer in het water en activeer de Meting.<br>Als u deze stap wilt overslaan, selecteer dan §Skip§.");
    page.replace("[Wizard_text_4]","<br>De laatste stap is het instellen van een service.<br>Deze service dient om de data te visualiseren en bruikbaar te maken.<br>Drie verschillende services worden ondersteund:<br>1.Bluetooth<br>2. Eigen server (Lokaal)<br>3. Cloud service (Internet)<br>");
    page.replace("[Wizard_Thank_You_1]","Dank u voor het kiezen van Hydrom!");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Netwerk");
    page.replace("[Link_Manuel]","§https_instructie.hydrom.io/connect-services-1/connect_to_mqtt");
    page.replace("[Text_Wifi_AP_Mode]","Elke keer als de hydrom wordt gestart in configuratie modus, dan zet de hydrom een wifi op.");
    page.replace("[Label_Devicename]","Devicename");
    page.replace("[Headline_SUCCESS]","SUCCES!");
    page.replace("[Headline_INFORMATION]","INFORMATIE:");
    page.replace("[Headline_ERROR]","ERROR!");
    page.replace("[Headline_WARNING]","WAARSCHUWING!");
    page.replace("[Battery]","Bat:");
    page.replace("[Button_Start]","start");
    page.replace("[Text_2_Step0_Sugar_Methode]","Zorg voor een kom met een inhoud van tenminste 2100ml.<br>Ook moet bij 400ml water de hydrom al vrij zweven.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Voer hier het initiële watervolume in ml in:");
    page.replace("[Text_4_Step0_Sugar_Methode]","Als u klaar bent met uw invoer, bevestigt u met $DONE$.<br>De Hydrom zal de huidige hoek meten en opslaan voor kalibratie.");
    page.replace("[Button_Done]","DONE");
    page.replace("[Headline_Plato]","Plato");
    page.replace("[Headline_Gravity]","Zwaartekracht");
    page.replace("[Headline_Temperature]","Temperatuur");
    page.replace("[Headline_Deviation]","Afwijking");
    page.replace("[Text_Update]","<h1>Firmware laden</h1><p>Vergewis u ervan dat u alleen de originele Hydrom-firmware downloadt. Als het uploaden fout gaat, vervalt de garantie en moet de firmware moeizaam opnieuw worden geïnstalleerd met speciale hardware.</p><p>De huidige firmwareversie: ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Update");
    page.replace("[Titel_Service]","Service");
    page.replace("[Only_Bluetooth]","Alleen Bluetooth");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading Dutch Language Pack");
    return true;
}

/**
 * @brief Loads Portuguese UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_Portuguese() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading Portuguese Language Pack");
    page.replace("[Brewblox_Expl]","Tem o Brewblox activado.<br>Para instruções detalhadas, ver <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>");
    page.replace("[Brewfather_Expl]","Tem o Brewfather activado.<br>Por favor substitua o XXXXX pelo ID do Brewfather APP.<br>Para instruções detalhadas, ver <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Voltar");
    page.replace("[Button_SAVE]","SAVE");
    page.replace("[Button_Skip]","Saltar");
    page.replace("[Button_Sleep_Deep]","Dormir profundamente");
    page.replace("[Button_Start_PWM]","Fazer Medição de Água Normal");
    page.replace("[Button_Start_Wizard]","Start Wizard");
    page.replace("[Button_TestMessage]","Testmessage");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Calibração");
    page.replace("[Craftbeerpi_Expl]","Tem o CraftbeerPi activado.");
    page.replace("[DeepSleep_Expl]","Para que o Hydrom funcione, o tempo de sono deve ser definido.<br>O Hydrom adormece e acorda na hora definida e envia os valores medidos para o serviço.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","Tem o Grainfather activado.<br>Para instruções detalhadas, ver <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>");
    page.replace("[Headine_Conclusion]","Conclusão");
    page.replace("[Headline_Activ_Client_Mode]","Activar o modo Cliente-Modo");
    page.replace("[Headline_AP_Mode]","ConfigurationWiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Costumize Formular");
    page.replace("[Headline_DeepSleep]","DeepSleep");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");
    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","Medição de água simples");
    page.replace("[Headline_Schritt]","Etapa");
    page.replace("[Headline_Testmessage]","Mensagem de teste");
    page.replace("[Home_Last_Update]","da última actualização");
    page.replace("[Home_Temperature]","Temperatura");
    page.replace("[Http_Expl]","Tem Http activado.<br>Para instruções detalhadas, ver <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>");
    page.replace("[InfluxDB_Expl]","Tem o InfluxDB activado.");
    page.replace("[Label_Database]","Base de dados");
    page.replace("[Label_Instance]","Instância");
    page.replace("[Label_Job]","Emprego");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Idioma");
    page.replace("[Label_Measurementsname]","Measurementsname");
    page.replace("[Label_Methode]","Metódico");
    page.replace("[Label_Methode_Reference]","Referência");
    page.replace("[Label_Methode_Sugar]","Açúcar");
    page.replace("[Label_Offset]","Offset");
    page.replace("[Label_Password]","Senha");
    page.replace("[Label_Port]","Porto");
    page.replace("[Label_Server]","Servidor");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Temperature Unit");
    page.replace("[Label_Tilt_Option_1]","Plato");
    page.replace("[Label_Tilt_Option_2]","Specific Gravity");
    page.replace("[Label_Tilt_Option_3]","Degree");
    page.replace("[Label_Tilt_Unit]","Tilt Unit");
    page.replace("[Label_Token]","Token");
    page.replace("[Label_Topic_Level]","Nível do Tópico");
    page.replace("[Label_Trans_PWR]","Poder de Transmitir BLE");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Nome de utilizador");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","Actualização falhada.<br>O Hydrom reinicia.<br>Então tente novamente<br>");
    page.replace("[Message_UPLOAD_Error]","O carregamento do ficheiro falhou.<br>Arquivo corrompido");
    page.replace("[Message_UPLOAD_Success]","Upload was successful.<br>The hydrom is now asleep.<br>Please restart the Hydrom");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"The connection could not be established. Please try again. <br>The Hydrom has the following conditions to the network: <br>Encryption: WEP or WPA2 (WPA3 is in progress) <br>Frequency: 2.4ghz");
    page.replace("[Message_WiFi_INFO]","The Hydrom is currently in Access Point mode.<br>This means that services such as Brewfather, Grainfather and Brewblox cannot be used, as the Device must be connected as a client to a Network.<br>Simply enter the WLAN data below and press §save§");
    page.replace("[Message_WiFi_Success]","The Hydrom is connected to the existing network!<br>In the existing network, the Hydrom is accessible either via the host name <a href=§http_{Devicename}§>{Devicename}</a> or via the IP address <a href=§http_{IPAdresse}§>{IPAdresse}</a>");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","You have MQTT enabled.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>");
    page.replace("[Network]","Network");
    page.replace("[Panel_Text_calibration]","Calibration");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Services");
    page.replace("[Panel_Text_settings]","Settings");
    page.replace("[Panel_Text_update]","Update");
    page.replace("[Password]","Password");
    page.replace("[Prometheus_Expl]","You have Prometheus enabled.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","You have Tcontrol enabled.");
    page.replace("[Tcp_Expl]","Tem o Tcp activado.");
    page.replace("[Text_Reference_Methode]","Coloque o hidrómetro no líquido para o qual conhece o valor de Platão ou SG e registe as leituras abaixo.<br>Se já não desejar registar este item, basta desmarcar ambos os interruptores e a página seguinte é o resumo.");
    page.replace("[Text_Step1_Reference_Methode]","Adicione o hidrómetro ao seu líquido de referência.<br>Esta também pode ser a cerveja que está actualmente a fabricar.<br>Então a sua próxima cerveja será fabricada de forma inteligente.");
    page.replace("[Text_Step2_Sugar_Methode]","Faça uma solução saturada de açúcar adicionando amountofsugarg de açúcar à água da etapa 1.");
    page.replace("[Text_Step3_Sugar_Methode]","Para diluir a mistura de água com açúcar adicione xyxml de água.<br> Se não tiver medido novamente e, portanto, conhecer outro valor de referência, pode deixar o valor de Platão como estava predefinido.");
    page.replace("[Text_Step4_Sugar_Methode]","Para diluir a mistura de açúcar e água adicionar xyxml de água.<br> Se não tiver feito uma nova medição e, portanto, não conhecer outro valor de referência, pode deixar o valor de Platão tal como foi predefinido.");
    page.replace("[Text_Step5_Sugar_Methode]","Adicionar novamente xyxml de água<br> Se não tiver medido novamente e, portanto, não conhecer outro valor de referência, pode deixar o valor de Platão como foi pré-definido.");
    page.replace("[Text_Step6_Sugar_Methode]","So now the second to last time dilute the mixture with xyxml water.<br> If you have not remeasured and therefore know another reference value, you can leave the Plato value as it was preset.");
    page.replace("[Text_Step7_Sugar_Methode]","Por favor adicione xyxml de água pela última vez.<br> Se não tiver remensurado e, portanto, conhecer outro valor de referência, pode deixar o valor de Platão tal como foi pré-definido.");
    page.replace("[Text_Wizard_Wifi]","Here you can enter the access data of your network to connect the Hydrom to the Internet or a local server.<br><br>You don't need that?<br>Well then just go with the button >>[Only_Bluetooth]<< and use the Hydrom with Bluetooth.");
    page.replace("[Titel_Calibration]","Calibração");
    page.replace("[Titel_Home]","Início");
    page.replace("[Titel_Information]","Informação");
    page.replace("[Titel_MPU_Calibration]","Calibração da MPU");
    page.replace("[Titel_Settings]","Definições");
    page.replace("[Titel_Support]","Apoio");
    page.replace("[Titel_Wifi_Settings]","Configurações Wifi");
    page.replace("[Ubidots_Expl]","Tem o Ubidots activado.<br>Para instruções detalhadas, ver <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","Este modo liga o Hydrom ao Wifi entrado abaixo e é assim um cliente e pode aceder à Internet se necessário.");
    page.replace("[WiFi_Name]","WiFi-Name");
    page.replace("[Wizard_Connect_Wifi]","Ligar à rede existente");
    page.replace("[Wizard_Procedure_1]","<h4>Procedimento da instalação:</h4><<<ul>>li>Configuração da rede</li>>li>Calibre o seu Hydrom</li>>li>Adicionar Serviço</li>>li>Põe o Hydrom a dormir</li></ul>");
    page.replace("[Wizard_text_1]","The Hydrom is only really usable when it is integrated into an existing Wifi.");
    page.replace("[Wizard_text_2]","With much dedication, my team and I have developed this product for you!");
    page.replace("[Wizard_text_3]","The Hydrom is delivered uncalibrated.<br>A pot of clear water (20°) is required for calibration.<br>Place the hydrometer in the water and activate the Measurement.<br>If you want to skip this step, select §Skip§.");
    page.replace("[Wizard_text_4]","<br>The last step is to set up a service.<br>This service serves to visualise the data and make it usable.<br>Three different services are supported:<br>1.Bluetooth<br>2. Own server (Local)<br>3. cloud service (internet)<br>");
    page.replace("[Wizard_Thank_You_1]","Thank you for choosing Hydrom!");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Rede");
    page.replace("[Link_Manuel]","https_instruction.hydrom.io/");
    page.replace("[Text_Wifi_AP_Mode]","Cada vez que a hidrom em modo de configuração é iniciada, então a hidrom estabelece um wifi.");
    page.replace("[Label_Devicename]","Devicename");
    page.replace("[Headline_SUCCESS]","SUCESSO!");
    page.replace("[Headline_INFORMATION]","INFORMAÇÃO:");
    page.replace("[Headline_ERROR]","ERROR!");
    page.replace("[Headline_WARNING]","ADVERTÊNCIA!");
    page.replace("[Battery]","Morcego:");
    page.replace("[Button_Start]","início");
    page.replace("[Text_2_Step0_Sugar_Methode]","Fornecer uma taça que contenha pelo menos 2100ml.<br>Ainda, a 400ml de água, o hidrom já deve estar a flutuar livremente.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Introduzir aqui o volume inicial de água em ml:");
    page.replace("[Text_4_Step0_Sugar_Methode]","Quando terminar a sua entrada, confirme com $DONE$.<br>O Hydrom irá medir o ângulo actual e guardá-lo para calibração.");
    page.replace("[Button_Done]","FEITO");
    page.replace("[Headline_Plato]","Platão");
    page.replace("[Headline_Gravity]","Gravidade");
    page.replace("[Headline_Temperature]","Temperatura");
    page.replace("[Headline_Deviation]","Desvio");
    page.replace("[Text_Update]","<h1>Load Firmware</h1>>p>Tenham a certeza absoluta de que descarregam apenas o firmware Hydrom original. Se o carregamento correr mal, então a garantia é anulada e o firmware deve ser laboriosamente reinstalado com hardware especial.</p><p> Versão actual do Firmware: ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Actualização");
    page.replace("[Titel_Service]","Serviço");
    page.replace("[Only_Bluetooth]","Only Bluetooth");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading Portuguese Language Pack");
    return true;
}

/**
 * @brief Loads Swedish UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_Swedish() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading Swedish Language Pack");
    page.replace("[Brewblox_Expl]","Brewblox är aktiverat.<br>För detaljerade instruktioner, se <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>.");
    page.replace("[Brewfather_Expl]","Du har Brewfather aktiverat.<br>Ersätt XXXXX med ID:t från Brewfather APP.<br>För detaljerade instruktioner, se <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Tillbaka");
    page.replace("[Button_SAVE]","SPARA");
    page.replace("[Button_Skip]","Hoppa över");
    page.replace("[Button_Sleep_Deep]","Sova");
    page.replace("[Button_Start_PWM]","Gör PlainwaterMätning");
    page.replace("[Button_Start_Wizard]","Starta guiden");
    page.replace("[Button_TestMessage]","Testmeddelande");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Kalibrering");
    page.replace("[Craftbeerpi_Expl]","Du har aktiverat CraftbeerPi.");
    page.replace("[DeepSleep_Expl]","För att hydrom ska fungera måste sömntiden ställas in.<br>Hydrom somnar och vaknar vid den definierade tiden och skickar mätvärdena till tjänsten.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","Du har Grainfather aktiverat.<br>För detaljerade instruktioner se <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Link</a>.");
    page.replace("[Headine_Conclusion]","Slutsats");
    page.replace("[Headline_Activ_Client_Mode]","Aktivera klientläge");
    page.replace("[Headline_AP_Mode]","ConfigurationWiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Kostymera formuläret");
    page.replace("[Headline_DeepSleep]","DeepSleep");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");

    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","Slättvattenmätning");
    page.replace("[Headline_Schritt]","Steg");
    page.replace("[Headline_Testmessage]","Testmeddelande");
    page.replace("[Home_Last_Update]","från senaste uppdatering");
    page.replace("[Home_Temperature]","Temperatur");
    page.replace("[Http_Expl]","Du har aktiverat Http.<br>För detaljerade instruktioner, se <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>.");
    page.replace("[InfluxDB_Expl]","Du har aktiverat InfluxDB.");
    page.replace("[Label_Database]","Databas");
    page.replace("[Label_Instance]","Instans");
    page.replace("[Label_Job]","Jobb");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Språk");
    page.replace("[Label_Measurementsname]","Measurementsname");
    page.replace("[Label_Methode]","Metod");
    page.replace("[Label_Methode_Reference]","Referens");
    page.replace("[Label_Methode_Sugar]","Socker");
    page.replace("[Label_Offset]","Förskjutning");
    page.replace("[Label_Password]","Lösenord");
    page.replace("[Label_Port]","Port");
    page.replace("[Label_Server]","Server");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Temperaturenhet");
    page.replace("[Label_Tilt_Option_1]","Plato");
    page.replace("[Label_Tilt_Option_2]","Specifik gravitation");
    page.replace("[Label_Tilt_Option_3]","Grad");
    page.replace("[Label_Tilt_Unit]","Enhet för lutning");
    page.replace("[Label_Token]","Token");
    page.replace("[Label_Topic_Level]","Ämnesnivå");
    page.replace("[Label_Trans_PWR]","BLE-sändningseffekt");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Användarnamn");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","Uppdateringen misslyckades.<br> Hydrom startar om.<br> Försök igen<br>");
    page.replace("[Message_UPLOAD_Error]","Uppladdningen av filen misslyckades.<br>Filen är skadad.");
    page.replace("[Message_UPLOAD_Success]","Uppladdningen lyckades.<br>Hydromen sover nu.<br>Vänligen starta om Hydromen.");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"Anslutningen kunde inte upprättas. Försök igen. <br>Hydrom har följande villkor till nätverket: <br>Kryptering: WEP eller WPA2 (WPA3 är på gång) <br>Frekvens: 2,4ghz");
    page.replace("[Message_WiFi_INFO]","Hydrom är för närvarande i läget Access Point.<br>Detta innebär att tjänster som Brewfather, Grainfather och Brewblox inte kan användas, eftersom enheten måste vara ansluten som en klient till ett nätverk.<br>Sätt in WLAN-data nedan och tryck på §save§");
    page.replace("[Message_WiFi_Success]","Hydrom är ansluten till det befintliga nätverket!<br>I det befintliga nätverket är Hydrom tillgänglig antingen via värdnamnet <a href=§http_{Devicename}§>{Devicename}</a> eller via IP-adressen <a href=§http_{IPAdresse}§>{IPAdresse}</a>.");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","Du har MQTT aktiverat.<br>För detaljerade instruktioner, se <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Link</a>.");
    page.replace("[Network]","Nätverk");
    page.replace("[Panel_Text_calibration]","Kalibrering");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Tjänster");
    page.replace("[Panel_Text_settings]","Inställningar");
    page.replace("[Panel_Text_update]","Uppdatera");
    page.replace("[Password]","Lösenord");
    page.replace("[Prometheus_Expl]","Prometheus är aktiverat.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","Du har Tcontrol aktiverat.");
    page.replace("[Tcp_Expl]","Du har Tcp aktiverat.");
    page.replace("[Text_Reference_Methode]","Placera hydrometern i den vätska för vilken du känner till Plato eller SG-värdet och registrera avläsningarna nedan.<br>Om du inte längre vill registrera denna punkt, avmarkera helt enkelt båda kontakterna och nästa sida är sammanfattningen.");
    page.replace("[Text_Step1_Reference_Methode]","Lägg hydrometern i din referensvätska.<br>Detta kan också vara den öl du brygger för närvarande.<br>Därefter kommer din nästa öl att bryggas smart.");
    page.replace("[Text_Step2_Sugar_Methode]","Gör en mättad sockerlösning genom att tillsätta amountofsugarg socker till vattnet från steg 1.");
    page.replace("[Text_Step3_Sugar_Methode]","För att späda ut sockervattenblandningen tillsätt xyxml vatten.<br> Om du inte har mätt om och därför känner till ett annat referensvärde kan du lämna Plato-värdet som det var förinställt.");
    page.replace("[Text_Step4_Sugar_Methode]","För att späda ut sockervattenblandningen tillsätt xyxml vatten.<br> Om du inte har mätt på nytt och därför känner till ett annat referensvärde kan du lämna Platonvärdet som det var förinställt.");
    page.replace("[Text_Step5_Sugar_Methode]","Tillsätt xyxml vatten igen<br> Om du inte har mätt om och därför känner till ett annat referensvärde kan du lämna Platonvärdet som det var förinställt.");
    page.replace("[Text_Step6_Sugar_Methode]","Späd nu för näst sista gången ut blandningen med xyxml vatten<br> Om du inte har mätt om och därför känner till ett annat referensvärde kan du lämna Platonvärdet som det var förinställt.");
    page.replace("[Text_Step7_Sugar_Methode]","Tillsätt xyxml vatten för sista gången.<br> Om du inte har mätt om och därför känner till ett annat referensvärde kan du lämna Platonvärdet som det var förinställt.");
    page.replace("[Text_Wizard_Wifi]","Here you can enter the access data of your network to connect the Hydrom to the Internet or a local server.<br><br>You don't need that?<br>Well then just go with the button >>[Only_Bluetooth]<< and use the Hydrom with Bluetooth.");
    page.replace("[Titel_Calibration]","Kalibrering");
    page.replace("[Titel_Home]","Hem");
    page.replace("[Titel_Information]","Information");
    page.replace("[Titel_MPU_Calibration]","MPU-kalibrering");
    page.replace("[Titel_Settings]","Inställningar");
    page.replace("[Titel_Support]","Stöd");
    page.replace("[Titel_Wifi_Settings]","Wifi-inställningar");
    page.replace("[Ubidots_Expl]","Du har aktiverat Ubidots.<br>För detaljerade instruktioner, se <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>.");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","Detta läge ansluter Hydrom till det Wifi som anges nedan och är således en klient och kan få tillgång till Internet vid behov.");
    page.replace("[WiFi_Name]","WiFi-namn");
    page.replace("[Wizard_Connect_Wifi]","Anslut till befintligt nätverk");
    page.replace("[Wizard_Procedure_1]","<h4> Förfarande för installationen:</h4><ul><li>Inställ nätverksinställningar</li><li>Kalibrera din Hydrom</li><li>Tillägg tjänst</li><li>Sätt Hydrom i vila</li><li>Sätt Hydrom i vila</li></ul>.");
    page.replace("[Wizard_text_1]","Hydrom är egentligen bara användbar när den är integrerad i ett befintligt Wifi.");
    page.replace("[Wizard_text_2]","Med mycket engagemang har mitt team och jag utvecklat den här produkten för dig!");
    page.replace("[Wizard_text_3]","Hydrom levereras okalibrerad.<br>En kruka med klart vatten (20°) krävs för kalibrering.<br>Placera hydrometern i vattnet och aktivera mätningen.<br>Om du vill hoppa över detta steg väljer du §Skip§.");
    page.replace("[Wizard_text_4]","<br>Det sista steget är att ställa in en tjänst.<br>Tjänsten tjänar till att visualisera data och göra dem användbara.<br>Tre olika tjänster stöds:<br>1. Bluetooth<br>2. Egen server (lokal)<br>3. Molntjänst (internet)<br>.");
    page.replace("[Wizard_Thank_You_1]","Tack för att du har valt Hydrom!");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Nätverk");
    page.replace("[Link_Manuel]","https_instruction.hydrom.io/");
    page.replace("[Text_Wifi_AP_Mode]","Varje gång hydrom startas i konfigurationsläget ställer hydrom upp ett wifi.");
    page.replace("[Label_Devicename]","Enhetsnamn");
    page.replace("[Headline_SUCCESS]","SUCCESS!");
    page.replace("[Headline_INFORMATION]","INFORMATION:");
    page.replace("[Headline_ERROR]","FEL!");
    page.replace("[Headline_WARNING]","VARNING!");
    page.replace("[Battery]","Bat:");
    page.replace("[Button_Start]","start");
    page.replace("[Text_2_Step0_Sugar_Methode]","Se till att det finns en skål som rymmer minst 2100 ml.<br>Också vid 400 ml vatten bör hydromet redan flyta fritt.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Ange den ursprungliga vattenvolymen i ml här:");
    page.replace("[Text_4_Step0_Sugar_Methode]","När du är klar med din inmatning bekräftar du med $DONE$.<br>Hydrom kommer att mäta den aktuella vinkeln och spara den för kalibrering.");
    page.replace("[Button_Done]","DONE");
    page.replace("[Headline_Plato]","Plato");
    page.replace("[Headline_Gravity]","Gravitation");
    page.replace("[Headline_Temperature]","Temperatur");
    page.replace("[Headline_Deviation]","Avvikelse");
    page.replace("[Text_Update]","<h1>Lad in firmware</h1><p>Var absolut säker på att du endast laddar ner Hydroms originalfirmware. Om uppladdningen går fel är garantin ogiltig och den fasta programvaran måste mödosamt installeras på nytt med speciell hårdvara.</p><p>Aktuell version av den fasta programvaran: ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Uppdatera");
    page.replace("[Titel_Service]","Service");
    page.replace("[Only_Bluetooth]","Endast Bluetooth");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading Swedish Language Pack");
    return true;
}

/**
 * @brief Loads Finnish UI string translations into the language variables.
 * @return true Always returns true.
 */
boolean load_Finnish() {
    Print_Info(1, Hydrom.current_Log_Level, "Start loading Finnish Language Pack");
    page.replace("[Brewblox_Expl]","Sinulla on Brewblox käytössä.<br> Yksityiskohtaiset ohjeet löydät osoitteesta <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewblox§ target=§_blank§>Link</a>.");
    page.replace("[Brewfather_Expl]","Sinulla on Brewfather käytössä.<br>Korvaa XXXXX Brewfather APP:n tunnisteella.<br>Edelliset ohjeet löydät osoitteesta <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_brewfather§ target=§_blank§>Link</a>.");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","Takaisin");
    page.replace("[Button_SAVE]","SAVE");
    page.replace("[Button_Skip]","Ohita");
    page.replace("[Button_Sleep_Deep]","Nukuttaa");
    page.replace("[Button_Start_PWM]","Do PlainwaterMeasurement");
    page.replace("[Button_Start_Wizard]","Start Wizard");
    page.replace("[Button_TestMessage]","Testmessage");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","Kalibrointi");
    page.replace("[Craftbeerpi_Expl]","Sinulla on CraftbeerPi käytössäsi.");
    page.replace("[DeepSleep_Expl]","Jotta Hydrom toimisi, on asetettava nukkumisaika.<br>Hydrom nukahtaa ja herää määritettynä aikana ja lähettää mitatut arvot palveluun.");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","You have activated the recognition of the headstand.<br>This means that when the hydrometer is upside down, no more measured values are sent.");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","Sinulla on Grainfather käytössäsi.<br>Esittelyohjeet löytyvät <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_grainfather§ target=§_blank§>Linkki</a>.");
    page.replace("[Headine_Conclusion]","Johtopäätös");
    page.replace("[Headline_Activ_Client_Mode]","Aktivoi Client-tila");
    page.replace("[Headline_AP_Mode]","ConfigurationWiFi");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","Bluetooth");
    page.replace("[Headline_Cost_Form]","Muokkaa lomaketta");
    page.replace("[Headline_DeepSleep]","DeepSleep");
    page.replace("[Headline_RecHeadstand]","Recognize<br>Headstand");

    page.replace("[Headline_TempComp]","TempComp");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","PlainWater-Measurement");
    page.replace("[Headline_Schritt]","Step");
    page.replace("[Headline_Testmessage]","Testiviesti");
    page.replace("[Home_Last_Update]","viimeisimmästä päivityksestä");
    page.replace("[Home_Temperature]","Lämpötila");
    page.replace("[Http_Expl]","Sinulla on Http käytössä.<br>Tarkemmat ohjeet löydät <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_http§ target=§_blank§>Link</a>.");
    page.replace("[InfluxDB_Expl]","Sinulla on InfluxDB käytössäsi.");
    page.replace("[Label_Database]","Tietokanta");
    page.replace("[Label_Instance]","Instanssi");
    page.replace("[Label_Job]","Työ");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","Kieli");
    page.replace("[Label_Measurementsname]","Measurementsname");
    page.replace("[Label_Methode]","Menetelmä");
    page.replace("[Label_Methode_Reference]","Viite");
    page.replace("[Label_Methode_Sugar]","Sokeri");
    page.replace("[Label_Offset]","Offset");
    page.replace("[Label_Password]","Salasana");
    page.replace("[Label_Port]","Portti");
    page.replace("[Label_Server]","Palvelin");
    page.replace("[Label_Temperature_Option_1]","Celsius");
    page.replace("[Label_Temperature_Option_2]","Fahrenheit");
    page.replace("[Label_Temperature_Option_3]","Kelvin");
    page.replace("[Label_Temperature_Unit]","Lämpötilan yksikkö");
    page.replace("[Label_Tilt_Option_1]","Plato");
    page.replace("[Label_Tilt_Option_2]","Ominaispaino");
    page.replace("[Label_Tilt_Option_3]","Astetta");
    page.replace("[Label_Tilt_Unit]","Kallistus Yksikkö");
    page.replace("[Label_Token]","Token");
    page.replace("[Label_Topic_Level]","Aihealueen taso");
    page.replace("[Label_Trans_PWR]","BLE-lähetysteho");
    page.replace("[Label_URL]","URL");
    page.replace("[Label_Username]","Käyttäjätunnus");
    page.replace("[Label_UUID]","Colour");
    page.replace("[Message_UPDATE_Error]","Päivitys epäonnistui.<br>Hydrom käynnistyy uudelleen.<br>Yritä sitten uudelleen<br>");
    page.replace("[Message_UPLOAD_Error]","Tiedoston lataus epäonnistui.<br>Tiedosto vioittunut.");
    page.replace("[Message_UPLOAD_Success]","Lataus onnistui.<br>Hydrom on nyt lepotilassa.<br>Käynnistä Hydrom uudelleen.");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"Yhteyttä ei saatu muodostettua. Yritä uudelleen. <br>Hydromilla on seuraavat edellytykset verkkoon: <br>Salaus: WEP tai WPA2 (WPA3 on tekeillä) <br>Taajuus: 2.4ghz");
    page.replace("[Message_WiFi_INFO]","Hydrom on tällä hetkellä Access Point -tilassa.<br>Tämä tarkoittaa, että palveluita, kuten Brewfather, Grainfather ja Brewblox, ei voi käyttää, koska laitteen on oltava kytkettynä asiakkaana verkkoon.<br>Syötä alla olevat WLAN-tiedot ja paina §save§.");
    page.replace("[Message_WiFi_Success]","Hydrom on liitetty olemassa olevaan verkkoon!<br>Olemassa olevassa verkossa Hydromiin pääsee käsiksi joko isäntänimen <a href=§http_{Devicename}§>{Devicename}</a> tai IP-osoitteen <a href=§http_{IPAdresse}§>{IPAdresse}</a> kautta.");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","Sinulla on MQTT käytössä.<br> Yksityiskohtaiset ohjeet löydät <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_mqtt§ target=§_blank§>Linkki</a>.");
    page.replace("[Network]","Verkko");
    page.replace("[Panel_Text_calibration]","Kalibrointi");
    page.replace("[Panel_Text_deepsleep]","DeepSleep");
    page.replace("[Panel_Text_net]","Wifi");
    page.replace("[Panel_Text_services]","Palvelut");
    page.replace("[Panel_Text_settings]","Asetukset");
    page.replace("[Panel_Text_update]","Päivitä");
    page.replace("[Password]","Salasana");
    page.replace("[Prometheus_Expl]","Prometheus on käytössäsi.");
    page.replace("[short_h]","h");
    page.replace("[short_m]","m");
    page.replace("[short_s]","s");
    page.replace("[Tcontrol_Expl]","Sinulla on Tcontrol käytössäsi.");
    page.replace("[Tcp_Expl]","Sinulla on Tcp käytössä.");
    page.replace("[Text_Reference_Methode]","Aseta hydrometri nesteeseen, jonka Plato- tai SG-arvon tiedät, ja kirjaa alla olevat lukemat.<br>Jos et enää halua kirjata tätä kohtaa, poista yksinkertaisesti molemmat kytkimet, ja seuraava sivu on yhteenveto.");
    page.replace("[Text_Step1_Reference_Methode]","Lisää hydrometri referenssinesteeseen.<br>Tämä voi olla myös olut, jota parhaillaan valmistat.<br>Silloin seuraava olut valmistetaan fiksusti.");
    page.replace("[Text_Step2_Sugar_Methode]","Tee tyydyttynyt sokeriliuos lisäämällä amountofsugarg sokeria vaiheesta 1 peräisin olevaan veteen.");
    page.replace("[Text_Step3_Sugar_Methode]","Laimenna sokeri-vesiseos lisäämällä xyxml vettä.<br> Jos et ole mitannut uudelleen ja tiedät siksi toisen vertailuarvon, voit jättää Platon-arvon esiasetetun mukaiseksi.");
    page.replace("[Text_Step4_Sugar_Methode]","Laimentaa sokerivesiseos lisäämällä xyxml vettä.<br> Jos et ole mitannut uudelleen ja tiedät sen vuoksi toisen viitearvon, voit jättää Plato-arvon esiasetetun mukaiseksi.");
    page.replace("[Text_Step5_Sugar_Methode]","Lisää xyxml vettä uudelleen<br> Jos et ole mitannut uudelleen ja tiedät siksi toisen viitearvon, voit jättää Platon-arvon ennalleen.");
    page.replace("[Text_Step6_Sugar_Methode]","Laimenna nyt toiseksi viimeistä kertaa seos xyxml-vedellä.<br> Jos et ole mitannut uudelleen ja tiedät siksi toisen vertailuarvon, voit jättää Platon-arvon ennalleen.");
    page.replace("[Text_Step7_Sugar_Methode]","Lisää viimeisen kerran xyxml vettä.<br> Jos et ole mitannut uudelleen ja tiedät siksi toisen viitearvon, voit jättää Platon-arvon sellaiseksi kuin se oli esiasetettu.");
    page.replace("[Text_Wizard_Wifi]","Zum Verdünnen der Zucker-Wasser-Mischung fügen Sie xyxml-Wasser hinzu.<br> Jos et ole jälkikäteen mitannut ja tunnet näin ollen yhden toisen viitearvon, voitte laskea Plato-arvon niin alas, kuin se oli voreingestellt.");
    page.replace("[Titel_Calibration]","Kalibrointi");
    page.replace("[Titel_Home]","Home");
    page.replace("[Titel_Information]","Tietoja");
    page.replace("[Titel_MPU_Calibration]","MPU-kalibrointi");
    page.replace("[Titel_Settings]","Asetukset");
    page.replace("[Titel_Support]","Tuki");
    page.replace("[Titel_Wifi_Settings]","Wifi asetukset");
    page.replace("[Ubidots_Expl]","Sinulla on Ubidots käytössä.<br> Yksityiskohtaiset ohjeet löydät osoitteesta <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_Ubidots§ target=§_blank§>Link</a>.");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","Tämä tila yhdistää Hydromin alla olevaan syötettyyn Wifiin ja on siten asiakas ja voi tarvittaessa käyttää Internetiä.");
    page.replace("[WiFi_Name]","WiFi-nimi");
    page.replace("[Wizard_Connect_Wifi]","Yhteys olemassa olevaan verkkoon");
    page.replace("[Wizard_Procedure_1]","<h4> Asennuksen kulku:</h4><ul><li><li>Verkkoasetusten määrittäminen</li><li>Kalibroi Hydrom</li><li>Lisää palvelu</li><li>Pane Hydrom lepotilaan</li></ul>.");
    page.replace("[Wizard_text_1]","Hydrom on oikeastaan käyttökelpoinen vain, kun se on integroitu olemassa olevaan Wifiin.");
    page.replace("[Wizard_text_2]","Olemme tiimini kanssa kehittäneet tämän tuotteen sinua varten suurella omistautumisella!");
    page.replace("[Wizard_text_3]","Hydrom toimitetaan kalibroimattomana.<br>Kalibrointiin tarvitaan astia kirkasta vettä (20°).<br>Aseta hydrometri veteen ja aktivoi mittaus.<br>Jos haluat ohittaa tämän vaiheen, valitse §Skip§.");
    page.replace("[Wizard_text_4]","<br>Viimeinen vaihe on palvelun määrittäminen.<br>Palvelun tarkoituksena on visualisoida tiedot ja tehdä niistä käyttökelpoisia.<br>Tuetaan kolmea eri palvelua:<br>1.Bluetooth<br>2. Oma palvelin (paikallinen)<br>3. Pilvipalvelu (internet)<br>");
    page.replace("[Wizard_Thank_You_1]","Kiitos, että valitsit Hydromin!");
    page.replace("[Wizard_titel_1]","Wizard");
    page.replace("[Wizard_titel_2]","Verkko");
    page.replace("[Link_Manuel]","https_instruction.hydrom.io/");
    page.replace("[Text_Wifi_AP_Mode]","Aina kun hydrom käynnistetään konfigurointitilassa, niin hydrom perustaa wlanin.");
    page.replace("[Label_Devicename]","Devicename");
    page.replace("[Headline_SUCCESS]","SUCCESS!");
    page.replace("[Headline_INFORMATION]","TIETOJA:");
    page.replace("[Headline_ERROR]","VIRHE!");
    page.replace("[Headline_WARNING]","VAROITUS!");
    page.replace("[Battery]","Lepakko:");
    page.replace("[Button_Start]","käynnistä");
    page.replace("[Text_2_Step0_Sugar_Methode]","Tarjoa kulho, johon mahtuu vähintään 2100 ml.<br>Myös 400 ml:n vedessä hydromin pitäisi jo kellua vapaasti.");
    page.replace("[Text_3_Step0_Sugar_Methode]","Syötä tähän alkuperäinen vesimäärä ml:na:");
    page.replace("[Text_4_Step0_Sugar_Methode]","Kun olet lopettanut syötön, vahvista $DONE$.<br>Hydrom mittaa nykyisen kulman ja tallentaa sen kalibrointia varten.");
    page.replace("[Button_Done]","DONE");
    page.replace("[Headline_Plato]","Platon");
    page.replace("[Headline_Gravity]","Gravity");
    page.replace("[Headline_Temperature]","Lämpötila");
    page.replace("[Headline_Deviation]","Deviation");
    page.replace("[Text_Update]","<h1>Load Firmware</h1><p>Varmista ehdottomasti, että lataat vain alkuperäisen Hydromin laiteohjelmiston. Jos lataus menee pieleen, takuu raukeaa ja laiteohjelmisto on asennettava vaivalloisesti uudelleen erikoislaitteistolla.</p><p> Nykyinen laiteohjelmiston versio: ");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","UPLOAD");
    page.replace("[Titel_Update]","Päivitys");
    page.replace("[Titel_Service]","Huolto");
    page.replace("[Only_Bluetooth]","Vain Bluetooth");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","Chat ID");
    Print_Info(1, Hydrom.current_Log_Level, "End loading Finnish Language Pack");
    return true;
}

/**
 * @brief Loads an empty (placeholder) language set into the language variables.
 * @return true Always returns true.
 */
boolean load_Emty() {
    page.replace("[Brewblox_Expl]","");
    page.replace("[Brewfather_Expl]","");
    page.replace("[BierBot_Expl]","You have BierBot enabled.<br>Please replace the Token with the API Key from the BierBot APP.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_bierbot§ target=§_blank§>Link</a>.");
    page.replace("[Button_Back]","");
    page.replace("[Button_SAVE]","");
    page.replace("[Button_Skip]","");
    page.replace("[Button_Sleep_Deep]","");
    page.replace("[Button_Start_PWM]","");
    page.replace("[Button_Start_Wizard]","");
    page.replace("[Button_TestMessage]","");
    page.replace("[PlainWaterMeasurement_Expl]","This very quick calibration is suitable for almost all users.<br>You place the hydrom in 20 degree warm clear water so that it floats freely.<br>The hydrom measures the angle and compares it with the empirical values.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/readme/plainwater§ target=§_blank§>Link</a>");
    page.replace("[Calibration_Expl]","The Hydrom is sufficiently calibrated for 99 of 100 users even without this detailed calibration. <br>This calibration is very time-consuming and should therefore only be carried out if the result of the plainwater calibration was not successful.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/reference-method§ target=§_blank§>Link</a>");
    page.replace("[ManuelAdjustment_Expl]","The formula you see here is the one the hydrom uses to establish a relationship between the angle and the fermentation curve.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/calibration/calibration-process/manually-change-the-formula§ target=§_blank§>Link</a>");
    page.replace("[Calibration]","");
    page.replace("[Craftbeerpi_Expl]","");
    page.replace("[DeepSleep_Expl]","");
    page.replace("[TempComp_Expl]","");
    page.replace("[RecHeadstand_Expl]","");
    page.replace("[Message_Warn_HOME]","You are still in configuration mode.<br>So the battery lasts only max. 10h. To use the device as intended select a cloud service and put the Hydrom into deep sleep.<br> <a href=§https_instruction.hydrom.io/§ target=§_blank§>Instruction</a>");
    page.replace("[Grainfather_Expl]","");
    page.replace("[Headine_Conclusion]","");
    page.replace("[Headline_Activ_Client_Mode]","");
    page.replace("[Headline_AP_Mode]","");
    page.replace("[Bluetooth_Expl]","You have activated Bluetooth!<br>This means that with a suitable smartphone/receiver you can now receive BLE messages from the Hydrom. <br>Please use the IOS App App4Hydrom or the Android App Tilt 2.<br>A classic connection is not possible with this transmission method.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/add-bluetooth§ target=§_blank§>Link</a>");
    page.replace("[Headline_Bluetooth]","");
    page.replace("[Headline_Cost_Form]","");
    page.replace("[Headline_DeepSleep]","");
    page.replace("[Headline_TempComp]","");
    page.replace("[short_factor_TempComp]","");
    page.replace("[Headline_PWM]","");
    page.replace("[Headline_Schritt]","");
    page.replace("[Headline_Testmessage]","");
    page.replace("[Home_Last_Update]","");
    page.replace("[Home_Temperature]","");
    page.replace("[Http_Expl]","");
    page.replace("[InfluxDB_Expl]","");
    page.replace("[Label_Database]","");
    page.replace("[Label_Instance]","");
    page.replace("[Label_Job]","");
    page.replace("[Label_Factor]","Factor");
    page.replace("[Label_Language]","");
    page.replace("[Label_Measurementsname]","");
    page.replace("[Label_Methode]","");
    page.replace("[Label_Methode_Reference]","");
    page.replace("[Label_Methode_Sugar]","");
    page.replace("[Label_Offset]","");
    page.replace("[Label_Password]","");
    page.replace("[Label_Port]","");
    page.replace("[Label_Server]","");
    page.replace("[Label_Temperature_Option_1]","");
    page.replace("[Label_Temperature_Option_2]","");
    page.replace("[Label_Temperature_Option_3]","");
    page.replace("[Label_Temperature_Unit]","");
    page.replace("[Label_Tilt_Option_1]","");
    page.replace("[Label_Tilt_Option_2]","");
    page.replace("[Label_Tilt_Option_3]","");
    page.replace("[Label_Tilt_Unit]","");
    page.replace("[Label_Token]","");
    page.replace("[Label_Topic_Level]","");
    page.replace("[Label_Trans_PWR]","");
    page.replace("[Label_URL]","");
    page.replace("[Label_Username]","");
    page.replace("[Label_UUID]","");
    page.replace("[Message_UPDATE_Error]","");
    page.replace("[Message_UPLOAD_Error]","");
    page.replace("[Message_UPLOAD_Success]","");
    page.replace("[Message_WiFi_Found]","The good news is that there is a suitable WLAN nearby.<br>Now all that's left is to get the password right.");
    page.replace("[Message_WiFi_Not_Found]","Unfortunately, no suitable WLAN could be found under the specified name.<br> - Please check the WLAN frequency (Only 2.4Ghz is supported)<br> - Please check the WLAN encryption (Only up to WPA2 is supported).");
    page.replace("[Message_WiFi_Error]","No connection could be established. Probably the password is wrong.<br>Please check if you have entered too many spaces.");//"");
    page.replace("[Message_WiFi_INFO]","");
    page.replace("[Message_WiFi_Success]","");
    page.replace("[Message_Calibration_Success]", "The hydrom was successfully calibrated!");
    page.replace("[Message_Calibration_Error]", "Calibration process failed!");
    page.replace("[Mqtt_Expl]","");
    page.replace("[Network]","");
    page.replace("[Only_Bluetooth]","");
    page.replace("[Panel_Text_calibration]","");
    page.replace("[Panel_Text_deepsleep]","");
    page.replace("[Panel_Text_net]","");
    page.replace("[Panel_Text_services]","");
    page.replace("[Panel_Text_settings]","");
    page.replace("[Panel_Text_update]","");
    page.replace("[Password]","");
    page.replace("[Prometheus_Expl]","");
    page.replace("[short_h]","");
    page.replace("[short_m]","");
    page.replace("[short_s]","");
    page.replace("[Tcontrol_Expl]","");
    page.replace("[Tcp_Expl]","");
    page.replace("[Text_Reference_Methode]","");
    page.replace("[Text_Step1_Reference_Methode]","");
    page.replace("[Text_Step2_Sugar_Methode]","");
    page.replace("[Text_Step3_Sugar_Methode]","");
    page.replace("[Text_Step4_Sugar_Methode]","");
    page.replace("[Text_Step5_Sugar_Methode]","");
    page.replace("[Text_Step6_Sugar_Methode]","");
    page.replace("[Text_Step7_Sugar_Methode]","");
    page.replace("[Text_Wizard_Wifi]","");
    page.replace("[Titel_Calibration]","");
    page.replace("[Titel_Home]","");
    page.replace("[Titel_Information]","");
    page.replace("[Titel_MPU_Calibration]","");
    page.replace("[Titel_Settings]","");
    page.replace("[Titel_Support]","");
    page.replace("[Titel_Wifi_Settings]","");
    page.replace("[Ubidots_Expl]","");
    page.replace("[GoogleSheets_Expl]","You have enabled GoogleSheets.<br>For detailed instructions, see <a href=§https_instruction.hydrom.io/connect-services-1/connect_to_GoogleSheets§ target=§_blank§>Link</a>");
    page.replace("[Wifi_Expl]","");
    page.replace("[WiFi_Name]","");
    page.replace("[Wizard_Connect_Wifi]","");
    page.replace("[Wizard_Procedure_1]","");
    page.replace("[Wizard_text_1]","");
    page.replace("[Wizard_text_2]","");
    page.replace("[Wizard_text_3]","");
    page.replace("[Wizard_text_4]","");
    page.replace("[Wizard_Thank_You_1]","");
    page.replace("[Wizard_titel_1]","");
    page.replace("[Wizard_titel_2]","");
    page.replace("[Link_Manuel]","");
    page.replace("[Text_Wifi_AP_Mode]","");
    page.replace("[Label_Devicename]","");
    page.replace("[Headline_SUCCESS]","");
    page.replace("[Headline_INFORMATION]","");
    page.replace("[Headline_ERROR]","");
    page.replace("[Headline_WARNING]","");
    page.replace("[Battery]","");
    page.replace("[Button_Start]","");
    page.replace("[Text_2_Step0_Sugar_Methode]","");
    page.replace("[Text_3_Step0_Sugar_Methode]","");
    page.replace("[Text_4_Step0_Sugar_Methode]","");
    page.replace("[Button_Done]","");
    page.replace("[Headline_Plato]","");
    page.replace("[Headline_Gravity]","");
    page.replace("[Headline_Temperature]","");
    page.replace("[Headline_Deviation]","");
    page.replace("[Text_Update]","");
    page.replace("[Text_Update_New]","<p>Available firmware: ");   
    page.replace("[Button_Upload]","");
    page.replace("[Titel_Update]","");
    page.replace("[Titel_Service]","");
    page.replace("[Telegram_Expl]","");
    page.replace("[Label_ChatID]","");
    Print_Info(1, Hydrom.current_Log_Level, "End loading Emty Language Pack");
    return true;
}