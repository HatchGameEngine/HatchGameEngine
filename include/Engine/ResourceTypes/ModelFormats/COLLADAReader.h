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
    static void ParseAsset(XMLNode* asset);
    static ColladaSource* ParseSource(vector<ColladaSource*> &sourceList, XMLNode* node);
    static void ParseMeshVertices(ColladaMesh* mesh, XMLNode* node);
    static void ParseMeshTriangles(ColladaMesh* mesh, XMLNode* node);
    static void ParseGeometry(XMLNode* geometryNode);
    static void ParseMesh(XMLNode* meshNode);
    static void ParseImage(XMLNode* parent, XMLNode* effect);
    static void ParseSurface(XMLNode* parentNode, XMLNode* surfaceNode);
    static void ParseSampler(XMLNode* parentNode, XMLNode* samplerNode);
    static void ParsePhongComponent(ColladaPhongComponent& component, XMLNode* phong);
    static void ParseEffectColor(XMLNode* contents, float* colors);
    static void ParseEffectFloat(XMLNode* contents, float &value);
    static void ParseEffectTechnique(ColladaEffect* effect, XMLNode* technique);
    static void ParseEffect(XMLNode* parent, XMLNode* effectNode);
    static void ParseMaterial(XMLNode* parentNode, XMLNode* materialNode);
    static void AssignEffectsToMaterials(void);
    static ColladaMesh* GetMeshFromGeometryURL(char* geometryURL);
    static ColladaController* GetControllerFromURL(char* controllerURL);
    static void InstanceMesh(ColladaScene* scene, ColladaNode* node);
    static void InstanceMaterial(ColladaNode* daeNode, XMLNode* node);
    static void BindMaterial(ColladaNode* daeNode, XMLNode* node);
    static void InstanceMeshMaterials(ColladaNode* node);
    static void ParseControllerNode(ColladaScene* scene, ColladaNode* parentNode, XMLNode *parent, XMLNode* child);
    static void ProcessMeshPrimitives(void);
    static void InstanceMeshFromController(ColladaScene* scene, ColladaNode* node, ColladaController* controller);
    static ColladaNode* ParseNode(ColladaScene* scene, XMLNode* node, ColladaNode* parent, int tree);
    static void ParseVisualScene(XMLNode* node);
    static void ParseControllerWeights(ColladaController* controller, XMLNode* node);
    static void ParseController(XMLNode* parent, XMLNode* skin);
    static bool DoConversion(ColladaScene *scene, IModel* imodel);

public:
    static IModel* Convert(IModel* model, Stream* stream);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_COLLADAREADER_H */
