#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/IO/Stream.h>
#include <Engine/TextFormats/XML/XMLNode.h>
#include <Engine/Types/Property.h>
#include <Engine/Types/Tileset.h>

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
	static bool ParseLayer(XMLNode* group);
	static bool ParseObjectGroup(XMLNode* group);
	static bool ParseGroupable(XMLNode* group);
	static bool ParseGroup(XMLNode* group);

public:
	static void Read(const char* sourceF, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H */
