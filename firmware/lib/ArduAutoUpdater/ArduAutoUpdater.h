/**
 * @file ArduAutoUpdater.h
 * @author TjGer22
 * @brief OTA firmware updater using the GitHub Releases API.
 * @date 2026
 *
 * @details
 * Declares the ArduAutoUpdater class which queries the GitHub
 * Releases API for a given repository, compares the latest tag
 * against the running firmware version and performs an HTTPS OTA
 * update when a newer release is found.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ARDUAUTOUPDATER_H
#define ARDUAUTOUPDATER_H
#include <Arduino.h>

/**
 * @brief ArduAutoUpdater — fetches the latest release from a public GitHub
 *        repository and performs an OTA firmware update if a newer version
 *        is available.
 *
 * Usage
 * -----
 * 1.  Set DEF_GITHUB_REPO in Globals.h:
 *         #define DEF_GITHUB_REPO "owner/repository"
 *
 * 2.  The GitHub Actions release pipeline uploads a file called exactly
 *     "firmware.bin" as a release asset. The updater finds it by that name.
 *
 * 3.  The release tag must match Firmwareversion in Globals.h
 *     (leading "v" is stripped automatically, so "v1.5.7" matches "1.5.7").
 *
 * Typical call sequence
 * ---------------------
 *   updater.set_current_Log_Level(Hydrom.current_Log_Level);
 *
 *   if (updater.FirmwareVersionCheck(Firmwareversion, DEF_GITHUB_REPO)) {
 *       String url = updater.getFirmwareBinaryUrl(DEF_GITHUB_REPO);
 *       if (url.length() > 0)
 *           updater.firmwareUpdate(url);   // reboots on success
 *   }
 *
 * API endpoint used
 * -----------------
 *   GET https://api.github.com/repos/{owner}/{repo}/releases/latest
 *   Accept: application/vnd.github+json
 */
class ArduAutoUpdater {
  public:
    ArduAutoUpdater(void);

    /** Mirror the global log-level so the updater can use Print_Info(). */
    void set_current_Log_Level(int8_t l_current_Log_Level);

    /**
     * Compare the running firmware version with the latest GitHub release tag.
     *
     * @param currentVersion  Version string currently running, e.g. "1.5.6".
     * @param githubRepo      GitHub repository in "owner/repo" format.
     * @return true  when a newer release is available on GitHub.
     * @return false when already up-to-date or when the request fails.
     */
    bool FirmwareVersionCheck(String currentVersion, String githubRepo);

    /**
     * Return the direct download URL of the firmware binary from the latest
     * GitHub release.
     *
     * @param githubRepo  GitHub repository in "owner/repo" format.
     * @param assetName   Release asset file name (default: "firmware.bin").
     * @return browser_download_url string, or empty string on failure.
     */
    String getFirmwareBinaryUrl(String githubRepo,
                                String assetName = "firmware.bin");

    /**
     * Return the tag name of the latest GitHub release, e.g. "v1.5.7".
     *
     * @param githubRepo  GitHub repository in "owner/repo" format.
     * @return Tag string, or empty string on failure.
     */
    String getLatestVersion(String githubRepo);

    /**
     * Download the binary at the given URL and flash it via ESP32 OTA.
     * The device reboots automatically on success.
     *
     * @param firmwareBinaryUrl  Direct HTTPS download URL.
     * @return true on success or no-update, false on error.
     */
    bool firmwareUpdate(String firmwareBinaryUrl);

  private:
    /**
     * Fetch the latest-release JSON from the GitHub Releases API.
     *
     * @param githubRepo  GitHub repository in "owner/repo" format.
     * @return Raw JSON string, or empty string on failure.
     */
    String fetchLatestReleaseJson(String githubRepo);
};

extern ArduAutoUpdater updater;
#endif
