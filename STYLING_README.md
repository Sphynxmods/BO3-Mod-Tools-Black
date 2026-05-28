# Mod Launcher Styling Guide

This file documents the launcher appearance settings, CSS hooks, and the most useful object names for custom styling.

## Where Styling Comes From

The launcher styling is built from:

- The built-in theme code in `mlMainWindow.cpp`
- The saved `CustomStylesheet` setting from the `Options` dialog
- Per-user settings stored through `QSettings`

The main styling entry point is:

- `mlMainWindow::UpdateTheme()`

The appearance options UI is here:

- `mlMainWindow::OnEditOptions()`

## Appearance Settings

These settings are saved through `QSettings`:

- `ThemeMode`
- `AccentColor`
- `CustomStylesheet`
- `AssetTreeBackgroundImage`
- `AssetTreeBackgroundOpacity`
- `LogBackgroundImage`
- `LogBackgroundOpacity`
- `LogColors/Default`
- `LogColors/Command`
- `LogColors/Info`
- `LogColors/Launch`
- `LogColors/Success`
- `LogColors/Warning`
- `LogColors/Error`

## Background Images

Two background slots are supported:

- Asset tree / maps / mods list
- Output console

## Built-In Themes

The launcher supports three built-in themes:

- `Original Updated` (default)
- `Original Classic`
- `Dark Modern`

`ThemeMode` stores one of:

- `original-updated`
- `original-classic`
- `dark-modern`

`UseDarkTheme` is still read as a legacy fallback for older settings, but new theme changes should use `ThemeMode`.

Backgrounds can use:

- Absolute file paths like `C:/Images/my_bg.png`
- Qt resource paths like `:/resources/forestbg.png`

Backgrounds are rendered through overlay labels and are scaled with `Qt::KeepAspectRatio`.

Relevant code:

- `PrepareBackgroundImageCache()`
- `UpdateBackgroundOverlays()`

## Custom CSS

The `Options` dialog has a `Custom CSS` box. Rules entered there are appended to the generated launcher stylesheet.

Use object-name selectors like this:

```css
#QuickActionButton {
    border-radius: 10px;
}

#OutputConsole {
    border-color: #ff8a2a;
}
```

## Common Object Names

Use these selectors with `#ObjectName`.

Main layout:

- `#AssetListPanel`
- `#CategoryTabs`
- `#AssetTree`
- `#AssetTreeViewport`
- `#AssetTreeBackgroundOverlay`
- `#ActionsPanel`
- `#OutputConsole`
- `#OutputConsoleViewport`
- `#OutputBackgroundOverlay`
- `#FooterStatusLabel`

Buttons and controls:

- `#BuildButton`
- `#BuildEnglishButton`
- `#QuickLaunchCombo`
- `#QuickActionButton`
- `#QuickMiniActionButton`
- `#DisplayNameAddButton`
- `#AccentSwatchButton`
- `#DangerMenuButton`

Item rows:

- `#ItemTitleWidget`
- `#ItemNameButton`
- `#ItemInternalNameLabel`
- `#QuickActionStrip`
- `#ItemSelectCheckBox`

Status and banners:

- `#GameStateLabel`
- `#BackgroundPreview`
- `#InfoBanner`
- `#WarningBanner`
- `#SuccessBanner`

Workshop UI:

- `#WorkshopVersionsList`
- `#WorkshopVersionRow`
- `#WorkshopVersionRowActive`
- `#WorkshopVersionTitleLabel`
- `#WorkshopVersionIdLabel`
- `#WorkshopVersionActiveBadge`
- `#MarkdownPreview`

## Example Tweaks

Make the list buttons rounder:

```css
#QuickActionButton {
    border-radius: 12px;
    padding: 4px 10px;
}
```

Change footer emphasis:

```css
#FooterStatusLabel {
    color: #ffffff;
    font-weight: 700;
}
```

Change selected tab mood:

```css
#CategoryTabs::tab:selected {
    background: #3a6ea5;
    border-color: #3a6ea5;
}
```

## Notes

- The launcher still uses some generic Qt selectors like `QPushButton`, `QCheckBox`, `QComboBox`, and `QMenu` for broad theme defaults.
- For launcher-specific overrides, prefer `#ObjectName` selectors.
- If a background image does not appear, verify the path first. For resource images, make sure the file is listed in `resources.qrc`.
