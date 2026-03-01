<h1>kTTY/krnel</h1>

![Name](https://img.shields.io/badge/zen2arc-krnel-blue?style=flat-square&logo=github)
![License](https://img.shields.io/badge/License-GNU%20GPL%20v3-blue?style=flat-square)
![Build](https://img.shields.io/badge/Build-Passing-green?style=flat-square)
![Stability](https://img.shields.io/badge/Running-Stable-green?style=flat-square)
![Zed](https://img.shields.io/badge/Made%20with%20Zed--blue?style=flat-square&logo=zedindustries)

<h2>About:</h2>

kTTY (formerly krnel) is a basic UNIX-like x86 kernel/OS written in C, includes basic plugin support for kernel extensions.

Shell currently SHOULD support bash scripts.

kitty! cats!

<h2>Building:</h2>

This piece of ***almost*** useless software includes a pretty straight-forward and beginner friendly method of building, make. You could choose the hard way and build manually, but you will have to figure it out yourself, i am not providing any tutorials for manual building.

For build instructions, visit the wiki. All instructions will be removed from the readme soon and moved to the wiki.

Arch Linux:
``
sudo pacman -S --needed nasm gcc grub libisoburn qemu-base qemu-desktop
``

Debian:
``
sudo apt install nasm gcc grub-common qemu-system-x86 qemu-utils
``

<h3>MacOS:</h3>

We currently do not offer instructions for MacOS

<h2>Structure:</h2>

krnel/<br>
├── kernel/<br>
│ ├── ata.c<br>
│ ├── kernel.c<br>
│ ├── shell.c<br>
│ ├── ext2.c<br>
│ ├── process.c<br>
│ ├── history.c<br>
│ ├── memory.c<br>
│ ├── fs.c<br>
│ ├── keyboard.c<br>
│ ├── kittywrite.c<br>
│ ├── plugin.c<br>
│ ├── vga.c<br>
│ ├── stackcheck.c<br>
│ ├── sysfetch.c<br>
│ ├── user.c<br>
├── boot/<br>
│ ├── boot.asm<br>
│ ├── linker.ld<br>
├── include/<br>
│ ├── ata.h<br>
│ ├── ext2.h<br>
│ ├── ext2_private.h<br>
│ ├── kernel.h<br>
│ └── plugin.h<br>
├── Makefile<br>
└── grub.cfg<br>

<h2>LICENSE</h2>

This software is licensed under GNU GPL v3

    Copyright (C) 2026  zen2arc (linuxzen@atomicmail.io)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
