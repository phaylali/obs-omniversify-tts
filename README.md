# OBS Omniversify TTS

A high-performance OBS Studio plugin designed for real-time chat preview and local Text-to-Speech (TTS) integration, optimized specifically for **AMD GPUs on Arch Linux**.

## Overview

`obs-omniversify-tts` provides streamers with a low-latency, GPU-accelerated TTS solution that offloads processing from the CPU to ensure smooth gameplay in demanding titles like Counter-Strike 2. The plugin integrates directly into OBS as a custom dock.

## Features

- **Real-time Chat Preview**: View and manage chat messages directly within OBS.
- **Local TTS Processing**: Fully local execution—no cloud latency or privacy concerns.
- **AMD & Cross-GPU Optimization**: Leverages ROCm or **Vulkan** for hardware-accelerated inference.
- **Zero CPU Hogging**: Designed to minimize CPU usage during intense gaming sessions.
- **Simulation Mode**: Built-in debugging tool with an input field to test voices and emoji handling before going live.
- **Emoji Support**: Visual preview supports emojis (using system or bundled fonts), while the TTS engine intelligently ignores them.
- **Engine Selection**: Toggle between different TTS engines (ROCm/Vulkan) and voices directly from the OBS Dock.

## Requirements

- **OS**: Arch Linux (or compatible distributions)
- **Hardware**: AMD GPU (or any Vulkan-capable GPU)
- **Software**: OBS Studio, ROCm drivers (optional) or Vulkan drivers

## Installation

*(Detailed installation steps coming soon)*

1. Ensure ROCm or Vulkan drivers are installed and configured on your Arch Linux system.
2. Clone this repository.
3. Run the automated build script:
   ```bash
   ./build.sh
   ```
4. Restart OBS and enable the `Omniversify TTS` dock.

## License

MIT License

## Support Us

<p align="center">
  <a href="https://ko-fi.com/omniversify">
    <img src="https://raw.githubusercontent.com/phaylali/Omniversify/main/public/images/kofi_logo.svg" width="200" alt="Ko-Fi" />
  </a>
</p>

<p align="center">
  <strong>Keep us going</strong>
</p>

---

&copy; 2026 [Omniversify](https://omniversify.com). All rights reserved.

_Made by Moroccans, for the Omniverse_

[![ReadMeSupportPalestine](https://raw.githubusercontent.com/Safouene1/support-palestine-banner/master/banner-project.svg)](https://donate.unrwa.org/-landing-page/en_EN)
