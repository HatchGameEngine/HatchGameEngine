#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H

#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Material.h>
#include <Engine/ResourceTypes/IModel.h>

class ModelImporter {
private:
	static Mesh* LoadMesh(IModel* imodel, struct aiMesh* amesh);
	static Material* LoadMaterial(IModel* imodel, struct aiMaterial* mat, unsigned i);
	static ModelNode* LoadNode(IModel* imodel, ModelNode* parent, const struct aiNode* anode);
	static Skeleton* LoadBones(IModel* imodel, Mesh* mesh, struct aiMesh* amesh);
	static SkeletalAnim*
	LoadAnimation(IModel* imodel, ModelAnim* parentAnim, struct aiAnimation* aanim);
	static bool DoConversion(const struct aiScene* scene, IModel* imodel);

public:
	static vector<int> MeshIDs;
	static char* ParentDirectory;

	static bool Convert(IModel* model, Stream* stream, const char* path);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H */
