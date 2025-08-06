# Source Switcher for OBS Studio

Plugin for OBS Studio to switch between a list of sources

## Features

- **Source Switching**: Automatically switch between multiple sources
- **Multiple Switching Modes**: Next, Previous, Random, Shuffle, First, Last
- **Time-based Switching**: Switch sources at specified intervals
- **Media State Switching**: Switch based on media playback states
- **Transition Support**: Use OBS transitions between sources
- **Hotkey Support**: Assign hotkeys for manual switching
- **Source Browser**: Easy source selection with dropdown browser for available sources
- **Multi-language Support**: Available in English, Portuguese, Slovak, and Chinese

## New in Latest Version

### Enhanced Source Selection UX
- **Source Browser Dropdown**: Select sources from a dropdown list of all available OBS sources
- **Duplicate Prevention**: Automatically prevents adding the same source twice
- **Improved Initialization**: Sources now display correctly immediately after OBS startup
- **Better User Experience**: No more manual typing of source names - browse and select with ease!

## Download

https://obsproject.com/forum/resources/source-switcher.941/

## Build
- Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
- Check out this repository to plugins/source-switcher
- Add `add_subdirectory(source-switcher)` to plugins/CMakeLists.txt
- Rebuild OBS Studio

## Usage

1. Add the Source Switcher to your scene
2. Open the Source Switcher properties
3. Use the "Browse Sources" dropdown to easily select and add sources to your list
4. Alternatively, use the + button to manually add sources by name
5. Remove unwanted sources using the trash can button
6. Configure your switching preferences (timing, transitions, etc.)
7. Set up hotkeys for manual control

## Donations
https://www.paypal.me/exeldro
