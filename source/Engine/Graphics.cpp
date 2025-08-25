#include <Engine/Graphics.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Error.h>
#include <Engine/Math/Math.h>

#include <Engine/Rendering/Software/SoftwareRenderer.h>
#ifdef USING_OPENGL
#include <Engine/Rendering/GL/GLRenderer.h>
#endif
#ifdef USING_DIRECT3D
#include <Engine/Rendering/D3D/D3DRenderer.h>
#endif
#include <Engine/Rendering/SDL2/SDL2Renderer.h>

#include <Engine/Bytecode/ScriptManager.h>

bool Graphics::Initialized = false;

HashMap<Texture*>* Graphics::TextureMap = NULL;
std::map<std::string, TextureReference*> Graphics::SpriteSheetTextureMap;
bool Graphics::VsyncEnabled = true;
bool Graphics::PrecompileShaders = false;
int Graphics::MultisamplingEnabled = 0;
int Graphics::FontDPI = 1;
bool Graphics::SupportsShaders = false;
bool Graphics::SupportsBatching = false;
bool Graphics::TextureBlend = false;
bool Graphics::TextureInterpolate = false;
Uint32 Graphics::PreferredPixelFormat = SDL_PIXELFORMAT_ARGB8888;
Uint32 Graphics::MaxTextureWidth = 1;
Uint32 Graphics::MaxTextureHeight = 1;
Uint32 Graphics::MaxTextureUnits = 1;
Texture* Graphics::TextureHead = NULL;

std::vector<Shader*> Graphics::Shaders;

std::vector<VertexBuffer*> Graphics::VertexBuffers;
Scene3D Graphics::Scene3Ds[MAX_3D_SCENES];

stack<GraphicsState> Graphics::StateStack;
Matrix4x4 Graphics::ViewMatrixStack[MATRIX_STACK_SIZE];
Matrix4x4 Graphics::ModelMatrixStack[MATRIX_STACK_SIZE];
size_t Graphics::MatrixStackID;

Matrix4x4* Graphics::ViewMatrix;
Matrix4x4* Graphics::ModelMatrix;

Viewport Graphics::CurrentViewport;
Viewport Graphics::BackupViewport;
ClipArea Graphics::CurrentClip;
ClipArea Graphics::BackupClip;

View* Graphics::CurrentView = NULL;

float Graphics::BlendColors[4];
float Graphics::TintColors[4];

int Graphics::BlendMode = BlendMode_NORMAL;
int Graphics::TintMode = TintMode_SRC_NORMAL;

bool Graphics::StencilEnabled = false;
int Graphics::StencilTest = StencilTest_Always;
int Graphics::StencilOpPass = StencilOp_Keep;
int Graphics::StencilOpFail = StencilOp_Keep;

Texture* Graphics::FramebufferTexture = NULL;
int Graphics::FramebufferWidth = 0;
int Graphics::FramebufferHeight = 0;

TileScanLine Graphics::TileScanLineBuffer[MAX_FRAMEBUFFER_HEIGHT];
Uint32 Graphics::PaletteColors[MAX_PALETTE_COUNT][0x100];
Uint8 Graphics::PaletteIndexLines[MAX_FRAMEBUFFER_HEIGHT];
bool Graphics::PaletteUpdated = false;
bool Graphics::PaletteIndexLinesUpdated = false;

Texture* Graphics::PaletteTexture = NULL;
Texture* Graphics::PaletteIndexTexture = NULL;
Uint32* Graphics::PaletteIndexTextureData = NULL;

Texture* Graphics::CurrentRenderTarget = NULL;
Sint32 Graphics::CurrentScene3D = -1;
Sint32 Graphics::CurrentVertexBuffer = -1;

Shader* Graphics::CurrentShader = NULL;
Shader* Graphics::PostProcessShader = NULL;
bool Graphics::SmoothFill = false;
bool Graphics::SmoothStroke = false;

float Graphics::PixelOffset = 0.0f;
bool Graphics::NoInternalTextures = false;
bool Graphics::UsePalettes = false;
bool Graphics::UsePaletteIndexLines = false;
bool Graphics::UseTinting = false;
bool Graphics::UseDepthTesting = false;
bool Graphics::UseSoftwareRenderer = false;

unsigned Graphics::CurrentFrame = 0;

GraphicsFunctions Graphics::Internal;
GraphicsFunctions* Graphics::GfxFunctions = &Graphics::Internal;
const char* Graphics::Renderer = "default";

void Graphics::Init() {
	Graphics::TextureMap = new HashMap<Texture*>(NULL, 32);

	for (size_t i = 0; i < MATRIX_STACK_SIZE; i++) {
		Matrix4x4::Identity(&Graphics::ViewMatrixStack[i]);
		Matrix4x4::Identity(&Graphics::ModelMatrixStack[i]);
	}
	Graphics::ViewMatrix = &Graphics::ViewMatrixStack[0];
	Graphics::ModelMatrix = &Graphics::ModelMatrixStack[0];
	Graphics::MatrixStackID = 1;

	Graphics::CurrentClip.Enabled = false;
	Graphics::BackupClip.Enabled = false;

	Graphics::Reset();

	int w, h;
	SDL_GetWindowSize(Application::Window, &w, &h);

	Viewport* vp = &Graphics::CurrentViewport;
	vp->X = vp->Y = 0.0f;
	vp->Width = w;
	vp->Height = h;
	Graphics::BackupViewport = Graphics::CurrentViewport;

	Graphics::GfxFunctions->Init();

	Log::Print(Log::LOG_VERBOSE, "Window Size: %d x %d", w, h);
	Log::Print(Log::LOG_INFO,
		"Window Pixel Format: %s",
		SDL_GetPixelFormatName(SDL_GetWindowPixelFormat(Application::Window)));
	Log::Print(Log::LOG_INFO, "VSync: %s", Graphics::VsyncEnabled ? "true" : "false");
	if (Graphics::MultisamplingEnabled) {
		Log::Print(Log::LOG_VERBOSE, "Multisample: %d", Graphics::MultisamplingEnabled);
	}
	else {
		Log::Print(Log::LOG_VERBOSE, "Multisample: false");
	}
	Log::Print(Log::LOG_VERBOSE,
		"Max Texture Size: %d x %d",
		Graphics::MaxTextureWidth,
		Graphics::MaxTextureHeight);

	InitCapabilities();

	Graphics::Initialized = true;
}
void Graphics::InitCapabilities() {
	Application::AddCapability("graphics_shaders", Graphics::SupportsShaders);
	Application::AddCapability("gpu_maxTextureWidth", (int)Graphics::MaxTextureWidth);
	Application::AddCapability("gpu_maxTextureHeight", (int)Graphics::MaxTextureHeight);
	Application::AddCapability("gpu_maxTextureUnits", (int)Graphics::MaxTextureUnits);
}
void Graphics::ChooseBackend() {
	char renderer[64];

	SoftwareRenderer::SetGraphicsFunctions();

	// Set renderers
	Graphics::Renderer = NULL;

#ifdef DEVELOPER_MODE
	if (Application::Settings->GetString("dev", "renderer", renderer, sizeof renderer)) {
#ifdef USING_OPENGL
		if (!strcmp(renderer, "opengl")) {
			Graphics::Renderer = "opengl";
			GLRenderer::SetGraphicsFunctions();
			return;
		}
#endif
#ifdef USING_DIRECT3D
		if (!strcmp(renderer, "direct3d")) {
			Graphics::Renderer = "direct3d";
			D3DRenderer::SetGraphicsFunctions();
			return;
		}
#endif
		if (!strcmp(renderer, "sdl2")) {
			Graphics::Renderer = "sdl2";
			SDL2Renderer::SetGraphicsFunctions();
			return;
		}
		if (!Graphics::Renderer) {
			Log::Print(Log::LOG_WARN,
				"Could not find renderer \"%s\" on this platform!",
				renderer);
		}
	}
#endif

// Compiler-directed renderer
#ifdef USING_DIRECT3D
	if (!Graphics::Renderer) {
		Graphics::Renderer = "direct3d";
		D3DRenderer::SetGraphicsFunctions();
	}
#endif
#ifdef USING_OPENGL
	if (!Graphics::Renderer) {
		Graphics::Renderer = "opengl";
		GLRenderer::SetGraphicsFunctions();
	}
#endif
#ifdef USING_METAL
	if (!Graphics::Renderer) {
		Graphics::Renderer = "metal";
		MetalRenderer::SetGraphicsFunctions();
	}
#endif

	if (!Graphics::Renderer) {
		Graphics::Renderer = "sdl2";
		SDL2Renderer::SetGraphicsFunctions();
	}
}
Uint32 Graphics::GetWindowFlags() {
	return Graphics::GfxFunctions->GetWindowFlags();
}
void Graphics::SetVSync(bool enabled) {
	Graphics::GfxFunctions->SetVSync(enabled);
}
void Graphics::Reset() {
	if (Graphics::Initialized) {
		Graphics::UseSoftwareRenderer = false;
		Graphics::UsePalettes = false;
		Graphics::UsePaletteIndexLines = false;
	}

	Graphics::BlendColors[0] = Graphics::BlendColors[1] = Graphics::BlendColors[2] =
		Graphics::BlendColors[3] = 1.0;

	Graphics::TintColors[0] = Graphics::TintColors[1] = Graphics::TintColors[2] =
		Graphics::TintColors[3] = 1.0;

	for (int p = 0; p < MAX_PALETTE_COUNT; p++) {
		for (int c = 0; c < 0x100; c++) {
			Graphics::PaletteColors[p][c] = 0xFF000000U;
			Graphics::PaletteColors[p][c] |= (c & 0x07) << 5; // Red?
			Graphics::PaletteColors[p][c] |= (c & 0x18) << 11; // Green?
			Graphics::PaletteColors[p][c] |= (c & 0xE0) << 16; // Blue?
		}
	}
	memset(Graphics::PaletteIndexLines, 0, sizeof(Graphics::PaletteIndexLines));
	Graphics::PaletteUpdated = Graphics::PaletteIndexLinesUpdated = true;

	Graphics::StencilEnabled = false;
	Graphics::StencilTest = StencilTest_Always;
	Graphics::StencilOpPass = StencilOp_Keep;
	Graphics::StencilOpFail = StencilOp_Keep;
}
void Graphics::Dispose() {
	Graphics::UnloadData();

	for (Texture *texture = Graphics::TextureHead, *next; texture != NULL; texture = next) {
		next = texture->Next;
		Graphics::DisposeTexture(texture);
	}
	Graphics::TextureHead = NULL;
	Graphics::PaletteTexture = NULL;
	Graphics::PaletteIndexTexture = NULL;
	Graphics::FramebufferTexture = NULL;

	if (Graphics::PaletteIndexTextureData != NULL) {
		Memory::Free(Graphics::PaletteIndexTextureData);
		Graphics::PaletteIndexTextureData = NULL;
	}

	Graphics::GfxFunctions->Dispose();

	delete Graphics::TextureMap;
}
void Graphics::UnloadData() {
	Graphics::UnloadSceneData();

	Graphics::DeleteShaders();
	Graphics::DeleteVertexBuffers();

	for (Uint32 i = 0; i < MAX_3D_SCENES; i++) {
		Graphics::DeleteScene3D(i);
	}

	Graphics::DeleteSpriteSheetMap();
}
void Graphics::DeleteShaders() {
	Graphics::CurrentShader = nullptr;
	Graphics::PostProcessShader = nullptr;

	for (int i = 0; i < MAX_SCENE_VIEWS; i++) {
		Scene::Views[i].CurrentShader = nullptr;
	}

	for (Uint32 i = 0; i < Graphics::Shaders.size(); i++) {
		delete Shaders[i];
	}

	Graphics::Shaders.clear();
}
void Graphics::DeleteVertexBuffers() {
	for (Uint32 i = 0; i < Graphics::VertexBuffers.size(); i++) {
		Graphics::DeleteVertexBuffer(i);
	}
	Graphics::VertexBuffers.clear();
}

