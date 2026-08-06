@page introduction_to_hatch Introduction

Hatch is a multiplatform game engine that is primarily focused on powering two-dimensional video games, such as those found on the Sega Mega Drive, the PlayStation, or the Game Boy Advance. For this reason, it natively supports features found in video game consoles similar to the ones previously mentioned, such as tilesets, parallax effects, and palettes. Rudimentary support for 3D graphics is provided, powered by OpenGL rendering.

While the engine is programmed in C++, a typical Hatch game is written using a custom scripting language created specifically for it: the **Hatch Scripting Language**, or HSL. It is object-oriented, with a syntax inspired by C++ and JavaScript. For a developer that already knows C++ or similar, picking up HSL will be faster than learning from scratch.

## What is Hatch capable of?

Originally, Hatch was designed as an engine for 2D retro-style projects, especially platformers, but it's constantly evolving. It offers a code-based workflow, and there is no dedicated official game editor or IDE, similar to frameworks like LÖVE. That said, Hatch requires very little to set up a project, and this kind of approach allows developers to adapt their existing game development environment (like code editors, asset creation tools, etc.)

As any game engine or framework does, Hatch provides to the developer essential core components necessary to build a game:

- Graphics: The rendering API includes methods to draw animated sprites, images, tiles, and shapes such as rectangles, triangles, lines, and ellipses. 3D models are also supported, with basic skinning support.
- Audio playback: The audio system allows playing and manipulating the volume, speed, and panning of sounds and music.
- Input handling: Keyboard, mouse, controllers, and touch screens are supported.
- Scripting: HSL is specific to Hatch and is how any game or application is programmed. It provides its own standard library with useful methods for both game development and general programming. However, since Hatch is free and open source software, it's possible to write code in C++, the programming language the engine is written in.
- Asset pipeline: Hatch supports popular file formats such as PNG and GIF for images, Ogg Vorbis and WAV files for audio, and Tiled map files for scene files. Animated sprites and tile collision data are loaded from Hatch-specific formats. Of course, tools are provided to convert game assets to said formats.

<!-- ### Games made in Hatch

Examples of games made with Hatch are Thomas the Transcendental Mole (2020), Scrunt: Invasion of the Glorps (2023), Marsupial Golf (2024), Miss Hatch: Coping with Brooding (2025), and Final Soldier (2026).

(add images)

-->

## Core engine concepts

This is a quick rundown of the main concepts you will often be using when developing a Hatch game.

### Scene

Everything in Hatch happens inside of a **scene**. A scene may be loaded from a **scene file**, which contains [entities](@ref Entity). The typical game made in Hatch has multiple scene files; in a very simple game, you might contain all of its gameplay in a single scene file, but for larger games, scenes may be used to contain levels, cutscenes or interfaces. It will depend on the type of game you are trying to build.

### Entity

A scene by itself does not do anything. An [entity](@ref Entity) however can be your player, an enemy, a particle effect, the HUD, an interface, you name it. The scene is not concerned about what an entity is exactly. It only runs its code.

### Resource

Most games do need to load some kind of asset, be it a sprite or a sound effect. In Hatch, any kind of asset managed by the engine is called a **resource**. A resource has two kinds of **scope**: a **game scope**, and a **scene scope**. Resources loaded with a game scope stay loaded for the entire execution of the game, and those loaded with a **scene scope** are unloaded whenever the current scene changes.

### Script

For an entity to do something useful, they require to be programmed. **Scripts** are written in HSL, Hatch's scripting language. Any entity in Hatch is spawned from a class, and spawning an entity is merely creating an instance of a class. There is no need to "import" a specific class or file when spawning an entity; Hatch knows what scripts it should load, and this happens transparently. Not all code happens inside an entity, however, and you are free to organize your scripts as you see fit.
