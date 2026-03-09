/**
 * @file Util.h
 * @author TjGer22
 * @brief Shared utility functions: statistics, string helpers and serial logging.
 * @date 2026
 *
 * @details
 * Declares free functions used across the firmware for median
 * filtering, standard-deviation calculation, IP-address conversion,
 * safe string copies and level-gated serial logging.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef UTIL_H
#define UTIL_H
#include <Arduino.h>

/**
 * @brief Computes the arithmetic mean of a float array.
 *
 * @param samples Pointer to the array of float samples.
 * @param count   Number of elements in the array.
 * @return float  The mean (average) of all values in the array.
 */
float _Median(float *samples, int count);

/**
 * @brief Computes the mean absolute deviation of a sample array from a
 *        known median.
 *
 * @param samples Pointer to the array of float samples.
 * @param count   Number of elements in the array.
 * @param median  Pre-computed mean/median value to measure deviation from.
 * @return float  The average absolute deviation of all samples from median.
 */
float _Deviation(float *samples, int count, float median);

/**
 * @brief Null-safe copy from a mutable char buffer to another.
 *
 * @param des Destination buffer (must be large enough to hold src content).
 * @param src Source buffer to copy from.
 */
void StringCopy(char *des, char *src);

/**
 * @brief Null-safe copy from a const char string to a mutable buffer.
 *
 * @param des Destination buffer (must be large enough to hold src content).
 * @param src Const source string to copy from.
 */
void StringCopyConst(char *des, const char *src);

/**
 * @brief Converts a packed 32-bit IPv4 address to a dotted-decimal string.
 *
 * @param addr  32-bit IP address in network byte order.
 * @return char* Pointer to a static string in the form "A.B.C.D".
 *               Note: the buffer is overwritten on each call.
 */
char *IpToString(uint32_t addr);

/**
 * @brief Parses a dotted-decimal IP string into a packed 32-bit integer.
 *
 * @param str  Mutable string in the form "A.B.C.D" (modified in place by strtok).
 * @return uint32_t Packed 32-bit IPv4 address in network byte order.
 */
uint32_t StringToIp(char *str);

/**
 * @brief Prints a log message to the serial console if the message's log level
 *        is within the active log threshold.
 *
 * @param l_choosen_Log_Level  Log level of this specific message (lower = more important).
 * @param l_current_Log_Level  Currently configured maximum log level for output.
 * @param l_Message            The message string to print.
 */
void Print_Info(int8_t l_choosen_Log_Level, int8_t l_current_Log_Level, String l_Message);

/**
 * @brief Unconditionally prints an error message to the serial console.
 *
 * @param l_Message The error message string to print.
 */
void Print_Error(String l_Message);

/**
 * @brief Obfuscates a string by interleaving 26 sub-strings in a fixed permutation.
 *        Used internally to obscure sensitive compile-time constants.
 *
 * @param a–z  26 string fragments to be recombined.
 * @return String The reassembled string.
 */
String Salzen(String a, String b, String c, String d, String e, String f, String g,
              String h, String i, String j, String k, String l, String m, String n,
              String o, String p, String q, String r, String s, String t, String u,
              String v, String w, String x, String y, String z);

/**
 * @brief Converts a wall-clock duration into a total number of seconds
 *        suitable for passing to SleepDeep().
 *
 * @param hours   Sleep duration – hours component (0–255).
 * @param minutes Sleep duration – minutes component (0–59).
 * @param seconds Sleep duration – seconds component (0–59).
 * @return uint32_t Total sleep duration in seconds.
 */
uint32_t calculateDeepSleepSeconds(uint8_t hours, uint8_t minutes, uint8_t seconds);

#endif
