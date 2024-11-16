#include <Engine/Rendering/Shader.h>
#include <Engine/Diagnostics/Memory.h>

Shader* Shader::New() {
    Shader* texture = (Shader*)Memory::TrackedCalloc("Shader::Shader", 1, sizeof(Shader));
    return texture;
}
