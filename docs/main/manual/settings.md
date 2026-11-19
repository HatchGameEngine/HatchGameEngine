@page settings Settings File

The **Settings File** is an [INI file](https://en.wikipedia.org/wiki/INI_file) loaded by the engine at startup. It contains lists of key-value pairs, which are divided by sections.

Example of a settings file:

```
[display]
vsync = true
scale = 2

[audio]
masterVolume = 100
musicVolume = 75
soundVolume = 100

[keys]
fullscreen = F4
screenshot = F11
```

Lines that start with a semicolon (`;`) are considered to be comments and are ignored:

```
[keys]
fullscreen = F4
; Disable screenshot key
; screenshot = F11
```

By default, the settings are loaded from `config://config.ini`, but this can be changed through the Game Configuration File. Otherwise, Hatch will load and save the settings using one the following paths:

**Windows**
- `C:/Users/<username>/AppData/Local/<GameDeveloper>/<GameName>/config.ini`
- `C:/Users/<username>/AppData/Local/<GameName>/config.ini`

**Linux, or other Unix systems**
- `.config/<GameDeveloper>/<GameName>/config.ini`
- `.config/<GameName>/config.ini`
- `$XDG_CONFIG_HOME/<GameDeveloper>/<GameName>/config.ini`
- `$XDG_CONFIG_HOME/<GameName>/config.ini`

# All settings

The following are lists of all settings recognized by the engine.

## Video related settings

### `display` section

Configures window related settings.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `scale` | integer | The scale of the screen. Must be between 1 and 8. | `2` |
| `fullscreen` | boolean | Enables fullscreen. | `false` |
| `borderless` | boolean | Whether the window lacks a border. | `false` |
| `vsync` | boolean | Enables VSync. | `false` |
| `frameSkip` | integer | How many frames are permitted to be skipped when the application is lagging. Must be between 0 and 15. | `15` |
| `defaultMonitor` | integer | Which monitor to show the window on. | `0` |

### `graphics` section

Configures rendering related settings.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `multisample` | integer | The multisampling (MSAA) level. If this value is 0, multisampling is disabled. | `0` |
| `precompileShaders` | integer | Whether to precompile the default shaders on startup. May reduce stuttering during gameplay. | `false` |
| `layerTileBuffering` | boolean | Whether to buffer tile layers in the GPU. May increase performance. | `true` |
| `showFramerate` | boolean | If enabled, shows the current frame rate on the top right of the screen. | `false` |
| `defaultFontOversampling` | integer | The oversampling value of the default font. Must be between 1 and 8. | None |
| `forceFontOversampling` | integer | Forces a specific oversampling value on all non-bitmap fonts. Must be between 1 and 8. | None |

## Audio related settings

### `audio` section

Configures audio related settings.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `masterVolume` | integer | The master volume. Must be between 0 and 100. | `100` |
| `musicVolume` | integer | The music volume. Must be between 0 and 100. | `100` |
| `soundVolume` | integer | The sound effects volume. Must be between 0 and 100. | `100` |

## Input related settings

See below for a [list of accepted keys](#list-of-accepted-keys).

### `keys` section

Configures default key binds.

| Name             | Description        | Default         |
| ---------------- | ------------------ | --------------- |
| `fullscreen` | Toggles fullscreen. | F4 |
| `screenshot` | Takes a screenshot. | None |
| `toggleFPSCounter` | Toggles the frame rate counter. | F2 |

Additionally, the following key binds are accepted when developer mode is enabled:

| Name             | Description        | Default         |
| ---------------- | ------------------ | --------------- |
| `devRestartApp` | Restarts the entire application. | F1 |
| `devRestartScene` | Restarts the current scene. | F6 |
| `devRecompile` | Recompiles all scripts, then restarts the current scene. | F5 |
| `devPerfSnapshot` | Logs a performance snapshot. | F3 |
| `devLogLayerInfo` | Logs information about the current scene's layers. | F7 |
| `devFastForward` | Toggles fast forward. | Backspace |
| `devToggleFrameStepper` | Toggles the frame stepper. | F9 |
| `devStepFrame` | Steps a single frame, enabling the frame stepper if it's not already enabled. | F10 |
| `devShowTileCol` | Shows tile collision. | None |
| `devShowObjectRegions` | Shows the update and render regions of entities. | None |
| `devViewHitboxes` | Toggles display of entity hitboxes. | None |
| `devPaletteViewer` | Toggles the palette viewer. | None |
| `devPreviousScene` | Goes to the previous scene in the Scene List. | Page Down |
| `devNextScene` | Goes to the next scene in the Scene List. | Page Up |
| `devMenuToggle` | Opens the system menu. | Escape |
| `devScriptDebugger` | Opens the script debugger. There must be a terminal open, otherwise this will just freeze the application. | None |
| `devQuit` | Closes the application. | None |

### List of accepted keys

<details>

<summary>Expand to see list</summary>

| Key name | Corresponds to |
| -------- | -------------- |
| A | A key |
| B | B key |
| C | C key |
| D | D key |
| E | E key |
| F | F key |
| G | G key |
| H | H key |
| I | I key |
| J | J key |
| K | K key |
| L | L key |
| M | M key |
| N | N key |
| O | O key |
| P | P key |
| Q | Q key |
| R | R key |
| S | S key |
| T | T key |
| U | U key |
| V | V key |
| W | W key |
| X | X key |
| Y | Y key |
| Z | Z key |
| 1 | 1 key |
| 2 | 2 key |
| 3 | 3 key |
| 4 | 4 key |
| 5 | 5 key |
| 6 | 6 key |
| 7 | 7 key |
| 8 | 8 key |
| 9 | 9 key |
| 0 | 0 key |
| RETURN | Return (or Enter) |
| ESCAPE | Escape |
| BACKSPACE | Backspace |
| TAB | Tab |
| SPACE | Space bar |
| MINUS | Minus (-) |
| EQUALS | Equals (=) |
| LEFTBRACKET | Left square bracket ([) |
| RIGHTBRACKET | Right square bracket (]) |
| BACKSLASH | Backslash (\\) |
| SEMICOLON | Semicolon (;) |
| APOSTROPHE | Apostrophe (') |
| GRAVE | Grave (`) |
| COMMA | Comma (,) |
| PERIOD | Period (.) |
| SLASH | Slash (/) |
| CAPSLOCK | Caps Lock |
| F1 | F1 |
| F2 | F2 |
| F3 | F3 |
| F4 | F4 |
| F5 | F5 |
| F6 | F6 |
| F7 | F7 |
| F8 | F8 |
| F9 | F9 |
| F10 | F10 |
| F11 | F11 |
| F12 | F12 |
| PRINTSCREEN | Print Screen |
| SCROLLLOCK | Scroll Lock |
| PAUSE | Pause/Break |
| INSERT | Insert |
| HOME | Home |
| PAGEUP | Page Up |
| DELETE | Delete |
| END | End |
| PAGEDOWN | Page Down |
| RIGHT | Right arrow |
| LEFT | Left arrow |
| DOWN | Down arrow |
| UP | Up arrow |
| NUMLOCKCLEAR | Num Lock |
| KP_DIVIDE | Keypad Divide (/) |
| KP_MULTIPLY | Keypad Multiply (*) |
| KP_MINUS | Keypad Minus (-) |
| KP_PLUS | Keypad Plus (+) |
| KP_ENTER | Keypad Enter |
| KP_1 | Keypad 1 |
| KP_2 | Keypad 2 |
| KP_3 | Keypad 3 |
| KP_4 | Keypad 4 |
| KP_5 | Keypad 5 |
| KP_6 | Keypad 6 |
| KP_7 | Keypad 7 |
| KP_8 | Keypad 8 |
| KP_9 | Keypad 9 |
| KP_0 | Keypad 0 |
| KP_PERIOD | Keypad Period (.) |
| LCTRL | Left Ctrl |
| LSHIFT | Left Shift |
| LALT | Left Alt |
| LGUI | Left GUI key |
| RCTRL | Right Ctrl |
| RSHIFT | Right Shift |
| RALT | Right Alt |
| RGUI | Right GUI key |

</details>

## Script compiler related settings

### `compiler` section

Configures the script compiler.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `log` | boolean | Toggles logging. | `false` |
| `optimizations` | boolean | Enables code optimizations. | `true` |
| `showWarnings` | boolean | Whether to log compile warnings. | `false` |
| `writeDebugInfo` | boolean | When enabled, the compiler writes debug information into the bytecode, like line numbers and variable names. | `false` on non-dev builds, `true` on dev builds |
| `writeSourceFilename` | boolean | When enables, preserves the path of the script file the bytecode was compiled from. | `false` on non-dev builds, `true` on dev builds |

## Development related settings

### `dev` section

Development-related settings.

### General settings

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `devMenu` | boolean | Enables developer mode. | `false` on non-dev builds, `true` on dev builds |
| `fastForward` | integer | How many frames to run when fast forwarding. | `4` |
| `logLevel` | integer | Log level. Messages lower than the specified number are not logged.<br><b>Log levels:</b><br>-2 = API<br>-1 = Verbose<br>0 = Info<br>1 = Warning<br>2 = Error<br>3 = Important<br>4 = Fatal | `0` |
| `writeLogFile` | boolean | Whether to write to the log file. | `true` |
| `trackMemory` | boolean | Track memory allocations. | `false` |

### Rendering

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `renderer` | string | Which rendering backend to use. It's not recommended to change this.<br><b>Accepted options:</b><br>`opengl` = OpenGL backend.<br>`sdl2` = SDL Renderer backend. | `opengl` |
| `noTiles` | boolean | Disables layer rendering. | `false` |
| `noObjectRender` | boolean | Disables entity rendering. | `false` |
| `viewCollision` | integer | Shows tile collision.<br><b>Accepted options:</b><br>0 = Disabled.<br>1 = View Path A.<br>2 = View Path B. | `0` |

### Scripting

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `enableScriptBreakpoints` | boolean | Whether breakpoints are enabled. | `true` |
| `branchLimit` | integer or boolean | Interrupts script execution when a call frame exceeds the specified amount of branches. If `true`, uses the default, and if `false` or `0`, disables it. | `100000` |
| `loadAllClasses` | boolean | Loads all script classes on startup. | `false` |
| `debugCompiler` | boolean | Logs a listing of the bytecode after compilation. | `false` |

### Performance measuring

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `viewPerformance` | boolean | Shows the performance viewer. | `false` |
| `autoPerfSnapshots` | boolean | Enables automatic performance snapshots. | `false` |
| `apsMinFrameTime` | integer | When the frame time in milliseconds exceeds this value, take a performance snapshot. | `20` |
| `apsMinInterval` | integer | Configures the time in milliseconds between automatic performance snapshots. | `5000` |

### VFS-related settings

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `useMemoryFileCache` | boolean | Keep the file cache in-memory. | `false` |
| `allowWritableResource` | boolean | Makes the main resource file writable.<br>Note that when the application mounts the Resources directory as the main resource file, the mount is writable by default. | `false` |

### Miscellaneous

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `sceneBrowserMode` | string | How the scene browser lists scenes in the developer menu.<br><b>Accepted options:</b><br>`default` = Use the Scene List if available, but otherwise, use the resources browser.<br>`scenelist`: Use the Scene List.<br>`resources` = Browse through resources. | `default` |
| `convertModels` | boolean | Converts and saves 3D models as .hmdl when they are loaded. | `false` |
| `doNothing` | boolean | Makes the game, uh, do nothing. | `false` |

# Scripting

The @ref Settings class in HSL can be used to access and modify the current settings, as well as load from and save to different .ini files. You may use @ref Settings to store and load settings for your own game. Example:

```java
var PlayerName;
var UIScale;
var AutoSave;

event LoadSettings() {
    PlayerName = Settings.GetString("player", "name") || "Matilda";
    UIScale = Settings.GetNumber("ui", "scale") || 1.0;
    AutoSave = Settings.GetBool("game", "autosave") || true;
}

event SaveSettings() {
    Settings.SetString("player", "name", PlayerName);
    Settings.SetNumber("ui", "scale", UIScale);
    Settings.SetBool("game", "autosave", AutoSave);

    Settings.Save();
}
```
