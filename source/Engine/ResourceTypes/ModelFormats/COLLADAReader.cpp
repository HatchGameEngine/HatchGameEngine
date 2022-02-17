#if INTERFACE
#include <Engine/ResourceTypes/ModelFormats/COLLADA.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/IO/Stream.h>
class COLLADAReader {
public:

};
#endif

/*

Implemented by Lactozilla
https://github.com/Lactozilla

*/

#include <Engine/ResourceTypes/ModelFormats/COLLADAReader.h>

static void TokenToString(Token tok, char** string)
{
    *string = XMLParser::TokenToString(tok);
}

// Caveat: This modifies the source string.
static void ParseFloatArray(vector<float> &dest, char* source) {
    size_t i = 0, size = strlen(source);
    char* start = source;

    while (i <= size)
    {
        if (*source == '\0' || *source == ' ' || *source == '\n')
        {
            *source = '\0';

            if ((source - start) > 0)
            {
                float fnum = atof(start);
                dest.push_back(fnum);
            }

            start = (source + 1);
        }

        source++;
        i++;
    }
}

// So does this function.
static void ParseIntegerArray(vector<int> &dest, char* source) {
    size_t i = 0, size = strlen(source);
    char* start = source;

    while (i <= size)
    {
        if (*source == '\0' || *source == ' ' || *source == '\n')
        {
            *source = '\0';

            if ((source - start) > 0)
            {
                int num = atoi(start);
                dest.push_back(num);
            }

            start = (source + 1);
        }

        source++;
        i++;
    }
}

static void FloatStringToArray(XMLNode* contents, float* array, int size, float defaultval)
{
    int i;
    vector<float> vec;

    vec.resize(size);
    for (i = 0; i < size; i++)
        vec[i] = defaultval;

    if (contents->name.Length) {
        char* list = (char* )calloc(1, contents->name.Length + 1);
        memcpy(list, contents->name.Start, contents->name.Length);
        ParseFloatArray(vec, list);
        free(list);
    }

    for (i = 0; i < size; i++)
        array[i] = vec[i];
}

//
// ASSET PARSING
//

PRIVATE STATIC void COLLADAReader::ParseAsset(ColladaModel* model, XMLNode* asset) {
    for (size_t i = 0; i < asset->children.size(); i++) {
        XMLNode* node = asset->children[i];
        if (XMLParser::MatchToken(node->name, "up_axis")) {
            XMLNode* axis = node->children[0];

            if (XMLParser::MatchToken(axis->name, "X_UP"))
                model->Axis = DAE_X_UP;
            else if (XMLParser::MatchToken(axis->name, "Y_UP"))
                model->Axis = DAE_Y_UP;
            else if (XMLParser::MatchToken(axis->name, "Z_UP"))
                model->Axis = DAE_Z_UP;
        }
    }
}

//
// LIBRARY PARSING
//

static ColladaMeshSource* FindMeshSource(vector<ColladaMeshSource*> list, char* name)
{
    for (int i = 0; i < list.size(); i++) {
        if (!strcmp(list[i]->Id, name))
            return list[i];
    }

    return nullptr;
}

static ColladaMeshFloatArray* FindAccessorSource(vector<ColladaMeshFloatArray*> arrays, char* name)
{
    for (int i = 0; i < arrays.size(); i++) {
        if (!strcmp(arrays[i]->Id, name))
            return arrays[i];
    }

    return nullptr;
}

