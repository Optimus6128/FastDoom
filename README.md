# FastDOOM
Doom port for DOS, based on PCDoom by @nukeykt. The goal of this port is to make it as fast as posible for 386/486 personal computers.

## FastDOOM vs Original

* Added FPS ingame viewer. Enabled with "-fps"
* Added FPS calculation after timedemo runs
* Added option to render visplanes (ceiling and floors) without textures. Enable with "-flatsurfaces"
* Added option to render Spectres and invisible objects like real transparent objects (harder to see, a little faster to render). Enable with "-flatshadows"
* Added option to render sky as a flat fixed color. Enable with "-flatsky"
* Added option to render Spectre and invisible objects like the Sega Saturn port did. Enable with "-saturn"
* New option to show only objects that are not far away from the player. All the enemies are still rendered as they're important. Enabled with "-near"
* PC Speaker uses all sounds available (just for fun!)
* Lot's of optimizations to make the game run faster / smoother
* Removed low memory limit (may cause crashes with low RAM)
* NEW DETAIL LEVEL: POTATO. It renders the full scene with a quarter width resolution (max 80x200). 16-bit ISA video cards can play the game full screen much better!! My Western Digital Paradise PVGA1A ISA (1989) can render the game at a constant >25 fps with a 486DX-50. Enabled with "-potato" command parameter and low detail enabled.
* New option to allow more than 8Mb of memory allocation. Enabled with "-ram"
* Removed network gaming support
* Removed joystick support
* Removed Y mouse movement (move forward/backwards)
* Added autorun support (F11 key).
* Added mono sound support. Enabled with "-mono" parameter.
* Added low quality sound support (8000Hz instead of 11025Hz). Enabled with "-lowsound" parameter.
* Replaced DOS/4GW with DOS/32A providing a good speedup!

## DEMO

|        | Doom 1.9 Audio | Doom 1.9 NoAudio | FastDoom 0.2 Audio | FastDoom 0.2 Audio FlatSurfaces | FastDoom 0.2 Audio FlatSurfaces FlatTransparency | FastDoom 0.2 Audio FlatSurfaces Sega Saturn transparency | FastDoom 0.2 NoAudio | FastDoom 0.2 NoAudio FlatSurfaces Sega Saturn transparency |
|--------|----------------|-------------------|--------------------|----------------------------------|----------------------------------------------------|-----------------------------------------------------------|-----------------------|--------------------------------------------------------------|
| FPS    | 19.56          | 21.68             | 22.44              | 25.77                            | 25.86                                              | 26.02                                                     | 24.79                 | 29.05                                                        |
| Gain | 100.00%        | 110.20%           | 114.70%            | 131.70%                          | 132.20%                                            | 133.00%                                                   | 126.74%               | 148.52%                                                      |

FastDoom 0.3 Live Demo:

[<img src="https://img.youtube.com/vi/vJWtdZHjjKY/maxresdefault.jpg" width="50%">](https://youtu.be/vJWtdZHjjKY)

FastDoom 0.2 Benchmark Demo:

[<img src="https://img.youtube.com/vi/oCgHcSkspmI/maxresdefault.jpg" width="50%">](https://youtu.be/oCgHcSkspmI)

## TODO / Nice to have

- [ ] Get more FPS! (assembly, optimizing loops, etc)
- [ ] Add option to remove long dead monsters, as lesser sprites are faster to render
- [ ] Add option to render up to X distance
- [ ] Rewrite sound library (lump cache is not working right, all sounds are being uncached after 0.4 seconds of not being used, which causes major stutters)
- [ ] Add uncapped framerate support (up to 70 fps)
- [ ] Add a realtime performance viewer (FPS, frametime, total R_DrawColumn and R_DrawSpan calls, ...)
- [ ] Add more drawing modes (VESA?, linear VGA?, ...)
- [ ] Optimize drawing engine (front to back renderer?, ...)

## Build instructions

1) Install Open Watcom C version 1.9.
2) Install Turbo Assembler 3.1, which is distributed with Borland C++ 3.1 (1992, not compatible with x64, use DOSBox or a real DOS environment).
3) Add Watcom's bin folder (binnt on Windows, binw on DOS) to the PATH.
4) Run make.bat.
5) (Optional) Install DOS/32A
6) (Optional) Execute "sb -r doom.exe" to replace DOS/4GW

## Contributing

Feel free to add issues or pull requests here on GitHub. I cannot guarantee that I will accept your changes, but feel free to fork the repo and make changes as you see fit. Thanks!
