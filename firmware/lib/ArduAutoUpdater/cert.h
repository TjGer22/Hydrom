/**
 * @file cert.h
 * @author TjGer22
 * @brief TLS root certificate for OTA HTTPS connections to GitHub.
 * @date 2026
 *
 * @details
 * Contains the DigiCert / ISRG root CA certificate as a PEM string
 * literal. Used by ArduAutoUpdater to verify the TLS chain when
 * connecting to api.github.com and GitHub asset CDN endpoints.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CERT_H
#define CERT_H

// ============================================================
// cert.h — DigiCert root certificate removed.
//
// The ArduAutoUpdater connects to gitlab.com using setInsecure()
// (no certificate pinning).  For a security-hardened build you
// can embed the ISRG Root X1 certificate here and call
//   l_client->setCACert(GITLAB_ROOT_CA);
// instead of setInsecure() in ArduAutoUpdater.cpp.
// ============================================================

#endif  // CERT_H