PRIVATE STATIC void COLLADAReader::ParseMeshSource(ColladaMesh* daemesh, XMLNode* node) {
    ColladaMeshSource* source = new ColladaMeshSource;

    TokenToString(node->attributes.Get("id"), &source->Id);

    // Find arrays first
    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "float_array")) {
            ColladaMeshFloatArray* floatArray = new ColladaMeshFloatArray;

            TokenToString(child->attributes.Get("id"), &floatArray->Id);
            floatArray->Count = XMLParser::TokenToNumber(child->attributes.Get("count"));

            XMLNode* contents = child->children[0];
            if (contents->name.Length) {
                char* list = (char* )calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseFloatArray(floatArray->Contents, list);
                free(list);
            }

            source->FloatArrays.push_back(floatArray);
        }
    }

    // Then find accessors
    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "technique_common")) {
            XMLNode* accessorNode = child->children[0];
            ColladaMeshAccessor* accessor = new ColladaMeshAccessor;

            char* accessorSource;
            TokenToString(accessorNode->attributes.Get("source"), &accessorSource);
            accessor->Source = FindAccessorSource(source->FloatArrays, (accessorSource + 1));
            accessor->Count = XMLParser::TokenToNumber(accessorNode->attributes.Get("count"));
            accessor->Stride = XMLParser::TokenToNumber(accessorNode->attributes.Get("stride"));

            // Parameters are irrelevant
            free(accessorSource);

            source->Accessors.push_back(accessor);
        }
    }

    daemesh->SourceList.push_back(source);
}

