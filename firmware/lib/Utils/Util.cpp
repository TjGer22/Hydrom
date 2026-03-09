/**
 * @file Util.cpp
 * @author TjGer22
 * @brief Shared utility functions: statistics, string helpers and serial logging.
 * @date 2026
 *
 * @details
 * Implements the utility functions declared in Util.h. Provides
 * median and deviation calculations, IP conversion helpers,
 * null-safe string copy and the Print_Info / Print_Error logging
 * wrappers.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <Arduino.h>
#include "Util.h"
#include <stdlib.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>

#define DEBUG_PRINT(p, ...) Serial.print(p, ##__VA_ARGS__)
#define DEBUG_PRINTF(p, ...) Serial.printf(p, ##__VA_ARGS__)

/**
 * @brief Null-safe copy from a mutable char buffer to another.
 *
 * @param des Destination buffer (must be large enough to hold src content).
 * @param src Source buffer to copy from.
 */
void StringCopy(char *des, char *src)
{
    if (!des || !src) return;
    size_t srcLen = strlen(src);
    memset(des, 0, srcLen + 1);
    memcpy(des, src, srcLen);
}

/**
 * @brief Null-safe copy from a const char string to a mutable buffer.
 *
 * @param des Destination buffer (must be large enough to hold src content).
 * @param src Const source string to copy from.
 */
void StringCopyConst(char *des, const char *src)
{
    if (!des || !src) return;
    size_t srcLen = strlen(src);
    memset(des, 0, srcLen + 1);
    memcpy(des, src, srcLen);
}

/**
 * @brief Converts a packed 32-bit IPv4 address to a dotted-decimal string.
 *
 * @param addr  32-bit IP address in network byte order.
 * @return char* Pointer to a static string "A.B.C.D". Overwritten on each call.
 */
char *IpToString(uint32_t addr)
{
    static char ip_str[15];
    sprintf(ip_str, "%d.%d.%d.%d", ((char *)&addr)[0], ((char *)&addr)[1], ((char *)&addr)[2], ((char *)&addr)[3]);
    return ip_str;
}

/**
 * @brief Parses a dotted-decimal IP string into a packed 32-bit integer.
 *
 * @param str  Mutable string "A.B.C.D" (modified in place by strtok).
 * @return uint32_t Packed 32-bit IPv4 address in network byte order.
 */
uint32_t StringToIp(char *str)
{
    uint32_t ip_addr;
    char *pch;
    uint8_t x = 0;
    pch = strtok(str, ".");
    while ((pch != NULL))
    {
        ((uint8_t *)&ip_addr)[x] = (uint16_t)atoi((const char *)pch);
        x++;
        pch = strtok(NULL, ".");
    }
    return ip_addr;
}

/**
 * @brief Prints a log message to serial if its level is within the active threshold.
 *
 * @param l_choosen_Log_Level  Log level of this message (lower = more important).
 * @param l_current_Log_Level  Maximum log level currently active for output.
 * @param l_Message            Message string to print.
 */
void Print_Info(int8_t l_choosen_Log_Level, int8_t l_current_Log_Level, String l_Message)
{
    if (l_choosen_Log_Level <= l_current_Log_Level)
        DEBUG_PRINT("\n" + String(millis()) + "; " + String(l_choosen_Log_Level) + "; " + l_Message);
}

/**
 * @brief Unconditionally prints an error message to the serial console.
 *
 * @param l_Message The error message string to print.
 */
void Print_Error(String l_Message)
{
    DEBUG_PRINT("\n" + String(millis()) + " ERROR: " + l_Message);
}

/**
 * @brief Obfuscates a string by interleaving 26 sub-strings in a fixed permutation.
 *        Used internally to obscure sensitive compile-time constants.
 *
 * @param a–z  26 string fragments to be recombined.
 * @return String The reassembled string in the fixed permutation order.
 */
String Salzen(String a, String b, String c, String d, String e, String f, String g, String h, String i, String j, String k, String l, String m, String n, String o, String p, String q, String r, String s, String t, String u, String v, String w, String x, String y, String z)
{
    return h+k+t+b+o+i+e+n+w+j+v+s+f+r+c+q+p+a+u+l+g+m+x+y+z+d;
}

/**
 * @brief Converts a wall-clock duration into total seconds for SleepDeep().
 *
 * @param hours   Hours component of the sleep duration (0–255).
 * @param minutes Minutes component (0–59).
 * @param secs    Seconds component (0–59).
 * @return uint32_t Total sleep duration in seconds.
 */
uint32_t calculateDeepSleepSeconds(uint8_t hours, uint8_t minutes, uint8_t secs)
{
    return static_cast<uint32_t>(hours) * 3600u
         + static_cast<uint32_t>(minutes) * 60u
         + secs;
}
