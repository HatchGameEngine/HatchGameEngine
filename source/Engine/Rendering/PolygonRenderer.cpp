#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Enums.h>
#include <Engine/Rendering/3D.h>
#include <Engine/Rendering/ArrayBuffer.h>
#include <Engine/Rendering/VertexBuffer.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Math/Matrix4x4.h>

class PolygonRenderer {
public:
    ArrayBuffer*  ArrayBuf;
    VertexBuffer* VertexBuf;
    Matrix4x4*    ModelMatrix;
    Matrix4x4*    NormalMatrix;

    Uint32        CurrentColor;

    bool          ClipPolygonsByFrustum;
    int           NumFrustumPlanes;
    Frustum       ViewFrustum[NUM_FRUSTUM_PLANES];
};
#endif

#include <Engine/Rendering/PolygonRenderer.h>
#include <Engine/Rendering/ModelRenderer.h>
#include <Engine/Math/Clipper.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Graphics.h>

PUBLIC STATIC int PolygonRenderer::FaceSortFunction(const void *a, const void *b) {
    const FaceInfo* faceA = (const FaceInfo *)a;
    const FaceInfo* faceB = (const FaceInfo *)b;
    return faceB->Depth - faceA->Depth;
}

PUBLIC void PolygonRenderer::BuildFrustumPlanes(float nearClippingPlane, float farClippingPlane) {
    // Near
    ViewFrustum[0].Plane.Z = nearClippingPlane * 0x10000;
    ViewFrustum[0].Normal.Z = 0x10000;

    // Far
    ViewFrustum[1].Plane.Z = farClippingPlane * 0x10000;
    ViewFrustum[1].Normal.Z = -0x10000;

    NumFrustumPlanes = 2;
}

