#ifndef ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H
#define ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H

#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/IO/Stream.h>

namespace TiledMapReader {
//private:
	VMValue ParseProperty(XMLNode* property);
	void ParsePropertyNode(XMLNode* node, HashMap<VMValue>* properties);
	ObjArray* ParsePolyPoints(XMLNode* node);
	Tileset* ParseTilesetImage(XMLNode* node, int firstgid, const char* parentFolder);
	void
	ParseTileAnimation(int tileID, int firstgid, Tileset* tilesetPtr, XMLNode* node);
	void ParseTile(Tileset* tilesetPtr, XMLNode* node);
	void LoadTileset(XMLNode* tileset, const char* parentFolder);
	bool ParseLayer(XMLNode* group);
	bool ParseObjectGroup(XMLNode* group);
	bool ParseGroupable(XMLNode* group);
	bool ParseGroup(XMLNode* group);

//public:
	void Read(const char* sourceF, const char* parentFolder);
};

#endif /* ENGINE_RESOURCETYPES_SCENEFORMATS_TILEDMAPREADER_H */
