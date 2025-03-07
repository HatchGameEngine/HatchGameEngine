#include <Engine/Graphics.h>
#include <Engine/Math/Clipper.h>
#include <Engine/Rendering/Material.h>
#include <Engine/Rendering/ModelRenderer.h>
#include <Engine/Rendering/PolygonRenderer.h>
#include <Engine/Rendering/Texture.h>

int PolygonRenderer::FaceSortFunction(const void* a, const void* b) {
	const FaceInfo* faceA = (const FaceInfo*)a;
	const FaceInfo* faceB = (const FaceInfo*)b;
	return faceB->Depth - faceA->Depth;
}

void PolygonRenderer::BuildFrustumPlanes(float nearClippingPlane, float farClippingPlane) {
	// Near
	ViewFrustum[0].Plane.Z = nearClippingPlane * 0x10000;
	ViewFrustum[0].Normal.Z = 0x10000;

	// Far
	ViewFrustum[1].Plane.Z = farClippingPlane * 0x10000;
	ViewFrustum[1].Normal.Z = -0x10000;

	NumFrustumPlanes = 2;
}

bool PolygonRenderer::SetBuffers() {
	VertexBuf = nullptr;
	ScenePtr = nullptr;
	ViewMatrix = nullptr;
	ProjectionMatrix = nullptr;

	if (Graphics::CurrentVertexBuffer != -1) {
		VertexBuf = Graphics::VertexBuffers[Graphics::CurrentVertexBuffer];
		if (VertexBuf == nullptr) {
			return false;
		}
	}
	else {
		if (Graphics::CurrentScene3D == -1) {
			return false;
		}

		Scene3D* scene = &Graphics::Scene3Ds[Graphics::CurrentScene3D];
		if (!scene->Initialized) {
			return false;
		}

		VertexBuf = scene->Buffer;
		ScenePtr = scene;
		ViewMatrix = &scene->ViewMatrix;
		ProjectionMatrix = &scene->ProjectionMatrix;
	}

	return true;
}