Point Graphics::ProjectToScreen(float x, float y, float z) {
	Point p;
	float vec4[4];
	Matrix4x4* matrix;

	matrix = &Graphics::ViewMatrixStack[MatrixStackID - 1];

	vec4[0] = x;
	vec4[1] = y;
	vec4[2] = z;
	vec4[3] = 1.0f;
	Matrix4x4::Multiply(matrix, &vec4[0]);

	p.X = (vec4[0] + 1.0f) / 2.0f * Graphics::CurrentViewport.Width;
	p.Y = (vec4[1] - 1.0f) / -2.0f * Graphics::CurrentViewport.Height;
	p.Z = vec4[2];
	return p;
}

Texture* Graphics::CreateTexture(Uint32 format, Uint32 access, Uint32 width, Uint32 height) {
	Texture* texture;
	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
		Graphics::NoInternalTextures) {
		texture = Texture::New(format, access, width, height);
		if (!texture) {
			return NULL;
		}
	}
	else {
		texture = Graphics::GfxFunctions->CreateTexture(format, access, width, height);
		if (!texture) {
			return NULL;
		}
	}

	texture->Next = Graphics::TextureHead;
	if (Graphics::TextureHead) {
		Graphics::TextureHead->Prev = texture;
	}

	Graphics::TextureHead = texture;

	return texture;
}
Texture* Graphics::CreateTextureFromPixels(Uint32 width, Uint32 height, void* pixels, int pitch) {
	Texture* texture = Graphics::CreateTexture(
		SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
	if (texture) {
		Graphics::UpdateTexture(texture, NULL, pixels, pitch);
	}

	return texture;
}
Texture* Graphics::CreateTextureFromSurface(SDL_Surface* surface) {
	Texture* texture = Graphics::CreateTexture(
		surface->format->format, SDL_TEXTUREACCESS_STATIC, surface->w, surface->h);
	if (texture) {
		Graphics::GfxFunctions->UpdateTexture(
			texture, NULL, surface->pixels, surface->pitch);
	}
	return texture;
}
int Graphics::LockTexture(Texture* texture, void** pixels, int* pitch) {
	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
		Graphics::NoInternalTextures) {
		return 1;
	}
	return Graphics::GfxFunctions->LockTexture(texture, pixels, pitch);
}
int Graphics::UpdateTexture(Texture* texture, SDL_Rect* src, void* pixels, int pitch) {
	memcpy(texture->Pixels, pixels, sizeof(Uint32) * texture->Width * texture->Height);
	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
		Graphics::NoInternalTextures) {
		return 1;
	}
	return Graphics::GfxFunctions->UpdateTexture(texture, src, pixels, pitch);
}
int Graphics::UpdateYUVTexture(Texture* texture,
	SDL_Rect* src,
	Uint8* pixelsY,
	int pitchY,
	Uint8* pixelsU,
	int pitchU,
	Uint8* pixelsV,
	int pitchV) {
	if (!Graphics::GfxFunctions->UpdateYUVTexture) {
		return 0;
	}
	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
		Graphics::NoInternalTextures) {
		return 1;
	}
	return Graphics::GfxFunctions->UpdateYUVTexture(
		texture, src, pixelsY, pitchY, pixelsU, pitchU, pixelsV, pitchV);
}
int Graphics::SetTexturePalette(Texture* texture, void* palette, unsigned numPaletteColors) {
	texture->SetPalette((Uint32*)palette, numPaletteColors);
	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
		!Graphics::GfxFunctions->SetTexturePalette || Graphics::NoInternalTextures) {
		return 1;
	}
	return Graphics::GfxFunctions->SetTexturePalette(texture, palette, numPaletteColors);
}
int Graphics::ConvertTextureToRGBA(Texture* texture) {
	texture->ConvertToRGBA();
	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
		Graphics::NoInternalTextures) {
		return 1;
	}
	return Graphics::GfxFunctions->UpdateTexture(
		texture, NULL, texture->Pixels, texture->Pitch);
}
int Graphics::ConvertTextureToPalette(Texture* texture, unsigned paletteNumber) {
	Uint32* colors = (Uint32*)Memory::TrackedMalloc("Texture::Colors", 256 * sizeof(Uint32));
	if (!colors) {
		return 0;
	}

	memcpy(colors, Graphics::PaletteColors[paletteNumber], 256 * sizeof(Uint32));

	texture->ConvertToPalette(colors, 256);
	texture->SetPalette(colors, 256);

	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions ||
		Graphics::NoInternalTextures) {
		return 1;
	}

	int ok = Graphics::GfxFunctions->UpdateTexture(
		texture, NULL, texture->Pixels, texture->Pitch);
	if (!ok) {
		return 0;
	}

	if (Graphics::GfxFunctions->SetTexturePalette) {
		ok |= Graphics::GfxFunctions->SetTexturePalette(
			texture, texture->PaletteColors, texture->NumPaletteColors);
	}

	return ok;
}
void Graphics::UnlockTexture(Texture* texture) {
	Graphics::GfxFunctions->UnlockTexture(texture);
}
void Graphics::DisposeTexture(Texture* texture) {
	if (texture == nullptr) {
		return;
	}

	Graphics::GfxFunctions->DisposeTexture(texture);

	if (texture->Next) {
		texture->Next->Prev = texture->Prev;
	}

	if (texture->Prev) {
		texture->Prev->Next = texture->Next;
	}
	else {
		Graphics::TextureHead = texture->Next;
	}

	texture->Dispose();

	Memory::Free(texture);
}
TextureReference* Graphics::GetSpriteSheet(string sheetPath) {
	if (Graphics::SpriteSheetTextureMap.count(sheetPath) != 0) {
		TextureReference* textureRef = Graphics::SpriteSheetTextureMap.at(sheetPath);
		textureRef->AddRef();
		return textureRef;
	}

	return nullptr;
}
TextureReference* Graphics::AddSpriteSheet(string sheetPath, Texture* texture) {
	TextureReference* ref = Graphics::GetSpriteSheet(sheetPath);
	if (ref == nullptr) {
		ref = new TextureReference(texture);
		Graphics::SpriteSheetTextureMap[sheetPath] = ref;
	}
	return ref;
}
void Graphics::DisposeSpriteSheet(string sheetPath) {
	if (Graphics::SpriteSheetTextureMap.count(sheetPath) != 0) {
		TextureReference* textureRef = Graphics::SpriteSheetTextureMap.at(sheetPath);
		if (textureRef->TakeRef()) {
			Graphics::DisposeTexture(textureRef->TexturePtr);
			Graphics::SpriteSheetTextureMap.erase(sheetPath);
			delete textureRef;
		}
	}
}
void Graphics::DeleteSpriteSheetMap() {
	for (std::map<std::string, TextureReference*>::iterator it =
			Graphics::SpriteSheetTextureMap.begin();
		it != Graphics::SpriteSheetTextureMap.end();
		it++) {
		Graphics::DisposeTexture(it->second->TexturePtr);
		delete it->second;
	}

	Graphics::SpriteSheetTextureMap.clear();
}

