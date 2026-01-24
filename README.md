# Web App Container

A lightweight, persistent web container built with **C++20** and **Qt 6.8+**. This application allows you to run web applications as standalone desktop apps with isolated profiles, custom icons, and system tray integration. It's a nice alternative to Electron or running web apps from Chrome/Chromium/Brave/Edge.

## ✨ Features

* **Isolated Profiles:** Each instance can have its own cookies, storage, and cache using the `--profile` flag.
* **Persistent Permissions:** Camera and Microphone grants are remembered per-domain in a local `settings.ini`.
* **Custom Branding:** Set the window title, taskbar icon, and tray icon dynamically via command-line arguments.
* **System Tray Integration:** Start minimized or hide the app to the tray to keep your workspace clean.
* **Linux Optimized:** Supports `app-id` (Wayland) and `WM_CLASS` (X11) for correct taskbar grouping.

## 🚀 Getting Started

### Prerequisites

* **Qt 6.8** or higher (specifically `QtWebEngine`, `QtWidgets`, and `QtSvg`)
* **CMake 3.16+**
* **C++20** compliant compiler (GCC/Clang)

### Packages

#### Artix

The package is available in the galaxy repository:

```bash
sudo pacman -S webappcontainer
```

### Building from Source

```bash
# Clone the repository
git clone https://github.com/josephcrowell/webappcontainer.git
cd webappcontainer

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)
```

### DRM/Widevine Support

#### Automatic Download (Default)

The build system automatically downloads Widevine CDM from Google's Chrome component repository. No Chrome installation needed!

```bash
cmake ..  # Widevine is enabled by default
make -j$(nproc)
```

#### Disable Widevine

To build without Widevine support:

```bash
cmake -DENABLE_WIDEVINE=OFF ..
```

#### Update Widevine

To update to the latest Widevine version:

```bash
# From the build directory
make update-widevine
```

#### Notes

* Widevine is downloaded as a Chrome component (CRX3 format) without requiring Chrome/Chromium installation
* Only Linux x86_64 is currently supported for automatic download
* The Widevine CDM enables DRM-protected content playback (Netflix, Spotify, etc.)

#### DRM Status

**Status**: ✅ **Widevine DRM is WORKING!**

The application automatically downloads and configures Widevine CDM for DRM-protected content playback (Netflix, Spotify, etc.).

**Verification:**
```bash
# Check that Widevine is enabled
./webappcontainer --url "https://open.spotify.com" 2>&1 | grep Widevine
# Output: "Widevine CDM enabled via: ...--widevine-path=..."
```

**Requirements:**

* Qt WebEngine must be built with proprietary codecs (default on most distributions)
* Widevine CDM 4.10.2934.0 (automatically downloaded during build)

## 🛠 Usage

Run the executable followed by your desired configuration:

```bash
webappcontainer [options]
```

### Command Line Arguments

| Option | Description |
| :--- | :--- |
| `-u, --url <url>` | The initial URL to open (e.g., `https://web.whatsapp.com or web.whatsapp.com`). |
| `-a, --app-id <id>` | Unique ID (e.g., `com.user.app`) for Linux taskbar grouping. |
| `-p, --profile <name>`| Name of the profile folder (stores cookies and settings). |
| `-n, --name <name>` | The display name for the window and tray tooltip. |
| `-i, --icon <path>` | Path to a PNG/SVG for the window/taskbar icon. |
| `-t, --tray-icon <path>`| Path to a PNG/SVG for the system tray icon. |
| `--minimized` | Start the application hidden in the system tray. |
| `--no-notify` | Don't notify when minimizing or closing to the tray. |
| `-h, --help` | Display help information and exit. |

### Example

To launch a dedicated Discord container:
```bash
webappcontainer --name "Discord" --url "https://discord.com/app" --profile "chat" --icon "./icons/discord.png" --app-id "com.joseph.discord"
```

## 📂 Directory Structure

Files are stored in your user's local data directory (e.g., `~/.local/share/JosephCrowell/Web App Container/`):
* `QtWebEngine/<profile_name>/settings.ini`: Stores window geometry and site permissions.
* `QtWebEngine/<profile_name>/Network/`: Stores persistent cookies.
* `QtWebEngine/<profile_name>/cache/`: Stores temporary web data.

## ⚖️ License

This project is licensed under the **GPL-2.0-or-later**.

```text
SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 Joseph Crowell
```
