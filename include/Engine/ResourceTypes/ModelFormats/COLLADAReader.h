#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_COLLADAREADER_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_COLLADAREADER_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/ResourceTypes/ModelFormats/COLLADA.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Material.h>

class COLLADAReader {
private:
    static void ParseAsset(ColladaModel* model, XMLNode* asset);
    static void ParseMeshSource(ColladaMesh* daemesh, XMLNode* node);
    static void ParseMeshVertices(ColladaMesh* daemesh, XMLNode* node);
    static void ParseMeshTriangles(ColladaMesh* daemesh, XMLNode* node);
    static void ParseGeometry(ColladaModel* model, XMLNode* geometry);
    static void ParseMesh(ColladaModel* model, XMLNode* mesh);
    static void ParseImage(ColladaModel* model, XMLNode* parent, XMLNode* effect);
    static void ParseSurface(ColladaModel* model, XMLNode* parent, XMLNode* surface);
    static void ParseSampler(ColladaModel* model, XMLNode* parent, XMLNode* sampler);
    static void ParsePhongComponent(ColladaModel* model, ColladaPhongComponent& component, XMLNode* phong);
    static void ParseEffectColor(XMLNode* contents, float* colors);
    static void ParseEffectFloat(XMLNode* contents, float &floatval);
    static void ParseEffectTechnique(ColladaModel* model, ColladaEffect* effect, XMLNode* technique);
    static void ParseEffect(ColladaModel* model, XMLNode* parent, XMLNode* effect);
    static void ParseMaterial(ColladaModel* model, XMLNode* parent, XMLNode* material);
    static void AssignEffectsToMaterials(ColladaModel* model);
    static void ParseNode(ColladaModel* model, XMLNode* node);
    static void ParseVisualScene(ColladaModel* model, XMLNode* scene);
    static void ParseLibrary(ColladaModel* model, XMLNode* library);
    static void DoConversion(ColladaModel* daemodel, IModel* imodel);

public:
    static IModel* Convert(IModel* model, Stream* stream);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_COLLADAREADER_H */
