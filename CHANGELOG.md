# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- A named shared memory mechanism for IPC to allow the GUI to communicate with the DLL.
- Purple row background for windows that have display affinity set to `WDA_EXCLUDEFROMCAPTURE`.
- The option to hide the taskbar icon.
- The option to randomize the injected DLL file's name.

### Changed

- Do not show a popup message if the hiding/unhiding operation is successful.
- Merge two DLL files into one to reduce the number of files.

## [1.1.0] - 2025-07-18

### Added

- The preferences system to store/load user preferences.
- The option to randomize the window titles.
- The option to hide Evanesco's own window from screen capture.
- A setup file for easier installation.

### Changed

- The code execution method from shellcode injection to DLL injection.

### Fixed

- The issue where invisible application windows are shown after unhidden.

## [1.0.0] - 2025-07-14

### Added

- Automatic refresh of the window and process lists.
- Filters to show only windows or processes with matching names.
- The functionality to hide x86 and x64 windows from screen capture.
