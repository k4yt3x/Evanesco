# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- The application to minimize to tray on close instead of minimize to adhere to conventions.
- Store the config file in `%APPDATA%` instead of the executable's directory.

### Fixed

- The issue where a notification is incorrectly sent when the application closes.

## [1.3.0] - 2025-08-02

### Added

- A confirmation dialog when resetting preferences.
- Indicator in the window title to show if Evanesco is running with administrator privileges.
- The Autohide feature to automatically hide windows added to a list.
- The `Hide All`, `Unhide All`, `Select All`, and `Deselect All` buttons.
- The option to enable system tray icon.
- The option to hide processes in the autohide list on Evanesco startup.
- The option to minimize Evanesco to the system tray.
- The option to randomize the tray icon's tooltip text.
- The option to randomize the tray icon.
- The option to toggle notification on a window getting automatically hidden.
- The option to wait for processes on the autohide list to create a window.

### Changed

- Disable auto refresh by default to prevent selection issues.
- Improve `WindowHider` logging clarity.
- Redesign the UI and move the buttons into a dock on the right.

### Removed

- The `View` menu bar entry and the actions under it.

## [1.2.0] - 2025-07-21

### Added

- A named shared memory mechanism for IPC to allow the GUI to communicate with the DLL.
- Purple row background for windows that have display affinity set to `WDA_EXCLUDEFROMCAPTURE`.
- The option to hide Evanesco's taskbar icon.
- The option to hide the target window's taskbar icon.
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