Uint32 Graphics::CreateVertexBuffer(Uint32 maxVertices, int unloadPolicy) {
	Uint32 idx = 0xFFFFFFFF;

	for (Uint32 i = 0; i < Graphics::VertexBuffers.size(); i++) {
		if (Graphics::VertexBuffers[i] == NULL) {
			idx = i;
			break;
		}
	}

	VertexBuffer* vtxbuf;
	if (Graphics::Internal.CreateVertexBuffer) {
		vtxbuf = (VertexBuffer*)Graphics::Internal.CreateVertexBuffer(maxVertices);
	}
	else {
		vtxbuf = new VertexBuffer(maxVertices);
	}

	vtxbuf->UnloadPolicy = unloadPolicy;

	if (idx == 0xFFFFFFFF) {
		size_t sz = Graphics::VertexBuffers.size();
		if (sz < MAX_VERTEX_BUFFERS) {
			Graphics::VertexBuffers.push_back(vtxbuf);
			idx = sz;
		}
	}
	else {
		Graphics::VertexBuffers[idx] = vtxbuf;
	}

	return idx;
}
void Graphics::DeleteVertexBuffer(Uint32 vertexBufferIndex) {
	if (vertexBufferIndex < 0 || vertexBufferIndex >= Graphics::VertexBuffers.size()) {
		return;
	}
	if (!Graphics::VertexBuffers[vertexBufferIndex]) {
		return;
	}

	if (Graphics::Internal.DeleteVertexBuffer) {
		Graphics::Internal.DeleteVertexBuffer(Graphics::VertexBuffers[vertexBufferIndex]);
	}
	else {
		delete Graphics::VertexBuffers[vertexBufferIndex];
	}
	Graphics::VertexBuffers[vertexBufferIndex] = NULL;
}

Shader* Graphics::CreateShader() {
	if (!Graphics::GfxFunctions->CreateShader) {
		return nullptr;
	}

	Shader* shader = Graphics::GfxFunctions->CreateShader();
	if (shader == nullptr) {
		return nullptr;
	}

	Shaders.push_back(shader);

	return shader;
}
void Graphics::DeleteShader(Shader* shader) {
	auto it = std::find(Shaders.begin(), Shaders.end(), shader);
	if (it != Shaders.end()) {
		delete shader;

		Shaders.erase(it);
	}
}
void Graphics::SetUserShader(Shader* shader) {
	if (!Graphics::GfxFunctions->SetUserShader) {
		return;
	}

	try {
		Graphics::CurrentShader = shader;
		Graphics::GfxFunctions->SetUserShader(shader);
	} catch (const std::runtime_error& error) {
		Graphics::CurrentShader = nullptr;
		throw;
	}
}

void Graphics::SetFilter(int filter) {
	if (Graphics::GfxFunctions->SetFilter) {
		Graphics::GfxFunctions->SetFilter(filter);
	}
}

void Graphics::SetTextureInterpolation(bool interpolate) {
	Graphics::TextureInterpolate = interpolate;
}

void Graphics::Clear() {
	Graphics::GfxFunctions->Clear();
}
void Graphics::Present() {
	Graphics::GfxFunctions->Present();
	Graphics::CurrentFrame++;
}

void Graphics::SoftwareStart(int viewIndex) {
	Graphics::GfxFunctions = &SoftwareRenderer::BackendFunctions;
	SoftwareRenderer::RenderStart(viewIndex);
}
void Graphics::SoftwareEnd(int viewIndex) {
	SoftwareRenderer::RenderEnd(viewIndex);
	Graphics::GfxFunctions = &Graphics::Internal;
	Graphics::UpdateTexture(Graphics::CurrentRenderTarget,
		NULL,
		Graphics::CurrentRenderTarget->Pixels,
		Graphics::CurrentRenderTarget->Width * 4);
}

void Graphics::UpdateGlobalPalette() {
	if (Graphics::PaletteTexture == nullptr) {
		Graphics::PaletteTexture = CreateTexture(SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			PALETTE_ROW_SIZE,
			MAX_PALETTE_COUNT);

		if (Graphics::PaletteTexture == nullptr) {
			return;
		}
	}

	Graphics::UpdateTexture(Graphics::PaletteTexture,
		nullptr,
		(Uint32*)Graphics::PaletteColors,
		PALETTE_ROW_SIZE * sizeof(Uint32));

	if (Graphics::GfxFunctions->UpdateGlobalPalette) {
		Graphics::GfxFunctions->UpdateGlobalPalette(Graphics::PaletteTexture);
	}
}
void Graphics::UpdatePaletteIndexTable() {
	if (Graphics::PaletteIndexTexture == nullptr) {
		Graphics::PaletteIndexTexture = CreateTexture(SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			PALETTE_INDEX_TEXTURE_SIZE,
			PALETTE_INDEX_TEXTURE_SIZE);

		if (Graphics::PaletteIndexTexture == nullptr) {
			return;
		}
	}

	if (Graphics::PaletteIndexTextureData == nullptr) {
		Graphics::PaletteIndexTextureData = (Uint32*)Memory::Calloc(
			PALETTE_INDEX_TEXTURE_SIZE * PALETTE_INDEX_TEXTURE_SIZE, sizeof(Uint32));
	}

	for (size_t i = 0; i < MAX_FRAMEBUFFER_HEIGHT; i++) {
		Graphics::PaletteIndexTextureData[i] = 0xFF000000 | PaletteIndexLines[i];
	}

	Graphics::UpdateTexture(Graphics::PaletteIndexTexture,
		nullptr,
		Graphics::PaletteIndexTextureData,
		PALETTE_INDEX_TEXTURE_SIZE * sizeof(Uint32));

	if (Graphics::GfxFunctions->UpdatePaletteIndexTable) {
		Graphics::GfxFunctions->UpdatePaletteIndexTable(Graphics::PaletteIndexTexture);
	}
}

void Graphics::UnloadSceneData() {
	for (Uint32 i = 0; i < Graphics::VertexBuffers.size(); i++) {
		VertexBuffer* buffer = Graphics::VertexBuffers[i];
		if (!buffer || buffer->UnloadPolicy > SCOPE_SCENE) {
			continue;
		}

		Graphics::DeleteVertexBuffer(i);
	}

	for (Uint32 i = 0; i < MAX_3D_SCENES; i++) {
		Scene3D* scene = &Graphics::Scene3Ds[i];
		if (!scene->Initialized || scene->UnloadPolicy > SCOPE_SCENE) {
			continue;
		}

		Graphics::DeleteScene3D(i);
	}
}

