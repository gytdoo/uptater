<h1 align="center">Uptater</h1>
<p align="center">
  <img src="icon.png" width="256" alt="Uptater Icon">
</p>
<p align="center">A clean, robust GUI for managing Arch Linux packages and AUR updates.</p>

## 📸 Preview
![Uptater Dashboard Preview](assets/uptater.gif)

## 📥 Downloads
[**Download Latest ZST or Binary**](https://github.com/gytdoo/uptater/releases)

To install the ZST Arch package, run this command, replacing "uptater.pkg.tar.zst" with the file you downloaded:
```bash
sudo pacman -U uptater.pkg.tar.zst
```
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

Run the following commands in the project directory via terminal to compile:

```bash
mkdir build && cd build && cmake -B . .. && make
