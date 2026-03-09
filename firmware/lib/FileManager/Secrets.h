/**
 * @file Secrets.h
 * @author TjGer22
 * @brief Placeholder — OTA credentials are no longer required.
 * @date 2026
 *
 * @details
 * The ArduAutoUpdater fetches firmware directly from the public
 * GitHub Releases API. No private tokens or obfuscated URL
 * fragments are needed. Set the repository in Globals.h via
 * DEF_GITHUB_REPO.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SECRETS_H
#define SECRETS_H

// ============================================================
// Secrets.h — no longer required for OTA firmware updates.
//
// The ArduAutoUpdater now fetches firmware directly from the
// public GitLab Releases API.  No private tokens or obfuscated
// URL fragments are needed.
//
// Set your GitLab project in Globals.h:
//   #define DEF_GITLAB_PROJECT "your-namespace/your-project"
// ============================================================

#endif  // SECRETS_H