bool Graphics::SetRenderTarget(Texture* texture) {
	if (texture && !Graphics::CurrentRenderTarget) {
		Graphics::BackupViewport = Graphics::CurrentViewport;
		Graphics::BackupClip = Graphics::CurrentClip;
	}

	if (!Graphics::GfxFunctions->SetRenderTarget(texture)) {
		return false;
	}

	Graphics::CurrentRenderTarget = texture;

	Viewport* vp = &Graphics::CurrentViewport;
	if (texture) {
		vp->X = 0.0;
		vp->Y = 0.0;
		vp->Width = texture->Width;
		vp->Height = texture->Height;
	}
	else {
		Graphics::CurrentViewport = Graphics::BackupViewport;
		Graphics::CurrentClip = Graphics::BackupClip;
	}

	Graphics::GfxFunctions->UpdateViewport();
	Graphics::GfxFunctions->UpdateClipRect();

	return true;
}
bool Graphics::CreateFramebufferTexture() {
	int maxWidth, maxHeight;
	GetScreenSize(maxWidth, maxHeight);

	bool createTexture = FramebufferTexture == nullptr;

	if (!createTexture) {
		if (FramebufferWidth != maxWidth || FramebufferHeight != maxHeight) {
			DisposeTexture(FramebufferTexture);

			createTexture = true;
		}
	}

	if (createTexture) {
		FramebufferTexture = CreateTexture(
			SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, maxWidth, maxHeight);

		if (FramebufferTexture == nullptr) {
			return false;
		}

		FramebufferWidth = maxWidth;
		FramebufferHeight = maxHeight;
	}

	return true;
}
bool Graphics::UpdateFramebufferTexture() {
	if (!Graphics::GfxFunctions->ReadFramebuffer || !Graphics::GfxFunctions->UpdateTexture ||
		!Graphics::CreateFramebufferTexture()) {
		return false;
	}

	Graphics::GfxFunctions->ReadFramebuffer(Graphics::FramebufferTexture->Pixels,
		Graphics::FramebufferTexture->Width,
		Graphics::FramebufferTexture->Height);
	Graphics::GfxFunctions->UpdateTexture(Graphics::FramebufferTexture,
		nullptr,
		(Uint32*)Graphics::FramebufferTexture->Pixels,
		Graphics::FramebufferTexture->Width * sizeof(Uint32));

	return true;
}
void Graphics::DoScreenPostProcess() {
	if (Graphics::PostProcessShader == nullptr) {
		return;
	}

	if (!Graphics::UpdateFramebufferTexture()) {
		return;
	}

	int ww, wh;
	SDL_GetWindowSize(Application::Window, &ww, &wh);
	Graphics::SetViewport(0.0, 0.0, ww, wh);
	Graphics::UpdateOrthoFlipped(ww, wh);
	Graphics::SetDepthTesting(false);
	Graphics::SetBlendMode(BlendMode_NORMAL);
	Graphics::SetBlendColor(1.0, 0.0, 0.0, 1.0);
	Graphics::TextureBlend = false;

	Graphics::SetUserShader(Graphics::PostProcessShader);
	Graphics::DrawTexture(Graphics::FramebufferTexture,
		0.0,
		0.0,
		Graphics::FramebufferTexture->Width,
		Graphics::FramebufferTexture->Height,
		0.0,
		0.0,
		Application::WindowWidth,
		Application::WindowHeight);
	Graphics::SetUserShader(nullptr);
}
void Graphics::CopyScreen(int source_x,
	int source_y,
	int source_w,
	int source_h,
	int dest_x,
	int dest_y,
	int dest_w,
	int dest_h,
	Texture* texture) {
	if (!Graphics::GfxFunctions->ReadFramebuffer) {
		return;
	}

	if (texture == nullptr) {
		return;
	}

	int maxWidth, maxHeight;
	Graphics::GetScreenSize(maxWidth, maxHeight);

	if (source_x < 0) {
		source_x = 0;
	}
	if (source_y < 0) {
		source_y = 0;
	}
	if (source_w > maxWidth) {
		source_w = maxWidth;
	}
	if (source_h > maxHeight) {
		source_h = maxHeight;
	}
	if (source_x >= source_w || source_y >= source_h) {
		return;
	}
	if (source_w <= 0 || source_h <= 0) {
		return;
	}

	if (dest_x < 0) {
		dest_x = 0;
	}
	if (dest_y < 0) {
		dest_y = 0;
	}
	if (dest_w > texture->Width) {
		dest_w = texture->Width;
	}
	if (dest_h > texture->Height) {
		dest_h = texture->Height;
	}
	if (dest_x >= dest_w || dest_y >= source_h) {
		return;
	}
	if (dest_w <= 0 || dest_h <= 0) {
		return;
	}

	if (source_x == 0 && source_y == 0 && dest_x == 0 && dest_y == 0 && dest_w == source_w &&
		dest_h == source_h) {
		Graphics::GfxFunctions->ReadFramebuffer(texture->Pixels, dest_w, dest_h);
		return;
	}

	if (!Graphics::CreateFramebufferTexture()) {
		return;
	}

	void* fbPixels = FramebufferTexture->Pixels;

	Graphics::GfxFunctions->ReadFramebuffer(fbPixels, source_w, source_h);

	Uint32* d = (Uint32*)texture->Pixels;
	Uint32* s = (Uint32*)fbPixels;

	int xs = FP16_DIVIDE(0x10000, FP16_DIVIDE(dest_w << 16, source_w << 16));
	int ys = FP16_DIVIDE(0x10000, FP16_DIVIDE(dest_h << 16, source_h << 16));

	for (int read_y = source_y << 16, write_y = dest_y; write_y < dest_h;
		write_y++, read_y += ys) {
		int isy = read_y >> 16;
		if (isy >= source_h) {
			return;
		}
		for (int read_x = source_x << 16, write_x = dest_x; write_x < dest_w;
			write_x++, read_x += xs) {
			int isx = read_x >> 16;
			if (isx >= source_w) {
				break;
			}
			d[(write_y * texture->Width) + write_x] = s[(isy * source_w) + isx];
		}
	}
}
void Graphics::UpdateOrtho(float width, float height) {
	Graphics::GfxFunctions->UpdateOrtho(0.0f, 0.0f, width, height);
}
void Graphics::UpdateOrthoFlipped(float width, float height) {
	Graphics::GfxFunctions->UpdateOrtho(0.0f, height, width, 0.0f);
}
void Graphics::UpdatePerspective(float fovy, float aspect, float nearv, float farv) {
	Graphics::GfxFunctions->UpdatePerspective(fovy, aspect, nearv, farv);
}
void Graphics::UpdateProjectionMatrix() {
	Graphics::GfxFunctions->UpdateProjectionMatrix();
}
void Graphics::MakePerspectiveMatrix(Matrix4x4* out,
	float fov,
	float near,
	float far,
	float aspect) {
	Graphics::GfxFunctions->MakePerspectiveMatrix(out, fov, near, far, aspect);
}
void Graphics::CalculateMVPMatrix(Matrix4x4* output,
	Matrix4x4* modelMatrix,
	Matrix4x4* viewMatrix,
	Matrix4x4* projMatrix) {
	if (viewMatrix && projMatrix) {
		Matrix4x4 modelView;

		if (modelMatrix) {
			Matrix4x4::Multiply(&modelView, modelMatrix, viewMatrix);
			Matrix4x4::Multiply(output, &modelView, projMatrix);
		}
		else {
			Matrix4x4::Multiply(output, viewMatrix, projMatrix);
		}
	}
	else if (modelMatrix) {
		*output = *modelMatrix;
	}
	else {
		Matrix4x4::Identity(output);
	}
}
void Graphics::GetScreenSize(int& outWidth, int& outHeight) {
	if (Graphics::CurrentRenderTarget) {
		outWidth = Graphics::CurrentRenderTarget->Width;
		outHeight = Graphics::CurrentRenderTarget->Height;
	}
	else {
		int w, h;
		SDL_GetWindowSize(Application::Window, &w, &h);
		outWidth = w;
		outHeight = h;
	}
}
void Graphics::SetViewport(float x, float y, float w, float h) {
	Viewport* vp = &Graphics::CurrentViewport;
	if (x < 0) {
		int w, h;
		vp->X = 0.0;
		vp->Y = 0.0;
		Graphics::GetScreenSize(w, h);
		vp->Width = w;
		vp->Height = h;
	}
	else {
		vp->X = x;
		vp->Y = y;
		vp->Width = w;
		vp->Height = h;
	}
	Graphics::GfxFunctions->UpdateViewport();
}
void Graphics::ResetViewport() {
	Graphics::SetViewport(-1.0, 0.0, 0.0, 0.0);
}
void Graphics::Resize(int width, int height) {
	Graphics::GfxFunctions->UpdateWindowSize(width, height);
}

void Graphics::SetClip(int x, int y, int width, int height) {
	Graphics::CurrentClip.Enabled = true;
	Graphics::CurrentClip.X = x;
	Graphics::CurrentClip.Y = y;
	Graphics::CurrentClip.Width = width;
	Graphics::CurrentClip.Height = height;
	Graphics::GfxFunctions->UpdateClipRect();
}
void Graphics::ClearClip() {
	Graphics::CurrentClip.Enabled = false;
	Graphics::GfxFunctions->UpdateClipRect();
}

Matrix4x4* SaveMatrix(Matrix4x4* matrixStack, size_t pos) {
	Matrix4x4* top = &matrixStack[pos - 1];
	Matrix4x4* push = &matrixStack[pos];
	Matrix4x4::Copy(push, top);
	return push;
}
void Graphics::Save() {
	if (MatrixStackID >= MATRIX_STACK_SIZE) {
		Error::Fatal("Draw.Save stack too big!");
	}

	ViewMatrix = SaveMatrix(ViewMatrixStack, MatrixStackID);
	ModelMatrix = SaveMatrix(ModelMatrixStack, MatrixStackID);
	MatrixStackID++;
}
void Graphics::Translate(float x, float y, float z) {
	Matrix4x4::Translate(ModelMatrix, ModelMatrix, x, y, z);
}
void Graphics::Rotate(float x, float y, float z) {
	Matrix4x4::Rotate(ModelMatrix, ModelMatrix, x, 1.0, 0.0, 0.0);
	Matrix4x4::Rotate(ModelMatrix, ModelMatrix, y, 0.0, 1.0, 0.0);
	Matrix4x4::Rotate(ModelMatrix, ModelMatrix, z, 0.0, 0.0, 1.0);
}
void Graphics::Scale(float x, float y, float z) {
	Matrix4x4::Scale(ModelMatrix, ModelMatrix, x, y, z);
}
void Graphics::Restore() {
	if (MatrixStackID == 1) {
		return;
	}
	MatrixStackID--;
	ViewMatrix = &ViewMatrixStack[MatrixStackID - 1];
	ModelMatrix = &ModelMatrixStack[MatrixStackID - 1];
}

BlendState Graphics::GetBlendState() {
	BlendState state;
	state.Opacity = (int)(Graphics::BlendColors[3] * 0xFF);
	state.Mode = Graphics::BlendMode;
	state.Tint.Enabled = Graphics::UseTinting;
	state.Tint.Color = ColorUtils::ToRGB(Graphics::TintColors);
	state.Tint.Amount = (int)(Graphics::TintColors[3] * 0xFF);
	state.Tint.Mode = Graphics::TintMode;
	state.FilterMode = 0;
	return state;
}

