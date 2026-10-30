#include <Engine/Diagnostics/Memory.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Utilities/StringUtils.h>

bool SceneLayer::PropertyExists(char* property) {
	return Properties && Properties->Exists(property);
}
Property SceneLayer::PropertyGet(char* property) {
	if (!PropertyExists(property)) {
		return Property::MakeNull();
	}
	return Properties->Get(property);
}

void SceneLayer::CopyFields(SceneLayer* dest) {
	dest->Name = StringUtils::Duplicate(Name);
	dest->Visible = Visible;
	dest->Temporary = Temporary;

	dest->RelativeX = RelativeX;
	dest->RelativeY = RelativeY;
	dest->ConstantX = ConstantX;
	dest->ConstantY = ConstantY;
	dest->OffsetX = OffsetX;
	dest->OffsetY = OffsetY;

	dest->Flags = Flags;
	dest->DrawGroup = DrawGroup;
	dest->DrawBehavior = DrawBehavior;

	if (Properties) {
		dest->Properties = new HashMap<Property>(NULL, 4);

		Properties->WithAll([dest](Uint32 hash, Property property) -> void {
			dest->Properties->Put(hash, Property::Duplicate(property));
		});
	}

	dest->UseBlending = UseBlending;
	dest->BlendMode = BlendMode;
	dest->Opacity = Opacity;

	dest->CurrentShader = CurrentShader;
	dest->UsePaletteIndexLines = UsePaletteIndexLines;

	dest->UsingCustomRenderFunction = UsingCustomRenderFunction;
	dest->CustomRenderFunction = CustomRenderFunction;
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
