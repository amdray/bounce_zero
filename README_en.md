# Bounce Zero

Port of the original game **Bounce (2002, Java/Sun for Nokia 7210)** to **PlayStation Portable (PSP)**.  
The game logic is fully reimplemented in **C**, using only the original assets from the Nokia phone version.  
This project is for research purposes and does not modify the original content.

## Screenshots
First screen  
![BOUN01179_00000](docs/screenshots/BOUN01179_00000.jpg)
Main menu / pause menu  
![BOUN01179_00001](docs/screenshots/BOUN01179_00001.jpg)
First level with HUD  
![BOUN01179_00002](docs/screenshots/BOUN01179_00002.jpg)
Level select  
![BOUN01179_00003](docs/screenshots/BOUN01179_00003.jpg)
Third level  
![BOUN01179_00004](docs/screenshots/BOUN01179_00004.jpg)

## Features
- Reads and uses original game data from the 2002 JAR version of Bounce; includes the original Nokia 7210 font for authenticity
- Full reimplementation of the game loop and physics
- Compatible with real PSP hardware and the PPSSPP emulator
- Minimal system requirements, no external dependencies

## Build
You need [PSP SDK (pspdev)](https://github.com/pspdev/pspdev) installed.

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkgconf libreadline8 libusb-0.1 libgpgme11 libarchive-tools fakeroot wget
wget https://github.com/pspdev/pspdev/releases/latest/download/pspdev-ubuntu-latest-x86_64.tar.gz
tar -xvf pspdev-ubuntu-latest-x86_64.tar.gz -C $HOME
export PSPDEV="$HOME/pspdev"
export PATH="$PATH:$PSPDEV/bin"
make
```

The resulting `EBOOT.PBP` will appear in the `release/` directory.

## Tools
The `tools/` folder contains utilities for generating fonts and atlases from source text data.  
These scripts are not required for building the game, but they are useful for reproducing the font pipeline.
Documentation for these tools is available in `docs/font_txt_format.md`.
There is also `tools/grid_watcher_1.py` for viewing/checking the tile grid.

## Run
Copy the contents of `release/` to your PSP memory card:

```
/PSP/GAME/BounceZero/
```

Or open `EBOOT.PBP` in the PPSSPP emulator.

## Compatibility
- PlayStation Portable 6.00 or higher
- PPSSPP emulator

## License
The source code is licensed under **MIT**.  
All original materials (*Bounce, 2002*) are owned by **Nokia** and/or **Sun Microsystems** and are used for research purposes only.

## About the experiment

All source code was written using the **Claude AI** model  
as part of a **vibe-coding** experiment — building a game engine  
from behavioral and logic descriptions without manual programming.  
This project exists for research purposes only.
