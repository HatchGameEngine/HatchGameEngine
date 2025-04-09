#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H

#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Material.h>
#include <Engine/ResourceTypes/IModel.h>

struct aiMesh;
struct aiMaterial;
struct aiNode;
struct aiScene;
struct aiAnimation;

namespace ModelImporter {
//private:
	Mesh* LoadMesh(IModel* imodel, struct aiMesh* amesh);
	Material* LoadMaterial(IModel* imodel, struct aiMaterial* mat, unsigned i);
	ModelNode* LoadNode(IModel* imodel, ModelNode* parent, const struct aiNode* anode);
	Skeleton* LoadBones(IModel* imodel, Mesh* mesh, struct aiMesh* amesh);
	SkeletalAnim* LoadAnimation(IModel* imodel, ModelAnim* parentAnim, struct aiAnimation* aanim);
	bool DoConversion(const struct aiScene* scene, IModel* imodel);

//public:
	extern vector<int> MeshIDs;
	extern char* ParentDirectory;

	bool Convert(IModel* model, Stream* stream, const char* path);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H */
