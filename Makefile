#######################################################################
##                         HATCH Game Engine                         ##
##                                                                   ##
##               Copyright (C) 2019-2024 ARQADIUM LLC.               ##
##             Copyright (C) 2024-2025 Alexander Nicholi             ##
##                    Released under BSD-2-Clause                    ##
#######################################################################

include meta/prologue.mk

# name of project. used in output binary naming
PROJECT := hatch

# put a ‘1’ for the desired target types to compile
EXEFILE := 1
SOFILE  :=
AFILE   :=

# space-separated path list for #includes
# <system> includes
INCLUDES := include source
# "local" includes
INCLUDEL := include source

# space-separated library name list for linking
# libraries
LIBS      := assimp ogg vorbis vorbisfile SDL2
LIBDIRS   :=
FWORKS    := Cocoa CoreFoundation CoreServices Foundation OpenGL
FWORKDIRS :=

CFILES := \
	source/Libraries/miniz.c \
	source/Libraries/spng.c \
	source/Libraries/stb_vorbis.c
CPPFILES := \
	source/Libraries/Clipper2/clipper.engine.cpp \
	source/Libraries/Clipper2/clipper.offset.cpp \
	source/Libraries/Clipper2/clipper.rectclip.cpp \
	source/Libraries/poly2tri/common/shapes.cpp \
	source/Libraries/poly2tri/sweep/sweep.cpp \
	source/Libraries/poly2tri/sweep/sweep_context.cpp \
	source/Libraries/poly2tri/sweep/advancing_front.cpp \
	source/Libraries/poly2tri/sweep/cdt.cpp \
	source/Engine/Types/Tileset.cpp \
	source/Engine/Types/DrawGroupList.cpp \
	source/Engine/Types/Entity.cpp \
	source/Engine/Types/ObjectRegistry.cpp \
	source/Engine/Types/ObjectList.cpp \
	source/Engine/FontFace.cpp \
	source/Engine/Scene.cpp \
	source/Engine/Input/InputAction.cpp \
	source/Engine/Input/Controller.cpp \
	source/Engine/Input/InputPlayer.cpp \
	source/Engine/Network/AndroidWifiP2P.cpp \
	source/Engine/Network/WebSocketClient.cpp \
	source/Engine/Network/HTTP.cpp \
	source/Engine/IO/TextStream.cpp \
	source/Engine/IO/MemoryStream.cpp \
	source/Engine/IO/ResourceStream.cpp \
	source/Engine/IO/FileStream.cpp \
	source/Engine/IO/VirtualFileStream.cpp \
	source/Engine/IO/Stream.cpp \
	source/Engine/IO/StandardIOStream.cpp \
	source/Engine/IO/Compression/RunLength.cpp \
	source/Engine/IO/Compression/ZLibStream.cpp \
	source/Engine/IO/Compression/LZ11.cpp \
	source/Engine/IO/Compression/LZSS.cpp \
	source/Engine/IO/Compression/Huffman.cpp \
	source/Engine/IO/SDLStream.cpp \
	source/Engine/IO/Serializer.cpp \
	source/Engine/IO/NetworkStream.cpp \
	source/Engine/Filesystem/VFS/MemoryVFS.cpp \
	source/Engine/Filesystem/VFS/FileSystemVFS.cpp \
	source/Engine/Filesystem/VFS/ArchiveVFS.cpp \
	source/Engine/Filesystem/VFS/VFSProvider.cpp \
	source/Engine/Filesystem/VFS/MemoryCache.cpp \
	source/Engine/Filesystem/VFS/VirtualFileSystem.cpp \
	source/Engine/Filesystem/VFS/HatchVFS.cpp \
	source/Engine/Filesystem/VFS/VFSEntry.cpp \
	source/Engine/Filesystem/Directory.cpp \
	source/Engine/Filesystem/File.cpp \
	source/Engine/Filesystem/Path.cpp \
	source/Engine/ResourceTypes/SoundFormats/SoundFormat.cpp \
	source/Engine/ResourceTypes/SoundFormats/OGG.cpp \
	source/Engine/ResourceTypes/SoundFormats/WAV.cpp \
	source/Engine/ResourceTypes/Image.cpp \
	source/Engine/ResourceTypes/SceneFormats/HatchSceneReader.cpp \
	source/Engine/ResourceTypes/SceneFormats/TiledMapReader.cpp \
	source/Engine/ResourceTypes/SceneFormats/RSDKSceneReader.cpp \
	source/Engine/ResourceTypes/ISprite.cpp \
	source/Engine/ResourceTypes/ISound.cpp \
	source/Engine/ResourceTypes/IModel.cpp \
	source/Engine/ResourceTypes/ImageFormats/JPEG.cpp \
	source/Engine/ResourceTypes/ImageFormats/PNG.cpp \
	source/Engine/ResourceTypes/ImageFormats/GIF.cpp \
	source/Engine/ResourceTypes/ImageFormats/ImageFormat.cpp \
	source/Engine/ResourceTypes/ResourceManager.cpp \
	source/Engine/ResourceTypes/ModelFormats/HatchModel.cpp \
	source/Engine/ResourceTypes/ModelFormats/RSDKModel.cpp \
	source/Engine/ResourceTypes/ModelFormats/MD3Model.cpp \
	source/Engine/ResourceTypes/ModelFormats/Importer.cpp \
	source/Engine/Hashing/MD5.cpp \
	source/Engine/Hashing/CombinedHash.cpp \
	source/Engine/Hashing/CRC32.cpp \
	source/Engine/Hashing/Murmur.cpp \
	source/Engine/Hashing/FNV1A.cpp \
	source/Engine/Math/Geometry.cpp \
	source/Engine/Math/Matrix4x4.cpp \
	source/Engine/Math/Clipper.cpp \
	source/Engine/Math/Math.cpp \
	source/Engine/Math/Random.cpp \
	source/Engine/Math/Vector.cpp \
	source/Engine/Math/Ease.cpp \
	source/Engine/Extensions/Discord.cpp \
	source/Engine/InputManager.cpp \
	source/Engine/Graphics.cpp \
	source/Engine/Utilities/ColorUtils.cpp \
	source/Engine/Utilities/StringUtils.cpp \
	source/Engine/Audio/AudioPlayback.cpp \
	source/Engine/Audio/AudioManager.cpp \
	source/Engine/Diagnostics/Memory.cpp \
	source/Engine/Diagnostics/Log.cpp \
	source/Engine/Diagnostics/Clock.cpp \
	source/Engine/Diagnostics/MemoryPools.cpp \
	source/Engine/Diagnostics/RemoteDebug.cpp \
	source/Engine/Diagnostics/PerformanceMeasure.cpp \
	source/Engine/Bytecode/Types.cpp \
	source/Engine/Bytecode/Bytecode.cpp \
	source/Engine/Bytecode/Compiler.cpp \
	source/Engine/Bytecode/StandardLibrary.cpp \
	source/Engine/Bytecode/VMThread.cpp \
	source/Engine/Bytecode/GarbageCollector.cpp \
	source/Engine/Bytecode/TypeImpl/ArrayImpl.cpp \
	source/Engine/Bytecode/TypeImpl/MapImpl.cpp \
	source/Engine/Bytecode/TypeImpl/StringImpl.cpp \
	source/Engine/Bytecode/TypeImpl/MaterialImpl.cpp \
	source/Engine/Bytecode/TypeImpl/FunctionImpl.cpp \
	source/Engine/Bytecode/SourceFileMap.cpp \
	source/Engine/Bytecode/ScriptManager.cpp \
	source/Engine/Bytecode/ScriptEntity.cpp \
	source/Engine/Bytecode/Values.cpp \
	source/Engine/Rendering/VertexBuffer.cpp \
	source/Engine/Rendering/FaceInfo.cpp \
	source/Engine/Rendering/ViewTexture.cpp \
	source/Engine/Rendering/Texture.cpp \
	source/Engine/Rendering/ModelRenderer.cpp \
	source/Engine/Rendering/Shader.cpp \
	source/Engine/Rendering/Software/SoftwareRenderer.cpp \
	source/Engine/Rendering/Software/Scanline.cpp \
	source/Engine/Rendering/Software/PolygonRasterizer.cpp \
	source/Engine/Rendering/PolygonRenderer.cpp \
	source/Engine/Rendering/GL/GLShader.cpp \
	source/Engine/Rendering/GL/GLShaderContainer.cpp \
	source/Engine/Rendering/GL/GLShaderBuilder.cpp \
	source/Engine/Rendering/GL/GLRenderer.cpp \
	source/Engine/Rendering/GameTexture.cpp \
	source/Engine/Rendering/TextureReference.cpp \
	source/Engine/Rendering/SDL2/SDL2Renderer.cpp \
	source/Engine/Rendering/Material.cpp \
	source/Engine/Rendering/D3D/D3DRenderer.cpp \
	source/Engine/Application.cpp \
	source/Engine/Scene/SceneInfo.cpp \
	source/Engine/Scene/View.cpp \
	source/Engine/Scene/SceneLayer.cpp \
	source/Engine/TextFormats/XML/XMLParser.cpp \
	source/Engine/TextFormats/INI/INI.cpp \
	source/Engine/Main.cpp \
	source/Engine/Media/MediaPlayer.cpp \
	source/Engine/Media/MediaSource.cpp \
	source/Engine/Media/Utils/MediaPlayerState.cpp \
	source/Engine/Media/Utils/PtrBuffer.cpp \
	source/Engine/Media/Utils/RingBuffer.cpp \
	source/Engine/Media/Decoders/AudioDecoder.cpp \
	source/Engine/Media/Decoders/VideoDecoder.cpp \
	source/Engine/Media/Decoder.cpp
