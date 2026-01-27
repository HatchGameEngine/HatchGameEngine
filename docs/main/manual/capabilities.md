@page capabilities Capabilities

A **capability** indicates the presence or limits of a specific feature of the hardware that the game is running on, hence the name. They were introduced in version 1.4. You can query a given capability by using @ref Device.GetCapability.

The following is a categorized list of all capabilities:

## Graphics

| Name             | Type             | Description        |
| ---------------- | ---------------- | ------------------ |
| `graphics_shaders` | boolean | Whether shaders are supported by the current renderer. See @ref rendering. |
| `gpu_maxTextureWidth` | integer | The maximum texture width supported by the GPU. |
| `gpu_maxTextureHeight` | integer | The maximum texture height supported by the GPU. |
| `gpu_maxTextureUnits` | integer | The maximum amount of texture units supported by the GPU. |
