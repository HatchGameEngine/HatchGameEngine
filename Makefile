# recursive wildcard
rwc = $(foreach d, $(wildcard $1*), $(call rwc,$d/,$2) $(filter $(subst *,%,$2),$d))

PLATFORM_UNKNOWN = 0
PLATFORM_WINDOWS = 1
PLATFORM_MACOS = 2
PLATFORM_LINUX = 3

PLATFORM = $(PLATFORM_UNKNOWN)
OUT_FOLDER =

# from https://www.msys2.org/wiki/Porting/
# nonzero value means we're using msys
MSYS_VERSION := $(if $(findstring Msys, $(shell uname -o)),$(word 1, $(subst ., ,$(shell uname -r))),0)

ifeq ($(OS),Windows_NT)
	PLATFORM = $(PLATFORM_WINDOWS)
	OUT_FOLDER = windows
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		PLATFORM = $(PLATFORM_LINUX)
		OUT_FOLDER = linux
	endif
	ifeq ($(UNAME_S),Darwin)
		PLATFORM = $(PLATFORM_MACOS)
		OUT_FOLDER = macos
	endif
endif

ifeq ($(PLATFORM),0)
	$(error Unknown platform)
endif

PROGRAM_SUFFIX :=
ifeq ($(PLATFORM),$(PLATFORM_WINDOWS))
PROGRAM_SUFFIX := .exe
endif

USING_LIBAV = 0
USING_CURL = 0
USING_LIBPNG = 1
USING_ASSIMP = 1

TARGET    = HatchGameEngine
TARGETDIR = builds/$(OUT_FOLDER)/$(TARGET)
OBJS      = main.o

SRC_C   = $(call rwc, source/, *.c)
SRC_CPP = $(call rwc, source/, *.cpp)
SRC_M   = $(call rwc, source/, *.m)

OBJ_DIRS := $(sort \
			$(addprefix out/$(OUT_FOLDER)/, $(dir $(SRC_C:source/%.c=%.o))) \
			$(addprefix out/$(OUT_FOLDER)/, $(dir $(SRC_CPP:source/%.cpp=%.o))))
ifeq ($(PLATFORM),$(PLATFORM_MACOS))
OBJ_DIRS += $(addprefix out/$(OUT_FOLDER)/, $(dir $(SRC_M:source/%.m=%.o)))
endif

OBJS     := $(addprefix out/$(OUT_FOLDER)/, $(SRC_C:source/%.c=%.o)) \
			$(addprefix out/$(OUT_FOLDER)/, $(SRC_CPP:source/%.cpp=%.o))
ifeq ($(PLATFORM),$(PLATFORM_MACOS))
OBJ_DIRS += $(addprefix out/$(OUT_FOLDER)/, $(SRC_M:source/%.m=%.o))
endif

INCLUDES  =	-Wall -Wno-deprecated -Wno-unused-variable \
			-I/usr/local/include/ \
			-I/usr/local/include/freetype2 \
			-Iinclude/ \
			-Isource/
LIBS	  =	-lpng16 -ljpeg -lfreetype $(shell sdl2-config --libs)
DEFINES   = -DTARGET_NAME=\"$(TARGET)\" \
			-DUSING_LIBPNG \
			-DUSING_LIBPNG_HEADER=\<libpng16/png.h\> \
			-DDEBUG \
			-DUSING_VM_DISPATCH_TABLE \
			$(shell sdl2-config --cflags)

ifeq ($(PLATFORM),$(PLATFORM_WINDOWS))
INCLUDES += -Imeta/win/include
LIBS += -lwsock32 -lws2_32
DEFINES += -DWIN32
ifneq ($(MSYS_VERSION),0)
DEFINES += -DMSYS
endif
endif
ifeq ($(PLATOFRM),$(PLATFORM_MACOS))
INCLUDES += -F/Library/Frameworks/ \
			-F/System/Library/Frameworks/ \
			-Fmeta/mac/ \
			-I/Library/Frameworks/SDL2.framework/Headers/ \
			-I/System/Library/Frameworks/OpenGL.framework/Headers/
DEFINES += -DMACOSX -DUSING_FRAMEWORK
endif
ifeq ($(PLATFORM),$(PLATFORM_LINUX))
DEFINES += -DLINUX
endif