void Graphics::PushState() {
	if (StateStack.size() == 256) {
		Error::Fatal("Graphics::PushState stack too big!");
	}

	GraphicsState state;

	state.CurrentShader = (void*)Graphics::CurrentShader;
	state.CurrentViewport = Graphics::CurrentViewport;
	state.CurrentClip = Graphics::CurrentClip;
	state.BlendMode = Graphics::BlendMode;
	state.TintMode = Graphics::TintMode;
	state.TextureBlend = Graphics::TextureBlend;
	state.UsePalettes = Graphics::UsePalettes;
	state.UsePaletteIndexLines = Graphics::UsePaletteIndexLines;
	state.UseTinting = Graphics::UseTinting;
	state.UseDepthTesting = Graphics::UseDepthTesting;

	memcpy(state.BlendColors, Graphics::BlendColors, sizeof(Graphics::BlendColors));
	memcpy(state.TintColors, Graphics::TintColors, sizeof(Graphics::TintColors));

	StateStack.push(state);

	Graphics::Save();
}
void Graphics::PopState() {
	if (StateStack.size() == 0) {
		return;
	}

	GraphicsState state = StateStack.top();

	Graphics::SetUserShader((Shader*)state.CurrentShader);

	Graphics::SetBlendMode(state.BlendMode);
	Graphics::SetBlendColor(state.BlendColors[0],
		state.BlendColors[1],
		state.BlendColors[2],
		state.BlendColors[3]);

	Graphics::SetTintEnabled(state.UseTinting);
	Graphics::SetTintMode(state.TintMode);
	Graphics::SetTintColor(
		state.TintColors[0], state.TintColors[1], state.TintColors[2], state.TintColors[3]);

	Graphics::CurrentViewport = state.CurrentViewport;
	Graphics::CurrentClip = state.CurrentClip;
	Graphics::TextureBlend = state.TextureBlend;
	Graphics::UsePalettes = state.UsePalettes;
	Graphics::UsePaletteIndexLines = state.UsePaletteIndexLines;
	Graphics::UseDepthTesting = state.UseDepthTesting;

	Graphics::SetDepthTesting(Graphics::UseDepthTesting);

	Graphics::GfxFunctions->UpdateViewport();
	Graphics::GfxFunctions->UpdateClipRect();

	StateStack.pop();

	Graphics::Restore();
}

void Graphics::SetBlendColor(float r, float g, float b, float a) {
	Graphics::BlendColors[0] = Math::Clamp(r, 0.0f, 1.0f);
	Graphics::BlendColors[1] = Math::Clamp(g, 0.0f, 1.0f);
	Graphics::BlendColors[2] = Math::Clamp(b, 0.0f, 1.0f);
	Graphics::BlendColors[3] = Math::Clamp(a, 0.0f, 1.0f);
	Graphics::GfxFunctions->SetBlendColor(r, g, b, a);
}
void Graphics::SetBlendMode(int blendMode) {
	Graphics::BlendMode = blendMode;
	switch (blendMode) {
	case BlendMode_NORMAL:
		Graphics::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA,
			BlendFactor_ONE,
			BlendFactor_INV_SRC_ALPHA);
		break;
	case BlendMode_ADD:
		Graphics::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_ONE,
			BlendFactor_SRC_ALPHA,
			BlendFactor_ONE);
		break;
	case BlendMode_MAX:
		Graphics::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_COLOR,
			BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_COLOR);
		break;
	case BlendMode_SUBTRACT:
		Graphics::SetBlendMode(BlendFactor_ZERO,
			BlendFactor_INV_SRC_COLOR,
			BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA);
		break;
	default:
		Graphics::SetBlendMode(BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA,
			BlendFactor_SRC_ALPHA,
			BlendFactor_INV_SRC_ALPHA);
	}
}
void Graphics::SetBlendMode(int srcC, int dstC, int srcA, int dstA) {
	Graphics::GfxFunctions->SetBlendMode(srcC, dstC, srcA, dstA);
}
void Graphics::SetTintColor(float r, float g, float b, float a) {
	Graphics::TintColors[0] = Math::Clamp(r, 0.0f, 1.0f);
	Graphics::TintColors[1] = Math::Clamp(g, 0.0f, 1.0f);
	Graphics::TintColors[2] = Math::Clamp(b, 0.0f, 1.0f);
	Graphics::TintColors[3] = Math::Clamp(a, 0.0f, 1.0f);
	Graphics::GfxFunctions->SetTintColor(r, g, b, a);
}
void Graphics::SetTintMode(int mode) {
	Graphics::TintMode = mode;
	Graphics::GfxFunctions->SetTintMode(mode);
}
void Graphics::SetTintEnabled(bool enabled) {
	Graphics::UseTinting = enabled;
	Graphics::GfxFunctions->SetTintEnabled(enabled);
}
void Graphics::SetLineWidth(float n) {
	Graphics::GfxFunctions->SetLineWidth(n);
}

void Graphics::StrokeLine(float x1, float y1, float x2, float y2) {
	Graphics::GfxFunctions->StrokeLine(x1, y1, x2, y2);
}
void Graphics::StrokeCircle(float x, float y, float rad, float thickness) {
	Graphics::GfxFunctions->StrokeCircle(x, y, rad, thickness);
}
void Graphics::StrokeEllipse(float x, float y, float w, float h) {
	Graphics::GfxFunctions->StrokeEllipse(x, y, w, h);
}
void Graphics::StrokeTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
	Graphics::GfxFunctions->StrokeLine(x1, y1, x2, y2);
	Graphics::GfxFunctions->StrokeLine(x2, y2, x3, y3);
	Graphics::GfxFunctions->StrokeLine(x3, y3, x1, y1);
}
void Graphics::StrokeRectangle(float x, float y, float w, float h) {
	Graphics::GfxFunctions->StrokeRectangle(x, y, w, h);
}
void Graphics::FillCircle(float x, float y, float rad) {
	Graphics::GfxFunctions->FillCircle(x, y, rad);
}
void Graphics::FillEllipse(float x, float y, float w, float h) {
	Graphics::GfxFunctions->FillEllipse(x, y, w, h);
}
void Graphics::FillTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {
	Graphics::GfxFunctions->FillTriangle(x1, y1, x2, y2, x3, y3);
}
void Graphics::FillRectangle(float x, float y, float w, float h) {
	Graphics::GfxFunctions->FillRectangle(x, y, w, h);
}

void Graphics::DrawTexture(Texture* texture,
	float sx,
	float sy,
	float sw,
	float sh,
	float x,
	float y,
	float w,
	float h,
	int paletteID) {
	if (Graphics::UsePaletteIndexLines) {
		paletteID = PALETTE_INDEX_TABLE_ID;
	}

	Graphics::GfxFunctions->DrawTexture(texture, sx, sy, sw, sh, x, y, w, h, paletteID);
}
void Graphics::DrawSprite(ISprite* sprite,
	int animation,
	int frame,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation,
	int paletteID) {
	if (Graphics::UsePaletteIndexLines) {
		paletteID = PALETTE_INDEX_TABLE_ID;
	}

	Graphics::GfxFunctions->DrawSprite(
		sprite, animation, frame, x, y, flipX, flipY, scaleW, scaleH, rotation, paletteID);
}
void Graphics::DrawSpritePart(ISprite* sprite,
	int animation,
	int frame,
	int sx,
	int sy,
	int sw,
	int sh,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation,
	int paletteID) {
	if (Graphics::UsePaletteIndexLines) {
		paletteID = PALETTE_INDEX_TABLE_ID;
	}

	Graphics::GfxFunctions->DrawSpritePart(sprite,
		animation,
		frame,
		sx,
		sy,
		sw,
		sh,
		x,
		y,
		flipX,
		flipY,
		scaleW,
		scaleH,
		rotation,
		paletteID);
}
void Graphics::DrawTexture(Texture* texture,
	float sx,
	float sy,
	float sw,
	float sh,
	float x,
	float y,
	float w,
	float h) {
	int paletteID = Graphics::UsePaletteIndexLines ? PALETTE_INDEX_TABLE_ID : 0;
	Graphics::GfxFunctions->DrawTexture(texture, sx, sy, sw, sh, x, y, w, h, paletteID);
}
void Graphics::DrawSprite(ISprite* sprite,
	int animation,
	int frame,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation) {
	int paletteID = Graphics::UsePaletteIndexLines ? PALETTE_INDEX_TABLE_ID : 0;
	DrawSprite(
		sprite, animation, frame, x, y, flipX, flipY, scaleW, scaleH, rotation, paletteID);
}
void Graphics::DrawSpritePart(ISprite* sprite,
	int animation,
	int frame,
	int sx,
	int sy,
	int sw,
	int sh,
	int x,
	int y,
	bool flipX,
	bool flipY,
	float scaleW,
	float scaleH,
	float rotation) {
	int paletteID = Graphics::UsePaletteIndexLines ? PALETTE_INDEX_TABLE_ID : 0;
	DrawSpritePart(sprite,
		animation,
		frame,
		sx,
		sy,
		sw,
		sh,
		x,
		y,
		flipX,
		flipY,
		scaleW,
		scaleH,
		rotation,
		paletteID);
}

