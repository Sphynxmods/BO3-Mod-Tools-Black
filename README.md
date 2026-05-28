# ModLauncher Source

Source for the Call of Duty: Black Ops III Mod Tools launcher.

This repository is meant to be published on GitHub with two release downloads:

- `1.0.0` - the public default release
- `Original` - the restore package for the stock files

The code is licensed under Apache 2.0. See `LICENSE` for details.

## For players

If you want the current public build, download `1.0.0` and drag the files into your Black Ops 3 Tools folder.

If you want to go back to the original shipped launcher, download `Original` and drag it into the same folder.

Both packages install into:

- `Call of Duty Black Ops III\modtools\`

Only the launcher entry is replaced. Do not overwrite the compiler, linker, or Radiant files for this release.

### 1.0.0 files

These files go in `Call of Duty Black Ops III\modtools\`:

- `modtools_launcher.bat`
- `ModLauncherCustomRuntime\ModLauncher_custom.exe`
- `ModLauncherCustomRuntime\D3Dcompiler_47.dll`
- `ModLauncherCustomRuntime\Qt6Core.dll`
- `ModLauncherCustomRuntime\Qt6Gui.dll`
- `ModLauncherCustomRuntime\Qt6Network.dll`
- `ModLauncherCustomRuntime\Qt6Svg.dll`
- `ModLauncherCustomRuntime\Qt6Widgets.dll`
- `ModLauncherCustomRuntime\steam_api64.dll`
- `ModLauncherCustomRuntime\steam_appid.txt`
- `ModLauncherCustomRuntime\generic\*`
- `ModLauncherCustomRuntime\iconengines\*`
- `ModLauncherCustomRuntime\imageformats\*`
- `ModLauncherCustomRuntime\networkinformation\*`
- `ModLauncherCustomRuntime\platforms\*`
- `ModLauncherCustomRuntime\styles\*`
- `ModLauncherCustomRuntime\tls\*`

### Original files

These files restore the stock Steam launcher entry:

- `modtools_launcher.bat`

## For source users

Build from Visual Studio using `build_modlauncher_custom.bat`.

The runtime launcher used by the public package is built from this source tree and then copied into the runtime folder under `modtools\ModLauncherCustomRuntime\`.

## Release notes

See `RELEASES.md` for the public release layout and upload checklist.