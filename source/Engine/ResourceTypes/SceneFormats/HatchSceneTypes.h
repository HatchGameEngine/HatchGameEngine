#ifndef HATCH_SCENE_TYPES_H
#define HATCH_SCENE_TYPES_H

struct SceneHash {
	Uint32 A, B, C, D;

	bool operator==(const SceneHash& b) { return A == b.A && B == b.B && C == b.C && D == b.D; }
	bool operator!=(const SceneHash& b) { return A != b.A || B != b.B || C != b.C || D != b.D; }
};

struct SceneClassProperty {
	char* Name;
	SceneHash Hash;
	Uint8 Type;
};

enum SceneClassVariable {
	HSCN_VAR_UINT8,
	HSCN_VAR_UINT16,
	HSCN_VAR_UINT32,
	HSCN_VAR_INT8,
	HSCN_VAR_INT16,
	HSCN_VAR_INT32,
	HSCN_VAR_ENUM,
	HSCN_VAR_BOOL,
	HSCN_VAR_STRING,
	HSCN_VAR_VECTOR2,
	HSCN_VAR_UNKNOWN,
	HSCN_VAR_COLOR
};

struct SceneClass {
	char* Name;
	SceneHash Hash;
	vector<SceneClassProperty> Properties;
};

#endif // HATCH_SCENE_TYPES_H
