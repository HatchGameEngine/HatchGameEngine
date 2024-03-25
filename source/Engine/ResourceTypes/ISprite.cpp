#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Sprites/Animation.h>
#include <Engine/Rendering/Texture.h>

class ISprite {
public:
    char              Filename[256];

    Texture*          Spritesheets[32];
    bool              SpritesheetsBorrowed[32];
    char              SpritesheetsFilenames[128][32];
    int               SpritesheetCount = 0;
    int               CollisionBoxCount = 0;

    vector<Animation> Animations;
};
#endif

#define MAX_SPRITESHEETS 32

#include <Engine/ResourceTypes/ISprite.h>

#include <Engine/Application.h>
#include <Engine/Graphics.h>

#include <Engine/ResourceTypes/ImageFormats/GIF.h>
#include <Engine/ResourceTypes/ImageFormats/JPEG.h>
#include <Engine/ResourceTypes/ImageFormats/PNG.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Memory.h>

#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>

#include <Engine/Utilities/StringUtils.h>

PUBLIC ISprite::ISprite() {
    memset(Spritesheets, 0, sizeof(Spritesheets));
    memset(SpritesheetsBorrowed, 0, sizeof(SpritesheetsBorrowed));
    memset(Filename, 0, 256);
}
PUBLIC ISprite::ISprite(const char* filename) {
    memset(Spritesheets, 0, sizeof(Spritesheets));
    memset(SpritesheetsBorrowed, 0, sizeof(SpritesheetsBorrowed));
    memset(Filename, 0, 256);

    strncpy(Filename, filename, 255);
    LoadAnimation(Filename);
}

PUBLIC STATIC Texture* ISprite::AddSpriteSheet(const char* filename) {
    Texture* texture = NULL;
    Uint32*  data = NULL;
    Uint32   width = 0;
    Uint32   height = 0;
    Uint32*  paletteColors = NULL;
    unsigned numPaletteColors = 0;

    const char* altered = filename;

    if (Graphics::SpriteSheetTextureMap->Exists(altered)) {
        texture = Graphics::SpriteSheetTextureMap->Get(altered);
        return texture;
    }

    float loadDelta = 0.0f;
    if (StringUtils::StrCaseStr(altered, ".png")) {
        Clock::Start();
        PNG* png = PNG::Load(altered);
        loadDelta = Clock::End();

        if (png && png->Data) {
            Log::Print(Log::LOG_VERBOSE, "PNG load took %.3f ms (%s)", loadDelta, altered);
            width = png->Width;
            height = png->Height;

            data = png->Data;
            Memory::Track(data, "Texture::Data");

            if (png->Paletted) {
                paletteColors = png->GetPalette();
                numPaletteColors = png->NumPaletteColors;
            }

            delete png;
        }
        else {
            Log::Print(Log::LOG_ERROR, "PNG could not be loaded!");
            return NULL;
        }
    }
    else if (StringUtils::StrCaseStr(altered, ".jpg") || StringUtils::StrCaseStr(altered, ".jpeg")) {
        Clock::Start();
        JPEG* jpeg = JPEG::Load(altered);
        loadDelta = Clock::End();

        if (jpeg && jpeg->Data) {
            Log::Print(Log::LOG_VERBOSE, "JPEG load took %.3f ms (%s)", loadDelta, altered);
            width = jpeg->Width;
            height = jpeg->Height;

            data = jpeg->Data;
            Memory::Track(data, "Texture::Data");

            delete jpeg;
        }
        else {
            Log::Print(Log::LOG_ERROR, "JPEG could not be loaded!");
            return NULL;
        }
    }
    else if (StringUtils::StrCaseStr(altered, ".gif")) {
        Clock::Start();
        GIF* gif = GIF::Load(altered);
        loadDelta = Clock::End();

        if (gif && gif->Data) {
            Log::Print(Log::LOG_VERBOSE, "GIF load took %.3f ms (%s)", loadDelta, altered);
            width = gif->Width;
            height = gif->Height;

            data = gif->Data;
            Memory::Track(data, "Texture::Data");

            if (gif->Paletted) {
                paletteColors = gif->GetPalette();
                numPaletteColors = gif->NumPaletteColors;
            }

            delete gif;
        }
        else {
            Log::Print(Log::LOG_ERROR, "GIF could not be loaded!");
            return NULL;
        }
    }
    else {
        Log::Print(Log::LOG_ERROR, "Unsupported image format for sprite!");
        return texture;
    }

    bool forceSoftwareTextures = false;
    Application::Settings->GetBool("display", "forceSoftwareTextures", &forceSoftwareTextures);
    if (forceSoftwareTextures)
        Graphics::NoInternalTextures = true;

    texture = Graphics::CreateTextureFromPixels(width, height, data, width * sizeof(Uint32));

    Graphics::SetTexturePalette(texture, paletteColors, numPaletteColors);

    Graphics::NoInternalTextures = false;

    Memory::Free(data);

    Graphics::SpriteSheetTextureMap->Put(altered, texture);

    return texture;
}

