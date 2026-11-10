#include <Engine/Diagnostics/Memory.h>
#include <Engine/Scene/SceneLayer.h>

SceneLayer::SceneLayer() {
	for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
		ViewVisible[i] = -1;
		ViewDrawGroup[i] = -1;
	}
}
bool SceneLayer::PropertyExists(char* property) {
	return Properties && Properties->Exists(property);
}
Property SceneLayer::PropertyGet(char* property) {
	if (!PropertyExists(property)) {
		return Property::MakeNull();
	}
	return Properties->Get(property);
}
bool SceneLayer::IsVisibleInView(int viewIndex) {
	if (viewIndex >= 0 && viewIndex < MAX_SCENE_VIEWS && ViewVisible[viewIndex] != -1) {
		return ViewVisible[viewIndex];
	}
	return Visible;
}
int SceneLayer::GetDrawGroupInView(int viewIndex) {
	if (viewIndex >= 0 && viewIndex < MAX_SCENE_VIEWS && ViewDrawGroup[viewIndex] != -1) {
		return ViewDrawGroup[viewIndex];
	}
	return DrawGroup;
}
SceneLayer::~SceneLayer() {
	Memory::Free(Name);

	if (Properties) {
		Properties->ForAll([](Uint32, Property property) -> void {
			Property::Delete(property);
		});
		delete Properties;
	}
}
