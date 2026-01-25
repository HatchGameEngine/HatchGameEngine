@page gameconfig Game Configuration File

The **Game Configuration File** is the first Resource read by Hatch after the game's resource files are loaded. It's used to configure engine-specific options, as well as define metadata for your game. For instance, you can set the game's name, its resolution, framerate, whether palettes are enabled, which scene file to load first, among others.

Hatch reads a file named `GameConfig.xml` from the root of the VFS, but it's not required to be present for the game to be able to start. All nodes must be under a root `gameconfig` node. The following is a list of all options read by Hatch:

## Main options

Configures basic information about the game.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `gameTitle` | string | The full game title. `name` is also accepted. | `"Hatch Game Engine"` |
| `shortTitle` | string | The shorter version of the game's title. | `"Hatch Game Engine"` |
| `version` | string | The game's version. | `"1.0"` |
| `description` | string | The game's description. | `"Cluck cluck I'm a chicken"` |
| `developer` | string | The game's developer. | |
| `gameIdentifier` | string | The game's identifier. | `"hatch"` |
| `developerIdentifier` | string | The game's developer identifier. | |
| `useDeveloperIdentifierInPaths` | boolean | Whether to use `developerIdentifier` in paths. | |
| `savesDir` | string | The name of the saves directory. | `"saves"` |
| `preferencesDir` | string | The name of the preferences directory. | |
| `startscene` | string | Which scene file to load first. | |
| `activeCategory` | string | The currently active scene list category. | |

The `version` option also accepts an alternative method of defining the game version:

```xml
<version>
    <major>1</major>
    <minor>4</minor>
    <patch>0</patch>
    <prerelease>indev</prerelease>
</version>
```

This generates the following version string: `1.4.0-indev`

## `engine` options

Configures general engine options.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `framerate` | integer | The rate of all fixed timestep updates. | `60` |
| `useFixedTimestep` | boolean | Whether to use a fixed timestep, instead of a variable timestep. | `true` |
| `enablePaletteUsage` | boolean | Whether to enable palette usage by default. | `false` |
| `loadAllClasses` | boolean | Whether to load all classes when loading the game. | `false` |
| `useSoftwareRenderer` | boolean | Whether to enable the software renderer by default. | `false` |
| `disableDefaultActions` | boolean | Whether to not add the default set of input actions. | `false` |
| `allowCmdLineSceneLoad` | boolean | Whether the game allows loading scenes from command line parameters. | `false` |
| `settingsFile` | string | The path to the settings file. | `"config://config.ini"` |
| `writeLogFile` | boolean | Whether to write to the logfile. | `true` |
| `logFilename` | string | The name of the logfile. | `"HatchGameEngine.log"` |

## `display` options

Configures display options.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `width` | integer | The width of the screen. | `424` |
| `height` | integer | The height of the screen. | `240` |

## `audio` options

Configures audio options.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `music` | integer | The music volume. | `100` |
| `sound` | integer | The sound volume. | `100` |

To set the default master/music/sound volume, you must use the `volume` attribute. Example:

```xml
<audio volume="50">
    <music volume="100"></music>
    <sound volume="50"></sound>
</audio>
```

## `keys` options

Configures the default keybinds.

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `fullscreen` | string | Fullscreen toggle. | `"F4"` |
| `toggleFPSCounter` | string | FPS counter toggle. | `"F2"` |

The following keybinds are developer mode keys:

| Name             | Type             | Description        | Default         |
| ---------------- | ---------------- | ------------------ | --------------- |
| `devMenuToggle` | string | Opens and closes the developer menu. | `"ESCAPE"` |
| `devRestartApp` | string | Restarts the application. | `"F1"` |
| `devRestartScene` | string | Restarts the current scene. | `"F6"` |
| `devRecompile` | string | Recompiles scripts that have changed, and restarts the current scene. | `"F5"` |
| `devPerfSnapshot` | string | Logs the performance measurements taken during the frame. | `"F3"` |
| `devLogLayerInfo` | string | Logs information about the layers in the current scene. | `"F7"` |
| `devFastForward` | string | Toggles fast forward. | `"BACKSPACE"` |
| `devToggleFrameStepper` | string | Toggles the frame stepper. | `"F9"` |
| `devStepFrame` | string | Steps through frames. | `"F10"` |
| `devQuit` | string | Closes the application. | |
| `devShowTileCol` | string | Toggles the tile collision viewer. | |
| `devShowObjectRegions` | string | Toggles the entity update and render regions viewer. | |
| `devViewHitboxes` | string | Toggles the entity collisions viewer. | |

## `controls` options

See the Input Actions section of the Input page for more details.

# Example `GameConfig.xml`

```xml
<gameconfig>
    <gameTitle>Miss Hatch: Coping with Brooding</gameTitle>
    <shortTitle>Miss Hatch</shortTitle>
    <developer>EggSoft</developer>
    <description>Help Miss Hatch take care of her nest</description>
    <version>
        <major>1</major>
        <minor>0</minor>
    </version>

    <engine>
        <enablePaletteUsage>true</enablePaletteUsage>
        <disableDefaultActions>true</disableDefaultActions>
    </engine>

    <startscene>scenes/eggsoft-screen.tmx</startscene>

    <display>
        <width>480</width>
        <height>270</height>
    </display>

    <audio volume="50">
        <music volume="100"></music>
        <sound volume="50"></sound>
    </audio>

    <keys>
        <fullscreen>F4</fullscreen>
    </keys>

    <controls>
        <action>up</action>
        <action>down</action>
        <action>left</action>
        <action>right</action>
        <action>start</action>
        <action>select</action>

        <player id="1">
            <default action="up">
                <key>UP</key>
                <button>DPAD_UP</button>
                <axis digital_threshold="0.5">-LEFTY</axis>
            </default>
            <default action="down">
                <key>DOWN</key>
                <button>DPAD_DOWN</button>
                <axis digital_threshold="0.5">+LEFTY</axis>
            </default>
            <default action="left">
                <key>LEFT</key>
                <button>DPAD_LEFT</button>
                <axis digital_threshold="0.5">-LEFTX</axis>
            </default>
            <default action="right">
                <key>RIGHT</key>
                <button>DPAD_RIGHT</button>
                <axis digital_threshold="0.5">+LEFTX</axis>
            </default>
            <default action="start">
                <key>RETURN</key>
                <button>START</button>
            </default>
            <default action="select">
                <key>RSHIFT</key>
                <button>BACK</button>
            </default>
        </player>

        <player id="2">
            <default copy="button" id="1"></default>
            <default copy="axis" id="1"></default>
        </player>

        <player id="3">
            <default copy="button" id="1"></default>
            <default copy="axis" id="1"></default>
        </player>

        <player id="4">
            <default copy="button" id="1"></default>
            <default copy="axis" id="1"></default>
        </player>
    </controls>
</gameconfig>
```