PRIVATE STATIC void COLLADAReader::ParseMeshVertices(ColladaMesh* daemesh, XMLNode* node) {
    TokenToString(node->attributes.Get("id"), &daemesh->Vertices.Id);

    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (XMLParser::MatchToken(child->name, "input")) {
            ColladaInput* input = new ColladaInput;

            char* inputSource;
            TokenToString(child->attributes.Get("source"), &inputSource);
            input->Source = FindMeshSource(daemesh->SourceList, (inputSource + 1));
            input->Offset = XMLParser::TokenToNumber(child->attributes.Get("offset"));
            TokenToString(child->attributes.Get("semantic"), &input->Semantic);

            daemesh->Vertices.Inputs.push_back(input);
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseMeshTriangles(ColladaMesh* daemesh, XMLNode* node) {
    TokenToString(node->attributes.Get("id"), &daemesh->Triangles.Id);
    daemesh->Triangles.Count = XMLParser::TokenToNumber(node->attributes.Get("count"));

    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];

        if (XMLParser::MatchToken(child->name, "input")) {
            ColladaInput* input = new ColladaInput;

            TokenToString(child->attributes.Get("semantic"), &input->Semantic);

            if (!input->Semantic)
            {
                delete input;
                continue;
            }

            char* inputSource;
            TokenToString(child->attributes.Get("source"), &inputSource);

            if (!strcmp(input->Semantic, "VERTEX"))
            {
                if (!strcmp(daemesh->Vertices.Id, (inputSource + 1)))
                {
                    if (daemesh->Vertices.Inputs.size() > 1)
                    {
                        for (int j = 0; j < daemesh->Vertices.Inputs.size(); j++)
                            input->Children.push_back(daemesh->Vertices.Inputs[j]);
                        input->Source = nullptr;
                    }
                    else
                    {
                        daemesh->Triangles.Inputs.push_back(daemesh->Vertices.Inputs[0]);
                        delete input;
                        continue;
                    }
                }
            }
            else
            {
                input->Source = FindMeshSource(daemesh->SourceList, (inputSource + 1));
            }

            input->Offset = XMLParser::TokenToNumber(child->attributes.Get("offset"));

            daemesh->Triangles.Inputs.push_back(input);
        } else if (XMLParser::MatchToken(child->name, "p")) {
            XMLNode* contents = child->children[0];
            if (contents->name.Length)
            {
                char* list = (char* )calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseIntegerArray(daemesh->Triangles.Primitives, list);
                free(list);
            }
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseGeometry(ColladaModel* model, XMLNode* geometry) {
    ColladaGeometry* daegeometry = new ColladaGeometry;

    TokenToString(geometry->attributes.Get("id"), &daegeometry->Id);
    TokenToString(geometry->attributes.Get("name"), &daegeometry->Name);

    for (size_t i = 0; i < geometry->children.size(); i++) {
        XMLNode* node = geometry->children[i];

        if (XMLParser::MatchToken(node->name, "mesh")) {
            ParseMesh(model, node);
            daegeometry->Mesh = model->Meshes.back();
        }
    }

    model->Geometries.push_back(daegeometry);
}

PRIVATE STATIC void COLLADAReader::ParseMesh(ColladaModel* model, XMLNode* mesh) {
    ColladaMesh* daemesh = new ColladaMesh;

    daemesh->Material = nullptr;

    for (size_t i = 0; i < mesh->children.size(); i++) {
        XMLNode* node = mesh->children[i];

        if (XMLParser::MatchToken(node->name, "source")) {
            ParseMeshSource(daemesh, node);
        } else if (XMLParser::MatchToken(node->name, "vertices")) {
            ParseMeshVertices(daemesh, node);
        } else if (XMLParser::MatchToken(node->name, "triangles")) {
            ParseMeshTriangles(daemesh, node);
        }
    }

    model->Meshes.push_back(daemesh);
}

PRIVATE STATIC void COLLADAReader::ParseImage(ColladaModel* model, XMLNode* parent, XMLNode* effect) {
    ColladaImage* daeimage = new ColladaImage;

    TokenToString(parent->attributes.Get("id"), &daeimage->Id);

    XMLNode* path = effect->children[0];
    if (path->name.Length) {
        char* pathstring = (char* )calloc(1, path->name.Length + 1);
        memcpy(pathstring, path->name.Start, path->name.Length);
        daeimage->Path = pathstring;
    }

    if (!daeimage->Path)
        return;
    else {
        char* str = daeimage->Path;
        char* filename = NULL;
        int i = (int)strlen(str);
        while (i-- >= 0)
        {
            if (str[i] == '/' || str[i] == '\\')
            {
                filename = &str[i+1];
                break;
            }
        }

        if (!filename || filename[0] == '\0')
            return;

        #define TEXPATH "Models\\Textures\\"
        free(daeimage->Path);
        daeimage->Path = (char*)calloc(sizeof(TEXPATH) + strlen(filename), 1);
        strcpy(daeimage->Path, TEXPATH);
        strcat(daeimage->Path, filename);
        #undef TEXPATH
    }

    model->Images.push_back(daeimage);
}

PRIVATE STATIC void COLLADAReader::ParseSurface(ColladaModel* model, XMLNode* parent, XMLNode* surface) {
    ColladaSurface* daesurface = new ColladaSurface;

    TokenToString(parent->attributes.Get("sid"), &daesurface->Id);
    TokenToString(surface->attributes.Get("type"), &daesurface->Type);

    daesurface->Image = nullptr;

    for (size_t i = 0; i < surface->children.size(); i++) {
        XMLNode* node = surface->children[i];
        if (XMLParser::MatchToken(node->name, "init_from")) {
            XMLNode* child = node->children[0];
            if (child->name.Length) {
                char* imagestring = (char* )calloc(1, child->name.Length + 1);
                memcpy(imagestring, child->name.Start, child->name.Length);

                for (size_t i = 0; i < model->Images.size(); i++) {
                    ColladaImage* image = model->Images[i];
                    if (!strcmp(imagestring, image->Id)) {
                        daesurface->Image = image;
                        break;
                    }
                }

                free(imagestring);
            }
        }
    }

    model->Surfaces.push_back(daesurface);
}

PRIVATE STATIC void COLLADAReader::ParseSampler(ColladaModel* model, XMLNode* parent, XMLNode* sampler) {
    ColladaSampler* daesampler = new ColladaSampler;

    TokenToString(parent->attributes.Get("sid"), &daesampler->Id);

    daesampler->Surface = nullptr;

    for (size_t i = 0; i < sampler->children.size(); i++) {
        XMLNode* node = sampler->children[i];
        // Sampler comes after surface so this is fine
        if (XMLParser::MatchToken(node->name, "source")) {
            XMLNode* child = node->children[0];

            if (!child->name.Length)
                continue;

            char* source = (char* )calloc(1, child->name.Length + 1);
            memcpy(source, child->name.Start, child->name.Length);

            for (size_t i = 0; i < model->Surfaces.size(); i++) {
                ColladaSurface* surface = model->Surfaces[i];
                if (!strcmp(source, surface->Id)) {
                    daesampler->Surface = surface;
                    break;
                }
            }

            free(source);
        }
    }

    model->Samplers.push_back(daesampler);
}

PRIVATE STATIC void COLLADAReader::ParsePhongComponent(ColladaModel* model, ColladaPhongComponent& component, XMLNode* phong) {
    for (size_t i = 0; i < phong->children.size(); i++) {
        XMLNode* node = phong->children[i];

        if (XMLParser::MatchToken(node->name, "color")) {
            ParseEffectColor(node->children[0], component.Color);
        } else if (XMLParser::MatchToken(node->name, "texture")) {
            char* sampler_name;
            TokenToString(node->attributes.Get("texture"), &sampler_name);

            component.Sampler = nullptr;

            for (size_t j = 0; j < model->Samplers.size(); j++) {
                ColladaSampler* sampler = model->Samplers[j];
                if (!strcmp(sampler->Id, sampler_name)) {
                    component.Sampler = sampler;
                    break;
                }
            }
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseEffectColor(XMLNode* contents, float* colors) {
    FloatStringToArray(contents, colors, 4, 1.0f);
}

PRIVATE STATIC void COLLADAReader::ParseEffectFloat(XMLNode* contents, float &floatval) {
    if (contents->name.Length) {
        char* floatstr = (char* )calloc(1, contents->name.Length + 1);
        memcpy(floatstr, contents->name.Start, contents->name.Length);
        floatval = atof(floatstr);
        free(floatstr);
    }
    else
        floatval = 0.0f;
}

PRIVATE STATIC void COLLADAReader::ParseEffectTechnique(ColladaModel* model, ColladaEffect* effect, XMLNode* technique) {
    for (size_t i = 0; i < technique->children.size(); i++) {
        XMLNode* node = technique->children[i];
        XMLNode* child = node->children[0];

        if (XMLParser::MatchToken(node->name, "specular")) {
            ParsePhongComponent(model, effect->Specular, node);
        } else if (XMLParser::MatchToken(node->name, "ambient")) {
            ParsePhongComponent(model, effect->Ambient, node);
        } else if (XMLParser::MatchToken(node->name, "emission")) {
            ParsePhongComponent(model, effect->Emission, node);
        } else if (XMLParser::MatchToken(node->name, "diffuse")) {
            ParsePhongComponent(model, effect->Diffuse, node);
        } else {
            XMLNode* grandchild = child->children[0];
            if (XMLParser::MatchToken(node->name, "shininess")) {
                ParseEffectFloat(grandchild, effect->Shininess);
            } else if (XMLParser::MatchToken(node->name, "transparency")) {
                ParseEffectFloat(grandchild, effect->Transparency);
            } else if (XMLParser::MatchToken(node->name, "index_of_refraction")) {
                ParseEffectFloat(grandchild, effect->IndexOfRefraction);
            }
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseEffect(ColladaModel* model, XMLNode* parent, XMLNode* effect) {
    ColladaEffect* daeeffect = new ColladaEffect;

    memset(daeeffect, 0x00, sizeof(ColladaEffect));

    TokenToString(parent->attributes.Get("id"), &daeeffect->Id);

    for (size_t i = 0; i < effect->children.size(); i++) {
        XMLNode* node = effect->children[i];
        XMLNode* child = node->children[0];

        if (XMLParser::MatchToken(node->name, "newparam")) {
            if (XMLParser::MatchToken(child->name, "surface")) {
                ParseSurface(model, node, child);
            } else if (XMLParser::MatchToken(child->name, "sampler2D")) {
                ParseSampler(model, node, child);
            }
        } else if (XMLParser::MatchToken(node->name, "technique")) {
            ParseEffectTechnique(model, daeeffect, child);
        }
    }

    model->Effects.push_back(daeeffect);
}

PRIVATE STATIC void COLLADAReader::ParseMaterial(ColladaModel* model, XMLNode* parent, XMLNode* material) {
    ColladaMaterial* daematerial = new ColladaMaterial;

    daematerial->Effect = nullptr;

    TokenToString(parent->attributes.Get("id"), &daematerial->Id);
    TokenToString(parent->attributes.Get("name"), &daematerial->Name);

    TokenToString(material->attributes.Get("url"), &daematerial->EffectLink);

    model->Materials.push_back(daematerial);
}

PRIVATE STATIC void COLLADAReader::AssignEffectsToMaterials(ColladaModel* model) {
    for (size_t i = 0; i < model->Materials.size(); i++) {
        ColladaMaterial* material = model->Materials[i];

        for (size_t j = 0; j < model->Effects.size(); j++) {
            ColladaEffect* effect = model->Effects[j];
            if (!strcmp((material->EffectLink + 1), effect->Id)) {
                material->Effect = effect;
                break;
            }
        }
    }
}

static void InstanceMesh(ColladaModel* model, ColladaNode* daenode, char* geometry_url)
{
    for (size_t i = 0; i < model->Geometries.size(); i++) {
        ColladaGeometry* geometry = model->Geometries[i];
        if (!strcmp(geometry_url, geometry->Name)) {
            daenode->Mesh = geometry->Mesh;
            break;
        }
    }
}

static void InstanceMaterial(ColladaModel* model, ColladaNode* daenode, XMLNode* node)
{
    char *target;
    TokenToString(node->attributes.Get("target"), &target);

    for (size_t i = 0; i < model->Materials.size(); i++) {
        ColladaMaterial* material = model->Materials[i];
        if (!strcmp((target + 1), material->Id)) {
            daenode->Material = material;
            break;
        }
    }

    free(target);
}

static void InstanceFromXML(ColladaModel* model, ColladaNode* daenode, XMLNode* node) {
    if (XMLParser::MatchToken(node->name, "instance_geometry")) {
        char *geometry_url;
        TokenToString(node->attributes.Get("url"), &geometry_url);

        InstanceMesh(model, daenode, (geometry_url + 1));

        free(geometry_url);
    } else if (XMLParser::MatchToken(node->name, "instance_material")) {
        InstanceMaterial(model, daenode, node);
    }
}

PRIVATE STATIC void COLLADAReader::ParseNode(ColladaModel* model, XMLNode* node) {
    ColladaNode* daenode = new ColladaNode;

    TokenToString(node->attributes.Get("id"), &daenode->Id);
    TokenToString(node->attributes.Get("name"), &daenode->Name);

    daenode->Mesh = nullptr;

    for (size_t i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];

        if (XMLParser::MatchToken(child->name, "matrix")) {
            FloatStringToArray(child->children[0], daenode->Matrix, 16, 0.0f);
        } else if (XMLParser::MatchToken(child->name, "instance_controller")) {
            // Controllers don't have too much things on them to warrant a new function
            for (size_t j = 0; j < child->children.size(); j++) {
                XMLNode* grandchild = child->children[j];

                if (XMLParser::MatchToken(grandchild->name, "skeleton")) {
                    // unimplemented
                } else if (XMLParser::MatchToken(grandchild->name, "bind_material")) {
                    XMLNode* common = grandchild->children[0];
                    if (common && XMLParser::MatchToken(common->name, "technique_common")) {
                        for (size_t k = 0; k < common->children.size(); k++)
                            InstanceFromXML(model, daenode, common->children[k]);
                    }
                }
            }
        } else {
            InstanceFromXML(model, daenode, child);
        }
    }

    // This is actually fucked. It makes no sense.
    // Okay, so nodes are part of scenes.
    // A node can instantiate a mesh, with instance_geometry.
    // Except that some models decide NOT to do that, and
    // put the mesh to be loaded in the "name" attribute
    // of the nodes. Why?!
    if (daenode->Mesh == nullptr)
        InstanceMesh(model, daenode, daenode->Name);

    // Assign material to mesh
    if (daenode->Mesh != nullptr && (daenode->Mesh->Material == nullptr))
        daenode->Mesh->Material = daenode->Material;

    model->Nodes.push_back(daenode);
}

PRIVATE STATIC void COLLADAReader::ParseVisualScene(ColladaModel* model, XMLNode* scene) {
    ColladaScene* daescene = new ColladaScene;

    for (size_t i = 0; i < scene->children.size(); i++) {
        XMLNode* child = scene->children[i];
        if (XMLParser::MatchToken(child->name, "node")) {
            ParseNode(model, child);
        }
    }

    model->Scenes.push_back(daescene);
}

PRIVATE STATIC void COLLADAReader::ParseLibrary(ColladaModel* model, XMLNode* library) {
    for (size_t i = 0; i < library->children.size(); i++) {
        XMLNode* node = library->children[i];
        XMLNode* child = node->children[0];

        if (XMLParser::MatchToken(node->name, "geometry")) {
            ParseGeometry(model, node);
        } else if (XMLParser::MatchToken(node->name, "material")) {
            ParseMaterial(model, node, child);
        } else if (XMLParser::MatchToken(node->name, "effect")) {
            ParseEffect(model, node, child);
        } else if (XMLParser::MatchToken(node->name, "image")) {
            ParseImage(model, node, child);
        } else if (XMLParser::MatchToken(node->name, "visual_scene")) {
            ParseVisualScene(model, node); // not child, that's whatever the first collada node is
        }
    }
}

//
// CONVERSION
//

struct ConversionBuffer {
    vector<Vector3> Positions;
    vector<Vector3> Normals;
    vector<Vector2> UVs;
    vector<Uint32>  Colors;

    vector<int> posIndices;
    vector<int> texIndices;
    vector<int> nrmIndices;
    vector<int> colIndices;

    IModel* Model;
    int TotalVertexCount;
};

void ProcessPrimitive(ConversionBuffer* convBuf, int meshIndex, ColladaInput* input, int prim) {
    ColladaMeshAccessor* accessor = input->Source->Accessors[0];
    vector<float> values = accessor->Source->Contents;

    int stride = accessor->Stride;
    int idx = (prim * stride);
    int vecsize = values.size();

    if (!strcmp(input->Semantic, "POSITION")) {
        // Resize vertex list to fit
        if (prim >= convBuf->Positions.size())
            convBuf->Positions.resize(prim + 1);

        // Insert vertex
        convBuf->Positions[prim].X = (int)(values[idx + 0] * 0x100);
        convBuf->Positions[prim].Y = (int)(values[idx + 1] * 0x100);
        convBuf->Positions[prim].Z = (int)(values[idx + 2] * 0x100);

        // Store tri's vertex index
        convBuf->posIndices.push_back(prim);

        convBuf->Model->VertexFlag |= VertexType_Position; // NOTE: Effectively does nothing, but for consistency this line exists.
    }
    else if (!strcmp(input->Semantic, "NORMAL")) {
        // Resize vertex list to fit
        if (prim >= convBuf->Normals.size())
            convBuf->Normals.resize(prim + 1);

        // Insert normal
        convBuf->Normals[prim].X = (int)(values[idx + 0] * 0x10000);
        convBuf->Normals[prim].Y = (int)(values[idx + 1] * 0x10000);
        convBuf->Normals[prim].Z = (int)(values[idx + 2] * 0x10000);

        // Store tri's normal index
        convBuf->nrmIndices.push_back(prim);

        convBuf->Model->VertexFlag |= VertexType_Normal;
    }
    else if (!strcmp(input->Semantic, "TEXCOORD")) {
        // Resize vertex list to fit
        if (prim >= convBuf->UVs.size())
            convBuf->UVs.resize(prim + 1);

        // Insert tex
        convBuf->UVs[prim].X = (int)(values[idx + 0] * 0x10000);
        convBuf->UVs[prim].Y = (int)(values[idx + 1] * 0x10000);

        // Store tri's tex index
        convBuf->texIndices.push_back(prim);

        convBuf->Model->VertexFlag |= VertexType_UV;
    }
    else if (!strcmp(input->Semantic, "COLOR")) {
        // Resize vertex list to fit
        if (prim >= convBuf->Colors.size())
            convBuf->Colors.resize(prim + 1);

        // Insert color
        Uint32 r = (Uint32)(values[idx + 0] * 0xFF);
        Uint32 g = (Uint32)(values[idx + 1] * 0xFF);
        Uint32 b = (Uint32)(values[idx + 2] * 0xFF);
        convBuf->Colors[prim] = r << 16 | g << 8 | b;

        // Store tri's color index
        convBuf->colIndices.push_back(prim);

        convBuf->Model->VertexFlag |= VertexType_Color;
    }
}
PRIVATE STATIC void COLLADAReader::DoConversion(ColladaModel* daemodel, IModel* imodel) {
	size_t meshCount = daemodel->Meshes.size();
    if (!meshCount)
        return;

    imodel->FrameCount = meshCount;
    imodel->VertexFlag = VertexType_Position;
    imodel->FaceVertexCount = 3;

    int totalVertices = 0;
    int maxVertices = 0;

    // Maximum mesh conversions: 32
    if (imodel->FrameCount > 32)
        imodel->FrameCount = 32;

    ConversionBuffer conversionBuffer[32];
    memset(&conversionBuffer, 0, sizeof(conversionBuffer));

    // Obtain data
    ConversionBuffer* conversion = conversionBuffer;
    for (int meshNum = 0; meshNum < imodel->FrameCount; meshNum++) {
        ColladaMesh* mesh = daemodel->Meshes[meshNum];

        size_t numInputs = mesh->Triangles.Inputs.size();

        int numVertices = 0;
        int numProcessedInputs = 0;
        int totalMeshVertices = 0;

        conversion->Model = imodel;

        for (int primitiveInput = 0; primitiveInput < mesh->Triangles.Primitives.size(); primitiveInput++) {
            int prim = mesh->Triangles.Primitives[primitiveInput];

            ColladaInput* input = nullptr;

            // Find matching input.
            if (numInputs > 1) {
                for (int inputIndex = 0; inputIndex < numInputs; inputIndex++) {
                    ColladaInput* thisInput = mesh->Triangles.Inputs[inputIndex];
                    if ((primitiveInput % numInputs) == thisInput->Offset) {
                        input = thisInput;
                        break;
                    }
                }
            }
            else
                input = mesh->Triangles.Inputs[0];

            // ?!
            if (input == nullptr)
                continue;

            // VERTEX input
            if (input->Children.size()) {
                for (int inputIndex = 0; inputIndex < input->Children.size(); inputIndex++) {
                    ColladaInput* childInput = input->Children[inputIndex];
                    ProcessPrimitive(conversion, meshNum, childInput, prim);

                    // Count each child input.
                    numProcessedInputs++;
                }

                // A VERTEX input counts as a vertex, obviously
                numVertices++;
            }
            else {
                ProcessPrimitive(conversion, meshNum, input, prim);

                // Count each processed input.
                numProcessedInputs++;

                // When enough inputs have been processed,
                // it counts as a vertex.
                // if ((numProcessedInputs % numInputs) == 0)
                if (numProcessedInputs == numInputs) {
                    numProcessedInputs = 0;
                    numVertices++;
                }
            }

            // When three vertices have been added, add a face.
            if (numVertices == imodel->FaceVertexCount) {
                totalVertices += numVertices;
                totalMeshVertices += numVertices;
                numVertices = 0;
            }
        }

        conversion->TotalVertexCount = totalMeshVertices;
        conversion++;
    }

    // Create model
    imodel->VertexCount = totalVertices;
    if (imodel->VertexFlag & VertexType_Normal)
        imodel->PositionBuffer = (Vector3*)Memory::Calloc(imodel->VertexCount * imodel->FrameCount, 2 * sizeof(Vector3));
    else
        imodel->PositionBuffer = (Vector3*)Memory::Calloc(imodel->VertexCount * imodel->FrameCount, sizeof(Vector3));

    if (imodel->VertexFlag & VertexType_UV)
        imodel->UVBuffer = (Vector2*)Memory::Calloc(imodel->VertexCount, sizeof(Vector2));

    if (imodel->VertexFlag & VertexType_Color)
        imodel->ColorBuffer = (Uint32*)Memory::Calloc(imodel->VertexCount, sizeof(Uint32));

    imodel->VertexIndexCount = imodel->VertexCount;
    imodel->VertexIndexBuffer = (Sint16*)Memory::Calloc((imodel->VertexIndexCount + 1), sizeof(Sint16));
    for (int i = 0; i < imodel->VertexIndexCount; i++) {
        imodel->VertexIndexBuffer[i] = i;
    }
    imodel->VertexIndexBuffer[imodel->VertexIndexCount] = -1;

    // Read vertices
    conversion = conversionBuffer;
    for (int meshNum = 0; meshNum < imodel->FrameCount; meshNum++) {
        // Read UVs
        if (imodel->VertexFlag & VertexType_UV) {
            for (int i = 0; i < conversion->texIndices.size(); i++) {
                imodel->UVBuffer[i] = conversion->UVs[conversion->texIndices[i]];
            }
        }
        // Read Colors
        if (imodel->VertexFlag & VertexType_Color) {
            for (int i = 0; i < conversion->colIndices.size(); i++) {
                imodel->ColorBuffer[i] = conversion->Colors[conversion->colIndices[i]];
            }
        }
        // Read Normals & Positions
        if (imodel->VertexFlag & VertexType_Normal) {
            Vector3* vert = &imodel->PositionBuffer[0];
            for (int i = 0; i < conversion->nrmIndices.size(); i++) {
                *vert = conversion->Positions[conversion->posIndices[i]];
                vert++;

                *vert = conversion->Normals[conversion->nrmIndices[i]];
                vert++;
            }
        }
        else {
            Vector3* vert = &imodel->PositionBuffer[0];
            for (int i = 0; i < conversion->posIndices.size(); i++) {
                *vert = conversion->Positions[conversion->posIndices[i]];
                vert++;
            }
        }

        conversion++;
    }
}

//
// MAIN PARSER
//

PUBLIC STATIC IModel* COLLADAReader::Convert(IModel* model, Stream* stream) {
    XMLNode* modelXML = XMLParser::ParseFromStream(stream);
    if (!modelXML) {
        Log::Print(Log::LOG_ERROR, "Could not read model from stream!");
        return nullptr;
    }

    ColladaModel daemodel;

    const char* matchAsset = "asset";
    const char* matchLibrary = "library_";

    if (modelXML->children[0])
        modelXML = modelXML->children[0];

    for (size_t i = 0; i < modelXML->children.size(); i++) {
        Token section_token = modelXML->children[i]->name;
        char* section_name = (char* )calloc(1, section_token.Length + 1);
        memcpy(section_name, section_token.Start, section_token.Length);

        // Parse asset
        if (!strcmp(section_name, matchAsset))
            ParseAsset(&daemodel, modelXML->children[i]);
        // Parse library
        else if (!strncmp(section_name, matchLibrary, strlen(matchLibrary)))
            ParseLibrary(&daemodel, modelXML->children[i]);

        free(section_name);
    }

    // Assign effects to materials
    AssignEffectsToMaterials(&daemodel);

    // Convert to IModel
    DoConversion(&daemodel, model);

    return model;
}