void PolygonRenderer::DrawPolygon3D(VertexAttribute* data,
	int vertexCount,
	int vertexFlag,
	Texture* texture) {
	VertexBuffer* vertexBuffer = VertexBuf;
	Uint32 colRGB = CurrentColor;

	Matrix4x4 mvpMatrix;
	if (DoProjection) {
		Graphics::CalculateMVPMatrix(&mvpMatrix, ModelMatrix, ViewMatrix, ProjectionMatrix);
	}
	else {
		Graphics::CalculateMVPMatrix(&mvpMatrix, ModelMatrix, nullptr, nullptr);
	}

	Uint32 arrayVertexCount = vertexBuffer->VertexCount;
	Uint32 maxVertexCount = arrayVertexCount + vertexCount;
	if (maxVertexCount > vertexBuffer->Capacity) {
		vertexBuffer->Resize(maxVertexCount);
	}

	VertexAttribute* arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];
	VertexAttribute* vertex = arrayVertexBuffer;
	int numVertices = vertexCount;
	int numBehind = 0;
	while (numVertices--) {
		// Calculate position
		APPLY_MAT4X4(vertex->Position, data->Position, mvpMatrix.Values);

		// Calculate normals
		if (vertexFlag & VertexType_Normal) {
			if (NormalMatrix) {
				APPLY_MAT4X4(vertex->Normal, data->Normal, NormalMatrix->Values);
			}
			else {
				COPY_NORMAL(vertex->Normal, data->Normal);
			}
		}
		else {
			vertex->Normal.X = vertex->Normal.Y = vertex->Normal.Z = vertex->Normal.W =
				0;
		}

		if (vertexFlag & VertexType_Color) {
			vertex->Color = ColorUtils::Multiply(data->Color, colRGB);
		}
		else {
			vertex->Color = colRGB;
		}

		if (vertexFlag & VertexType_UV) {
			vertex->UV = data->UV;
		}

		if (DoClipping && vertex->Position.Z <= 0x10000) {
			numBehind++;
		}

		vertex++;
		data++;
	}

	// Don't clip polygons if drawing into a vertex buffer, since
	// the vertices are not in clip space
	if (DoClipping) {
		// Check if the polygon is at least partially inside
		// the frustum
		if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount)) {
			return;
		}

		// Vertices are now in clip space, which means that the
		// polygon can be frustum clipped
		if (ClipPolygonsByFrustum) {
			PolygonClipBuffer clipper;

			vertexCount = ClipPolygon(clipper, arrayVertexBuffer, vertexCount);
			if (vertexCount == 0) {
				return;
			}

			Uint32 maxVertexCount = arrayVertexCount + vertexCount;
			if (maxVertexCount > vertexBuffer->Capacity) {
				vertexBuffer->Resize(maxVertexCount);
				arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];
			}

			CopyVertices(clipper.Buffer, arrayVertexBuffer, vertexCount);
		}
		else if (numBehind != 0) {
			return;
		}
	}

	FaceInfo* face = &vertexBuffer->FaceInfoBuffer[vertexBuffer->FaceCount];
	face->DrawMode = DrawMode;
	face->NumVertices = vertexCount;
	face->CullMode = FaceCullMode;
	face->SetMaterial(texture);
	face->SetBlendState(Graphics::GetBlendState());

	vertexBuffer->VertexCount += vertexCount;
	vertexBuffer->FaceCount++;
}
void PolygonRenderer::DrawSceneLayer3D(SceneLayer* layer, int sx, int sy, int sw, int sh) {
	static vector<AnimFrame> animFrames;
	static vector<Texture*> textureSources;
	animFrames.reserve(Scene::TileSpriteInfos.size());
	textureSources.reserve(Scene::TileSpriteInfos.size());

	int vertexCountPerFace = 4;
	int tileWidth = Scene::TileWidth;
	int tileHeight = Scene::TileHeight;
	Uint32 colRGB = CurrentColor;

	Matrix4x4 mvpMatrix;
	if (DoProjection) {
		Graphics::CalculateMVPMatrix(&mvpMatrix, ModelMatrix, ViewMatrix, ProjectionMatrix);
	}
	else {
		Graphics::CalculateMVPMatrix(&mvpMatrix, ModelMatrix, nullptr, nullptr);
	}

	VertexBuffer* vertexBuffer = VertexBuf;
	int arrayVertexCount = vertexBuffer->VertexCount;
	int arrayFaceCount = vertexBuffer->FaceCount;

	for (size_t i = 0; i < Scene::TileSpriteInfos.size(); i++) {
		TileSpriteInfo& info = Scene::TileSpriteInfos[i];
		animFrames[i] =
			info.Sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
		textureSources[i] = info.Sprite->Spritesheets[animFrames[i].SheetNumber];
	}

	Uint32 totalVertexCount = 0;
	for (int y = sy; y < sh; y++) {
		for (int x = sx; x < sw; x++) {
			Uint32 tileID = (Uint32)(layer->Tiles[x + (y << layer->WidthInBits)] &
				TILE_IDENT_MASK);
			if (tileID != Scene::EmptyTile && tileID < Scene::TileSpriteInfos.size()) {
				totalVertexCount += vertexCountPerFace;
			}
		}
	}

	Uint32 maxVertexCount = arrayVertexCount + totalVertexCount;
	if (maxVertexCount > vertexBuffer->Capacity) {
		vertexBuffer->Resize(maxVertexCount);
	}

	FaceInfo* faceInfoItem = &vertexBuffer->FaceInfoBuffer[arrayFaceCount];
	VertexAttribute* arrayVertexBuffer = &vertexBuffer->Vertices[arrayVertexCount];

	for (int y = sy, destY = 0; y < sh; y++, destY++) {
		for (int x = sx, destX = 0; x < sw; x++, destX++) {
			Uint32 tileAtPos = layer->Tiles[x + (y << layer->WidthInBits)];
			Uint32 tileID = tileAtPos & TILE_IDENT_MASK;
			if (tileID == Scene::EmptyTile || tileID >= Scene::TileSpriteInfos.size()) {
				continue;
			}

			// 0--1
			// |  |
			// 3--2
			VertexAttribute data[4];
			AnimFrame& frameStr = animFrames[tileID];
			Texture* texture = textureSources[tileID];

			Sint64 textureWidth = FP16_TO(texture->Width);
			Sint64 textureHeight = FP16_TO(texture->Height);

			float uv_left = (float)frameStr.X;
			float uv_right = (float)(frameStr.X + frameStr.Width);
			float uv_top = (float)frameStr.Y;
			float uv_bottom = (float)(frameStr.Y + frameStr.Height);

			float left_u, right_u, top_v, bottom_v;
			int flipX = tileAtPos & TILE_FLIPX_MASK;
			int flipY = tileAtPos & TILE_FLIPY_MASK;

			if (flipX) {
				left_u = uv_right;
				right_u = uv_left;
			}
			else {
				left_u = uv_left;
				right_u = uv_right;
			}

			if (flipY) {
				top_v = uv_bottom;
				bottom_v = uv_top;
			}
			else {
				top_v = uv_top;
				bottom_v = uv_bottom;
			}

			data[0].Position.X = FP16_TO(destX * tileWidth);
			data[0].Position.Z = FP16_TO(destY * tileHeight);
			data[0].Position.Y = 0;
			data[0].UV.X = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
			data[0].UV.Y = FP16_DIVIDE(FP16_TO(top_v), textureHeight);
			data[0].Normal.X = data[0].Normal.Y = data[0].Normal.Z = data[0].Normal.W =
				0;

			data[1].Position.X = data[0].Position.X + FP16_TO(tileWidth);
			data[1].Position.Z = data[0].Position.Z;
			data[1].Position.Y = 0;
			data[1].UV.X = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
			data[1].UV.Y = FP16_DIVIDE(FP16_TO(top_v), textureHeight);
			data[1].Normal.X = data[1].Normal.Y = data[1].Normal.Z = data[1].Normal.W =
				0;

			data[2].Position.X = data[1].Position.X;
			data[2].Position.Z = data[1].Position.Z + FP16_TO(tileHeight);
			data[2].Position.Y = 0;
			data[2].UV.X = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
			data[2].UV.Y = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);
			data[2].Normal.X = data[2].Normal.Y = data[2].Normal.Z = data[2].Normal.W =
				0;

			data[3].Position.X = data[0].Position.X;
			data[3].Position.Z = data[2].Position.Z;
			data[3].Position.Y = 0;
			data[3].UV.X = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
			data[3].UV.Y = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);
			data[3].Normal.X = data[3].Normal.Y = data[3].Normal.Z = data[3].Normal.W =
				0;

			VertexAttribute* vertex = arrayVertexBuffer;
			int vertexIndex = 0;
			while (vertexIndex < vertexCountPerFace) {
				// Calculate position
				APPLY_MAT4X4(vertex->Position,
					data[vertexIndex].Position,
					mvpMatrix.Values);

				// Calculate normals
				if (NormalMatrix) {
					APPLY_MAT4X4(vertex->Normal,
						data[vertexIndex].Normal,
						NormalMatrix->Values);
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
			if (DoClipping) {
				// Check if the polygon is at least
				// partially inside the frustum
				if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount)) {
					continue;
				}

				// Vertices are now in clip space,
				// which means that the polygon can be
				// frustum clipped
				if (ClipPolygonsByFrustum) {
					PolygonClipBuffer clipper;

					vertexCount = ClipPolygon(
						clipper, arrayVertexBuffer, vertexCount);
					if (vertexCount == 0) {
						continue;
					}

					Uint32 maxVertexCount = arrayVertexCount + vertexCount;
					if (maxVertexCount > vertexBuffer->Capacity) {
						vertexBuffer->Resize(maxVertexCount);
						faceInfoItem =
							&vertexBuffer
								 ->FaceInfoBuffer[arrayFaceCount];
						arrayVertexBuffer =
							&vertexBuffer->Vertices[arrayVertexCount];
					}

					CopyVertices(
						clipper.Buffer, arrayVertexBuffer, vertexCount);
				}
			}

			faceInfoItem->DrawMode = DrawMode;
			faceInfoItem->CullMode = FaceCullMode;
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
void PolygonRenderer::DrawModel(IModel* model, Uint16 animation, Uint32 frame) {
	if (animation < 0 || frame < 0) {
		return;
	}
	else if (model->Animations.size() > 0 && animation >= model->Animations.size()) {
		return;
	}

	Uint32 maxVertexCount = VertexBuf->VertexCount + model->VertexIndexCount;
	if (maxVertexCount > VertexBuf->Capacity) {
		VertexBuf->Resize(maxVertexCount);
	}

	ModelRenderer rend = ModelRenderer(this);

	rend.DrawMode = ScenePtr != nullptr ? ScenePtr->DrawMode : 0;
	rend.FaceCullMode = ScenePtr != nullptr ? ScenePtr->FaceCullMode : FaceCull_None;
	rend.CurrentColor = CurrentColor;
	rend.DoProjection = DoProjection;
	rend.ClipFaces = DoProjection;
	rend.SetMatrices(ModelMatrix, ViewMatrix, ProjectionMatrix, NormalMatrix);
	rend.DrawModel(model, animation, frame);
}
void PolygonRenderer::DrawModelSkinned(IModel* model, Uint16 armature) {
	if (model->UseVertexAnimation) {
		DrawModel(model, 0, 0);
		return;
	}

	if (armature >= model->Armatures.size()) {
		return;
	}

	Uint32 maxVertexCount = VertexBuf->VertexCount + model->VertexIndexCount;
	if (maxVertexCount > VertexBuf->Capacity) {
		VertexBuf->Resize(maxVertexCount);
	}

	ModelRenderer rend = ModelRenderer(this);

	rend.DrawMode = ScenePtr != nullptr ? ScenePtr->DrawMode : 0;
	rend.FaceCullMode = ScenePtr != nullptr ? ScenePtr->FaceCullMode : FaceCull_None;
	rend.CurrentColor = CurrentColor;
	rend.DoProjection = DoProjection;
	rend.ClipFaces = DoProjection;
	rend.ArmaturePtr = model->Armatures[armature];
	rend.SetMatrices(ModelMatrix, ViewMatrix, ProjectionMatrix, NormalMatrix);
	rend.DrawModel(model, 0, 0);
}
void PolygonRenderer::DrawVertexBuffer() {
	Matrix4x4 mvpMatrix;
	if (DoProjection) {
		Graphics::CalculateMVPMatrix(&mvpMatrix, ModelMatrix, ViewMatrix, ProjectionMatrix);
	}
	else {
		Graphics::CalculateMVPMatrix(&mvpMatrix, ModelMatrix, nullptr, nullptr);
	}

	// destination
	VertexBuffer* destVertexBuffer = ScenePtr->Buffer;
	int arrayFaceCount = destVertexBuffer->FaceCount;
	int arrayVertexCount = destVertexBuffer->VertexCount;

	// source
	Uint32 maxVertexCount = arrayVertexCount + VertexBuf->VertexCount;
	if (maxVertexCount > destVertexBuffer->Capacity) {
		destVertexBuffer->Resize(maxVertexCount + 256);
	}

	FaceInfo* faceInfoItem = &destVertexBuffer->FaceInfoBuffer[arrayFaceCount];
	VertexAttribute* arrayVertexBuffer = &destVertexBuffer->Vertices[arrayVertexCount];
	VertexAttribute* arrayVertexItem = arrayVertexBuffer;

	// Copy the vertices into the vertex buffer
	VertexAttribute* srcVertexItem = &VertexBuf->Vertices[0];

	for (int f = 0; f < VertexBuf->FaceCount; f++) {
		FaceInfo* srcFaceInfoItem = &VertexBuf->FaceInfoBuffer[f];
		int vertexCount = srcFaceInfoItem->NumVertices;
		int vertexCountPerFace = vertexCount;
		while (vertexCountPerFace--) {
			APPLY_MAT4X4(arrayVertexItem->Position,
				srcVertexItem->Position,
				mvpMatrix.Values);

			if (NormalMatrix) {
				APPLY_MAT4X4(arrayVertexItem->Normal,
					srcVertexItem->Normal,
					NormalMatrix->Values);
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

		if (DoClipping) {
			// Check if the polygon is at least partially
			// inside the frustum
			if (!CheckPolygonVisible(arrayVertexBuffer, vertexCount)) {
				continue;
			}

			// Vertices are now in clip space, which means
			// that the polygon can be frustum clipped
			if (ClipPolygonsByFrustum) {
				PolygonClipBuffer clipper;

				vertexCount = ClipPolygon(clipper, arrayVertexBuffer, vertexCount);
				if (vertexCount == 0) {
					continue;
				}

				Uint32 maxVertexCount = arrayVertexCount + vertexCount;
				if (maxVertexCount > destVertexBuffer->Capacity) {
					destVertexBuffer->Resize(maxVertexCount + 256);
					faceInfoItem =
						&destVertexBuffer->FaceInfoBuffer[arrayFaceCount];
					arrayVertexBuffer =
						&destVertexBuffer->Vertices[arrayVertexCount];
				}

				CopyVertices(clipper.Buffer, arrayVertexBuffer, vertexCount);
			}
		}

		faceInfoItem->DrawMode = DrawMode;
		faceInfoItem->CullMode = FaceCullMode;
		faceInfoItem->UseMaterial = srcFaceInfoItem->UseMaterial;
		if (faceInfoItem->UseMaterial) {
			faceInfoItem->MaterialInfo = srcFaceInfoItem->MaterialInfo;
		}
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
int PolygonRenderer::ClipPolygon(PolygonClipBuffer& clipper,
	VertexAttribute* input,
	int numVertices) {
	clipper.NumPoints = 0;
	clipper.MaxPoints = MAX_POLYGON_VERTICES;

	int numOutVertices =
		Clipper::FrustumClip(&clipper, ViewFrustum, NumFrustumPlanes, input, numVertices);
	if (numOutVertices < 3 || numOutVertices >= MAX_POLYGON_VERTICES) {
		return 0;
	}

	return numOutVertices;
}
bool PolygonRenderer::CheckPolygonVisible(VertexAttribute* vertex, int vertexCount) {
	int numBehind[3] = {0, 0, 0};
	int numVertices = vertexCount;
	while (numVertices--) {
		if (vertex->Position.X < -vertex->Position.W ||
			vertex->Position.X > vertex->Position.W) {
			numBehind[0]++;
		}
		if (vertex->Position.Y < -vertex->Position.W ||
			vertex->Position.Y > vertex->Position.W) {
			numBehind[1]++;
		}
		if (vertex->Position.Z <= 0) {
			numBehind[2]++;
		}

		vertex++;
	}

	if (numBehind[0] == vertexCount || numBehind[1] == vertexCount ||
		numBehind[2] == vertexCount) {
		return false;
	}

	return true;
}
void PolygonRenderer::CopyVertices(VertexAttribute* buffer,
	VertexAttribute* output,
	int numVertices) {
	while (numVertices--) {
		COPY_VECTOR(output->Position, buffer->Position);
		COPY_NORMAL(output->Normal, buffer->Normal);
		output->Color = buffer->Color;
		output->UV = buffer->UV;
		output++;
		buffer++;
	}
}
