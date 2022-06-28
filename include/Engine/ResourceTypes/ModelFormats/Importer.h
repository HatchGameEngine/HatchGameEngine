#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Material.h>
#include <Engine/ResourceTypes/IModel.h>

class ModelImporter {
private:
    static Mesh* LoadMesh(IModel* imodel, struct aiMesh* amesh);
    static Material* LoadMaterial(IModel* imodel, struct aiMaterial* mat);
    static ModelNode* LoadNode(IModel* imodel, ModelNode* parent, const struct aiNode* anode);
    static void LoadBones(IModel* imodel, Mesh* mesh, struct aiMesh* amesh);
    static ModelAnim* LoadAnimation(IModel* imodel, struct aiAnimation* aanim);
    static bool DoConversion(const struct aiScene* scene, IModel* imodel);

public:
    static vector<int> MeshIDs;
    static char*       ParentDirectory;

    static bool Convert(IModel* model, Stream* stream, const char* path);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_IMPORTER_H */