// Handles UTF-8 text and obtains UCS codepoints
std::vector<Uint32> Graphics::GetTextCodepoints(Font* font, const char* text) {
	size_t textLength = strlen(text);

	std::vector<Uint32> codepoints;
	codepoints.reserve(textLength);

	for (size_t i = 0; i < textLength;) {
		if (text[i] == '\n') {
			codepoints.push_back((Uint32)text[i]);
			i++;
			continue;
		}

		int numBytes = 1;
		int decoded = StringUtils::DecodeUTF8Char(&text[i], numBytes);
		if (decoded == -1) {
			decoded = 0;
		}

		Uint32 codepoint = (Uint32)decoded;
		if (!font->IsValidCodepoint(codepoint)) {
			i += numBytes;
			continue;
		}

		// If the glyph is valid
		if (font->RequestGlyph(codepoint)) {
			codepoints.push_back(codepoint);
		}

		i += numBytes;
	}

	return codepoints;
}

void Graphics::DrawText(Font* font, const char* text, float x, float y, float fontSize) {
	// Obtain codepoints and check if the font needs to be updated
	std::vector<Uint32> codepoints = GetTextCodepoints(font, text);

	size_t numCodepoints = codepoints.size();
	if (numCodepoints == 0) {
		return;
	}

	font->Update();

	if (font->Sprite == nullptr || font->Sprite->Spritesheets.size() == 0) {
		return;
	}

	// Draw the codepoints
	float currX = x;
	float currY = y;

	float ascent = font->Ascent;
	float descent = font->Descent;
	float leading = font->Leading;

	float scale = fontSize / font->Size;
	float xyScale = scale / font->Oversampling;

	for (size_t i = 0; i < numCodepoints; i++) {
		Uint32 codepoint = codepoints[i];
		if (codepoints[i] == '\n') {
			currX = x;
			currY += (ascent - descent + leading) * scale;
			continue;
		}
		else if (codepoints[i] == ' ') {
			currX += font->SpaceWidth * scale;
			continue;
		}

		FontGlyph& glyph = font->Glyphs[codepoint];

		float glyphX = currX + (glyph.OffsetX * xyScale);
		float glyphY = currY + (glyph.OffsetY * xyScale);

		glyphY += ascent * scale;

		Graphics::DrawSprite(font->Sprite,
			0,
			glyph.FrameID,
			glyphX,
			glyphY,
			false,
			false,
			xyScale,
			xyScale,
			0.0f);

		currX += glyph.Advance * scale;
	}
}

void Graphics::DrawSceneLayer_InitTileScanLines(SceneLayer* layer, View* currentView) {
	switch (layer->DrawBehavior) {
	case DrawBehavior_HorizontalParallax: {
		int viewX = (int)currentView->X;
		int viewY = (int)currentView->Y;
		int viewHeight = (int)currentView->Height;
		int layerWidth = layer->Width * 16;
		int layerHeight = layer->Height * 16;
		int layerOffsetX = layer->OffsetX;
		int layerOffsetY = layer->OffsetY;

		// Set parallax positions
		ScrollingInfo* info = &layer->ScrollInfos[0];
		for (int i = 0; i < layer->ScrollInfoCount; i++) {
			info->Offset = Scene::Frame * info->ConstantParallax;
			info->Position =
				(info->Offset +
					((viewX + layerOffsetX) * info->RelativeParallax)) >>
				8;
			if (layer->Flags & SceneLayer::FLAGS_REPEAT_Y) {
				info->Position %= layerWidth;
				if (info->Position < 0) {
					info->Position += layerWidth;
				}
			}
			info++;
		}

		// Create scan lines
		Sint64 scrollOffset = Scene::Frame * layer->ConstantY;
		Sint64 scrollLine =
			(scrollOffset + ((viewY + layerOffsetY) * layer->RelativeY)) >> 8;
		scrollLine %= layerHeight;
		if (scrollLine < 0) {
			scrollLine += layerHeight;
		}

		int* deformValues;
		Uint8* parallaxIndex;
		TileScanLine* scanLine;
		const int maxDeformLineMask = (MAX_DEFORM_LINES >> 1) - 1;

		scanLine = TileScanLineBuffer;
		parallaxIndex = &layer->ScrollIndexes[scrollLine];
		deformValues =
			&layer->DeformSetA[(scrollLine + layer->DeformOffsetA) & maxDeformLineMask];
		for (int i = 0; i < layer->DeformSplitLine; i++) {
			// Set scan line start positions
			info = &layer->ScrollInfos[*parallaxIndex];
			scanLine->SrcX = info->Position;
			if (info->CanDeform) {
				scanLine->SrcX += *deformValues;
			}
			scanLine->SrcX <<= 16;
			scanLine->SrcY = scrollLine << 16;

			scanLine->DeltaX = 0x10000;
			scanLine->DeltaY = 0x0000;

			// Iterate lines
			// NOTE: There is no protection from over-reading deform indexes past 512 here.
			scanLine++;
			scrollLine++;
			deformValues++;

			// If we've reach the last line of the layer, return to the first.
			if (scrollLine == layerHeight) {
				scrollLine = 0;
				parallaxIndex = &layer->ScrollIndexes[scrollLine];
			}
			else {
				parallaxIndex++;
			}
		}

		deformValues =
			&layer->DeformSetB[(scrollLine + layer->DeformOffsetB) & maxDeformLineMask];
		for (int i = layer->DeformSplitLine; i < viewHeight; i++) {
			// Set scan line start positions
			info = &layer->ScrollInfos[*parallaxIndex];
			scanLine->SrcX = info->Position;
			if (info->CanDeform) {
				scanLine->SrcX += *deformValues;
			}
			scanLine->SrcX <<= 16;
			scanLine->SrcY = scrollLine << 16;

			scanLine->DeltaX = 0x10000;
			scanLine->DeltaY = 0x0000;

			// Iterate lines
			// NOTE: There is no protection from over-reading deform indexes past 512 here.
			scanLine++;
			scrollLine++;
			deformValues++;

			// If we've reach the last line of the layer, return to the first.
			if (scrollLine == layerHeight) {
				scrollLine = 0;
				parallaxIndex = &layer->ScrollIndexes[scrollLine];
			}
			else {
				parallaxIndex++;
			}
		}
		break;
	}
	case DrawBehavior_VerticalParallax: {
		break;
	}
	case DrawBehavior_CustomTileScanLines: {
		Sint64 scrollOffset = Scene::Frame * layer->ConstantY;
		Sint64 scrollPositionX =
			((scrollOffset +
				 (((int)currentView->X + layer->OffsetX) * layer->RelativeY)) >>
				8);
		scrollPositionX %= layer->Width * 16;
		scrollPositionX <<= 16;
		Sint64 scrollPositionY =
			((scrollOffset +
				 (((int)currentView->Y + layer->OffsetY) * layer->RelativeY)) >>
				8);
		scrollPositionY %= layer->Height * 16;
		scrollPositionY <<= 16;

		TileScanLine* scanLine = TileScanLineBuffer;
		for (int i = 0; i < currentView->Height; i++) {
			scanLine->SrcX = scrollPositionX;
			scanLine->SrcY = scrollPositionY;
			scanLine->DeltaX = 0x10000;
			scanLine->DeltaY = 0x0;

			scrollPositionY += 0x10000;
			scanLine++;
		}

		break;
	}
	}
}

