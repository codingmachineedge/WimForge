---
title: Screenshot Gallery
description: A route-by-route tour of the WimForge Material desktop application.
---

# Screenshot Gallery

These captures use the populated, non-destructive demo project in English.
Paths are intentionally neutral, every route uses the same viewport and
scale, and no real Windows image, product key, credential, or private project is
shown.

## Overview
![WimForge Overview with the project metrics, four-step build flow, safety rails, navigation, and current-job status](screenshots/overview.png)

## Source and editions
![Source and editions page with source inspection, clone-before-editing guidance, mount workspace, and output settings](screenshots/source.png)

## Customize
![Customize page with image change categories, reversible project controls, and configuration fields](screenshots/customize.png)

## Group Policy Studio
![Group Policy Studio with the installed-policy catalog, a selected Delivery Optimization policy, desired-state tabs, a schema-generated numeric editor, and a Git-backed commit action](screenshots/group-policy.png)

## Unattended Studio
![Unattended Studio with answer-file profile controls, setup passes, validation, and computer-name settings](screenshots/unattended.png)

## Package Studio
![Package Studio with the Full AI Development profile, software search, package providers, and enabled package cards](screenshots/package-studio.png)

## WinForge Bridge
![WinForge Bridge with typed recipe actions, runtime capability information, and verified OEM staging controls](screenshots/winforge-bridge.png)

## Review and run
![Review and run page with operation validation, dependency-aware plan details, and explicit run controls](screenshots/review-run.png)

## History and recovery
![History Time Machine with the append-only action timeline, branch controls, undo and restore actions, and A/B comparison pane](screenshots/history.png)

## Settings
![Settings page with language, theme, density, motion, project, concurrency, and recovery preferences](screenshots/settings.png)

!!! info "Reproduce the gallery"
    Build the desktop application, then run
    `scripts/capture-documentation-screenshots.ps1`. The script launches each
    route in demo mode and asks WimForge to save a frame directly from its Qt
    Quick window.
