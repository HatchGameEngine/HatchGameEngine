#include <Engine/Diagnostics/Memory.h>
#include <Engine/Rendering/Shader.h>

Shader* Shader::New() {
	Shader* texture = (Shader*)Memory::TrackedCalloc("Shader::Shader", 1, sizeof(Shader));
	return texture;
}