void Graphics::DrawTile(int tile, int x, int y, bool flipX, bool flipY, bool usePaletteIndexLines) {
	TileSpriteInfo info = Scene::TileSpriteInfos[tile];

	int paletteID;
	if (usePaletteIndexLines) {
		paletteID = PALETTE_INDEX_TABLE_ID;
	}
	else {
		paletteID = Scene::Tilesets[info.TilesetID].PaletteID;
	}

	Graphics::GfxFunctions->DrawSprite(info.Sprite,
		info.AnimationIndex,
		info.FrameIndex,
		x,
		y,
		flipX,
		flipY,
		1.0f,
		1.0f,
		0.0,
		(int)paletteID);
}
void Graphics::DrawTilePart(int tile,
	int sx,
	int sy,
	int sw,
	int sh,
	int x,
	int y,
	bool flipX,
	bool flipY,
	bool usePaletteIndexLines) {
	TileSpriteInfo info = Scene::TileSpriteInfos[tile];

	int paletteID;
	if (usePaletteIndexLines) {
		paletteID = PALETTE_INDEX_TABLE_ID;
	}
	else {
		paletteID = Scene::Tilesets[info.TilesetID].PaletteID;
	}

	Graphics::GfxFunctions->DrawSpritePart(info.Sprite,
		info.AnimationIndex,
		info.FrameIndex,
		sx,
		sy,
		sw,
		sh,
		x,
		y,
		flipX,
		flipY,
		1.0f,
		1.0f,
		0.0,
		(int)paletteID);
}
void Graphics::DrawSceneLayer_HorizontalParallax(SceneLayer* layer, View* currentView) {
	int viewX = (int)currentView->X;
	int viewY = (int)currentView->Y;

	int tileWidth = Scene::TileWidth;
	int tileWidthHalf = tileWidth >> 1;
	int tileHeight = Scene::TileHeight;
	int tileHeightHalf = tileHeight >> 1;

	int layerWidthInBits = layer->WidthInBits;
	int layerWidthInPixels = layer->Width * tileWidth;
	int layerHeightInPixels = layer->Height * tileHeight;
	int layerWidthTileMask = layer->WidthMask;
	int layerHeightTileMask = layer->HeightMask;

	int layerOffsetX = layer->OffsetX;
	int layerOffsetY = layer->OffsetY;

	int startX = 0, startY = 0;
	int endX = (int)currentView->Width + tileWidth;
	int endY = (int)currentView->Height + tileHeight;

	int scrollOffset = Scene::Frame * layer->ConstantY;
	int srcY = (scrollOffset + ((viewY + layerOffsetY) * layer->RelativeY)) >> 8;
	int rowStartX = viewX + layerOffsetX;

	// Draw more of the view if it is being rotated on the Z axis
	if (currentView->RotateZ != 0.0f) {
		int offsetY = currentView->Height / 2;
		int offsetX = currentView->Width / 2;

		startY = -offsetY;
		endY += offsetY;
		srcY -= offsetY;

		startX = -offsetX;
		endX += offsetX;
		rowStartX -= offsetX;
	}

	bool usePaletteIndexLines = Graphics::UsePaletteIndexLines && layer->UsePaletteIndexLines;

	for (int dst_y = startY; dst_y < endY; dst_y += tileHeight, srcY += tileHeight) {
		if (srcY < 0 || srcY >= layerHeightInPixels) {
			if ((layer->Flags & SceneLayer::FLAGS_REPEAT_Y) == 0) {
				continue;
			}

			if (srcY < 0) {
				srcY = -(srcY % layerHeightInPixels);
			}
			else {
				srcY %= layerHeightInPixels;
			}
		}

		int srcX = rowStartX;
		for (int dst_x = startX; dst_x < endX; dst_x += tileWidth, srcX += tileWidth) {
			if (srcX < 0 || srcX >= layerWidthInPixels) {
				if ((layer->Flags & SceneLayer::FLAGS_REPEAT_X) == 0) {
					continue;
				}

				if (srcX < 0) {
					srcX = -(srcX % layerWidthInPixels);
				}
				else {
					srcX %= layerWidthInPixels;
				}
			}

			int sourceTileCellX = (srcX / tileWidth) & layerWidthTileMask;
			int sourceTileCellY = (srcY / tileHeight) & layerHeightTileMask;
			int tile = layer->Tiles[sourceTileCellX +
				(sourceTileCellY << layerWidthInBits)];

			int tileID = tile & TILE_IDENT_MASK;
			if (tileID == Scene::EmptyTile) {
				continue;
			}

			int srcTX = srcX & (tileWidth - 1);
			int srcTY = srcY & (tileHeight - 1);

			Graphics::DrawTile(tileID,
				viewX + ((dst_x - srcTX) + tileWidthHalf),
				viewY + ((dst_y - srcTY) + tileHeightHalf),
				(tile & TILE_FLIPX_MASK) != 0,
				(tile & TILE_FLIPY_MASK) != 0,
				usePaletteIndexLines);
		}
	}
}
void Graphics::DrawSceneLayer_HorizontalScrollIndexes(SceneLayer* layer, View* currentView) {
	int max_y = (int)currentView->Height;

	int tileWidth = Scene::TileWidth;
	int tileWidthHalf = tileWidth >> 1;
	int tileHeight = Scene::TileHeight;
	int tileHeightHalf = tileHeight >> 1;

	int layerWidthInBits = layer->WidthInBits;
	int layerWidthInPixels = layer->Width * tileWidth;
	int layerWidthTileMask = layer->WidthMask;
	int layerHeightTileMask = layer->HeightMask;

	bool usePaletteIndexLines = Graphics::UsePaletteIndexLines && layer->UsePaletteIndexLines;

	TileScanLine* scanLine = TileScanLineBuffer;
	for (int dst_y = 0; dst_y < max_y;) {
		Sint64 srcX = scanLine->SrcX >> 16;
		Sint64 srcY = scanLine->SrcY >> 16;

		int tileDrawHeight = 1;
		int check_y = dst_y + 1;
		while (check_y < max_y) {
			if (srcX != TileScanLineBuffer[check_y].SrcX >> 16) {
				break;
			}

			if ((srcY + tileDrawHeight) % tileHeight == 0) {
				break;
			}

			tileDrawHeight++;
			check_y++;
		}

		for (int dst_x = 0; dst_x < (int)currentView->Width + 16; dst_x += 16, srcX += 16) {
			bool isInLayer = srcX >= 0 && srcX < layerWidthInPixels;
			if (!isInLayer && layer->Flags & SceneLayer::FLAGS_REPEAT_X) {
				if (srcX < 0) {
					srcX = -(srcX % layerWidthInPixels);
				}
				else {
					srcX %= layerWidthInPixels;
				}
				isInLayer = true;
			}

			if (!isInLayer) {
				continue;
			}

			int sourceTileCellX = (srcX >> 4) & layerWidthTileMask;
			int sourceTileCellY = (srcY >> 4) & layerHeightTileMask;
			int tile = layer->Tiles[sourceTileCellX +
				(sourceTileCellY << layerWidthInBits)];

			if ((tile & TILE_IDENT_MASK) != Scene::EmptyTile) {
				int tileID = tile & TILE_IDENT_MASK;
				bool flipX = (tile & TILE_FLIPX_MASK) != 0;
				bool flipY = (tile & TILE_FLIPY_MASK) != 0;

				int srcTX = srcX & 15;
				int srcTY = srcY & 15;

				int partY = srcTY;
				if (flipY) {
					partY = tileHeight - partY - 1;
				}

				Graphics::DrawTilePart(tileID,
					0,
					partY,
					tileWidth,
					tileDrawHeight,
					currentView->X + ((dst_x - srcTX) + tileWidthHalf),
					currentView->Y + ((dst_y - srcTY) + tileHeightHalf),
					flipX,
					flipY,
					usePaletteIndexLines);
			}
		}

		scanLine += tileDrawHeight;
		dst_y += tileDrawHeight;
	}
}
void Graphics::DrawSceneLayer(SceneLayer* layer,
	View* currentView,
	int layerIndex,
	bool useCustomFunction) {
	// If possible, uses optimized software-renderer call instead.
	if (Graphics::GfxFunctions == &SoftwareRenderer::BackendFunctions) {
		SoftwareRenderer::DrawSceneLayer(layer, currentView, layerIndex, useCustomFunction);
		return;
	}

	if (layer->UsingCustomRenderFunction && useCustomFunction) {
		Graphics::RunCustomSceneLayerFunction(&layer->CustomRenderFunction, layerIndex);
		return;
	}

	Shader* shader = (Shader*)layer->CurrentShader;
	if (shader) {
		Graphics::SetUserShader(shader);
	}

	if (layer->UsingScrollIndexes) {
		Graphics::DrawSceneLayer_InitTileScanLines(layer, currentView);
		Graphics::DrawSceneLayer_HorizontalScrollIndexes(layer, currentView);
	}
	else {
		Graphics::DrawSceneLayer_HorizontalParallax(layer, currentView);
	}

	if (shader) {
		Graphics::SetUserShader(nullptr);
	}
}
void Graphics::RunCustomSceneLayerFunction(ObjFunction* func, int layerIndex) {
	VMThread* thread = &ScriptManager::Threads[0];
	if (func->Arity == 0) {
		thread->RunEntityFunction(func, 0);
	}
	else {
		thread->Push(INTEGER_VAL(layerIndex));
		thread->RunEntityFunction(func, 1);
	}
}

// 3D drawing
void Graphics::DrawPolygon3D(void* data,
	int vertexCount,
	int vertexFlag,
	Texture* texture,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (Graphics::GfxFunctions->DrawPolygon3D) {
		Graphics::GfxFunctions->DrawPolygon3D(
			data, vertexCount, vertexFlag, texture, modelMatrix, normalMatrix);
	}
}
void Graphics::DrawSceneLayer3D(void* layer,
	int sx,
	int sy,
	int sw,
	int sh,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (Graphics::GfxFunctions->DrawSceneLayer3D) {
		Graphics::GfxFunctions->DrawSceneLayer3D(
			layer, sx, sy, sw, sh, modelMatrix, normalMatrix);
	}
}
void Graphics::DrawModel(void* model,
	Uint16 animation,
	Uint32 frame,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (Graphics::GfxFunctions->DrawModel) {
		Graphics::GfxFunctions->DrawModel(
			model, animation, frame, modelMatrix, normalMatrix);
	}
}
void Graphics::DrawModelSkinned(void* model,
	Uint16 armature,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (Graphics::GfxFunctions->DrawModelSkinned) {
		Graphics::GfxFunctions->DrawModelSkinned(
			model, armature, modelMatrix, normalMatrix);
	}
}
void Graphics::DrawVertexBuffer(Uint32 vertexBufferIndex,
	Matrix4x4* modelMatrix,
	Matrix4x4* normalMatrix) {
	if (Graphics::GfxFunctions->DrawVertexBuffer) {
		Graphics::GfxFunctions->DrawVertexBuffer(
			vertexBufferIndex, modelMatrix, normalMatrix);
	}
}
void Graphics::BindVertexBuffer(Uint32 vertexBufferIndex) {
	if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		UnbindVertexBuffer();
		return;
	}

	if (Graphics::GfxFunctions->BindVertexBuffer) {
		Graphics::GfxFunctions->BindVertexBuffer(vertexBufferIndex);
	}
	CurrentVertexBuffer = vertexBufferIndex;
}
void Graphics::UnbindVertexBuffer() {
	if (Graphics::GfxFunctions->UnbindVertexBuffer) {
		Graphics::GfxFunctions->UnbindVertexBuffer();
	}
	CurrentVertexBuffer = -1;
}
void Graphics::BindScene3D(Uint32 sceneIndex) {
	if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];

	if (!scene->UseCustomProjectionMatrix) {
		View* currentView = Graphics::CurrentView;
		if (currentView) {
			float fov = scene->FOV * M_PI / 180.0f;
			float aspect = currentView->Width / currentView->Height;
			Graphics::MakePerspectiveMatrix(&scene->ProjectionMatrix,
				fov,
				scene->NearClippingPlane,
				scene->FarClippingPlane,
				aspect);
		}
	}

	if (Graphics::GfxFunctions->BindScene3D) {
		Graphics::GfxFunctions->BindScene3D(sceneIndex);
	}
	CurrentScene3D = sceneIndex;
}
void Graphics::ClearScene3D(Uint32 sceneIndex) {
	if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
	scene->Clear();

	if (Graphics::GfxFunctions->ClearScene3D) {
		Graphics::GfxFunctions->ClearScene3D(sceneIndex);
	}
}
void Graphics::DrawScene3D(Uint32 sceneIndex, Uint32 drawMode) {
	if (Graphics::GfxFunctions->DrawScene3D) {
		Graphics::GfxFunctions->DrawScene3D(sceneIndex, drawMode);
	}
}