PUBLIC void ISprite::ReserveAnimationCount(int count) {
    Animations.reserve(count);
}
PUBLIC void ISprite::AddAnimation(const char* name, int animationSpeed, int frameToLoop) {
    size_t strl = strlen(name);

    Animation an;
    an.Name = (char*)Memory::Malloc(strl + 1);
    strcpy(an.Name, name);
    an.Name[strl] = 0;
    an.AnimationSpeed = animationSpeed;
    an.FrameToLoop = frameToLoop;
    an.Flags = 0;
    Animations.push_back(an);
}
PUBLIC void ISprite::AddAnimation(const char* name, int animationSpeed, int frameToLoop, int frmAlloc) {
    AddAnimation(name, animationSpeed, frameToLoop);
    Animations.back().Frames.reserve(frmAlloc);
}
PUBLIC void ISprite::AddFrame(int duration, int left, int top, int width, int height, int pivotX, int pivotY) {
    ISprite::AddFrame(duration, left, top, width, height, pivotX, pivotY, 0);
}
PUBLIC void ISprite::AddFrame(int duration, int left, int top, int width, int height, int pivotX, int pivotY, int id) {
    AddFrame(Animations.size() - 1, duration, left, top, width, height, pivotX, pivotY, id);
}
PUBLIC void ISprite::AddFrame(int animID, int duration, int left, int top, int width, int height, int pivotX, int pivotY, int id) {
    AnimFrame anfrm;
    anfrm.Advance = id;
    anfrm.Duration = duration;
    anfrm.X = left;
    anfrm.Y = top;
    anfrm.Width = width;
    anfrm.Height = height;
    anfrm.OffsetX = pivotX;
    anfrm.OffsetY = pivotY;
    anfrm.SheetNumber = 0;
    anfrm.BoxCount = 0;

    // Possibly buffer the position in the renderer.
    Graphics::MakeFrameBufferID(this, &anfrm);

    Animations[animID].Frames.push_back(anfrm);
}
PUBLIC void ISprite::RemoveFrames(int animID) {
    for (size_t i = 0; i < Animations[animID].Frames.size(); i++)
        Graphics::DeleteFrameBufferID(&Animations[animID].Frames[i]);
    Animations[animID].Frames.clear();
}

PUBLIC void ISprite::ConvertToRGBA() {
    for (int a = 0; a < SpritesheetCount; a++) {
        if (Spritesheets[a])
            Graphics::ConvertTextureToRGBA(Spritesheets[a]);
    }
}
PUBLIC void ISprite::ConvertToPalette(unsigned paletteNumber) {
    for (int a = 0; a < SpritesheetCount; a++) {
        if (Spritesheets[a])
            Graphics::ConvertTextureToPalette(Spritesheets[a], paletteNumber);
    }
}

PUBLIC bool ISprite::LoadAnimation(const char* filename) {
    char* str, altered[4096];
    int animationCount, previousAnimationCount;

    Stream* reader = ResourceStream::New(filename);
    if (!reader) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
    }

#ifdef ISPRITE_DEBUG
    Log::Print(Log::LOG_VERBOSE, "\"%s\"", filename);
