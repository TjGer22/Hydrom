/**
 * @file ArduAutoUpdater.cpp
 * @author TjGer22
 * @brief OTA firmware updater using the GitHub Releases API.
 * @date 2026
 *
 * @details
 * Implements the ArduAutoUpdater class. Fetches release metadata
 * via HTTPS, parses the JSON response, locates the binary asset
 * and streams the firmware image to the ESP32 OTA partition.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ArduAutoUpdater.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

#include "Util.h"

/******************************************************************************
 * Module-level state
 ******************************************************************************/
ArduAutoUpdater updater;
static int8_t g_current_Log_Level = 0;

/** GitHub Releases API base URL. */
static const char GITHUB_API_BASE[] =
    "https://api.github.com/repos/";

/******************************************************************************
 * Constructor
 ******************************************************************************/
ArduAutoUpdater::ArduAutoUpdater(void) {
}

/******************************************************************************
 * Public interface
 ******************************************************************************/

/**
 * @brief Sets the log verbosity level used by all ArduAutoUpdater output calls.
 *
 * @param l_current_Log_Level Desired log level (higher = more verbose).
 */
void ArduAutoUpdater::set_current_Log_Level(int8_t l_current_Log_Level) {
    g_current_Log_Level = l_current_Log_Level;
}

// ---------------------------------------------------------------------------

/**
 * @brief Checks whether a newer firmware release is available on GitHub.
 *
 * Fetches the latest release tag from the GitHub Releases API and compares
 * it against the currently running firmware version string.  A leading 'v'
 * or 'V' in the tag is stripped before the comparison.
 *
 * @param currentVersion Version string of the firmware currently running
 *                       (e.g. "1.5.7").
 * @param githubRepo     GitHub repository in "owner/repo" format.
 * @return true  A newer release is available.
 * @return false The firmware is up to date, or the version could not be
 *               retrieved.
 */
bool ArduAutoUpdater::FirmwareVersionCheck(String currentVersion,
                                           String githubRepo) {
    String latestTag = getLatestVersion(githubRepo);
    if (latestTag.length() == 0) {
        Print_Error("FirmwareVersionCheck: could not retrieve latest version");
        return false;
    }

    // Strip a leading 'v' / 'V' so "v1.5.7" compares equal to "1.5.7"
    if (latestTag.startsWith("v") || latestTag.startsWith("V")) {
        latestTag = latestTag.substring(1);
    }

    Print_Info(3, g_current_Log_Level,
               "FirmwareVersionCheck: running=" + currentVersion +
               "  latest=" + latestTag);

    if (latestTag.equals(currentVersion)) {
        Print_Info(4, g_current_Log_Level,
                   "FirmwareVersionCheck: firmware is up to date");
        return false;
    }

    Print_Info(3, g_current_Log_Level,
               "FirmwareVersionCheck: update available -> " + latestTag);
    return true;
}

// ---------------------------------------------------------------------------

/**
 * @brief Returns the tag name of the latest GitHub release.
 *
 * @param githubRepo GitHub repository in "owner/repo" format.
 * @return String    The tag name (e.g. "v1.5.7"), or an empty string on
 *                   error.
 */
String ArduAutoUpdater::getLatestVersion(String githubRepo) {
    String json = fetchLatestReleaseJson(githubRepo);
    if (json.length() == 0) {
        return "";
    }

    // Parse only tag_name to save heap.
    StaticJsonDocument<64> filter;
    filter["tag_name"] = true;

    DynamicJsonDocument doc(512);
    DeserializationError err =
        deserializeJson(doc, json, DeserializationOption::Filter(filter));

    if (err) {
        Print_Error("getLatestVersion: JSON parse error - " +
                    String(err.c_str()));
        return "";
    }

    String tag = doc["tag_name"].as<String>();
    Print_Info(4, g_current_Log_Level, "getLatestVersion: tag=" + tag);
    return tag;
}

// ---------------------------------------------------------------------------

/**
 * @brief Returns the download URL for a named asset in the latest release.
 *
 * Fetches the latest release JSON, iterates over the assets array and
 * returns the browser_download_url for the asset whose name matches
 * @p assetName (case-insensitive).
 *
 * @param githubRepo GitHub repository in "owner/repo" format.
 * @param assetName  File name of the firmware binary asset to locate.
 * @return String    Direct download URL of the asset, or an empty string
 *                   if the asset was not found or an error occurred.
 */