Uint32 Graphics::CreateScene3D(int unloadPolicy) {
	for (Uint32 i = 0; i < MAX_3D_SCENES; i++) {
		if (!Graphics::Scene3Ds[i].Initialized) {
			Graphics::InitScene3D(i, 0);
			Graphics::Scene3Ds[i].UnloadPolicy = unloadPolicy;
			return i;
		}
	}

	return 0xFFFFFFFF;
}
void Graphics::DeleteScene3D(Uint32 sceneIndex) {
	if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];
	if (scene->Initialized) {
		if (Graphics::Internal.DeleteVertexBuffer) {
			Graphics::Internal.DeleteVertexBuffer(scene->Buffer);
		}
		else {
			delete scene->Buffer;
		}

		scene->Initialized = false;
	}
}
void Graphics::InitScene3D(Uint32 sceneIndex, Uint32 numVertices) {
	if (sceneIndex < 0 || sceneIndex >= MAX_3D_SCENES) {
		return;
	}

	Scene3D* scene = &Graphics::Scene3Ds[sceneIndex];

	Matrix4x4 viewMat;
	Matrix4x4::Identity(&viewMat);
	scene->SetViewMatrix(&viewMat);

	if (!numVertices) {
		numVertices = 192;
	}

	if (Graphics::Internal.CreateVertexBuffer) {
		scene->Buffer = (VertexBuffer*)Graphics::Internal.CreateVertexBuffer(numVertices);
	}
	else {
		scene->Buffer = new VertexBuffer(numVertices);
	}

	scene->ClipPolygons = true;
	scene->Initialized = true;
	scene->UnloadPolicy = SCOPE_GAME;

	scene->Clear();
}

void Graphics::MakeSpritePolygon(VertexAttribute data[4],
	float x,
	float y,
	float z,
	int flipX,
	int flipY,
	float scaleX,
	float scaleY,
	Texture* texture,
	int frameX,
	int frameY,
	int frameW,
	int frameH) {
	data[3].Position.X = FP16_TO(x);
	data[3].Position.Y = FP16_TO(y - (frameH * scaleY));
	data[3].Position.Z = FP16_TO(z);

	data[2].Position.X = FP16_TO(x + (frameW * scaleX));
	data[2].Position.Y = FP16_TO(y - (frameH * scaleY));
	data[2].Position.Z = FP16_TO(z);

	data[1].Position.X = FP16_TO(x + (frameW * scaleX));
	data[1].Position.Y = FP16_TO(y);
	data[1].Position.Z = FP16_TO(z);

	data[0].Position.X = FP16_TO(x);
	data[0].Position.Y = FP16_TO(y);
	data[0].Position.Z = FP16_TO(z);

	for (int i = 0; i < 4; i++) {
		data[i].Normal.X = data[i].Normal.Y = data[i].Normal.Z = data[i].Normal.W = 0;
		data[i].Position.W = 0;
	}

	Graphics::MakeSpritePolygonUVs(data, flipX, flipY, texture, frameX, frameY, frameW, frameH);
}
void Graphics::MakeSpritePolygonUVs(VertexAttribute data[4],
	int flipX,
	int flipY,
	Texture* texture,
	int frameX,
	int frameY,
	int frameW,
	int frameH) {
	Sint64 textureWidth = FP16_TO(texture->Width);
	Sint64 textureHeight = FP16_TO(texture->Height);

	int uv_left = frameX;
	int uv_right = frameX + frameW;
	int uv_top = frameY;
	int uv_bottom = frameY + frameH;

	int left_u, right_u, top_v, bottom_v;

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

	data[3].UV.X = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
	data[3].UV.Y = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);

	data[2].UV.X = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
	data[2].UV.Y = FP16_DIVIDE(FP16_TO(bottom_v), textureHeight);

	data[1].UV.X = FP16_DIVIDE(FP16_TO(right_u), textureWidth);
	data[1].UV.Y = FP16_DIVIDE(FP16_TO(top_v), textureHeight);

	data[0].UV.X = FP16_DIVIDE(FP16_TO(left_u), textureWidth);
	data[0].UV.Y = FP16_DIVIDE(FP16_TO(top_v), textureHeight);
}

void Graphics::MakeFrameBufferID(ISprite* sprite) {
	if (Graphics::GfxFunctions->MakeFrameBufferID) {
		Graphics::GfxFunctions->MakeFrameBufferID(sprite);
	}
}
void Graphics::DeleteFrameBufferID(ISprite* sprite) {
	if (Graphics::GfxFunctions->DeleteFrameBufferID) {
		Graphics::GfxFunctions->DeleteFrameBufferID(sprite);
	}
}

void Graphics::SetDepthTesting(bool enabled) {
	if (Graphics::GfxFunctions->SetDepthTesting) {
		Graphics::GfxFunctions->SetDepthTesting(enabled);
	}
}

bool Graphics::SpriteRangeCheck(ISprite* sprite, int animation, int frame) {
	// #ifdef DEBUG
	if (!sprite) {
		return true;
	}
	if (animation < 0 || animation >= (int)sprite->Animations.size()) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Animation %d does not exist in sprite %s!",
			animation,
			sprite->Filename);
		return true;
	}
	if (frame < 0 || frame >= (int)sprite->Animations[animation].Frames.size()) {
		ScriptManager::Threads[0].ThrowRuntimeError(false,
			"Frame %d in animation \"%s\" does not exist in sprite %s!",
			frame,
			sprite->Animations[animation].Name,
			sprite->Filename);
		return true;
	}
	// #endif
	return false;
}

void Graphics::ConvertFromARGBtoNative(Uint32* argb, int count) {
	if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ABGR8888) {
		ColorUtils::ConvertFromARGBtoABGR(argb, count);
	}
}
void Graphics::ConvertFromNativeToARGB(Uint32* argb, int count) {
	if (Graphics::PreferredPixelFormat == SDL_PIXELFORMAT_ABGR8888) {
		ColorUtils::ConvertFromABGRtoARGB(argb, count);
	}
}

void Graphics::SetStencilEnabled(bool enabled) {
	Graphics::StencilEnabled = enabled;

	if (Graphics::GfxFunctions->SetStencilEnabled) {
		Graphics::GfxFunctions->SetStencilEnabled(enabled);
	}
}
void Graphics::SetStencilTestFunc(int stencilTest) {
	if (stencilTest >= StencilTest_Never && stencilTest <= StencilTest_GEqual) {
		StencilTest = stencilTest;
		if (Graphics::GfxFunctions->SetStencilTestFunc) {
			Graphics::GfxFunctions->SetStencilTestFunc(stencilTest);
		}
	}
}
void Graphics::SetStencilPassFunc(int stencilOp) {
	if (stencilOp >= StencilOp_Keep && stencilOp <= StencilOp_DecrWrap) {
		StencilOpPass = stencilOp;
		if (Graphics::GfxFunctions->SetStencilPassFunc) {
			Graphics::GfxFunctions->SetStencilPassFunc(stencilOp);
		}
	}
}
void Graphics::SetStencilFailFunc(int stencilOp) {
	if (stencilOp >= StencilOp_Keep && stencilOp <= StencilOp_DecrWrap) {
		StencilOpFail = stencilOp;
		if (Graphics::GfxFunctions->SetStencilFailFunc) {
			Graphics::GfxFunctions->SetStencilFailFunc(stencilOp);
		}
	}
}
void Graphics::SetStencilValue(int value) {
	if (value < 0) {
		value = 0;
	}
	else if (value > 255) {
		value = 255;
	}
	if (Graphics::GfxFunctions->SetStencilValue) {
		Graphics::GfxFunctions->SetStencilValue(value);
	}
}
void Graphics::SetStencilMask(int mask) {
	if (mask < 0) {
		mask = 0;
	}
	else if (mask > 255) {
		mask = 255;
	}
	if (Graphics::GfxFunctions->SetStencilMask) {
		Graphics::GfxFunctions->SetStencilMask(mask);
	}
}
void Graphics::ClearStencil() {
	if (Graphics::GfxFunctions->ClearStencil) {
		Graphics::GfxFunctions->ClearStencil();
	}
}
