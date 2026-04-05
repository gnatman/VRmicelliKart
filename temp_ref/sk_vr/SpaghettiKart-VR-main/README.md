![Spaghetti Kart](docs/spaghettigithublight.png#gh-light-mode-only)
![Spaghetti Kart](docs/spaghettigithubnight.png#gh-dark-mode-only)

# Spaghetti Kart in **Virtual Reality**!
## [Download Release v0.2](https://github.com/poregon/SpaghettiKart-VR/releases)
>###### *Requires an MK64 z64 backup to extract game assets.*
> Release v0.2 fixes some HUD elements sizing/distance
#

## TO DO
* Spaghetti's Menus and game menu/hud's 2D depthless textures only show on main application window, not stereo.
* HD texture support
* Additional aspect ratio support
#

## Thank you
Thank you [ShinyWindow](https://github.com/ShinyWindow) for your work on the original Ship of Harkinian VR and libultraship-vr
* [Shipwright-VR](https://github.com/ShinyWindow/Shipwright-VR) (use my updated Shipwright-VR)
* [libultraship-vr](https://github.com/ShinyWindow/libultraship-vr) (use my updated libultraship-vr)
#

## Important Information For VR

* Use **DirectX11** ONLY. OpenGL/Metal not supported yet.
* Use a **4:3 aspect ratio**. Scale window accordingly.
* Disable **MSAA**. No plans to support it.
* Keep **Internal Resolution** at 100%.
* Keep **Enable Advanced Settings** disabled while playing.


## To set the window size up for a larger resolution:
1. Temporarily enable **Advanced Settings** under Settings > Graphics
2. Set **Aspect Ratio** to 4:3
3. Manually resize window **uniformly** (no vertical/horizontal black bars).
4. Disable **Advanced Settings** and close menu
#

# Quick Start

SpaghettiKart does not include any copyrighted assets.  You are required to provide a supported copy of the game.

### 1. Verify your ROM dump
The US ROM is the only supported version. You can verify you have dumped a supported copy of the game by using the SHA-1 File Checksum Online at https://www.romhacking.net/hash/. The hash for a US ROM is SHA-1: 579C48E211AE952530FFC8738709F078D5DD215E.

### 2. Verify your ROM is in .z64 format
Your ROM needs to be in .z64 format. If it's in .n64 format, use the following to convert it to a .z64: https://hack64.net/tools/swapper.php

### 2. Download SpaghettiKart from [Releases](https://github.com/poregon/SpaghettiKart-VR/releases)

### 3. Generating the O2R from the ROM
#### Windows
* Extract every file from the zip into a folder of your choosing.
* Run "Spaghettify.exe" and select your US ROM.

#### Linux
* Extract every file from the zip into a folder of your choosing.
* Ensure `zenity` or `kdialog` package is installed.
* Run "spaghetti.appimage" and select your US ROM. You may have to chmod +x the appimage via terminal.

#### Nintendo Switch
* Run one of the PC releases to generate an `mk64.o2r` file. After launching the game on PC, you will be able to find these files in the same directory as `Spaghettify.exe` or `spaghetti.appimage`.
* Copy the files to your sd card

### 4. Play!
* Launch `Spaghettify.exe`
Congratulations, you are now sailing with SpaghettiKart! Have fun!

# Configuration

### Default controls configuration
| N64 | A | B | L | R | Z | Start | Analogue stick | C buttons | D-Pad |
| - | - | - | - | - | - | - | - | - | - |
| Keyboard | Shift | Ctrl | Q | Space | Z | Enter | Arrow keys | TGFH (↑ ↓ ← →) | Num 8 2 4 6 |
| SDL Gamepad | A | X | LB | RB | LT | Start | L-Stick | R-Stick Up, B, Y, R-Stick Right (↑ ↓ ← →) | D-Pad |

### Other shortcuts
| Keys | Action |
| - | - |
| F11 | Fullscreen |
| Tab | Toggle Alternate assets |
| Ctrl+R | Reset |
| Esc | Settings |

### Graphics Backends
Currently, there are three rendering APIs supported: DirectX11 (Windows), OpenGL (all platforms), and Metal (macOS). You can change which API to use in the `Settings` menu of the menubar, which requires a restart.  If you're having an issue with crashing, you can change the API in the `spaghettify.cfg.json` file by finding the line `"Backend":{`... and changing the `id` value to `3` and set the `Name` to `OpenGL`. `DirectX 11` with id `2` is the default on Windows. `Metal` with id `4` is the default on macOS.

# Custom Assets
Custom assets are packed in `.o2r` or stored `.zip` files. To use custom assets, place them in the `mods` folder.

If you're interested in creating and/or packing your own custom asset `.o2r` files, check out the following tools:
* [**retro - O2R generator**](https://github.com/HarbourMasters64/retro)
* [**fast64 - Blender plugin**](https://github.com/HarbourMasters/fast64)

**Note that .otr archives are not supported in SpaghettiKart!**

# Development
### Building

If you want to manually compile SpaghettiKart, please consult the [building instructions](https://github.com/poregon/SpaghettiKart-VR/blob/main/docs/BUILDING.md).

<a href="https://github.com/poregon/libultraship-vr/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/poweredbylus.darkmode.png">
    <img alt="Powered by libultraship" src="./docs/poweredbylus.lightmode.png">
  </picture>
</a>
