@page command_line_options Command Line Options

**Command Line Options** are arguments specified when running Hatch. They alter the behavior of the application in some way, such as loading a specific scene, or loading a resource file from a given path. Examples of ways command line options can be given to the application are running Hatch from a terminal, launching it from a shell script, or specified in a Windows shortcut.

All command line options are prefixed by `--` (two hyphens). Additionally, a path to a .hatch file or to a file inside the `Resources/` directory can be specified directly after the path to the executable, which respectively will load a resource file or a scene file.

## All command line arguments

| Argument          | Description |
| ----------------- | ----------- |
| `--resource-file <path>` | Specifies the resource file to load. This may be a .hatch file, or a directory containing the resources. |
| `--scripts-dir <path>` | Specifies the path to the directory containing scripts. |
| `--scene <resource-path>` | Specifies a scene file to load. This must be the name of a resource, not a path in the filesystem. |
