# recursive wildcard
rwc = $(foreach d, $(wildcard $1*), $(call rwc,$d/,$2) $(filter $(subst *,%,$2),$d))

PLATFORM_UNKNOWN = 0
PLATFORM_WINDOWS = 1
PLATFORM_MACOS = 2
PLATFORM_LINUX = 3

PLATFORM = $(PLATFORM_UNKNOWN)
OUT_FOLDER =

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

ifeq ($(PLATFORM),$(PLATFORM_MACOS))
USING_ASSIMP = 0
endif

TARGET    = SonicGalactic
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

OBJS := $(addprefix out/$(OUT_FOLDER)/, $(SRC_C:source/%.c=%.o)) \
			$(addprefix out/$(OUT_FOLDER)/, $(SRC_CPP:source/%.cpp=%.o))
ifeq ($(PLATFORM),$(PLATFORM_MACOS))
OBJS += $(addprefix out/$(OUT_FOLDER)/, $(SRC_M:source/%.m=%.o))
endif

INCLUDES := -Wall -Wno-deprecated -Wno-unused-variable \
	-Iinclude/ \
	-Isource/
LIBS    := $(shell sdl2-config --libs) -lassimp
DEFINES := -DTARGET_NAME=\"$(TARGET)\" \
	-DUSING_VM_DISPATCH_TABLE \
	$(shell sdl2-config --cflags) \
	-DUSING_ASSIMP

ifeq ($(PLATFORM),$(PLATFORM_WINDOWS))
INCLUDES += -Imeta/win/include
LIBS += -lwsock32 -lws2_32
DEFINES += -DWIN32
ifneq ($(MSYS_VERSION),0)
DEFINES += -DMSYS
endif
endif
ifeq ($(PLATFORM),$(PLATFORM_MACOS))
INCLUDES += -I/opt/homebrew/include
LIBS += -lobjc
DEFINES += -DMACOSX -DUSING_OPENGL
endif
ifeq ($(PLATFORM),$(PLATFORM_LINUX))
DEFINES += -DLINUX
endif

# Compiler Optimzations
ifeq ($(USING_COMPILER_OPTS), 1)
DEFINES	 +=	-Ofast
DEFINES	 +=	-DUSING_COMPILER_OPTS
endif

# OGG Audio
#LIBS	 +=	-logg -lvorbis -lvorbisfile
# zlib Compression
#LIBS	 +=	-lz

ifeq ($(PLATFORM),$(PLATFORM_MACOS))
LINKER = -framework IOKit \
	-framework OpenGL \
	-framework CoreFoundation \
	-framework CoreServices \
	-framework Foundation \
	-framework Cocoa
endif

all:
	mkdir -p $(OBJ_DIRS)
	$(MAKE) build

clean:
	rm -rf $(OBJS)

build: $(OBJS)
	$(CXX) $^ $(INCLUDES) $(LIBS) $(LINKER) -o "$(TARGETDIR)" -std=c++17

$(OBJ_DIRS):
	mkdir -p $@

pkg:
	rm -rf "$(TARGETDIR).app"
	mkdir "$(TARGETDIR).app"
	mkdir "$(TARGETDIR).app/Contents"
	mkdir "$(TARGETDIR).app/Contents/Frameworks"
	mkdir "$(TARGETDIR).app/Contents/MacOS"
	mkdir "$(TARGETDIR).app/Contents/Resources"
	mv "$(TARGETDIR)" "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	test -f source/Data.hatch && cp -a source/Data.hatch "$(TARGETDIR).app/Contents/Resources/Data.hatch" || true
	test -f source/config.ini && cp -a source/config.ini "$(TARGETDIR).app/Contents/Resources/config.ini" || true
	test -f meta/mac/icon.icns && cp -a meta/mac/icon.icns "$(TARGETDIR).app/Contents/Resources/icon.icns" || true
# Making PkgInfo
	echo "APPL????" > "$(TARGETDIR).app/Contents/PkgInfo"
# Making Info.plist
	cp -a meta/mac/Info.plist "$(TARGETDIR).app/Contents/Info.plist"
	cp -aL $(shell brew --prefix)/lib/libassimp.dylib "$(TARGETDIR).app/Contents/Frameworks/_libassimp.dylib"
	cp -aL $(shell brew --prefix)/lib/libSDL2.dylib "$(TARGETDIR).app/Contents/Frameworks/_libSDL2.dylib"
	codesign --force -s - "$(TARGETDIR).app/Contents/Frameworks/_libassimp.dylib"
	install_name_tool -change '/opt/homebrew/opt/assimp/lib/libassimp.5.dylib' @executable_path/../Frameworks/_libassimp.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	codesign --force -s - "$(TARGETDIR).app/Contents/Frameworks/_libSDL2.dylib"
	install_name_tool -change '/opt/homebrew/opt/sdl2/lib/libSDL2-2.0.0.dylib' @executable_path/../Frameworks/_libSDL2.dylib "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"
	codesign --force -s - "$(TARGETDIR).app/Contents/MacOS/$(TARGET)"

out/$(OUT_FOLDER)/%.o: source/%.cpp
	$(CXX) -c -g $(INCLUDES) $(DEFINES) -o "$@" "$<" -std=c++17

out/$(OUT_FOLDER)/%.o: source/%.c
	$(CC) -c -g $(INCLUDES) $(DEFINES) -o "$@" "$<" -std=c11

out/$(OUT_FOLDER)/%.o: source/%.m
	$(CC) -c -g $(INCLUDES) $(DEFINES) -o "$@" "$<"