String ArduAutoUpdater::getFirmwareBinaryUrl(String githubRepo,
                                             String assetName) {
    String json = fetchLatestReleaseJson(githubRepo);
    if (json.length() == 0) {
        return "";
    }

    // Filter to only the assets array fields we need.
    StaticJsonDocument<128> filter;
    filter["assets"][0]["name"]                 = true;
    filter["assets"][0]["browser_download_url"] = true;

    // GitHub releases typically have a small number of assets;
    // 4 KB is sufficient for most projects.
    DynamicJsonDocument doc(4096);
    DeserializationError err =
        deserializeJson(doc, json, DeserializationOption::Filter(filter));

    if (err) {
        Print_Error("getFirmwareBinaryUrl: JSON parse error - " +
                    String(err.c_str()));
        return "";
    }

    JsonArray assets = doc["assets"].as<JsonArray>();
    for (JsonObject asset : assets) {
        String name = asset["name"].as<String>();
        if (name.equalsIgnoreCase(assetName)) {
            String url = asset["browser_download_url"].as<String>();
            Print_Info(3, g_current_Log_Level,
                       "getFirmwareBinaryUrl: found \"" + name +
                       "\" -> " + url);
            return url;
        }
    }

    Print_Error("getFirmwareBinaryUrl: asset \"" + assetName +
                "\" not found in latest release of " + githubRepo);
    return "";
}

// ---------------------------------------------------------------------------

/**
 * @brief Downloads and flashes a firmware binary via HTTP OTA update.
 *
 * Streams the binary at @p firmwareBinaryUrl directly into the ESP32 OTA
 * partition.  The device reboots automatically upon a successful flash
 * (httpUpdate.rebootOnUpdate(true)).
 *
 * @param firmwareBinaryUrl Direct HTTPS URL to the firmware .bin file.
 * @return true  The update succeeded or no update was necessary.
 * @return false The HTTP update failed; error details are printed to the log.
 */
bool ArduAutoUpdater::firmwareUpdate(String firmwareBinaryUrl) {
    WiFiClientSecure l_client;
    l_client.setInsecure();
    httpUpdate.rebootOnUpdate(true);

    Print_Info(2, g_current_Log_Level,
               "firmwareUpdate: downloading " + firmwareBinaryUrl);

    switch (httpUpdate.update(l_client, firmwareBinaryUrl)) {
        case HTTP_UPDATE_FAILED:
            Print_Error(
                "firmwareUpdate: HTTP_UPDATE_FAILED (" +
                String(httpUpdate.getLastError()) + "): " +
                String(httpUpdate.getLastErrorString().c_str()));
            return false;

        case HTTP_UPDATE_NO_UPDATES:
            Print_Info(3, g_current_Log_Level,
                       "firmwareUpdate: HTTP_UPDATE_NO_UPDATES");
            return true;

        case HTTP_UPDATE_OK:
            Print_Info(3, g_current_Log_Level,
                       "firmwareUpdate: HTTP_UPDATE_OK");
            return true;
    }
    return false;
}

/******************************************************************************
 * Private helpers
 ******************************************************************************/

/**
 * @brief Fetches the raw JSON body of the latest GitHub release.
 *
 * Sends an HTTPS GET request to the GitHub Releases API endpoint and
 * returns the full response body.  The caller is responsible for parsing
 * the JSON.
 *
 * @param githubRepo GitHub repository in "owner/repo" format.
 * @return String    Raw JSON response body, or an empty string on failure.
 */
String ArduAutoUpdater::fetchLatestReleaseJson(String githubRepo) {
    // e.g. https://api.github.com/repos/owner/repo/releases/latest
    String url = String(GITHUB_API_BASE) + githubRepo + "/releases/latest";

    Print_Info(3, g_current_Log_Level,
               "fetchLatestReleaseJson: GET " + url);

    WiFiClientSecure *l_client = new WiFiClientSecure;
    if (!l_client) {
        Print_Error("fetchLatestReleaseJson: could not allocate WiFiClientSecure");
        return "";
    }

    l_client->setInsecure();

    HTTPClient https;
    String     payload;

    if (https.begin(*l_client, url)) {
        // GitHub API requires a User-Agent header and recommends Accept.
        https.addHeader("Accept",               "application/vnd.github+json");
        https.addHeader("X-GitHub-Api-Version", "2022-11-28");
        https.addHeader("User-Agent",           "ArduAutoUpdater-ESP32");

        delay(100);
        int httpCode = https.GET();
        delay(100);

        if (httpCode == HTTP_CODE_OK) {
            payload = https.getString();
            Print_Info(4, g_current_Log_Level,
                       "fetchLatestReleaseJson: received " +
                       String(payload.length()) + " bytes");
        } else {
            Print_Error("fetchLatestReleaseJson: HTTP " +
                        String(httpCode) + " - " +
                        https.errorToString(httpCode));
        }
        https.end();
    } else {
        Print_Error("fetchLatestReleaseJson: https.begin() failed for " + url);
    }

    delete l_client;
    return payload;
}
