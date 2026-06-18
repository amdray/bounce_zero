[Russian version](README.md)

# Bounce Zero

Port of the original game **Bounce (2002, Java/Sun for Nokia 7210)** to **PlayStation Portable (PSP)**.  
The game logic is fully rewritten in **C**, using only the original game assets from the Nokia phone version.  
The project was created for research purposes, without modifying the content.


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
- Full reimplementation of the game loop and physics
- Reading and using original game data from the JAR version of Bounce 2002:
    - level files 001-011: a J2ME level format parser implemented, with preloading/caching
    - PNG textures: custom loader built on `stb_image` with texture placement in VRAM
    - OTT sound files: RTTTL/OTT parser implemented with real-time PCM generation
    - `lang` files: localization parser implemented with language selection based on PSP system language (with fallback to `lang.xx`)
    - Nokia 7210 fonts (9, 12, 16, 23, 24): extracted from FONT.xml and packed into the CBMF binary format (T4-nibble); rendered via GU_PSM_T4 + CLUT on the PSP GU
    - ability to select a level (1-11)
    - save system via Sony PSP Savedata Utility (`DATA.BIN`, `SCE_UTILITY_SAVEDATA_*`)
- Compatible with real PSP hardware and the PPSSPP emulator

## Shortcomings, known issues
- physics timer in the original is 33 frames, in this port — 30 frames — **fixed in 1.1**
- fonts were rendered pixel-by-pixel without hardware acceleration — **fixed in 1.2** (migrated to CBMF + PSP GU T4/CLUT)
- minor physics discrepancies, not affecting gameplay, will be corrected in a new version — **fixed in 1.1**

## Cheat codes
- During gameplay: `L + R` (press `L` while holding `R`) — toggles invincibility mode.
- Effect: the ball does not pop from hazards (an early `return` fires in `pop_ball()`).
- A sound signal plays when toggled.

## Build
You need [PSP SDK (pspdev)](https://github.com/pspdev/pspdev) installed. Then:

```bash
make
```

The resulting `EBOOT.PBP` will appear in the `release/` directory.

## Run
Copy the contents of the `release/` folder to the PSP memory card:

```
/PSP/GAME/BounceZero/
```

or open `EBOOT.PBP` in the PPSSPP emulator.

## Compatibility
- PlayStation Portable 6.00 and higher
- PPSSPP emulator

## License
The source code is distributed under the **MIT** license.  
All original materials (*Bounce, 2002*) are owned by **Nokia** and/or **Sun Microsystems** and are used for research purposes only.

## About the experiment

All source code was written using the **Claude AI** model as part of a **vibe-coding** experiment — recording a full project based on descriptions of behavior and logic, without manual programming.
