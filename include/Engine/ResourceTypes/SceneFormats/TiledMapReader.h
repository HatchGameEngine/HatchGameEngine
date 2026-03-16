#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/IO/Stream.h>
#include <Engine/Scene/LayerGroup.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/Types/Property.h>
#include <Engine/Types/Tileset.h>

struct TiledLayer {
	bool Visible;
	float Opacity = 1.0;
	float OffsetX = 0.0;
	float OffsetY = 0.0;
	float ParallaxX = 1.0;
	float ParallaxY = 1.0;
	float ConstantScrollX = 0.0;
	float ConstantScrollY = 0.0;
	HashMap<Property>* Properties = nullptr;
};

class TiledMapReader {
private:
	static Property ParseProperty(XMLNode* property);
	static void ParsePropertyNode(XMLNode* node, HashMap<Property>* properties);
	static PropertyArray ParsePolyPoints(XMLNode* node);
	static Tileset* ParseTilesetImage(XMLNode* node, int firstgid, const char* parentFolder);
	static void
	ParseTileAnimation(int tileID, int firstgid, Tileset* tilesetPtr, XMLNode* node);
	static void ParseTile(Tileset* tilesetPtr, XMLNode* node);
	static void LoadTileset(XMLNode* tileset, const char* parentFolder);
	static void ParseSharedLayerFields(TiledLayer* layer, XMLNode* node);
	static bool ParseLayer(XMLNode* layer, LayerGroup* group);
	static bool ParseObjectGroup(XMLNode* objectgroup);
	static bool ParseGroupable(XMLNode* node, LayerGroup* group);
	static bool ParseGroup(XMLNode* node, LayerGroup* parent);

public:
	static void Read(const char* sourceF, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H */
