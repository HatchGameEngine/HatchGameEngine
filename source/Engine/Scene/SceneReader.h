#ifndef ENGINE_RESOURCETYPES_SCENE_SCENEREADER_H
#define ENGINE_RESOURCETYPES_SCENE_SCENEREADER_H

class SceneReader {
public:
	virtual bool Read(Stream* stream);
	virtual bool Read(const char* sourceF);
	virtual bool LoadIntoScene(const char* parentFolder);
	virtual void Dispose();

	~SceneReader();
};

#endif /* ENGINE_RESOURCETYPES_SCENE_SCENEREADER_H */