# Compiler Optimzations
ifeq ($(USING_COMPILER_OPTS), 1)
DEFINES	 +=	-Ofast
DEFINES	 +=	-DUSING_COMPILER_OPTS
endif

# Open Asset Importer Library
ifeq ($(USING_ASSIMP), 1)
LIBS	 +=	-lassimp
DEFINES	 +=	-DUSING_ASSIMP
endif

# FFmpeg Libraries
ifeq ($(USING_LIBAV), 1)
LIBS	 +=	-lavcodec -lavformat -lavutil -lswscale -lswresample
DEFINES	 +=	-DUSING_LIBAV
endif

# Networking Libraries
ifeq ($(USING_CURL), 1)
LIBS 	 +=	-lcurl -lcrypto
DEFINES	 +=	-DUSING_CURL
endif

# OGG Audio
LIBS	 +=	-logg -lvorbis -lvorbisfile
# zlib Compression
LIBS	 +=	-lz

ifeq ($(PLATFORM),$(PLATFORM_MACOS))
LINKER	  =	-framework SDL2 \
			-framework OpenGL \
			-framework CoreFoundation \
			-framework CoreServices \
			-framework Foundation
			# \
			-framework Cocoa
endif

all:
	mkdir -p $(OBJ_DIRS)
	$(MAKE) build

clean:
	rm -rf $(OBJS)

build: $(OBJS)
	$(CXX) $^ $(INCLUDES) $(LIBS) $(LINKER) -o "$(TARGETDIR)" -std=c++11

$(OBJ_DIRS):
	mkdir -p $@

