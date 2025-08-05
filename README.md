# Source Switcher for OBS Studio

Plugin for OBS Studio to switch between a list of sources

## Features

- **Source Switching**: Automatically switch between multiple sources
- **Multiple Switching Modes**: Next, Previous, Random, Shuffle, First, Last
- **Time-based Switching**: Switch sources at specified intervals
- **Media State Switching**: Switch based on media playback states
- **Transition Support**: Use OBS transitions between sources
- **Hotkey Support**: Assign hotkeys for manual switching
- **Source Browser**: Easy source selection with "Browse Sources" and "Refresh Sources" buttons
- **Multi-language Support**: Available in English, Portuguese, Slovak, and Chinese

## New in Latest Version

### Enhanced Source Selection
- **Browse Sources Button**: Automatically adds all available sources to the list
- **Refresh Sources Button**: Updates the source list with all currently available sources
- **Improved UX**: No more manual typing of source names - just click and select!

## Download

https://obsproject.com/forum/resources/source-switcher.941/

## Build
- Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
- Check out this repository to plugins/source-switcher
- Add `add_subdirectory(source-switcher)` to plugins/CMakeLists.txt
- Rebuild OBS Studio

## Usage

1. Add the Source Switcher to your scene
2. Click "Browse Sources" to automatically populate the source list
3. Remove any sources you don't want to switch between
4. Configure your switching preferences (timing, transitions, etc.)
5. Set up hotkeys for manual control

## Donations
https://www.paypal.me/exeldro
