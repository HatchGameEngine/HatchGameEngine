#if INTERFACE
#include <Engine/ResourceTypes/ModelFormats/COLLADA.h>
#include <Engine/TextFormats/XML/XMLParser.h>
#include <Engine/IO/Stream.h>
#include <Engine/Rendering/Material.h>
class COLLADAReader {
public:

};
#endif

/*

Implemented by Lactozilla
https://github.com/Lactozilla

*/

#include <Engine/ResourceTypes/ModelFormats/COLLADAReader.h>

static ColladaModel* CurrentModel = nullptr;
static ColladaBone*  CurrentSkeleton = nullptr;

//
// AUXILIARY FUNCTIONS
//

static void TokenToString(Token tok, char** string) {
    *string = (char*)calloc(1, tok.Length + 1);
    memcpy(*string, tok.Start, tok.Length);
}

// Caveat: This modifies the source string.
static void ParseFloatArray(vector<float> &dest, char* source) {
    size_t i = 0, size = strlen(source);
    char* start = source;

    while (i <= size) {
        if (*source == '\0' || *source == ' ' || *source == '\n') {
            *source = '\0';

            if ((source - start) > 0) {
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

    while (i <= size) {
        if (*source == '\0' || *source == ' ' || *source == '\n') {
            *source = '\0';

            if ((source - start) > 0) {
                int num = atoi(start);
                dest.push_back(num);
            }

            start = (source + 1);
        }

        source++;
        i++;
    }
}

static void ParseNameArray(vector<char*> &dest, char* source) {
    size_t i = 0, size = strlen(source);
    char* start = source;

    while (i <= size) {
        if (*source == '\0' || *source == ' ' || *source == '\n') {
            *source = '\0';

            if ((source - start) > 0) {
                size_t len = strlen(start);
                char *name = (char*)memcpy(calloc(len+1, 1), start, len);
                dest.push_back(name);
            }

            start = (source + 1);
        }

        source++;
        i++;
    }
}

static void FloatStringToArray(XMLNode* contents, float* array, int size, float defaultval) {
    vector<float> vec;

    for (int i = 0; i < size; i++) {
        array[i] = defaultval;
    }

    if (contents->name.Length) {
        char* list = (char*)calloc(1, contents->name.Length + 1);
        memcpy(list, contents->name.Start, contents->name.Length);
        ParseFloatArray(vec, list);
        free(list);
    }

    for (int i = 0; i < size; i++) {
        array[i] = vec[i];
    }
}

static bool MatchToken(Token tok, const char* string) {
    if (strlen(string) != tok.Length)
        return false;
    return XMLParser::MatchToken(tok, string);
}

//
// ASSET PARSING
//

PRIVATE STATIC void COLLADAReader::ParseAsset(XMLNode* asset) {
    for (int i = 0; i < asset->children.size(); i++) {
        XMLNode* node = asset->children[i];
        if (MatchToken(node->name, "up_axis")) {
            XMLNode* axis = node->children[0];

            if (MatchToken(axis->name, "X_UP"))
                CurrentModel->Axis = DAE_X_UP;
            else if (MatchToken(axis->name, "Y_UP"))
                CurrentModel->Axis = DAE_Y_UP;
            else if (MatchToken(axis->name, "Z_UP"))
                CurrentModel->Axis = DAE_Z_UP;
        }
    }
}

//
// LIBRARY PARSING
//

static ColladaSource* FindSourceInList(vector<ColladaSource*> &list, char* name) {
    for (int i = 0; i < list.size(); i++) {
        if (!strcmp(list[i]->Id, name)) {
            return list[i];
        }
    }

    return nullptr;
}

static ColladaFloatArray* FindFloatAccessorSource(vector<ColladaFloatArray*> &arrays, char* name) {
    for (int i = 0; i < arrays.size(); i++) {
        if (!strcmp(arrays[i]->Id, name)) {
            return arrays[i];
        }
    }

    return nullptr;
}

static ColladaNameArray* FindNameAccessorSource(vector<ColladaNameArray*> &arrays, char* name) {
    for (int i = 0; i < arrays.size(); i++) {
        if (!strcmp(arrays[i]->Id, name)) {
            return arrays[i];
        }
    }

    return nullptr;
}

PRIVATE STATIC ColladaSource* COLLADAReader::ParseSource(vector<ColladaSource*> &sourceList, XMLNode* node) {
    ColladaSource* source = new ColladaSource;

    TokenToString(node->attributes.Get("id"), &source->Id);

    // Find arrays first
    for (int i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (MatchToken(child->name, "float_array")) {
            ColladaFloatArray* floatArray = new ColladaFloatArray;

            TokenToString(child->attributes.Get("id"), &floatArray->Id);
            floatArray->Count = XMLParser::TokenToNumber(child->attributes.Get("count"));

            XMLNode* contents = child->children[0];
            if (contents->name.Length) {
                char* list = (char*)calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseFloatArray(floatArray->Contents, list);
                free(list);
            }

            source->FloatArrays.push_back(floatArray);
        } else if (MatchToken(child->name, "Name_array")) {
            ColladaNameArray* nameArray = new ColladaNameArray;

            TokenToString(child->attributes.Get("id"), &nameArray->Id);
            nameArray->Count = XMLParser::TokenToNumber(child->attributes.Get("count"));

            XMLNode* contents = child->children[0];
            if (contents->name.Length) {
                char* list = (char*)calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseNameArray(nameArray->Contents, list);
                free(list);
            }

            source->NameArrays.push_back(nameArray);
        }
    }

    // Then find accessors
    for (int i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (MatchToken(child->name, "technique_common")) {
            XMLNode* accessorNode = child->children[0];
            ColladaAccessor* accessor = new ColladaAccessor;

            TokenToString(accessorNode->attributes.Get("source"), &accessor->Source);
            accessor->FloatArray = FindFloatAccessorSource(source->FloatArrays, (accessor->Source + 1));
            accessor->NameArray = FindNameAccessorSource(source->NameArrays, (accessor->Source + 1));
            accessor->Count = XMLParser::TokenToNumber(accessorNode->attributes.Get("count"));
            accessor->Stride = XMLParser::TokenToNumber(accessorNode->attributes.Get("stride"));

            for (int i = 0; i < accessorNode->children.size(); i++) {
                XMLNode* grandchild = accessorNode->children[i];
                if (MatchToken(grandchild->name, "param")) {
                    TokenToString(grandchild->attributes.Get("name"), &accessor->Parameter);
                }
            }

            source->Accessors.push_back(accessor);
        }
    }

    sourceList.push_back(source);

    return source;
}

PRIVATE STATIC void COLLADAReader::ParseMeshVertices(ColladaMesh* mesh, XMLNode* node) {
    TokenToString(node->attributes.Get("id"), &mesh->Vertices.Id);

    for (int i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (MatchToken(child->name, "input")) {
            ColladaInput* input = new ColladaInput;

            char* inputSource;
            TokenToString(child->attributes.Get("source"), &inputSource);
            input->Source = FindSourceInList(mesh->SourceList, (inputSource + 1));
            input->Offset = XMLParser::TokenToNumber(child->attributes.Get("offset"));
            TokenToString(child->attributes.Get("semantic"), &input->Semantic);

            mesh->Vertices.Inputs.push_back(input);
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseMeshTriangles(ColladaMesh* mesh, XMLNode* node) {
    ColladaTriangles* triangles = new ColladaTriangles;

    TokenToString(node->attributes.Get("id"), &triangles->Id);
    triangles->Count = XMLParser::TokenToNumber(node->attributes.Get("count"));

    for (int i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];

        if (MatchToken(child->name, "input")) {
            ColladaInput* input = new ColladaInput;

            TokenToString(child->attributes.Get("semantic"), &input->Semantic);

            if (!input->Semantic) {
                delete input;
                continue;
            }

            char* inputSource;
            TokenToString(child->attributes.Get("source"), &inputSource);

            if (!strcmp(input->Semantic, "VERTEX")) {
                if (!strcmp(mesh->Vertices.Id, (inputSource + 1))) {
                    if (mesh->Vertices.Inputs.size() > 1) {
                        for (int j = 0; j < mesh->Vertices.Inputs.size(); j++)
                            input->Children.push_back(mesh->Vertices.Inputs[j]);
                        input->Source = nullptr;
                    } else {
                        triangles->Inputs.push_back(mesh->Vertices.Inputs[0]);
                        delete input;
                        continue;
                    }
                }
            } else {
                input->Source = FindSourceInList(mesh->SourceList, (inputSource + 1));
            }

            input->Offset = XMLParser::TokenToNumber(child->attributes.Get("offset"));

            triangles->Inputs.push_back(input);
        } else if (MatchToken(child->name, "p")) {
            XMLNode* contents = child->children[0];
            if (contents->name.Length) {
                char* list = (char*)calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseIntegerArray(triangles->Primitives, list);
                free(list);
            }
        }
    }

    mesh->Triangles.push_back(triangles);
}

PRIVATE STATIC void COLLADAReader::ParseGeometry(XMLNode* geometryNode) {
    ColladaGeometry* geometry = new ColladaGeometry;

    TokenToString(geometryNode->attributes.Get("id"), &geometry->Id);
    TokenToString(geometryNode->attributes.Get("name"), &geometry->Name);

    for (int i = 0; i < geometryNode->children.size(); i++) {
        XMLNode* node = geometryNode->children[i];

        if (MatchToken(node->name, "mesh")) {
            ParseMesh(node);
            geometry->Mesh = CurrentModel->Meshes.back();
        }
    }

    CurrentModel->Geometries.push_back(geometry);
}

PRIVATE STATIC void COLLADAReader::ParseMesh(XMLNode* meshNode) {
    ColladaMesh* mesh = new ColladaMesh;

    for (int i = 0; i < meshNode->children.size(); i++) {
        XMLNode* node = meshNode->children[i];

        if (MatchToken(node->name, "source")) {
            ParseSource(mesh->SourceList, node);
        } else if (MatchToken(node->name, "vertices")) {
            ParseMeshVertices(mesh, node);
        } else if (MatchToken(node->name, "triangles")) {
            ParseMeshTriangles(mesh, node);
        }
    }

    CurrentModel->Meshes.push_back(mesh);
}

PRIVATE STATIC void COLLADAReader::ParseImage(XMLNode* parent, XMLNode* effect) {
    ColladaImage* image = new ColladaImage;

    TokenToString(parent->attributes.Get("id"), &image->Id);

    XMLNode* path = effect->children[0];
    if (path->name.Length) {
        image->Path = (char*)calloc(1, path->name.Length + 1);
        memcpy(image->Path, path->name.Start, path->name.Length);
        CurrentModel->Images.push_back(image);
    }
}

PRIVATE STATIC void COLLADAReader::ParseSurface(XMLNode* parentNode, XMLNode* surfaceNode) {
    ColladaSurface* surface = new ColladaSurface;

    TokenToString(parentNode->attributes.Get("sid"), &surface->Id);
    TokenToString(surfaceNode->attributes.Get("type"), &surface->Type);

    surface->Image = nullptr;

    for (int i = 0; i < surfaceNode->children.size(); i++) {
        XMLNode* node = surfaceNode->children[i];
        if (MatchToken(node->name, "init_from")) {
            XMLNode* child = node->children[0];
            if (child->name.Length) {
                char* imageString = (char*)calloc(1, child->name.Length + 1);
                memcpy(imageString, child->name.Start, child->name.Length);

                for (int j = 0; j < CurrentModel->Images.size(); j++) {
                    ColladaImage* image = CurrentModel->Images[j];
                    if (!strcmp(imageString, image->Id)) {
                        surface->Image = image;
                        break;
                    }
                }

                free(imageString);
            }
        }
    }

    CurrentModel->Surfaces.push_back(surface);
}

PRIVATE STATIC void COLLADAReader::ParseSampler(XMLNode* parentNode, XMLNode* samplerNode) {
    ColladaSampler* sampler = new ColladaSampler;

    TokenToString(parentNode->attributes.Get("sid"), &sampler->Id);

    sampler->Surface = nullptr;

    for (int i = 0; i < samplerNode->children.size(); i++) {
        XMLNode* node = samplerNode->children[i];
        // Sampler comes after surface so this is fine
        if (MatchToken(node->name, "source")) {
            XMLNode* child = node->children[0];

            if (!child->name.Length)
                continue;

            char* source = (char*)calloc(1, child->name.Length + 1);
            memcpy(source, child->name.Start, child->name.Length);

            for (int i = 0; i < CurrentModel->Surfaces.size(); i++) {
                ColladaSurface* surface = CurrentModel->Surfaces[i];
                if (!strcmp(source, surface->Id)) {
                    sampler->Surface = surface;
                    break;
                }
            }

            free(source);
        }
    }

    CurrentModel->Samplers.push_back(sampler);
}

PRIVATE STATIC void COLLADAReader::ParsePhongComponent(ColladaPhongComponent& component, XMLNode* phong) {
    for (int i = 0; i < phong->children.size(); i++) {
        XMLNode* node = phong->children[i];

        if (MatchToken(node->name, "color")) {
            ParseEffectColor(node->children[0], component.Color);
        } else if (MatchToken(node->name, "texture")) {
            char* samplerName;
            TokenToString(node->attributes.Get("texture"), &samplerName);

            component.Sampler = nullptr;

            for (int j = 0; j < CurrentModel->Samplers.size(); j++) {
                ColladaSampler* sampler = CurrentModel->Samplers[j];
                if (!strcmp(sampler->Id, samplerName)) {
                    component.Sampler = sampler;
                    break;
                }
            }

            free(samplerName);
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseEffectColor(XMLNode* contents, float* colors) {
    FloatStringToArray(contents, colors, 4, 1.0f);
}

PRIVATE STATIC void COLLADAReader::ParseEffectFloat(XMLNode* contents, float &value) {
    if (contents->name.Length) {
        char* floatstr = (char*)calloc(1, contents->name.Length + 1);
        memcpy(floatstr, contents->name.Start, contents->name.Length);
        value = atof(floatstr);
        free(floatstr);
    } else
        value = 0.0f;
}

PRIVATE STATIC void COLLADAReader::ParseEffectTechnique(ColladaEffect* effect, XMLNode* technique) {
    for (int i = 0; i < technique->children.size(); i++) {
        XMLNode* node = technique->children[i];
        XMLNode* child = node->children[0];

        if (MatchToken(node->name, "specular")) {
            ParsePhongComponent(effect->Specular, node);
        } else if (MatchToken(node->name, "ambient")) {
            ParsePhongComponent(effect->Ambient, node);
        } else if (MatchToken(node->name, "emission")) {
            ParsePhongComponent(effect->Emission, node);
        } else if (MatchToken(node->name, "diffuse")) {
            ParsePhongComponent(effect->Diffuse, node);
        } else {
            XMLNode* grandchild = child->children[0];
            if (MatchToken(node->name, "shininess")) {
                ParseEffectFloat(grandchild, effect->Shininess);
            } else if (MatchToken(node->name, "transparency")) {
                ParseEffectFloat(grandchild, effect->Transparency);
            } else if (MatchToken(node->name, "index_of_refraction")) {
                ParseEffectFloat(grandchild, effect->IndexOfRefraction);
            }
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseEffect(XMLNode* parent, XMLNode* effectNode) {
    ColladaEffect* effect = new ColladaEffect;

    memset(effect, 0x00, sizeof(ColladaEffect));

    TokenToString(parent->attributes.Get("id"), &effect->Id);

    for (int i = 0; i < effectNode->children.size(); i++) {
        XMLNode* node = effectNode->children[i];
        XMLNode* child = node->children[0];

        if (MatchToken(node->name, "newparam")) {
            if (MatchToken(child->name, "surface")) {
                ParseSurface(node, child);
            } else if (MatchToken(child->name, "sampler2D")) {
                ParseSampler(node, child);
            }
        } else if (MatchToken(node->name, "technique")) {
            ParseEffectTechnique(effect, child);
        }
    }

    CurrentModel->Effects.push_back(effect);
}

PRIVATE STATIC void COLLADAReader::ParseMaterial(XMLNode* parentNode, XMLNode* materialNode) {
    ColladaMaterial* material = new ColladaMaterial;

    material->Effect = nullptr;

    TokenToString(parentNode->attributes.Get("id"), &material->Id);
    TokenToString(parentNode->attributes.Get("name"), &material->Name);

    TokenToString(materialNode->attributes.Get("url"), &material->EffectLink);

    CurrentModel->Materials.push_back(material);
}

PRIVATE STATIC void COLLADAReader::AssignEffectsToMaterials(void) {
    for (int i = 0; i < CurrentModel->Materials.size(); i++) {
        ColladaMaterial* material = CurrentModel->Materials[i];
        for (int j = 0; j < CurrentModel->Effects.size(); j++) {
            ColladaEffect* effect = CurrentModel->Effects[j];
            if (!strcmp((material->EffectLink + 1), effect->Id)) {
                material->Effect = effect;
                break;
            }
        }
    }
}

PRIVATE STATIC ColladaMesh* COLLADAReader::GetMeshFromGeometryURL(char* geometryURL) {
    for (int i = 0; i < CurrentModel->Geometries.size(); i++) {
        ColladaGeometry* geometry = CurrentModel->Geometries[i];
        if (!strcmp(geometryURL, geometry->Id)) {
            return geometry->Mesh;
        }
    }

    return nullptr;
}

PRIVATE STATIC ColladaController* COLLADAReader::GetControllerFromURL(char* controllerURL) {
    for (int i = 0; i < CurrentModel->Controllers.size(); i++) {
        ColladaController* controller = CurrentModel->Controllers[i];
        if (!strcmp(controllerURL, controller->Id)) {
            return controller;
        }
    }

    return nullptr;
}

static ColladaMesh *InstancedMesh = nullptr;

PRIVATE STATIC void COLLADAReader::InstanceMesh(ColladaScene* scene, ColladaNode* node) {
    ColladaMesh* refMesh = node->Mesh;

    InstancedMesh = new ColladaMesh;
    InstancedMesh->SourceList = refMesh->SourceList;
    InstancedMesh->Triangles = refMesh->Triangles;

    InstanceMeshMaterials(node);
    memcpy(&InstancedMesh->Vertices, &refMesh->Vertices, sizeof(ColladaVertices));

    ProcessMeshPrimitives();

    scene->Meshes.push_back(InstancedMesh);
}

PRIVATE STATIC void COLLADAReader::InstanceMaterial(ColladaNode* daeNode, XMLNode* node) {
    char* target;
    TokenToString(node->attributes.Get("target"), &target);

    for (int i = 0; i < CurrentModel->Materials.size(); i++) {
        ColladaMaterial* material = CurrentModel->Materials[i];
        if (!strcmp((target + 1), material->Id)) {
            daeNode->MaterialList.push_back(material);
            break;
        }
    }

    free(target);
}

PRIVATE STATIC void COLLADAReader::BindMaterial(ColladaNode* daeNode, XMLNode* node) {
    XMLNode* common = node->children[0];
    if (common && MatchToken(common->name, "technique_common")) {
        for (int i = 0; i < common->children.size(); i++) {
            if (MatchToken(common->children[i]->name, "instance_material")) {
                InstanceMaterial(daeNode, common->children[i]);
            }
        }
    }
}

PRIVATE STATIC void COLLADAReader::InstanceMeshMaterials(ColladaNode* node) {
    for (int i = 0; i < node->MaterialList.size(); i++)
        InstancedMesh->MaterialList.push_back(node->MaterialList[i]);
}

PRIVATE STATIC void COLLADAReader::ParseControllerNode(ColladaScene* scene, ColladaNode* parentNode, XMLNode *parent, XMLNode* child) {
    if (MatchToken(child->name, "skeleton")) {
        char* controllerURL;

        TokenToString(parent->attributes.Get("url"), &controllerURL);

        // Find which controller this instance_controller refers to,
        // then find which node its skeleton refers to,
        // and assign that controller's root bone to the skeleton's node.
        for (int i = 0; i < CurrentModel->Controllers.size(); i++) {
            ColladaController* controller = CurrentModel->Controllers[i];

            controller->RootBone = nullptr;

            if (strcmp(controllerURL+1, controller->Id))
                continue;

            for (int j = 0; j < CurrentModel->Nodes.size(); j++) {
                ColladaNode* node = CurrentModel->Nodes[j];

                XMLNode* contents = child->children[0];
                if (!contents->name.Length)
                    continue;

                char* root = (char*)calloc(1, contents->name.Length + 1);
                memcpy(root, contents->name.Start, contents->name.Length);

                // Set the controller's root bone to this node.
                if (!strcmp(root, node->Id)) {
                    controller->RootBone = node;
                    free(root);
                    break;
                }

                free(root);
            }
        }
    } else if (MatchToken(child->name, "bind_material")) {
        BindMaterial(parentNode, child);
    }
}

static void ProcessPrimitive(ColladaPreProc *preproc, ColladaInput* input, int prim) {
    ColladaAccessor* accessor = input->Source->Accessors[0];
    vector<float> values = accessor->FloatArray->Contents;
    int idx = (prim * accessor->Stride);

    #define RESIZE_PROC(proc) \
        if (prim >= proc.size()) \
            proc.resize(prim + 1);

    #define PROC_VALUES3(proc) \
        RESIZE_PROC(proc) \
        proc[prim].Values[0] = values[idx + 0]; \
        proc[prim].Values[1] = values[idx + 1]; \
        proc[prim].Values[2] = values[idx + 2];

    #define PROC_VALUES2(proc) \
        RESIZE_PROC(proc) \
        proc[prim].Values[0] = values[idx + 0]; \
        proc[prim].Values[1] = values[idx + 1];

    if (!strcmp(input->Semantic, "POSITION")) {
        PROC_VALUES3(preproc->Positions)
        preproc->posIndices.push_back(prim);
    } else if (!strcmp(input->Semantic, "NORMAL")) {
        PROC_VALUES3(preproc->Normals)
        preproc->nrmIndices.push_back(prim);
    } else if (!strcmp(input->Semantic, "TEXCOORD")) {
        PROC_VALUES2(preproc->UVs)
        preproc->texIndices.push_back(prim);
    } else if (!strcmp(input->Semantic, "COLOR")) {
        PROC_VALUES3(preproc->Colors)
        preproc->colIndices.push_back(prim);
    }

    #undef RESIZE_PROC
    #undef PROC_VALUES3
    #undef PROC_VALUES2
}

PRIVATE STATIC void COLLADAReader::ProcessMeshPrimitives(void) {
    ColladaMesh* mesh = InstancedMesh;
    mesh->Processed.NumVertices = 0;

    memset(&mesh->Processed, 0x00, sizeof(ColladaPreProc));

    for (int i = 0; i < mesh->Triangles.size(); i++) {
        ColladaTriangles* triangles = mesh->Triangles[i];

        int numInputs = triangles->Inputs.size();
        int numProcessedInputs = 0;
        int numVertices = 0;

        for (int primitiveIndex = 0; primitiveIndex < triangles->Primitives.size(); primitiveIndex++) {
            ColladaInput* input = nullptr;

            // Find matching input.
            if (numInputs > 1) {
                for (int inputIndex = 0; inputIndex < numInputs; inputIndex++) {
                    ColladaInput* thisInput = triangles->Inputs[inputIndex];
                    if ((primitiveIndex % numInputs) == thisInput->Offset) {
                        input = thisInput;
                        break;
                    }
                }
            }
            else
                input = triangles->Inputs[0];

            // Huh?
            if (input == nullptr)
                continue;

            int primitive = triangles->Primitives[primitiveIndex];

            // VERTEX input
            if (input->Children.size()) {
                for (int inputIndex = 0; inputIndex < input->Children.size(); inputIndex++) {
                    ProcessPrimitive(&mesh->Processed, input->Children[inputIndex], primitive);
                    numProcessedInputs++; // Count each child input.
                }

                // A VERTEX input counts as a vertex.
                mesh->Processed.NumVertices++;
                numVertices++;
            } else {
                ProcessPrimitive(&mesh->Processed, input, primitive);

                // Count each processed input.
                numProcessedInputs++;

                // When enough inputs have been processed, it counts as a vertex.
                if (numProcessedInputs == numInputs) {
                    mesh->Processed.NumVertices++;
                    numVertices++;
                    numProcessedInputs = 0;
                }
            }
        }

        mesh->VertexCounts.push_back(numVertices);
    }
}

static Matrix4x4 *GlobalInverseTransform = nullptr;
static Matrix4x4 *BindShapeMatrix = nullptr;

static void FindJointNode(ColladaBone* bone, char* name) {
    bone->Joint = -1;
    for (int i = 0; i < InstancedMesh->JointList.size(); i++) {
        if (!strcmp(name, InstancedMesh->JointList[i]->Name)) {
            InstancedMesh->JointList[i]->Bone = bone;
            bone->Joint = i;
            break;
        }
    }
}

static void CopyBoneMatrix(float* dst, float* src) {
    memcpy(dst, src, sizeof(float) * 16);
}

static Matrix4x4* GenerateMatrix4x4(float* src) {
    Matrix4x4 *mat4 = Matrix4x4::Create();

    if (src) {
        for (int i = 0; i < 16; i++) {
            mat4->Values[i] = src[i];
        }
    }

    return mat4;
}

static void GenerateGlobalInverseTransform(float* matrix) {
    float inv[16];
    float src[16];

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            src[(i*4)+j] = matrix[(j*4)+i];
        }
    }

    GlobalInverseTransform = Matrix4x4::Create();

    inv[0] = src[5] * src[10] * src[15] - src[5] * src[11] * src[14] - src[9] * src[6] * src[15] + src[9]
        * src[7] * src[14] + src[13] * src[6] * src[11] - src[13] * src[7] * src[10];
    inv[4] = -src[4] * src[10] * src[15] + src[4] * src[11] * src[14] + src[8] * src[6] * src[15] - src[8]
        * src[7] * src[14] - src[12] * src[6] * src[11] + src[12] * src[7] * src[10];
    inv[8] = src[4] * src[9] * src[15] - src[4] * src[11] * src[13] - src[8] * src[5] * src[15] + src[8]
        * src[7] * src[13] + src[12] * src[5] * src[11] - src[12] * src[7] * src[9];
    inv[12] = -src[4] * src[9] * src[14] + src[4] * src[10] * src[13] +src[8] * src[5] * src[14] - src[8]
        * src[6] * src[13] - src[12] * src[5] * src[10] + src[12] * src[6] * src[9];
    inv[1] = -src[1] * src[10] * src[15] + src[1] * src[11] * src[14] + src[9] * src[2] * src[15] - src[9]
        * src[3] * src[14] - src[13] * src[2] * src[11] + src[13] * src[3] * src[10];
    inv[5] = src[0] * src[10] * src[15] - src[0] * src[11] * src[14] - src[8] * src[2] * src[15] + src[8]
        * src[3] * src[14] + src[12] * src[2] * src[11] - src[12] * src[3] * src[10];
    inv[9] = -src[0] * src[9] * src[15] + src[0] * src[11] * src[13] + src[8] * src[1] * src[15] - src[8]
        * src[3] * src[13] - src[12] * src[1] * src[11] + src[12] * src[3] * src[9];
    inv[13] = src[0] * src[9] * src[14] - src[0] * src[10] * src[13] - src[8] * src[1] * src[14] + src[8]
        * src[2] * src[13] + src[12] * src[1] * src[10] - src[12] * src[2] * src[9];
    inv[2] = src[1] * src[6] * src[15] - src[1] * src[7] * src[14] - src[5] * src[2] * src[15] + src[5]
        * src[3] * src[14] + src[13] * src[2] * src[7] - src[13] * src[3] * src[6];
    inv[6] = -src[0] * src[6] * src[15] + src[0] * src[7] * src[14] + src[4] * src[2] * src[15] - src[4]
        * src[3] * src[14] - src[12] * src[2] * src[7] + src[12] * src[3] * src[6];
    inv[10] = src[0] * src[5] * src[15] - src[0] * src[7] * src[13] - src[4] * src[1] * src[15] + src[4]
        * src[3] * src[13] + src[12] * src[1] * src[7] - src[12] * src[3] * src[5];
    inv[14] = -src[0] * src[5] * src[14] + src[0] * src[6] * src[13] + src[4] * src[1] * src[14] - src[4]
        * src[2] * src[13] - src[12] * src[1] * src[6] + src[12] * src[2] * src[5];
    inv[3] = -src[1] * src[6] * src[11] + src[1] * src[7] * src[10] + src[5] * src[2] * src[11] - src[5]
        * src[3] * src[10] - src[9] * src[2] * src[7] + src[9] * src[3] * src[6];
    inv[7] = src[0] * src[6] * src[11] - src[0] * src[7] * src[10] - src[4] * src[2] * src[11] + src[4]
        * src[3] * src[10] + src[8] * src[2] * src[7] - src[8] * src[3] * src[6];
    inv[11] = -src[0] * src[5] * src[11] + src[0] * src[7] * src[9] + src[4] * src[1] * src[11] - src[4]
        * src[3] * src[9] - src[8] * src[1] * src[7] + src[8] * src[3] * src[5];
    inv[15] = src[0] * src[5] * src[10] - src[0] * src[6] * src[9] - src[4] * src[1] * src[10] + src[4]
        * src[2] * src[9] + src[8] * src[1] * src[6] - src[8] * src[2] * src[5];

    float det = src[0] * inv[0] + src[1] * inv[4] + src[2] * inv[8] + src[3] * inv[12];
    if (det > 0.0f) {
        det = 1.0f / det;
        for (int i = 0; i < 16; i++)
            GlobalInverseTransform->Values[i] = inv[i] * det;
    }
}

static void CalculateBoneLocalTransform(ColladaBone* bone, ColladaBone* parentBone) {
    Matrix4x4 *parentMatrix = nullptr;

    if (parentBone == nullptr || parentBone->LocalTransform == nullptr)
        parentMatrix = Matrix4x4::Create();
    else
        parentMatrix = parentBone->LocalTransform;

    bone->LocalTransform = Matrix4x4::Create();
    Matrix4x4::Multiply(bone->LocalTransform, parentMatrix, bone->LocalMatrix);
}

static void CalculateBoneFinalTransform(ColladaBone* bone, ColladaNode *node) {
    bone->FinalTransform = Matrix4x4::Create();

    if (bone->Joint != -1) {
        Matrix4x4 *localWithInverse = Matrix4x4::Create();
        Matrix4x4::Multiply(localWithInverse, bone->LocalTransform, GlobalInverseTransform);
        Matrix4x4::Multiply(bone->FinalTransform, bone->InverseBindMatrix, localWithInverse);
        delete localWithInverse;
    } else {
        Matrix4x4::Multiply(bone->FinalTransform, bone->InverseBindMatrix, GlobalInverseTransform);
    }
}

static void CalculateBone(ColladaBone* bone, ColladaBone* parentBone, ColladaNode *node, ColladaController *controller) {
    FindJointNode(bone, node->Sid);

    bone->LocalMatrix = nullptr;
    bone->InverseBindMatrix = nullptr;
    bone->LocalTransform = nullptr;
    bone->FinalTransform = nullptr;

    ColladaBone* childrenParent = parentBone;

    if (bone->Joint != -1) {
        ColladaSource* invBindMatrix = controller->InvBindMatrixSource;
        if (invBindMatrix && invBindMatrix->Accessors.size()) {
            ColladaAccessor* bindAccessor = invBindMatrix->Accessors[0];
            ColladaFloatArray* floatArray = nullptr;

            for (int i = 0; i < invBindMatrix->FloatArrays.size(); i++) {
                char* Id = invBindMatrix->FloatArrays[i]->Id;
                if (!strcmp(bindAccessor->Source+1, Id)) {
                    floatArray = invBindMatrix->FloatArrays[i];
                    break;
                }
            }

            if (floatArray && floatArray->Count) {
                float dst[16];
                for (int i = 0; i < 16; i++)
                    dst[i] = floatArray->Contents[(bone->Joint * 16) + i];
                bone->InverseBindMatrix = GenerateMatrix4x4(dst);
                Matrix4x4::Transpose(bone->InverseBindMatrix);
            }

            if (bone->InverseBindMatrix == nullptr)
                Log::Print(Log::LOG_ERROR, "Bone with associated node %s has no inverse bind matrix!", node->Sid);
        }

        bone->LocalMatrix = GenerateMatrix4x4(node->Matrix);
        Matrix4x4::Transpose(bone->LocalMatrix);
        CalculateBoneLocalTransform(bone, parentBone);
        CalculateBoneFinalTransform(bone, node);

        childrenParent = bone;
    }

    // Set parent and children
    bone->Parent = parentBone;
    bone->NumChildren = node->Children.size();

    if (bone->NumChildren) {
        bone->Children = new ColladaBone[bone->NumChildren];
        for (int i = 0; i < bone->NumChildren; i++) {
            ColladaBone* childBone = ((ColladaBone*)bone->Children) + i;
            CalculateBone(childBone, childrenParent, node->Children[i], controller);
        }
    }
}

static ColladaBone* GenerateSkeleton(ColladaController* controller, ColladaNode *rootBoneNode) {
    ColladaBone* root = new ColladaBone;
    CalculateBone(root, nullptr, rootBoneNode, controller);
    return root;
}

static void DeleteBone(ColladaBone* bone) {
    delete bone->LocalMatrix;
    delete bone->LocalTransform;
    delete bone->InverseBindMatrix;
    delete bone->FinalTransform;

    if (bone->NumChildren) {
        for (int i = 0; i < bone->NumChildren; i++) {
            ColladaBone* childBone = ((ColladaBone*)bone->Children) + i;
            DeleteBone(childBone);
        }
        delete[] bone->Children;
    }
}

static void DeleteSkeleton(ColladaBone* bone) {
    DeleteBone(bone);
    delete bone;
}

static bool BuildJoints(ColladaController* controller, vector<ColladaNode*> &nodeList) {
    ColladaSource* source = controller->JointList[0];

    if (!source)
        return false;

    ColladaAccessor* accessor = source->Accessors[0];
    if (!accessor)
        return false;

    ColladaNameArray *nameArray = accessor->NameArray;
    if (!nameArray)
        return false;

    for (int i = 0; i < nameArray->Count; i++) {
        char *jointName = nameArray->Contents[i];

        for (int j = 0; j < nodeList.size(); j++) {
            ColladaNode* node = nodeList[j];
            if (!strcmp(jointName, node->Sid)) {
                ColladaJoint* joint = new ColladaJoint;
                joint->Name = jointName;
                joint->Bone = nullptr;
                InstancedMesh->JointList.push_back(joint);
                break;
            }
        }
    }

    return true;
}

static void PoseInstancedMesh(VertexWeightInfo* vwInfo, int numVertexWeights, float* weights) {
    ColladaPreProc *preproc = &InstancedMesh->Processed;
    if (vwInfo == nullptr || !numVertexWeights)
        return;

    int curVertex = 0;

    float weighted[4];
    float vtx[4];

    weighted[3] = 1.0f;
    vtx[3] = 1.0f;

    while (numVertexWeights--) {
        float* vtxValues = preproc->Positions[curVertex].Values;

        for (int i = 0; i < 3; i++)
            weighted[i] = 0.0f;

        for (int i = 0; i < vwInfo->Influences; i++) {
            ColladaJoint* joint = InstancedMesh->JointList[vwInfo->JointIndices[i]];
            ColladaBone* bone = joint->Bone;
            float weight = weights[vwInfo->WeightIndices[i]];

            for (int i = 0; i < 3; i++) {
                vtx[i] = vtxValues[i];
            }
            Matrix4x4::Multiply((Matrix4x4*)bone->FinalTransform, vtx);

            for (int i = 0; i < 3; i++) {
                vtx[i] *= weight;
                weighted[i] += vtx[i];
            }
        }

        Matrix4x4::Multiply(BindShapeMatrix, weighted);
        for (int i = 0; i < 3; i++) {
            vtxValues[i] = weighted[i];
        }

        vwInfo++;
        curVertex++;
    }
}

PRIVATE STATIC void COLLADAReader::InstanceMeshFromController(ColladaScene* scene, ColladaNode* node, ColladaController* controller) {
    ColladaMesh* refMesh = GetMeshFromGeometryURL(controller->Skin + 1);
    if (!refMesh)
        return;

    InstancedMesh = new ColladaMesh;
    InstancedMesh->SourceList = refMesh->SourceList;
    InstancedMesh->Triangles = refMesh->Triangles;

    InstanceMeshMaterials(node);
    memcpy(&InstancedMesh->Vertices, &refMesh->Vertices, sizeof(ColladaVertices));

    ColladaBone* skeleton = nullptr;

    if (controller->RootBone) {
        // Create the joint list
        if (controller->JointList.size()) {
            if (BuildJoints(controller, scene->Nodes)) {
                ColladaNode* rootBone = controller->RootBone;
                ColladaNode* invTransformBone = rootBone;

                while (invTransformBone->Parent)
                    invTransformBone = invTransformBone->Parent;

                GenerateGlobalInverseTransform(invTransformBone->Matrix);
                skeleton = GenerateSkeleton(controller, rootBone);
            }
        }
    }

    ProcessMeshPrimitives();

    if (skeleton) {
        BindShapeMatrix = GenerateMatrix4x4(controller->BindShapeMatrix);
        Matrix4x4::Transpose(BindShapeMatrix);
        PoseInstancedMesh(controller->VWInfo, controller->NumVertexWeights, controller->Weights);
        DeleteSkeleton(skeleton);
    }

    scene->Meshes.push_back(InstancedMesh);
}

PRIVATE STATIC ColladaNode* COLLADAReader::ParseNode(ColladaScene* scene, XMLNode* node, ColladaNode* parent, int tree) {
    ColladaNode* daeNode = new ColladaNode;

    TokenToString(node->attributes.Get("id"), &daeNode->Id);
    TokenToString(node->attributes.Get("name"), &daeNode->Name);
    TokenToString(node->attributes.Get("sid"), &daeNode->Sid);

    daeNode->Parent = parent;
    daeNode->Mesh = nullptr;

    for (int i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];

        if (MatchToken(child->name, "matrix")) {
            FloatStringToArray(child->children[0], daeNode->Matrix, 16, 0.0f);
        } else if (MatchToken(child->name, "node")) {
            ColladaNode* offspring = ParseNode(scene, child, daeNode, tree + 1);
            daeNode->Children.push_back(offspring);
        } else if (MatchToken(child->name, "instance_geometry")) {
            for (int j = 0; j < child->children.size(); j++) {
                XMLNode* grandchild = child->children[j];
                if (MatchToken(grandchild->name, "bind_material")) {
                    BindMaterial(daeNode, child);
                }
            }

            char *geometryURL;
            TokenToString(child->attributes.Get("url"), &geometryURL);
            daeNode->Mesh = GetMeshFromGeometryURL(geometryURL + 1);
            free(geometryURL);

            if (daeNode->Mesh) {
                InstanceMesh(scene, daeNode);
            }
        } else if (MatchToken(child->name, "instance_controller")) {
            ColladaController* controller;

            for (int j = 0; j < child->children.size(); j++) {
                ParseControllerNode(scene, daeNode, child, child->children[j]);
            }

            char *controllerURL;
            TokenToString(child->attributes.Get("url"), &controllerURL);
            controller = GetControllerFromURL(controllerURL + 1);
            free(controllerURL);

            if (controller) {
                InstanceMeshFromController(scene, daeNode, controller);
            }
        }
    }

    scene->Nodes.push_back(daeNode);
    CurrentModel->Nodes.push_back(daeNode);

    return daeNode;
}

PRIVATE STATIC void COLLADAReader::ParseVisualScene(XMLNode* node) {
    ColladaScene *scene = new ColladaScene;

    for (int i = 0; i < node->children.size(); i++) {
        XMLNode* child = node->children[i];
        if (MatchToken(child->name, "node"))
            ParseNode(scene, child, nullptr, 0);
    }

    CurrentModel->Scenes.push_back(scene);
}

PRIVATE STATIC void COLLADAReader::ParseControllerWeights(ColladaController* controller, XMLNode* node) {
    controller->NumVertexWeights = XMLParser::TokenToNumber(node->attributes.Get("count"));
    controller->NumInputs = 0;

    for (int i = 0; i < node->children.size(); i++) {
        XMLNode* grandchild = node->children[i];

        if (MatchToken(grandchild->name, "input")) {
            char* semantic;
            char* inputSource;
            int offset;

            TokenToString(grandchild->attributes.Get("semantic"), &semantic);
            TokenToString(grandchild->attributes.Get("source"), &inputSource);
            offset = XMLParser::TokenToNumber(grandchild->attributes.Get("offset"));

            if (!strcmp(semantic, "JOINT")) {
                controller->VWJoints = FindSourceInList(controller->SourceList, (inputSource + 1));
                controller->VWJointOffset = offset;
            } else if (!strcmp(semantic, "WEIGHT")) {
                controller->VWWeights = FindSourceInList(controller->SourceList, (inputSource + 1));
                controller->VWWeightsOffset = offset;

                controller->NumWeights = 0;
                controller->Weights = nullptr;

                ColladaSource* source = controller->VWWeights;
                if (source) {
                    ColladaAccessor* accessor = source->Accessors[0];
                    ColladaFloatArray* floatArray = nullptr;

                    for (int i = 0; i < source->FloatArrays.size(); i++) {
                        char* Id = source->FloatArrays[i]->Id;
                        if (!strcmp(accessor->Source+1, Id)) {
                            floatArray = source->FloatArrays[i];
                            break;
                        }
                    }

                    if (floatArray) {
                        int numWeights = floatArray->Count;

                        controller->NumWeights = numWeights;
                        controller->Weights = (float*)calloc(numWeights, sizeof(float));

                        for (int i = 0; i < numWeights; i++) {
                            controller->Weights[i] = floatArray->Contents[i];
                        }
                    }
                }
            }

            controller->NumInputs++;

            free(inputSource);
            free(semantic);
        } else if (MatchToken(grandchild->name, "vcount")) {
            XMLNode* contents = grandchild->children[0];
            if (contents->name.Length) {
                char* list = (char*)calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseIntegerArray(controller->Influences, list);
                free(list);
            }
        } else if (MatchToken(grandchild->name, "v")) {
            XMLNode* contents = grandchild->children[0];

            vector<int> weightInfo;
            if (contents->name.Length) {
                char* list = (char*)calloc(1, contents->name.Length + 1);
                memcpy(list, contents->name.Start, contents->name.Length);
                ParseIntegerArray(weightInfo, list);
                free(list);
            }

            VertexWeightInfo* vwInfo = (VertexWeightInfo*)calloc(controller->NumVertexWeights, sizeof(VertexWeightInfo));
            VertexWeightInfo* vw_p = vwInfo;

            int w = 0;

            for (int i = 0; i < controller->NumVertexWeights; i++) {
                int numInfluences = controller->Influences[i];

                vw_p->Influences = numInfluences;
                vw_p->JointIndices = (int*)calloc(numInfluences, sizeof(int));
                vw_p->WeightIndices = (int*)calloc(numInfluences, sizeof(int));

                int jIdx = 0, wIdx = 0;
                int jOff = controller->VWJointOffset;
                int wOff = controller->VWWeightsOffset;

                while (numInfluences--) {
                    int input = 0;
                    while (input < controller->NumInputs) {
                        switch (input++) {
                            case 0:
                                vw_p->JointIndices[jIdx++] = weightInfo[w++];
                                break;
                            case 1:
                                vw_p->WeightIndices[wIdx++] = weightInfo[w++];
                                break;
                            default:
                                break;
                        }
                    }
                }

                vw_p++;
            }

            controller->VWInfo = vwInfo;
        }
    }
}

PRIVATE STATIC void COLLADAReader::ParseController(XMLNode* parent, XMLNode* skin) {
    ColladaController* controller = new ColladaController;

    TokenToString(parent->attributes.Get("id"), &controller->Id);
    TokenToString(skin->attributes.Get("source"), &controller->Skin);

    controller->InvBindMatrixSource = nullptr;

    for (int i = 0; i < skin->children.size(); i++) {
        XMLNode* child = skin->children[i];

        if (MatchToken(child->name, "bind_shape_matrix")) {
            FloatStringToArray(child->children[0], controller->BindShapeMatrix, 16, 0.0f);
        } else if (MatchToken(child->name, "source")) {
            ColladaSource* source = ParseSource(controller->SourceList, child);

            for (int j = 0; j < source->Accessors.size(); j++) {
                ColladaAccessor* accessor = source->Accessors[j];
                if (!strcmp(accessor->Parameter, "JOINT"))
                    controller->JointList.push_back(source);
                else if (!strcmp(accessor->Parameter, "TRANSFORM"))
                    controller->MatrixList.push_back(source);
                else if (!strcmp(accessor->Parameter, "WEIGHT"))
                    controller->WeightList.push_back(source);
            }
        } else if (MatchToken(child->name, "joints")) {
            for (int j = 0; j < child->children.size(); j++) {
                XMLNode* grandchild = child->children[j];
                if (MatchToken(grandchild->name, "input")) {
                    char* semantic;
                    char* inputSource;

                    TokenToString(grandchild->attributes.Get("semantic"), &semantic);
                    TokenToString(grandchild->attributes.Get("source"), &inputSource);

                    if (!strcmp(semantic, "INV_BIND_MATRIX"))
                        controller->InvBindMatrixSource = FindSourceInList(controller->SourceList, (inputSource + 1));

                    free(inputSource);
                    free(semantic);
                }
            }
        } else if (MatchToken(child->name, "vertex_weights")) {
            ParseControllerWeights(controller, child);
        }
    }

    CurrentModel->Controllers.push_back(controller);
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

    Mesh* Mesh;
    int TotalVertexCount;
};

static void ConvertPrimitives(ConversionBuffer* convBuf, ColladaPreProc *preproc) {
    float *values;
    int numProc, curPrim;

    numProc = preproc->posIndices.size();
    convBuf->Positions.resize(numProc);

    for (int i = 0; i < numProc; i++) {
        curPrim = preproc->posIndices[i];
        values = preproc->Positions[curPrim].Values;

        convBuf->Positions[curPrim].X = (int)(values[0] * 0x100);
        convBuf->Positions[curPrim].Y = (int)(values[1] * 0x100);
        convBuf->Positions[curPrim].Z = (int)(values[2] * 0x100);

        convBuf->posIndices.push_back(curPrim);
    }

    numProc = preproc->nrmIndices.size();
    if (numProc) {
        convBuf->Mesh->VertexFlag |= VertexType_Normal;
        convBuf->Normals.resize(numProc);

        for (int i = 0; i < numProc; i++) {
            curPrim = preproc->nrmIndices[i];
            values = preproc->Normals[curPrim].Values;

            convBuf->Normals[curPrim].X = (int)(values[0] * 0x10000);
            convBuf->Normals[curPrim].Y = (int)(values[1] * 0x10000);
            convBuf->Normals[curPrim].Z = (int)(values[2] * 0x10000);

            convBuf->nrmIndices.push_back(curPrim);
        }
    }

    numProc = preproc->texIndices.size();
    if (numProc) {
        convBuf->Mesh->VertexFlag |= VertexType_UV;
        convBuf->UVs.resize(numProc);

        for (int i = 0; i < numProc; i++) {
            curPrim = preproc->texIndices[i];
            values = preproc->UVs[curPrim].Values;

            convBuf->UVs[curPrim].X = (int)(values[0] * 0x10000);
            convBuf->UVs[curPrim].Y = (int)(values[1] * 0x10000);

            convBuf->texIndices.push_back(curPrim);
        }
    }

    numProc = preproc->colIndices.size();
    if (numProc) {
        convBuf->Mesh->VertexFlag |= VertexType_Color;
        convBuf->Colors.resize(numProc);

        for (int i = 0; i < numProc; i++) {
            curPrim = preproc->colIndices[i];
            values = preproc->Colors[curPrim].Values;

            Uint32 r = (Uint32)(values[0] * 0xFF);
            Uint32 g = (Uint32)(values[1] * 0xFF);
            Uint32 b = (Uint32)(values[2] * 0xFF);
            convBuf->Colors[curPrim] = r << 16 | g << 8 | b;

            convBuf->colIndices.push_back(curPrim);
        }
    }
}

PRIVATE STATIC bool COLLADAReader::DoConversion(ColladaScene *scene, IModel* imodel) {
	int meshCount = scene->Meshes.size();
    if (!meshCount)
        return false;

    Mesh* meshes = (Mesh*)Memory::Malloc(meshCount * sizeof(Mesh));

    imodel->FrameCount = 1;
    imodel->MeshCount = meshCount;
    imodel->FaceVertexCount = 3;

    // Maximum mesh conversions: 32
    if (imodel->MeshCount > 32)
        imodel->MeshCount = 32;

    ConversionBuffer conversionBuffer[32];
    memset(&conversionBuffer, 0, sizeof(conversionBuffer));

    int vertexFlag = 0;
    int numVertices = 0;
    int totalVertices = 0;

    // Obtain data
    ConversionBuffer* conversion = conversionBuffer;
    for (int meshNum = 0; meshNum < imodel->MeshCount; meshNum++) {
        ColladaMesh* daeMesh = scene->Meshes[meshNum];

        conversion->Mesh = &meshes[meshNum];
        conversion->Mesh->VertexFlag = VertexType_Position;

        ConvertPrimitives(conversion, &daeMesh->Processed);

        numVertices = daeMesh->Processed.NumVertices;
        totalVertices += numVertices;

        vertexFlag |= conversion->Mesh->VertexFlag;
        conversion->TotalVertexCount = numVertices;
        conversion++;
    }

    // Create model
    if (vertexFlag & VertexType_Normal)
        imodel->PositionBuffer = (Vector3*)Memory::Calloc(totalVertices, 2 * sizeof(Vector3));
    else
        imodel->PositionBuffer = (Vector3*)Memory::Calloc(totalVertices, sizeof(Vector3));

    if (vertexFlag & VertexType_UV)
        imodel->UVBuffer = (Vector2*)Memory::Calloc(totalVertices, sizeof(Vector2));

    if (vertexFlag & VertexType_Color) {
        imodel->ColorBuffer = (Uint32*)Memory::Calloc(totalVertices, sizeof(Uint32));
        for (int i = 0; i < totalVertices; i++) {
            imodel->ColorBuffer[i] = 0xFFFFFFFF;
        }
    }

    imodel->VertexCount = totalVertices;
    imodel->TotalVertexIndexCount = totalVertices;

    // Read vertices
    int uvBufPos = 0, colorBufPos = 0, vertBufPos = 0, vertIdx = 0;
    conversion = conversionBuffer;
    for (int meshNum = 0; meshNum < imodel->MeshCount; meshNum++) {
        int meshVertCount = conversion->TotalVertexCount;
        Mesh* mesh = &meshes[meshNum];

        mesh->VertexIndexCount = meshVertCount;
        mesh->VertexIndexBuffer = (Sint16*)Memory::Calloc((meshVertCount + 1), sizeof(Sint16));

        for (int i = 0; i < meshVertCount; i++) {
            mesh->VertexIndexBuffer[i] = vertIdx++;
        }
        mesh->VertexIndexBuffer[meshVertCount] = -1;

        // Read UVs
        if (mesh->VertexFlag & VertexType_UV) {
            for (int i = 0; i < conversion->texIndices.size(); i++) {
                imodel->UVBuffer[uvBufPos+i] = conversion->UVs[conversion->texIndices[i]];
            }
            uvBufPos += meshVertCount;
        }
        // Read Colors
        if (mesh->VertexFlag & VertexType_Color) {
            for (int i = 0; i < conversion->colIndices.size(); i++) {
                imodel->ColorBuffer[colorBufPos+i] = conversion->Colors[conversion->colIndices[i]];
            }
            colorBufPos += meshVertCount;
        }
        // Read Normals & Positions
        if (mesh->VertexFlag & VertexType_Normal) {
            Vector3* vert = &imodel->PositionBuffer[vertBufPos << 1];
            for (int i = 0; i < conversion->nrmIndices.size(); i++, vertBufPos++) {
                *vert = conversion->Positions[conversion->posIndices[i]];
                vert++;

                *vert = conversion->Normals[conversion->nrmIndices[i]];
                vert++;
            }
        } else {
            Vector3* vert = &imodel->PositionBuffer[vertBufPos];
            for (int i = 0; i < conversion->posIndices.size(); i++, vertBufPos++) {
                *vert = conversion->Positions[conversion->posIndices[i]];
                vert++;
            }
        }

        conversion++;
    }

    // Read materials
    imodel->Materials = nullptr;
    imodel->MaterialCount = 0;

    for (int meshNum = 0; meshNum < imodel->MeshCount; meshNum++) {
        Mesh* mesh = &meshes[meshNum];
        ColladaMesh* daeMesh = scene->Meshes[meshNum];

        int faceCount = mesh->VertexIndexCount / imodel->FaceVertexCount;
        int facePointer = 0;
        mesh->FaceMaterials = (Uint8*)Memory::Calloc(faceCount, sizeof(Uint8));

        for (int i = 0; i < daeMesh->MaterialList.size(); i++) {
            Material* material = Material::New();
            ColladaMaterial* meshMaterial = daeMesh->MaterialList[i];

            if (meshMaterial->Effect) {
                ColladaEffect* effect = meshMaterial->Effect;

                // Copy material properties from its effect
                for (int j = 0; j < 4; j++) {
                    material->Specular[j] = effect->Specular.Color[j];
                    material->Ambient[j] = effect->Ambient.Color[j];
                    material->Emission[j] = effect->Emission.Color[j];
                    material->Diffuse[j] = effect->Diffuse.Color[j];
                }

                material->Shininess = effect->Shininess;
                material->Transparency = effect->Transparency;
                material->IndexOfRefraction = effect->IndexOfRefraction;

                // Assign texture from the diffuse sampler's surface
                ColladaSampler* sampler = effect->Diffuse.Sampler;
                if (sampler) {
                    ColladaSurface* surface = sampler->Surface;
                    if (surface && surface->Image && surface->Image->Path) {
                        material->Texture = new Image(surface->Image->Path);
                    }
                }
            }

            Uint8 materialNum = imodel->MaterialCount;
            int vertexCount = daeMesh->VertexCounts[i];
            int size = (vertexCount / imodel->FaceVertexCount);
            for (int j = 0; j < size; j++) {
                mesh->FaceMaterials[j + facePointer] = materialNum;
            }
            facePointer += size;

            // Set face materials
            imodel->MaterialCount++;
            imodel->Materials = (Material**)Memory::Realloc(imodel->Materials, imodel->MaterialCount * sizeof(Material*));
            imodel->Materials[materialNum] = material;
        }
    }

    imodel->Meshes = meshes;

    return true;
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

    ColladaModel daeModel;

    const char* matchAsset = "asset";
    const char* matchLibrary = "library_";

    if (modelXML->children[0])
        modelXML = modelXML->children[0];

    CurrentModel = &daeModel;

    int numNodes = modelXML->children.size();
    char* sectionName = nullptr;

    #define GET_SECTION_NAME(node) \
        Token sectionToken = node->name; \
        int sectionNameLength = sectionToken.Length; \
        sectionName = (char*)realloc(sectionName, sectionNameLength + 1); \
        sectionName[sectionNameLength] = '\0'; \
        memcpy(sectionName, sectionToken.Start, sectionNameLength);

    // Parse assets.
    for (int i = 0; i < numNodes; i++) {
        GET_SECTION_NAME(modelXML->children[i]);
        if (!strcmp(sectionName, matchAsset)) {
            ParseAsset(modelXML->children[i]);
        }
    }

    // Parse libraries.
    #define CHECK_LIBRARY(librarySection) \
        GET_SECTION_NAME(modelXML->children[i]); \
        if (strncmp(sectionName, matchLibrary, strlen(matchLibrary))) \
            continue; \
        if (strcmp(sectionName+strlen(matchLibrary), librarySection)) \
            continue; \
        XMLNode* library = modelXML->children[i];

    #define CHECK_LIBRARY_NODE(nodeName) MatchToken((node = library->children[i])->name, nodeName)
    #define CHECK_LIBRARY_NODE_CHILD(nodeName) \
        XMLNode* child = nullptr; \
        if (node->children.size()) \
            child = node->children[0]; \
        if (child == nullptr) { \
            Log::Print(Log::LOG_ERROR, "Library " nodeName " has no child! Ignored."); \
            continue; \
        }

    #define PARSE_LIBRARY_SECTION(librarySection, libraryName, parsefunc) \
        for (int i = 0; i < numNodes; i++) { \
            CHECK_LIBRARY(librarySection); \
            int numLibraryNodes = library->children.size(); \
            for (int i = 0; i < numLibraryNodes; i++) { \
                XMLNode* node; \
                if (CHECK_LIBRARY_NODE(libraryName)) { \
                    parsefunc(node); \
                } \
            } \
        }

    #define PARSE_LIBRARY_SECTION_WITH_CHILD(librarySection, libraryName, parsefunc) \
        for (int i = 0; i < numNodes; i++) { \
            CHECK_LIBRARY(librarySection); \
            int numLibraryNodes = library->children.size(); \
            for (int i = 0; i < numLibraryNodes; i++) { \
                XMLNode* node; \
                if (CHECK_LIBRARY_NODE(libraryName)) { \
                    CHECK_LIBRARY_NODE_CHILD(libraryName) \
                    parsefunc(node, child); \
                } \
            } \
        }

    // Parse images.
    PARSE_LIBRARY_SECTION_WITH_CHILD("images", "image", ParseImage);

    // Parse effects.
    PARSE_LIBRARY_SECTION_WITH_CHILD("effects", "effect", ParseEffect);

    // Parse materials.
    PARSE_LIBRARY_SECTION_WITH_CHILD("materials", "material", ParseMaterial);

    // Parse geometries.
    PARSE_LIBRARY_SECTION("geometries", "geometry", ParseGeometry);

    // Parse controllers.
    PARSE_LIBRARY_SECTION_WITH_CHILD("controllers", "controller", ParseController);

    // Parse visual scenes.
    PARSE_LIBRARY_SECTION("visual_scenes", "visual_scene", ParseVisualScene);

    #undef CHECK_LIBRARY
    #undef CHECK_LIBRARY_NODE
    #undef CHECK_LIBRARY_NODE_CHILD
    #undef PARSE_LIBRARY_SECTION
    #undef PARSE_LIBRARY_SECTION_WITH_CHILD

    free(sectionName);

    #undef GET_SECTION_NAME

    // Assign effects to materials
    AssignEffectsToMaterials();

    // Convert to IModel
    if (DoConversion(CurrentModel->Scenes[0], model))
        return model;

    return nullptr;
}
