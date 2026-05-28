# BO3 Mod Tools Black

BO3 Mod Tools Black is a reworked Call of Duty: Black Ops III Mod Tools launcher.
It keeps the Mod Tools workflow in one place and adds a large set of launcher-side
features for browsing, publishing, logging, and customization.

## Releases

This project ships with two public downloads:

- `1.0.0` - the main public release, used by default
- `Original` - the restore package for the stock launcher entry

Both releases install into:

- `Call of Duty Black Ops III\modtools\`

These are launcher-only releases. Do not replace the compiler, linker, or Radiant files.

## Install

1. Download the release you want.
2. Extract it into `Call of Duty Black Ops III\modtools\`.
3. Overwrite the launcher files when prompted.

For `1.0.0`, the important files are:

- `modtools_launcher.bat`
- `ModLauncherCustomRuntime\*`

For `Original`, the package only restores:

- `modtools_launcher.bat`

## Source

If you want to build from source, open `ModLauncherSrc` in Visual Studio and use
`build_modlauncher_custom.bat`.

The rebuilt runtime is copied to `modtools\ModLauncherCustomRuntime\` by the rebuild script.

## Features

### Browsing and project management

- Favorites and Recents help you get back to projects faster.
- Workshop Versions let you save and restore project snapshots before publishing.
- Notes per mod/map let you keep project-specific reminders.
- Display names let you rename projects in the launcher without changing the real folder name.
- Colorized display names let you assign a color to a project for visual grouping.
- Map/Mod tags on Favorites and Recents make the list easier to scan.
- Browsing tabs are recategorized into Recent, Favorites, ZM Maps, MP Maps, Mods, and All.

### Themes

- Original Updated is the modern default look.
- Original Classic keeps the classic launcher feel and classic selection behavior.
- Dark Modern is a darker, cleaner theme.

### Quick actions and right-click menu

- Quick Launch Map starts a map faster from the launcher.
- Quick Actions on the maps/mods list let you run or build directly from an item.
- The right-click menu was reworked so common actions are easier to reach.
- Right-click publishing lets you publish a map or mod directly from the item menu.

### Publishing

- Reworked Publish flow guides the upload process.
- Publish Drafts let you prepare a Workshop post before uploading.
- Debrief Description and Steam Workshop Description are handled separately.
- Ready for Publish removes XPAKs and compiles all languages automatically.

### Online mode and launch flow

- Online Mode supports Steam handoff and shows launch progress while the game starts.
- The launcher can run the game even when the Run box is not ticked.
- Rebuild while the game is running is supported.

### Build behavior

- Build now defaults to all languages.
- Build (English) only builds English.
- Multi-prompt delete asks twice before deleting a project.

### Console log

- The console log uses color coding for different message types.
- Different tabs split errors and warnings into separate views.
- Quick-copy buttons make it easy to copy a log block.
- Console filters in the bottom-right let you hide or show log categories.

### Settings

- The settings menu was reworked.
- APE Reset clears cache data.
- Custom CSS can override launcher styling.
- Custom backgrounds are supported for the maps/mods list and the console log.
- Many toggles are exposed so you can enable or disable launcher features.

## How some features work

- Display names only affect the launcher UI. They do not rename the real folder.
- Colorized display names are for organization and visual grouping.
- Workshop Versions are stored snapshots, so you can move between versions before uploading.
- Notes are saved per map or mod, so each project can carry its own info.
- Ready for Publish is the fast prep step before uploading. It strips XPAKs and runs the language compile pass for you.
- Online Mode shows a progress window while Steam hands off the launch request.

## Upload checklist

- `1.0.0` is the default public release.
- `Original` restores the stock launcher entry.
- Upload the zipped release assets, not the raw folder tree.

## License

The code is licensed under Apache 2.0. See `LICENSE` for details.
