# DY - Download Anything

A fast, lightweight GTK4 video downloader written in pure C.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Language](https://img.shields.io/badge/language-C-brightgreen.svg)

## Features

- 🚀 **Fast & Lightweight** - Written in pure C, compiles in seconds
- 📊 **Real-time Progress** - Live download percentage and speed indicator
- 🎵 **MP3 Conversion** - Download audio-only in MP3 format
- 🌐 **1000+ Sites Supported** - YouTube, Twitter, Instagram, TikTok, Reddit, and more
- 📁 **File Browser** - GUI folder picker for easy directory selection
- ⚡ **Smart URL Detection** - Paste video URLs or webpage URLs, DY finds the video
- 🎨 **Clean GTK4 Interface** - Modern, native Linux GUI

## Screenshots

![DY Screenshot](screenshot.png)

## Installation

### Dependencies

- GTK4
- yt-dlp
- ffmpeg (for video processing)

**Arch Linux:**
```bash
sudo pacman -S gtk4 yt-dlp ffmpeg
```

**Ubuntu/Debian:**
```bash
sudo apt install libgtk-4-dev yt-dlp ffmpeg
```

### Build

```bash
git clone https://github.com/yourusername/dy.git
cd dy
make
```

### Install (Optional)

```bash
make install
```

This copies the binary to `~/.local/bin/dy`

## Usage

Run the application:
```bash
./dy
```

Or if installed:
```bash
dy
```

1. Paste any video URL or webpage URL
2. Choose download directory (or use default ~/Downloads)
3. Check "Download as MP3" if you want audio only
4. Click Download
5. Watch real-time progress and speed!

## Supported Sites

DY uses `yt-dlp` under the hood, which supports 1000+ sites including:

- YouTube
- Twitter/X
- Instagram
- TikTok
- Reddit
- Vimeo
- Dailymotion
- Facebook
- Twitch
- And many more!

## Why C?

- **Simple** - No complex build systems, just `make`
- **Fast** - Compiles in under a second
- **Lightweight** - Small binary, minimal dependencies
- **Reliable** - No runtime errors, no garbage collector
- **Native** - Direct GTK4 bindings, no abstractions

## License

MIT License - See LICENSE file for details

## Contributing

Pull requests welcome! This is a simple project to demonstrate clean C + GTK4 development.

## Author

Built with ❤️ and pure C
