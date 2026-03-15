#include <Engine/Diagnostics/Memory.h>
#include <Engine/Scene/SceneLayer.h>

bool SceneLayer::PropertyExists(char* property) {
	return Properties && Properties->Exists(property);
}
Property SceneLayer::PropertyGet(char* property) {
	if (!PropertyExists(property)) {
		return Property::MakeNull();
	}
	return Properties->Get(property);
}
SceneLayer::~SceneLayer() {
	if (Name) {
		Memory::Free(Name);
	}
	if (Properties) {
		Properties->ForAll([](Uint32, Property property) -> void {
			Property::Delete(property);
		});
		delete Properties;
	}
}