package:
	@#@rm -rf "$(TARGETDIR).app"
	@mkdir -p "$(TARGETDIR).app"
	@mkdir -p "$(TARGETDIR).app/Contents"
	@mkdir -p "$(TARGETDIR).app/Contents/Frameworks"
	@mkdir -p "$(TARGETDIR).app/Contents/MacOS"
	@mkdir -p "$(TARGETDIR).app/Contents/Resources"
	@# @rm -rf $(OBJS)
	@mkdir -p $(OBJ_DIRS)
	@make build
	@chmod +x "$(TARGETDIR)"
	@cp "$(TARGETDIR)" "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@[ -f "source/Data.hatch" ] && cp "source/Data.hatch" "$(TARGETDIR).app/Contents/Resources/Data.hatch" || true
	@[ -f "source/config.ini" ] && cp "source/config.ini" "$(TARGETDIR).app/Contents/Resources/config.ini" || true
	@[ -f "meta/mac/icon.icns" ] && cp "meta/mac/icon.icns" "$(TARGETDIR).app/Contents/Resources/icon.icns" || true
	@# Making PkgInfo
	@echo "APPL????" > "$(TARGETDIR).app/Contents/PkgInfo"
	@# Making Info.plist
	@echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > "$(TARGETDIR).app/Contents/Info.plist"
	@echo "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "<plist version=\"1.0\">" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "    <dict>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <key>CFBundleExecutable</key>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <string>$(TARGET)</string>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <key>CFBundleDisplayName</key>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <string>$(TARGET)</string>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@[ -f "meta/mac/icon.icns" ] && echo "        <key>CFBundleIconFile</key>" >> "$(TARGETDIR).app/Contents/Info.plist" && echo "        <string>icon</string>" >> "$(TARGETDIR).app/Contents/Info.plist" || true
	@# GCSupportsControllerUserInteraction
	@echo "        <key>CFBundlePackageType</key>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <string>APPL</string>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <key>CFBundleShortVersionString</key>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <string>1.0.0</string>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <key>CFBundleVersion</key>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <string>1.0.0</string>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <key>NSHighResolutionCapable</key>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "        <string>true</string>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "    </dict>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@echo "</plist>" >> "$(TARGETDIR).app/Contents/Info.plist"
	@# Copy libs
	@cp "meta/mac/libpng16.16.dylib" "$(TARGETDIR).app/Contents/Frameworks/_libpng16.16.dylib"
	@cp "/usr/local/opt/jpeg/lib/libjpeg.9.dylib" "$(TARGETDIR).app/Contents/Frameworks/_libjpeg.9.dylib"
	@cp "/usr/local/opt/freetype/lib/libfreetype.6.dylib" "$(TARGETDIR).app/Contents/Frameworks/_libfreetype.6.dylib"
	@cp "meta/mac/libogg.0.dylib" "$(TARGETDIR).app/Contents/Frameworks/_libogg.0.dylib"
	@cp "meta/mac/libvorbis.0.dylib" "$(TARGETDIR).app/Contents/Frameworks/_libvorbis.0.dylib"
	@cp "meta/mac/libvorbisfile.3.dylib" "$(TARGETDIR).app/Contents/Frameworks/_libvorbisfile.3.dylib"
	@rsync -a "meta/mac/SDL2.framework/" "$(TARGETDIR).app/Contents/Frameworks/_SDL2.framework/"
	@# @cp "meta/mac/libcurl.4.dylib" "$(TARGETDIR).app/Contents/Frameworks/_libcurl.4.dylib"
	@# Link to copies
	@install_name_tool -change /usr/local/opt/libpng/lib/libpng16.16.dylib @executable_path/../Frameworks/_libpng16.16.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@install_name_tool -change /usr/local/opt/jpeg/lib/libjpeg.9.dylib @executable_path/../Frameworks/_libjpeg.9.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@install_name_tool -change /usr/local/opt/freetype/lib/libfreetype.6.dylib @executable_path/../Frameworks/_libfreetype.6.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@install_name_tool -change /usr/local/opt/libogg/lib/libogg.0.dylib @executable_path/../Frameworks/_libogg.0.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@install_name_tool -change /usr/local/opt/libvorbis/lib/libvorbis.0.dylib @executable_path/../Frameworks/_libvorbis.0.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@install_name_tool -change /usr/local/opt/libvorbis/lib/libvorbisfile.3.dylib @executable_path/../Frameworks/_libvorbisfile.3.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@install_name_tool -change @rpath/SDL2.framework/Versions/A/SDL2 @executable_path/../Frameworks/_SDL2.framework/Versions/A/SDL2 "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@install_name_tool -change /usr/local/opt/curl/lib/libcurl.4.dylib @executable_path/../Frameworks/_libcurl.4.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@# Link copies to themselves
	@#@install_name_tool -change /usr/local/opt/libogg/lib/libogg.0.dylib @executable_path/../Frameworks/_libogg.0.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@#@install_name_tool -change /usr/local/opt/libpng/lib/libpng16.16.dylib @executable_path/../Frameworks/_libpng16.16.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@#@install_name_tool -change /usr/local/opt/libvorbis/lib/libvorbis.0.dylib @executable_path/../Frameworks/_libvorbis.0.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@#@install_name_tool -change /usr/local/opt/libvorbis/lib/libvorbisfile.3.dylib @executable_path/../Frameworks/_libvorbisfile.3.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	@# Link Freetype to local PNG
	@install_name_tool -change /usr/local/opt/libpng/lib/libpng16.16.dylib @executable_path/../Frameworks/_libpng16.16.dylib "$(TARGETDIR).app/Contents/Frameworks/_libfreetype.6.dylib"
	@# Link Vorbis to local OGG
	@install_name_tool -change /usr/local/opt/libogg/lib/libogg.0.dylib @executable_path/../Frameworks/_libogg.0.dylib "$(TARGETDIR).app/Contents/Frameworks/_libvorbis.0.dylib"
	@# Link Vorbisfile to local OGG and Vorbis
	@install_name_tool -change /usr/local/opt/libogg/lib/libogg.0.dylib @executable_path/../Frameworks/_libogg.0.dylib "$(TARGETDIR).app/Contents/Frameworks/_libvorbisfile.3.dylib"
	@install_name_tool -change /usr/local/Cellar/libvorbis/1.3.6/lib/libvorbis.0.dylib @executable_path/../Frameworks/_libvorbis.0.dylib "$(TARGETDIR).app/Contents/Frameworks/_libvorbisfile.3.dylib"

out/$(OUT_FOLDER)/%.o: source/%.cpp
	$(CXX) -c -g $(INCLUDES) $(DEFINES) -o "$@" "$<" -std=c++11

out/$(OUT_FOLDER)/%.o: source/%.c
	$(CC) -c -g $(INCLUDES) $(DEFINES) -o "$@" "$<" -std=c11

out/$(OUT_FOLDER)/%.o: source/%.m
	$(CC) -c -g $(INCLUDES) $(DEFINES) -o "$@" "$<"