PUBLIC void PolygonRenderer::DrawPolygon3D(VertexAttribute* data, int vertexCount, int vertexFlag, Texture* texture, Uint32 colRGB) {
    ArrayBuffer* arrayBuffer = ArrayBuf;
    VertexBuffer* vertexBuffer = VertexBuf;
    Matrix4x4* modelMatrix = ModelMatrix;
    Matrix4x4* normalMatrix = NormalMatrix;

    Matrix4x4 mvpMatrix;
    if (arrayBuffer)
        Graphics::CalculateMVPMatrix(&mvpMatrix, modelMatrix, &arrayBuffer->ViewMatrix, &arrayBuffer->ProjectionMatrix);
    else
        Graphics::CalculateMVPMatrix(&mvpMatrix, modelMatrix, NULL, NULL);

    Uint32 arrayVertexCount = vertexBuffer->VertexCount;
    Uint32 maxVertexCount = arrayVertexCount + vertexCount;
    if (maxVertexCount > vertexBuffer->Capacity)
        vertexBuffer->Resize(maxVertexCount);

    VertexAttribute* arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];
    VertexAttribute* vertex = arrayVertexBuffer;
    int numVertices = vertexCount;
    int numBehind = 0;
    while (numVertices--) {
        // Calculate position
        APPLY_MAT4X4(vertex->Position, data->Position, mvpMatrix.Values);

        // Calculate normals
        if (vertexFlag & VertexType_Normal) {
            if (normalMatrix) {
                APPLY_MAT4X4(vertex->Normal, data->Normal, normalMatrix->Values);
            }
            else {
                COPY_NORMAL(vertex->Normal, data->Normal);
            }
        }
        else
            vertex->Normal.X = vertex->Normal.Y = vertex->Normal.Z = vertex->Normal.W = 0;

        if (vertexFlag & VertexType_Color)
            vertex->Color = ColorUtils::Multiply(data->Color, colRGB);
        else
            vertex->Color = colRGB;

        if (vertexFlag & VertexType_UV)
            vertex->UV = data->UV;

        if (arrayBuffer && vertex->Position.Z <= 0x10000)
            numBehind++;

        vertex++;
        data++;
    }

    // Don't clip polygons if drawing into a vertex buffer, since the vertices are not in clip space
    if (arrayBuffer) {
        // Check if the polygon is at least partially inside the frustum
        if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount))
            return;

        // Vertices are now in clip space, which means that the polygon can be frustum clipped
        if (ClipPolygonsByFrustum) {
            PolygonClipBuffer clipper;

            vertexCount = ClipPolygon(clipper, arrayVertexBuffer, vertexCount);
            if (vertexCount == 0)
                return;

            Uint32 maxVertexCount = arrayVertexCount + vertexCount;
            if (maxVertexCount > vertexBuffer->Capacity) {
                vertexBuffer->Resize(maxVertexCount);
                arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];
            }

            CopyVertices(clipper.Buffer, arrayVertexBuffer, vertexCount);
        } else if (numBehind != 0)
            return;
    }

    FaceInfo* faceInfoItem = &vertexBuffer->FaceInfoBuffer[vertexBuffer->FaceCount];
    faceInfoItem->NumVertices = vertexCount;
    faceInfoItem->SetMaterial(texture);
    if (arrayBuffer)
        faceInfoItem->SetBlendState(Graphics::GetBlendState());

    vertexBuffer->VertexCount += vertexCount;
    vertexBuffer->FaceCount++;
}
PUBLIC void PolygonRenderer::DrawSceneLayer3D(SceneLayer* layer, int sx, int sy, int sw, int sh, Uint32 colRGB) {
    int vertexCountPerFace = 4;
    int tileSize = 16;

    ArrayBuffer* arrayBuffer = ArrayBuf;
    VertexBuffer* vertexBuffer = VertexBuf;
    Matrix4x4* modelMatrix = ModelMatrix;
    Matrix4x4* normalMatrix = NormalMatrix;

    Matrix4x4 mvpMatrix;
    if (arrayBuffer)
        Graphics::CalculateMVPMatrix(&mvpMatrix, modelMatrix, &arrayBuffer->ViewMatrix, &arrayBuffer->ProjectionMatrix);
    else
        Graphics::CalculateMVPMatrix(&mvpMatrix, modelMatrix, NULL, NULL);

    int arrayVertexCount = vertexBuffer->VertexCount;
    int arrayFaceCount = vertexBuffer->FaceCount;

    vector<AnimFrame> animFrames;
    vector<Texture*> textureSources;
    animFrames.resize(Scene::TileSpriteInfos.size());
    textureSources.resize(Scene::TileSpriteInfos.size());
    for (size_t i = 0; i < Scene::TileSpriteInfos.size(); i++) {
        TileSpriteInfo info = Scene::TileSpriteInfos[i];
        animFrames[i] = info.Sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
        textureSources[i] = info.Sprite->Spritesheets[animFrames[i].SheetNumber];
    }

    Uint32 totalVertexCount = 0;
    for (int y = sy; y < sh; y++) {
        for (int x = sx; x < sw; x++) {
            Uint32 tileID = (Uint32)(layer->Tiles[x + (y << layer->WidthInBits)] & TILE_IDENT_MASK);
            if (tileID != Scene::EmptyTile && tileID < Scene::TileSpriteInfos.size())
                totalVertexCount += vertexCountPerFace;
        }
    }

    Uint32 maxVertexCount = arrayVertexCount + totalVertexCount;
    if (maxVertexCount > vertexBuffer->Capacity)
        vertexBuffer->Resize(maxVertexCount);

    FaceInfo* faceInfoItem = &vertexBuffer->FaceInfoBuffer[arrayFaceCount];
    VertexAttribute* arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];

    for (int y = sy, destY = 0; y < sh; y++, destY++) {
        for (int x = sx, destX = 0; x < sw; x++, destX++) {
            Uint32 tileAtPos = layer->Tiles[x + (y << layer->WidthInBits)];
            Uint32 tileID = tileAtPos & TILE_IDENT_MASK;
            if (tileID == Scene::EmptyTile || tileID >= Scene::TileSpriteInfos.size())
                continue;

            // 0--1
            // |  |
            // 3--2
            VertexAttribute data[4];
            AnimFrame frameStr = animFrames[tileID];
            Texture* texture = textureSources[tileID];

            Sint64 textureWidth = FP16_TO(texture->Width);
            Sint64 textureHeight = FP16_TO(texture->Height);

            float uv_left   = (float)frameStr.X;
            float uv_right  = (float)(frameStr.X + frameStr.Width);
            float uv_top    = (float)frameStr.Y;
            float uv_bottom = (float)(frameStr.Y + frameStr.Height);

            float left_u, right_u, top_v, bottom_v;
            int flipX = tileAtPos & TILE_FLIPX_MASK;
            int flipY = tileAtPos & TILE_FLIPY_MASK;

            if (flipX) {
                left_u  = uv_right;
                right_u = uv_left;
            } else {
                left_u  = uv_left;
                right_u = uv_right;
            }

            if (flipY) {
                top_v    = uv_bottom;
                bottom_v = uv_top;
            } else {
                top_v    = uv_top;
                bottom_v = uv_bottom;
            }

            data[0].Position.X = FP16_TO(destX * tileSize);
            data[0].Position.Z = FP16_TO(destY * tileSize);
            data[0].Position.Y = 0;
            data[0].UV.X       = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
            data[0].UV.Y       = FP16_DIVIDE(FP16_TO(top_v), textureHeight);
            data[0].Normal.X   = data[0].Normal.Y = data[0].Normal.Z = data[0].Normal.W = 0;

            data[1].Position.X = data[0].Position.X + FP16_TO(tileSize);
            data[1].Position.Z = data[0].Position.Z;
            data[1].Position.Y = 0;
            data[1].UV.X       = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
            data[1].UV.Y       = FP16_DIVIDE(FP16_TO(top_v), textureHeight);
            data[1].Normal.X   = data[1].Normal.Y = data[1].Normal.Z = data[1].Normal.W = 0;

            data[2].Position.X = data[1].Position.X;
            data[2].Position.Z = data[1].Position.Z + FP16_TO(tileSize);
            data[2].Position.Y = 0;
            data[2].UV.X       = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
            data[2].UV.Y       = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);
            data[2].Normal.X   = data[2].Normal.Y = data[2].Normal.Z = data[2].Normal.W = 0;

            data[3].Position.X = data[0].Position.X;
            data[3].Position.Z = data[2].Position.Z;
            data[3].Position.Y = 0;
            data[3].UV.X       = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
            data[3].UV.Y       = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);
            data[3].Normal.X   = data[3].Normal.Y = data[3].Normal.Z = data[3].Normal.W = 0;

            VertexAttribute* vertex = arrayVertexBuffer;
            int vertexIndex = 0;
            while (vertexIndex < vertexCountPerFace) {
                // Calculate position
                APPLY_MAT4X4(vertex->Position, data[vertexIndex].Position, mvpMatrix.Values);

                // Calculate normals
                if (normalMatrix) {
                    APPLY_MAT4X4(vertex->Normal, data[vertexIndex].Normal, normalMatrix->Values);
                }
                else {
                    COPY_NORMAL(vertex->Normal, data[vertexIndex].Normal);
                }

                vertex->UV = data[vertexIndex].UV;
                vertex->Color = colRGB;

                vertex++;
                vertexIndex++;
            }

            Uint32 vertexCount = vertexCountPerFace;
            if (arrayBuffer) {
                // Check if the polygon is at least partially inside the frustum
                if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount))
                    continue;

                // Vertices are now in clip space, which means that the polygon can be frustum clipped
                if (ClipPolygonsByFrustum) {
                    PolygonClipBuffer clipper;

                    vertexCount = ClipPolygon(clipper, arrayVertexBuffer, vertexCount);
                    if (vertexCount == 0)
                        continue;

                    Uint32 maxVertexCount = arrayVertexCount + vertexCount;
                    if (maxVertexCount > vertexBuffer->Capacity) {
                        vertexBuffer->Resize(maxVertexCount);
                        faceInfoItem = &vertexBuffer->FaceInfoBuffer[arrayFaceCount];
                        arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];
                    }

                    CopyVertices(clipper.Buffer, arrayVertexBuffer, vertexCount);
                }
            }

            faceInfoItem->SetMaterial(texture);
            faceInfoItem->SetBlendState(Graphics::GetBlendState());
            faceInfoItem->NumVertices = vertexCount;
            faceInfoItem++;
            arrayVertexCount += vertexCount;
            arrayVertexBuffer += vertexCount;
            arrayFaceCount++;
        }
    }

    vertexBuffer->VertexCount = arrayVertexCount;
    vertexBuffer->FaceCount = arrayFaceCount;
}
PUBLIC void PolygonRenderer::DrawModel(IModel* model, Uint16 animation, Uint32 frame) {
    if (animation < 0 || frame < 0)
        return;
    else if (model->AnimationCount > 0 && animation >= model->AnimationCount)
        return;

    ArrayBuffer* arrayBuffer = ArrayBuf;
    VertexBuffer* vertexBuffer = VertexBuf;

    Matrix4x4* viewMatrix = nullptr;
    Matrix4x4* projMatrix = nullptr;

    if (arrayBuffer) {
        viewMatrix = &arrayBuffer->ViewMatrix;
        projMatrix = &arrayBuffer->ProjectionMatrix;
    }

    Uint32 maxVertexCount = vertexBuffer->VertexCount + model->VertexIndexCount;
    if (maxVertexCount > vertexBuffer->Capacity)
        vertexBuffer->Resize(maxVertexCount);

    ModelRenderer rend = ModelRenderer(this);

    rend.CurrentColor = CurrentColor;
    rend.ClipFaces = arrayBuffer != nullptr;
    rend.SetMatrices(ModelMatrix, viewMatrix, projMatrix, NormalMatrix);
    rend.DrawModel(model, animation, frame);
}
PUBLIC void PolygonRenderer::DrawModelSkinned(IModel* model, Uint16 armature) {
    if (model->UseVertexAnimation) {
        DrawModel(model, 0, 0);
        return;
    }

    if (armature >= model->ArmatureCount)
        return;

    ArrayBuffer* arrayBuffer = ArrayBuf;
    VertexBuffer* vertexBuffer = VertexBuf;

    Matrix4x4* viewMatrix = nullptr;
    Matrix4x4* projMatrix = nullptr;

    if (arrayBuffer) {
        viewMatrix = &arrayBuffer->ViewMatrix;
        projMatrix = &arrayBuffer->ProjectionMatrix;
    }

    Uint32 maxVertexCount = vertexBuffer->VertexCount + model->VertexIndexCount;
    if (maxVertexCount > vertexBuffer->Capacity)
        vertexBuffer->Resize(maxVertexCount);

    ModelRenderer rend = ModelRenderer(this);

    rend.CurrentColor = CurrentColor;
    rend.ClipFaces = arrayBuffer != nullptr;
    rend.ArmaturePtr = model->ArmatureList[armature];
    rend.SetMatrices(ModelMatrix, viewMatrix, projMatrix, NormalMatrix);
    rend.DrawModel(model, 0);
}
PUBLIC void PolygonRenderer::DrawVertexBuffer() {
    ArrayBuffer* arrayBuffer = ArrayBuf;
    VertexBuffer* vertexBuffer = VertexBuf;
    Matrix4x4* normalMatrix = NormalMatrix;

    Matrix4x4 mvpMatrix;
    Graphics::CalculateMVPMatrix(&mvpMatrix, ModelMatrix, &arrayBuffer->ViewMatrix, &arrayBuffer->ProjectionMatrix);

    // destination
    VertexBuffer* destVertexBuffer = arrayBuffer->Buffer;
    int arrayFaceCount = destVertexBuffer->FaceCount;
    int arrayVertexCount = destVertexBuffer->VertexCount;

    // source
    int srcFaceCount = vertexBuffer->FaceCount;
    int srcVertexCount = vertexBuffer->VertexCount;
    if (!srcFaceCount || !srcVertexCount)
        return;

    Uint32 maxVertexCount = arrayVertexCount + srcVertexCount;
    if (maxVertexCount > destVertexBuffer->Capacity)
        destVertexBuffer->Resize(maxVertexCount);

    FaceInfo* faceInfoItem = &destVertexBuffer->FaceInfoBuffer[arrayFaceCount];
    VertexAttribute* arrayVertexBuffer = &destVertexBuffer->Vertices[arrayVertexCount];
    VertexAttribute* arrayVertexItem = arrayVertexBuffer;

    // Copy the vertices into the vertex buffer
    VertexAttribute* srcVertexItem = &vertexBuffer->Vertices[0];

    for (int f = 0; f < srcFaceCount; f++) {
        FaceInfo* srcFaceInfoItem = &vertexBuffer->FaceInfoBuffer[f];
        int vertexCount = srcFaceInfoItem->NumVertices;
        int vertexCountPerFace = vertexCount;
        while (vertexCountPerFace--) {
            APPLY_MAT4X4(arrayVertexItem->Position, srcVertexItem->Position, mvpMatrix.Values);

            if (normalMatrix) {
                APPLY_MAT4X4(arrayVertexItem->Normal, srcVertexItem->Normal, normalMatrix->Values);
            }
            else {
                COPY_NORMAL(arrayVertexItem->Normal, srcVertexItem->Normal);
            }

            arrayVertexItem->Color = srcVertexItem->Color;
            arrayVertexItem->UV = srcVertexItem->UV;
            arrayVertexItem++;
            srcVertexItem++;
        }

        arrayVertexItem = arrayVertexBuffer;

        // Check if the polygon is at least partially inside the frustum
        if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount))
            continue;

        // Vertices are now in clip space, which means that the polygon can be frustum clipped
        if (ClipPolygonsByFrustum) {
            PolygonClipBuffer clipper;

            vertexCount = ClipPolygon(clipper, arrayVertexBuffer, vertexCount);
            if (vertexCount == 0)
                continue;

            Uint32 maxVertexCount = arrayVertexCount + vertexCount;
            if (maxVertexCount > destVertexBuffer->Capacity) {
                destVertexBuffer->Resize(maxVertexCount);
                faceInfoItem = &destVertexBuffer->FaceInfoBuffer[arrayFaceCount];
                arrayVertexBuffer = &destVertexBuffer->Vertices[arrayVertexCount];
            }

            CopyVertices(clipper.Buffer, arrayVertexBuffer, vertexCount);
        }

        faceInfoItem->UseMaterial = srcFaceInfoItem->UseMaterial;
        if (faceInfoItem->UseMaterial)
            faceInfoItem->MaterialInfo = srcFaceInfoItem->MaterialInfo;
        faceInfoItem->NumVertices = vertexCount;
        faceInfoItem->SetBlendState(Graphics::GetBlendState());
        faceInfoItem++;
        srcFaceInfoItem++;

        arrayVertexCount += vertexCount;
        arrayVertexBuffer += vertexCount;
        arrayVertexItem = arrayVertexBuffer;
        arrayFaceCount++;
    }

    destVertexBuffer->VertexCount = arrayVertexCount;
    destVertexBuffer->FaceCount = arrayFaceCount;
}
PUBLIC int PolygonRenderer::ClipPolygon(PolygonClipBuffer& clipper, VertexAttribute* input, int numVertices) {
    clipper.NumPoints = 0;
    clipper.MaxPoints = MAX_POLYGON_VERTICES;

    int numOutVertices = Clipper::FrustumClip(&clipper, ViewFrustum, NumFrustumPlanes, input, numVertices);
    if (numOutVertices < 3 || numOutVertices >= MAX_POLYGON_VERTICES)
        return 0;

    return numOutVertices;
}
PUBLIC STATIC bool PolygonRenderer::CheckPolygonVisible(VertexAttribute* vertex, int vertexCount) {
    int numBehind[3] = { 0, 0, 0 };
    int numVertices = vertexCount;
    while (numVertices--) {
        if (vertex->Position.X < -vertex->Position.W || vertex->Position.X > vertex->Position.W)
            numBehind[0]++;
        if (vertex->Position.Y < -vertex->Position.W || vertex->Position.Y > vertex->Position.W)
            numBehind[1]++;
        if (vertex->Position.Z <= 0)
            numBehind[2]++;

        vertex++;
    }

    if (numBehind[0] == vertexCount || numBehind[1] == vertexCount || numBehind[2] == vertexCount)
        return false;

    return true;
}
PUBLIC STATIC void PolygonRenderer::CopyVertices(VertexAttribute* buffer, VertexAttribute* output, int numVertices) {
    while (numVertices--) {
        COPY_VECTOR(output->Position, buffer->Position);
        COPY_NORMAL(output->Normal, buffer->Normal);
        output->Color = buffer->Color;
        output->UV = buffer->UV;
        output++;
        buffer++;
    }
}