MFILES := \
	source/Engine/Platforms/MacOS/Filesystem.m
PRVHFILES := \
	source/Libraries/jsmn.h \
	source/Libraries/Clipper2/clipper.rectclip.h \
	source/Libraries/Clipper2/clipper.minkowski.h \
	source/Libraries/Clipper2/clipper.version.h \
	source/Libraries/Clipper2/clipper.offset.h \
	source/Libraries/Clipper2/clipper.core.h \
	source/Libraries/Clipper2/clipper.h \
	source/Libraries/Clipper2/clipper.export.h \
	source/Libraries/Clipper2/clipper.engine.h \
	source/Libraries/nanoprintf.h \
	source/Libraries/stb_image.h \
	source/Libraries/miniz.h \
	source/Libraries/poly2tri/poly2tri.h \
	source/Libraries/poly2tri/common/utils.h \
	source/Libraries/poly2tri/common/dll_symbol.h \
	source/Libraries/poly2tri/common/shapes.h \
	source/Libraries/poly2tri/sweep/advancing_front.h \
	source/Libraries/poly2tri/sweep/sweep.h \
	source/Libraries/poly2tri/sweep/sweep_context.h \
	source/Libraries/poly2tri/sweep/cdt.h \
	source/Libraries/stb_vorbis.h \
	source/Libraries/spng.h \
	source/Engine/Platforms/MacOS/Filesystem.h \
	source/Engine/Platforms/iOS/MediaPlayer.h \
	source/Engine/Types/EntityTypes.h \
	source/Engine/Types/Collision.h \
	source/Engine/Input/Input.h \
	source/Engine/Input/ControllerRumble.h \
	source/Engine/Network/WebSocketIncludes.h \
	source/Engine/IO/Compression/CompressionEnums.h \
	source/Engine/Includes/StandardSDL2.h \
	source/Engine/Includes/Version.h \
	source/Engine/Includes/Endian.h \
	source/Engine/Includes/BijectiveMap.h \
	source/Engine/Includes/DateTime.h \
	source/Engine/Includes/HashMap.h \
	source/Engine/Includes/ChainedHashMap.h \
	source/Engine/Includes/Standard.h \
	source/Engine/Includes/Token.h \
	source/Engine/Includes/PrintBuffer.h \
	source/Engine/ResourceTypes/ResourceType.h \
	source/Engine/ResourceTypes/SceneFormats/HatchSceneTypes.h \
	source/Engine/Math/VectorTypes.h \
	source/Engine/Math/GeometryTypes.h \
	source/Engine/Math/FixedPoint.h \
	source/Engine/Audio/AudioIncludes.h \
	source/Engine/Audio/AudioChannel.h \
	source/Engine/Diagnostics/MemoryPools.h \
	source/Engine/Diagnostics/PerformanceTypes.h \
	source/Engine/Bytecode/Types.h \
	source/Engine/Bytecode/CompilerEnums.h \
	source/Engine/Rendering/Metal/MetalFuncs.h \
	source/Engine/Rendering/Metal/Includes.h \
	source/Engine/Rendering/Mesh.h \
	source/Engine/Rendering/Scene3D.h \
	source/Engine/Rendering/Software/Contour.h \
	source/Engine/Rendering/Software/SoftwareEnums.h \
	source/Engine/Rendering/GraphicsFunctions.h \
	source/Engine/Rendering/Enums.h \
	source/Engine/Rendering/GL/ShaderIncludes.h \
	source/Engine/Rendering/GL/Includes.h \
	source/Engine/Rendering/SDL2/SDL2MetalFunc.h \
	source/Engine/Rendering/3D.h \
	source/Engine/Sprites/Animation.h \
	source/Engine/Scene/TileAnimation.h \
	source/Engine/Scene/SceneEnums.h \
	source/Engine/Scene/SceneConfig.h \
	source/Engine/TextFormats/XML/XMLNode.h \
	source/Engine/TextFormats/INI/INIStructs.h \
	source/Engine/Media/Includes/AVCodec.h \
	source/Engine/Media/Includes/AVUtils.h \
	source/Engine/Media/Includes/SWScale.h \
	source/Engine/Media/Includes/AVFormat.h \
	source/Engine/Media/Includes/SWResample.h \
	source/Engine/Media/Utils/Codec.h
