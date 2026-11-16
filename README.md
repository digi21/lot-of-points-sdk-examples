# Lot Of Points SDK examples

This repository contains the Lot Of Points software development kit (SDK) headers and example extensions for Lot Of Points
(lop). The examples are written in C++ and in Python. Each Python script has the same behaviour as the C++ command of the
same name, so the two implementations can be compared.

## SDK version

`include/lop_sdk.h` defines `LOP_SDK_API_VERSION` as SDK version 0.7. lop 1.0.31 or later loads extensions compiled with
SDK 0.7 and with SDK 0.6.

## Repository layout

| Path | Content |
|---|---|
| `include/lop_sdk.h` | C interface of the SDK. |
| `include/lop/sdk.hpp` | C++ wrapper over `lop_sdk.h`. |
| `cpp/` | C++20 example extension, built as one dynamic-link library (`lop_examples.dll`) with CMake. |
| `python/` | Python example scripts, each with its ribbon icon (`.png`). |
| `tools/png_to_header.ps1` | PowerShell script that writes a PNG file as a C array in a header (`*_icon.h`), for `lop_command::icon_png`. |

## Examples

Each example is a command with a ribbon button, except where noted. The C++ command and the Python script of the same name
produce the same result.

| Example | What it does |
|---|---|
| `information` | Reads every open cloud without modifying it, in a background task, and writes to the Output panel the point counts by class and return, the coordinate and intensity statistics, the count of points with a colour other than black, and the area covered. |
| `remove_classification` | Assigns class 1 (unclassified) to the points of the editable clouds that keep the indices of the original file, in a background task, and records the changes as one undoable event. |
| `height_classifier` | Assigns the ground class to the points whose height above the lowest point of their cell does not exceed the maximum height, and another class to the other points, over a square XY grid in a background task. Records the changes as one undoable event. |
| `point_data` | The user clicks points of the clouds and the command writes to the Output panel the coordinates and attributes of each point. Esc ends the command. |
| `select_object` | The user clicks a point of an object and the command selects the points of the object: the points of the same class joined to the clicked point through cubic cells that touch (26 neighbours). The user then deletes or classifies the selection from the Actions tab. Esc ends the command. |
| `points_panel` | A ribbon button shows a panel with a radius. One panel button counts the points of the visible clouds around each clicked point and writes the result and a history row in the panel. Another panel button opens a dialog with the radius and copies the accepted radius to the panel. |

Command names, descriptions and messages are available in Spanish and in English; the extensions choose the language that
lop reports.

## Build the C++ examples

Requirements: Visual Studio 2026 with the C++ desktop workload (MSVC) and CMake 3.20 or later, for x64.

```powershell
cmake -S cpp -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

The output is `build\Release\lop_examples.dll`. The `build/` folder is ignored by git.

## Install the examples in lop

- Copy `lop_examples.dll` to `%AppData%\lop\algorithms`. lop also loads libraries from the `algorithms` folder next to the
  lop executable.
- Copy the `.py` files and the `.png` file with the same name to `%AppData%\lop\commands`.

## Regenerate an embedded icon

The `cpp/*_icon.h` headers embed the PNG files of `cpp/icons`. To regenerate one:

```powershell
tools\png_to_header.ps1 cpp\icons\information.png cpp\information_icon.h information_icon
```

## License

MIT License. See `LICENSE`.
