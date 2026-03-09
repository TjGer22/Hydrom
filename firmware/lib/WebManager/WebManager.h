/**
 * @file WebManager.h
 * @author TjGer22
 * @brief Embedded HTTP and WebSocket server for the device configuration UI.
 * @date 2026
 *
 * @details
 * Declares the WebManager class which hosts the configuration web
 * interface on port 80, handles REST-style API endpoints and
 * pushes live sensor data to connected browsers via WebSockets.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef WEBMANAGER_H
#define WEBMANAGER_H

#include <Arduino.h>
#include "NetworkManager.h"

typedef enum
{
    MSG_SUCCESS,
    MSG_INFO,
    MSG_ERROR,
    MSG_WARNING
} WebMessageType_t;

class WebManager
{
public:
    /** @brief Default constructor. */
    WebManager(void);
    /**
     * @brief Initialises internal state flags before the HTTP server is started.
     */
    void begin();
    /**
     * @brief Registers all HTTP route handlers and static file paths, then starts
     *        the HTTP server and the WebSocket server.
     */
    void start(void);
    /**
     * @brief Stops the HTTP and WebSocket servers and releases their resources.
     */
    void stop(void);
    /**
     * @brief Processes pending HTTP client requests; must be called in the main loop.
     */
    void loop(void);
    
private:
};

extern WebManager web;
#endif