PUBHFILES := \
	include/Engine/InputManager.h \
	include/Engine/Types/ObjectList.h \
	include/Engine/Types/DrawGroupList.h \
	include/Engine/Types/Entity.h \
	include/Engine/Types/Tileset.h \
	include/Engine/Types/ObjectRegistry.h \
	include/Engine/Input/Controller.h \
	include/Engine/Input/InputPlayer.h \
	include/Engine/Input/InputAction.h \
	include/Engine/Application.h \
	include/Engine/FontFace.h \
	include/Engine/Network/HTTP.h \
	include/Engine/Network/WebSocketClient.h \
	include/Engine/IO/MemoryStream.h \
	include/Engine/IO/Stream.h \
	include/Engine/IO/ResourceStream.h \
	include/Engine/IO/NetworkStream.h \
	include/Engine/IO/StandardIOStream.h \
	include/Engine/IO/Serializer.h \
	include/Engine/IO/SDLStream.h \
	include/Engine/IO/VirtualFileStream.h \
	include/Engine/IO/FileStream.h \
	include/Engine/IO/TextStream.h \
	include/Engine/IO/Compression/LZ11.h \
	include/Engine/IO/Compression/ZLibStream.h \
	include/Engine/IO/Compression/RunLength.h \
	include/Engine/IO/Compression/Huffman.h \
	include/Engine/IO/Compression/LZSS.h \
	include/Engine/Filesystem/VFS/HatchVFS.h \
	include/Engine/Filesystem/VFS/VirtualFileSystem.h \
	include/Engine/Filesystem/VFS/FileSystemVFS.h \
	include/Engine/Filesystem/VFS/ArchiveVFS.h \
	include/Engine/Filesystem/VFS/VFSEntry.h \
	include/Engine/Filesystem/VFS/MemoryCache.h \
	include/Engine/Filesystem/VFS/MemoryVFS.h \
	include/Engine/Filesystem/VFS/VFSProvider.h \
	include/Engine/Filesystem/Path.h \
	include/Engine/Filesystem/File.h \
	include/Engine/Filesystem/Directory.h \
	include/Engine/ResourceTypes/IModel.h \
	include/Engine/ResourceTypes/SoundFormats/OGG.h \
	include/Engine/ResourceTypes/SoundFormats/WAV.h \
	include/Engine/ResourceTypes/SoundFormats/SoundFormat.h \
	include/Engine/ResourceTypes/SceneFormats/RSDKSceneReader.h \
	include/Engine/ResourceTypes/SceneFormats/TiledMapReader.h \
	include/Engine/ResourceTypes/SceneFormats/HatchSceneReader.h \
	include/Engine/ResourceTypes/ISprite.h \
	include/Engine/ResourceTypes/ISound.h \
	include/Engine/ResourceTypes/ImageFormats/GIF.h \
	include/Engine/ResourceTypes/ImageFormats/PNG.h \
	include/Engine/ResourceTypes/ImageFormats/JPEG.h \
	include/Engine/ResourceTypes/ImageFormats/ImageFormat.h \
	include/Engine/ResourceTypes/ResourceManager.h \
	include/Engine/ResourceTypes/Image.h \
	include/Engine/ResourceTypes/ModelFormats/RSDKModel.h \
	include/Engine/ResourceTypes/ModelFormats/MD3Model.h \
	include/Engine/ResourceTypes/ModelFormats/Importer.h \
	include/Engine/ResourceTypes/ModelFormats/HatchModel.h \
	include/Engine/Hashing/Murmur.h \
	include/Engine/Hashing/MD5.h \
	include/Engine/Hashing/FNV1A.h \
	include/Engine/Hashing/CombinedHash.h \
	include/Engine/Hashing/CRC32.h \
	include/Engine/Math/Matrix4x4.h \
	include/Engine/Math/Ease.h \
	include/Engine/Math/Clipper.h \
	include/Engine/Math/Math.h \
	include/Engine/Math/Vector.h \
	include/Engine/Math/Random.h \
	include/Engine/Math/Geometry.h \
	include/Engine/Extensions/Discord.h \
	include/Engine/Utilities/StringUtils.h \
	include/Engine/Utilities/ColorUtils.h \
	include/Engine/Audio/AudioManager.h \
	include/Engine/Audio/AudioPlayback.h \
	include/Engine/Diagnostics/RemoteDebug.h \
	include/Engine/Diagnostics/Log.h \
	include/Engine/Diagnostics/Memory.h \
	include/Engine/Diagnostics/Clock.h \
	include/Engine/Diagnostics/PerformanceMeasure.h \
	include/Engine/Bytecode/Compiler.h \
	include/Engine/Bytecode/StandardLibrary.h \
	include/Engine/Bytecode/GarbageCollector.h \
	include/Engine/Bytecode/SourceFileMap.h \
	include/Engine/Bytecode/ScriptEntity.h \
	include/Engine/Bytecode/TypeImpl/StringImpl.h \
	include/Engine/Bytecode/TypeImpl/MaterialImpl.h \
	include/Engine/Bytecode/TypeImpl/FunctionImpl.h \
	include/Engine/Bytecode/TypeImpl/MapImpl.h \
	include/Engine/Bytecode/TypeImpl/ArrayImpl.h \
	include/Engine/Bytecode/Values.h \
	include/Engine/Bytecode/VMThread.h \
	include/Engine/Bytecode/Bytecode.h \
	include/Engine/Bytecode/ScriptManager.h \
	include/Engine/Rendering/TextureReference.h \
	include/Engine/Rendering/ViewTexture.h \
	include/Engine/Rendering/Shader.h \
	include/Engine/Rendering/GameTexture.h \
	include/Engine/Rendering/Material.h \
	include/Engine/Rendering/Software/SoftwareRenderer.h \
	include/Engine/Rendering/Software/Scanline.h \
	include/Engine/Rendering/Software/PolygonRasterizer.h \
	include/Engine/Rendering/FaceInfo.h \
	include/Engine/Rendering/GL/GLRenderer.h \
	include/Engine/Rendering/GL/GLShaderContainer.h \
	include/Engine/Rendering/GL/GLShaderBuilder.h \
	include/Engine/Rendering/GL/GLShader.h \
	include/Engine/Rendering/PolygonRenderer.h \
	include/Engine/Rendering/ModelRenderer.h \
	include/Engine/Rendering/Texture.h \
	include/Engine/Rendering/SDL2/SDL2Renderer.h \
	include/Engine/Rendering/VertexBuffer.h \
	include/Engine/Rendering/D3D/D3DRenderer.h \
	include/Engine/Graphics.h \
	include/Engine/Scene/TileConfig.h \
	include/Engine/Scene/ScrollingInfo.h \
	include/Engine/Scene/SceneLayer.h \
	include/Engine/Scene/ScrollingIndex.h \
	include/Engine/Scene/View.h \
	include/Engine/Scene/TileSpriteInfo.h \
	include/Engine/Scene/SceneInfo.h \
	include/Engine/TextFormats/XML/XMLParser.h \
	include/Engine/TextFormats/INI/INI.h \
	include/Engine/Scene.h \
	include/Engine/Media/MediaSource.h \
	include/Engine/Media/MediaPlayer.h \
	include/Engine/Media/Decoder.h \
	include/Engine/Media/Utils/PtrBuffer.h \
	include/Engine/Media/Utils/RingBuffer.h \
	include/Engine/Media/Utils/MediaPlayerState.h \
	include/Engine/Media/Decoders/AudioDecoder.h \
	include/Engine/Media/Decoders/VideoDecoder.h

DEFINES := \
	_THREAD_SAFE \
	GL_SILENCE_DEPRECATION \
	MACOSX \
	TARGET_NAME=\"$(PROJECT)\" \
	USING_ASSIMP \
	USING_OPENGL

CFLAGS := -std=c11
CXXFLAGS := \
	-std=c++17 \
	-Wno-pedantic \
	-Wno-unused-but-set-variable \
	-Wno-unused-function \
	-Wno-unused-label \
	-Wno-unused-lambda-capture \
	-Wno-unused-value \
	-Wno-unused-variable

# the linker has to care about this too, unfortunately
LDFLAGS := -std=c++17

# this defines all our usual targets
include meta/epilogue.mk
