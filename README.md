# Lot Of Points SDK examples

This repository contains the Lot Of Points software development kit (SDK) headers and example extensions for Lot Of Points
(lop). The examples are written in C++ and in Python. Each Python script has the same behaviour as the C++ command of the
same name, so the two implementations can be compared.

## SDK version

`include/lop_sdk.h` defines `LOP_SDK_API_VERSION` as SDK version 0.8. A lop release that accepts SDK 0.8 loads extensions
compiled with SDK 0.8, 0.7 and 0.6; lop 1.0.31 loads extensions compiled with SDK 0.7 and 0.6, so it does not load
`lop_examples.dll` built from these headers. The `record_camera_path` and `play_camera_path` commands use the camera
functions of SDK 0.8 (`set_view_frame_notices`, `set_camera_pose`, `animate_camera` and `show_banner`).

## Repository layout

| Path | Content |
|---|---|
| `include/lop_sdk.h` | C interface of the SDK. |
| `include/lop/sdk.hpp` | C++ wrapper over `lop_sdk.h`. |
| `cpp/` | C++20 example extension, built as one dynamic-link library (`lop_examples.dll`) with CMake. |
| `cpp/third_party/` | nlohmann/json 3.11.3 (MIT), used by the camera path commands. |
| `cpp/tests/` | `camera_path_tests`, the tests of the camera path model, registered with CTest. |
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
| `record_camera_path` | Samples the camera pose (position, viewing direction, up vector and vertical field of view) while the user navigates with the mouse, the keyboard or any other command. Regular samples are separated by at least 1/60 s; consecutive identical poses retain only their first and last sample. The final sample may be closer than 1/60 s to the preceding sample. The banner shows the duration and the sample count and has a Stop recording button; Esc, the ribbon button or closing the document also end the recording. After Stop recording, a save file picker asks for the `.lopcamerapath` file; in the other cases, or if the user cancels the picker, the file goes to the data folder of the extension (`camera-paths\camera-path-YYYYMMDD-HHMMSS.lopcamerapath`). The Output panel receives the duration, the sample count and the file path. |
| `play_camera_path` | Plays the last path recorded in the session, or asks for a `.lopcamerapath` file if there is none. The camera travels to the first pose with `animate_camera` and then the command assigns the pose of the path in each frame with `set_camera_pose`. The banner has Pause (Resume), Slower, Faster, Loop, Stop and Open… buttons; Space pauses or resumes playback, Left and Right change the speed (0.25 to 8 times) and Esc stops. The playback also ends at the end of the path (with Loop off), when the user presses a mouse button, turns the wheel or presses W, A, S or D, and when another component moves the camera. |
| `points_panel` | A ribbon button shows a panel with a radius. One panel button counts the points of the visible clouds around each clicked point and writes the result and a history row in the panel. Another panel button opens a dialog with the radius and copies the accepted radius to the panel. |

Command names, descriptions and messages are available in Spanish and in English; the extensions choose the language that
lop reports. The camera path commands, and the name of the ribbon section of every command, are also available in Catalan,
German, Basque, French, Galician, Italian and Portuguese; no native speaker has reviewed those seven translations.

## Camera path file format

`record_camera_path` writes and `play_camera_path` reads a JSON file with the extension `.lopcamerapath`:

```json
{
  "Version": 1,
  "Samples": [
    { "TimeSeconds": 0.0, "Position": [443210.12, 4712345.12, 1234.57],
      "LookDirection": [-0.1234, 0.9877, -0.0988], "Up": [0.0123, -0.0988, 0.9951], "VerticalFieldOfViewDegrees": 45.0 }
  ]
}
```

- `Position`, `LookDirection` and `Up` are in the world coordinates of the clouds; `VerticalFieldOfViewDegrees` is the vertical
  field of view of the viewer, in degrees.
- `Version` is 1. The file has at least one sample; the first `TimeSeconds` is 0 and the times strictly increase; every
  number is finite; `LookDirection` and `Up` each have a length greater than 1e-9 and the cross product of their normalized vectors has a length of at least 1e-9; the field of view is greater than 0 and
  less than 180.
- Playback interpolates the position and the field of view linearly and the orientation by spherical interpolation (slerp)
  between the two samples around each instant.

## Build the C++ examples

Requirements: Visual Studio 2026 with the C++ desktop workload (MSVC) and CMake 3.20 or later, for x64.

```powershell
cmake -S cpp -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

The output is `build\Release\lop_examples.dll`. The `build/` folder is ignored by git.

The same build creates `camera_path_tests.exe`, which tests the camera path model without lop (interpolation, recording,
playback and the file format). Run it with CTest:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

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

`cpp/third_party/nlohmann/json.hpp` is the single-header release 3.11.3 of [JSON for Modern C++](https://github.com/nlohmann/json)
by Niels Lohmann, also under the MIT License; its notice is in `cpp/third_party/nlohmann/LICENSE.MIT`.
