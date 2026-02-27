<h1>kTTY/krnel</h1>

![Name](https://img.shields.io/badge/zen2arc-krnel-blue?style=flat-square&logo=github)
![License](https://img.shields.io/badge/License-GNU%20GPL%20v3-blue?style=flat-square)
![Build](https://img.shields.io/badge/Build-Failing-red?style=flat-square)
![Zed](https://img.shields.io/badge/Made%20with%20Zed--blue?style=flat-square&logo=zedindustries)

<h2>Maintainer Contact:</h2>

![Matrix](https://img.shields.io/badge/Matrix--blue?style=social&logo=matrix&link=https%3A%2F%2Fmatrix.to%2F%23%2F%40zen2arc%3Amatrix.org)
![Telegram](https://img.shields.io/badge/Telegram--blue?style=social&logo=telegram&link=https%3A%2F%2Ft.me%2Fzen2arc)

<h2>About:</h2>

kTTY (formerly krnel) is a basic UNIX-like x86 kernel/OS written in C, includes basic plugin support for kernel extensions.

Shell currently SHOULD support bash scripts.

<h2>Building:</h2>

This piece of ***almost*** useless software includes a pretty straight-forward and beginner friendly method of building, make. You could choose the hard way and build manually, but you will have to figure it out yourself, i am not providing any tutorials for manual building.

<h3>Windows:</h3>

1. Install WSL using PowerShell using ``wsl --install``
2. Now, install your preferred distribution from Microsoft Store or manually.
3. Finally, we've got to the dependencies. They are pretty minimal
    1. NASM
    2. GCC
    3. GRUB
    4. xorriso (for iso creation only)
4. After you downloaded and installed all dependencies, you could start building by just entering ``make`` into your terminal, its that easy!

<h3>Linux:</h3>

The literal same as Windows, but without WSL obviously.

<h3>macOS:</h3>

Hell nah.

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