#endif

    /// =======================
    /// RSDKv5 Animation Format
    /// =======================

    // Check MAGIC
    if (reader->ReadUInt32() != 0x00525053) {
        reader->Close();
        return false;
    }

    // Total frame count
    reader->ReadUInt32();

    // Get texture count
    this->SpritesheetCount = reader->ReadByte();

    // Load textures
    for (int i = 0; i < this->SpritesheetCount; i++) {
        if (i >= MAX_SPRITESHEETS) {
            Log::Print(Log::LOG_ERROR, "Too many spritesheets in sprite %s!", filename);
            exit(-1);
        }

        str = reader->ReadHeaderedString();
#ifdef ISPRITE_DEBUG
        Log::Print(Log::LOG_VERBOSE, " - %s", str);
#endif

        strcpy(SpritesheetsFilenames[i], str);

        snprintf(altered, sizeof altered, "Sprites/%s", str);
        Memory::Free(str);

        if (Graphics::SpriteSheetTextureMap->Exists(altered))
            SpritesheetsBorrowed[i] = true;

        Spritesheets[i] = AddSpriteSheet(altered);
        // Spritesheets[i] = Image::LoadTextureFromResource(altered);
    }

    // Get collision group count
    this->CollisionBoxCount = reader->ReadByte();

    // Load collision groups
    for (int i = 0; i < this->CollisionBoxCount; i++) {
        Memory::Free(reader->ReadHeaderedString());
    }

    animationCount = reader->ReadUInt16();
    previousAnimationCount = (int)Animations.size();
    Animations.resize(previousAnimationCount + animationCount);

    // Load animations
    int frameID = 0;
    for (int a = 0; a < animationCount; a++) {
        Animation an;
        an.Name = reader->ReadHeaderedString();
        an.FrameCount = reader->ReadUInt16();
        an.FrameListOffset = frameID;
        an.AnimationSpeed = reader->ReadUInt16();
        an.FrameToLoop = reader->ReadByte();

        // 0: No rotation
        // 1: Full rotation
        // 2: Snaps to multiples of 45 degrees
        // 3: Snaps to multiples of 90 degrees
        // 4: Snaps to multiples of 180 degrees
        // 5: Static rotation using extra frames
        an.Flags = reader->ReadByte();

#ifdef ISPRITE_DEBUG
        Log::Print(Log::LOG_VERBOSE, "    \"%s\" (%d) (Flags: %02X, FtL: %d, Spd: %d, Frames: %d)", an.Name, a, an.Flags, an.FrameToLoop, an.AnimationSpeed, frameCount);
#endif
        an.Frames.resize(an.FrameCount);

        for (int i = 0; i < an.FrameCount; i++) {
            AnimFrame anfrm;
            anfrm.SheetNumber = reader->ReadByte();
            frameID++;

            if (anfrm.SheetNumber >= SpritesheetCount)
                Log::Print(Log::LOG_ERROR, "Sheet number %d outside of range of sheet count %d! (Animation %d, Frame %d)", anfrm.SheetNumber, SpritesheetCount, a, i);

            anfrm.Duration = reader->ReadInt16();
            anfrm.Advance = reader->ReadUInt16();
            anfrm.X = reader->ReadUInt16();
            anfrm.Y = reader->ReadUInt16();
            anfrm.Width = reader->ReadUInt16();
            anfrm.Height = reader->ReadUInt16();
            anfrm.OffsetX = reader->ReadInt16();
            anfrm.OffsetY = reader->ReadInt16();

            anfrm.BoxCount = this->CollisionBoxCount;
            if (anfrm.BoxCount) {
                anfrm.Boxes = (CollisionBox*)Memory::Malloc(anfrm.BoxCount * sizeof(CollisionBox));
                for (int h = 0; h < anfrm.BoxCount; h++) {
                    anfrm.Boxes[h].Left = reader->ReadInt16();
                    anfrm.Boxes[h].Top = reader->ReadInt16();
                    anfrm.Boxes[h].Right = reader->ReadInt16();
                    anfrm.Boxes[h].Bottom = reader->ReadInt16();
                }
            }

            // Possibly buffer the position in the renderer.
            Graphics::MakeFrameBufferID(this, &anfrm);

#ifdef ISPRITE_DEBUG
            Log::Print(Log::LOG_VERBOSE, "       (X: %d, Y: %d, W: %d, H: %d, OffX: %d, OffY: %d)", anfrm.X, anfrm.Y, anfrm.Width, anfrm.Height, anfrm.OffsetX, anfrm.OffsetY);
#endif
            an.Frames[i] = anfrm;
        }
        Animations[previousAnimationCount + a] = an;
    }
    reader->Close();

    return true;
}
PUBLIC int  ISprite::FindAnimation(const char* animname) {
    for (Uint32 a = 0; a < Animations.size(); a++)
        if (Animations[a].Name[0] == animname[0] && !strcmp(Animations[a].Name, animname))
            return a;

    return -1;
}
PUBLIC void ISprite::LinkAnimation(vector<Animation> ani) {
    Animations = ani;
}
PUBLIC bool ISprite::SaveAnimation(const char* filename) {
    Stream* stream = FileStream::New(filename, FileStream::WRITE_ACCESS);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
    }

    /// =======================
    /// RSDKv5 Animation Format
    /// =======================

    // Check MAGIC
    stream->WriteUInt32(0x00525053);

    // Total frame count
    Uint32 totalFrameCount = 0;
    for (size_t a = 0; a < Animations.size(); a++) {
        totalFrameCount += Animations[a].Frames.size();
    }
    stream->WriteUInt32(totalFrameCount);

    // Get texture count
    stream->WriteByte(this->SpritesheetCount);

    // Load textures
    for (int i = 0; i < this->SpritesheetCount; i++) {
        stream->WriteHeaderedString(SpritesheetsFilenames[i]);
    }

    // Get collision group count
    stream->WriteByte(this->CollisionBoxCount);

    // Write collision groups
    for (int i = 0; i < this->CollisionBoxCount; i++) {
        stream->WriteHeaderedString("Dummy");
    }

    // Get animation count
    stream->WriteUInt16(Animations.size());

    // Write animations
    for (size_t a = 0; a < Animations.size(); a++) {
        Animation an = Animations[a];
        stream->WriteHeaderedString(an.Name);
        stream->WriteUInt16(an.Frames.size());
        stream->WriteUInt16(an.AnimationSpeed);
        stream->WriteByte(an.FrameToLoop);

        // 0: No rotation
        // 1: Full rotation
        // 2: Snaps to multiples of 45 degrees
        // 3: Snaps to multiples of 90 degrees
        // 4: Snaps to multiples of 180 degrees
        // 5: Static rotation using extra frames
        stream->WriteByte(an.Flags);

        for (size_t i = 0; i < an.Frames.size(); i++) {
            AnimFrame anfrm = an.Frames[i];
            stream->WriteByte(anfrm.SheetNumber);
            stream->WriteUInt16(anfrm.Duration);
            stream->WriteUInt16(anfrm.Advance);
            stream->WriteUInt16(anfrm.X);
            stream->WriteUInt16(anfrm.Y);
            stream->WriteUInt16(anfrm.Width);
            stream->WriteUInt16(anfrm.Height);
            stream->WriteInt16(anfrm.OffsetX);
            stream->WriteInt16(anfrm.OffsetY);

            for (int h = 0; h < anfrm.BoxCount; h++) {
                stream->WriteUInt16(anfrm.Boxes[h].Left);
                stream->WriteUInt16(anfrm.Boxes[h].Top);
                stream->WriteUInt16(anfrm.Boxes[h].Right);
                stream->WriteUInt16(anfrm.Boxes[h].Bottom);
            }
        }
    }
    stream->Close();

    return true;
}

PUBLIC void ISprite::Dispose() {
    for (size_t a = 0; a < Animations.size(); a++) {
        for (size_t i = 0; i < Animations[a].Frames.size(); i++) {
            AnimFrame* anfrm = &Animations[a].Frames[i];
            if (anfrm->BoxCount) {
                Memory::Free(anfrm->Boxes);
                anfrm->Boxes = NULL;
            }
        }
        if (Animations[a].Name) {
            Memory::Free(Animations[a].Name);
            Animations[a].Name = NULL;
        }

        RemoveFrames(a);

        Animations[a].Frames.clear();
        Animations[a].Frames.shrink_to_fit();
    }

    Animations.clear();
    Animations.shrink_to_fit();

    for (int a = 0; a < SpritesheetCount; a++) {
        if (Spritesheets[a]) {
            // if (!SpritesheetsBorrowed[a])
            //     Graphics::DisposeTexture(Spritesheets[a]);
            Spritesheets[a] = NULL;
        }
    }
}

PUBLIC ISprite::~ISprite() {
    Dispose();
}
