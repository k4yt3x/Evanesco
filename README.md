# 👁️‍🗨️ Evanesco

Hide any window from screen capture on Windows.

[Demo Video](https://github.com/user-attachments/assets/cd4cb072-0137-4d7b-ba56-bc21c1c269a5)

## Description

This lightweight utility allows you to easily hide any window on your system from screenshots, screen recording, screen sharing, malware, anti-cheat, and other tools. Here are some examples:

- Windows Snipping Tool
- OBS Studio
- Microsoft Teams
- Discord

In case you're wondering, the name "Evanesco" means "disappear" in Latin. I wanted to use a more descriptive name, but I just couldn't resist using the chant of the [invisibility spell](https://bg3.wiki/wiki/Invisibility_(spell)) from Baldur's Gate.

## Roadmap

- [ ] Spy++-like drag-and-drop finder tool
- [ ] Run as a background process to automatically hide certain windows
- [ ] More covert methods to run the shellcode (e.g., thread hijacking, `QueueUserAPC`)
- [ ] More covert methods to load the shellcode (e.g., `LoadLibraryExW`, `LdrLoadDll`, manual mapping)

## Usage

> [!TIP]
> You may want to run Evanesco with administrator privileges so you can hide other windows that are running with administrator privileges.

1. [Download](https://github.com/k4yt3x/Evanesco/releases) and extract Evanesco.
2. Double-click the `Evanesco.exe` file to launch the GUI application.
3. Click to select windows or processes. You can press and hold `Shift` to select multiple continuous processes or press and hold `Ctrl` to select multiple individual processes.
4. Press the `Hide` button to hide the windows or the `Unhide` button to unhide the windows.

You can also use Evanesco from the command line:

```console
Hide windows from screen capture and recording.

Usage:
  evanesco [hide|unhide] --process <processId>
  evanesco [hide|unhide] --window <windowHandle>
  evanesco (Without arguments to launch the GUI)

Examples:
  evanesco hide --process 1234
  evanesco unhide --window 12AB34
```

## How It Works

TL;DR: Remote thread + `SetWindowDisplayAffinity`.

Windows provides the [`SetWindowDisplayAffinity`](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity) function to allow applications to specify where the content of their windows can be displayed. Calling this function with the handle of the window we want to hide from screen captures and the display affinity set to `WDA_EXCLUDEFROMCAPTURE` will cause the window to be transparent and invisible to screen capture software.

However, there is a security restriction where the `SetWindowDisplayAffinity` function can only be called by the process that owns the window, which means we cannot simply write an application that enumerates all windows and calls `SetWindowDisplayAffinity` to hide the windows. The call originate from the process that owns those windows.

Evanesco bypasses this restriction by injecting a piece of shellcode into the process that owns the window we want to hide. This process is very similar to classic DLL injection. Below is an over-simplified overview of this process:

1. Obtain the address of the `SetWindowDisplayAffinity` function in the target process.
2. Allocate a chunk of memory in the target process with read/write/execute permissions (`PAGE_EXECUTE_READWRITE`).
3. Craft the shellcode from a template and insert the function addresses and arguments.
4. Write the shellcode to the target process with `WriteProcessMemory`.
5. Call `CreateRemoteThread` to execute the shellcode in the target process.

The actual implementation is more complex. You can read the source code to see how it works in detail.

## License

This project is licensed under [GNU GPL version 3](https://www.gnu.org/licenses/gpl-3.0.txt).\
Copyright (C) 2025 K4YT3X.

![AGPLv3](https://www.gnu.org/graphics/gplv3-127x51.png)
