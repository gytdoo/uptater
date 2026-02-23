# Uptater

A clean, robust GUI for managing Arch Linux packages and AUR updates.

## 📸 Preview
![Uptater Dashboard Preview](assets/uptater.gif)

## 📥 Downloads
[**Download Latest Release**](https://github.com/gytdoo/uptater/releases)

## 🛠️ Build Requirements

To build **Uptater** from source, ensure the following dependencies are installed on your system:

### Build Dependencies
* **Build Tools**: `cmake`, `git`, `pkgconf`
* **Libraries**: `qt6-base`, `qtermwidget`
* **Qt Tools**: `qt6-tools` (for MOC and RCC)

### Runtime Dependencies
* `qtermwidget`
* `pacman-contrib` (Required for `checkupdates`)
* `curl` (For remote script execution)

## Building from Source

Run the following commands in the project directory via terminal to compile the project:

```bash
mkdir build && cd build && cmake -B . .. && make
