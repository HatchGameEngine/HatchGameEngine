#!/bin/sh
#

command -v gecho 2>&1 >/dev/null || {
	/bin/echo 'Please install coreutils using homebrew first.';
};

# this is the engine executable file name
ident=hatch;
icon=meta/mac/icon.icns;
plist=meta/mac/Info.plist;
config=source/config.ini;
oname=HatchGameEngine;

test "$1" != '' && oname="$1";
test "$2" != '' && icon="$2";
test "$3" != '' && plist="$3";
test "$4" != '' && config="$4";

stdbuf -o0 gecho 'makedotapp.sh [output name] [icon file] [plist file]';
stdbuf -o0 gecho "output name: ${oname}";
stdbuf -o0 gecho "icon: ${icon}";
stdbuf -o0 gecho "plist file: ${plist}";

grm -rf "${oname}.app";
gmkdir "${oname}.app";
gmkdir "${oname}.app/Contents";
gmkdir "${oname}.app/Contents/Frameworks";
gmkdir "${oname}.app/Contents/MacOS";
gmkdir "${oname}.app/Contents/Resources";

if test -f source/Data.hatch; then \
	gcp -a source/Data.hatch \
		"${oname}.app/Contents/Resources/Data.hatch";
fi

if test -f "${config}"; then \
	gcp -a "${config}" "${oname}.app/Contents/Resources/config.ini";
fi

if test -f "${icon}"; then \
	gcp -a "${icon}" "${oname}.app/Contents/Resources/icon.icns";
fi

# create PkgInfo
stdbuf -o0 gecho 'APPL????' > "${oname}.app/Contents/PkgInfo";

# copy over Info.plist
gcp -a "${plist}" "${oname}.app/Contents/Info.plist";

# copy over required shared libraries
gcp -aL $(brew --prefix)/lib/libassimp.dylib \
	"${oname}.app/Contents/Frameworks/_libassimp.dylib";
gcp -aL $(brew --prefix)/lib/libSDL2.dylib \
	"${oname}.app/Contents/Frameworks/_libSDL2.dylib";

# move (not copy!) the executable itself
# copying causes macOS security theatre to go off
gmv "${ident}" "${oname}.app/Contents/MacOS/${ident}";

# fix code signing nonsense
codesign --force -s - \
	"${oname}.app/Contents/Frameworks/_libassimp.dylib";
codesign --force -s - \
	"${oname}.app/Contents/Frameworks/_libSDL2.dylib";

# fix dynamic linkage
install_name_tool -change \
	'/opt/homebrew/opt/assimp/lib/libassimp.5.dylib' \
	@executable_path/../Frameworks/_libassimp.dylib \
	"${oname}.app/Contents/MacOS/${ident}";
install_name_tool -change \
	'/opt/homebrew/opt/sdl2/lib/libSDL2-2.0.0.dylib' \
	@executable_path/../Frameworks/_libSDL2.dylib \
	"${oname}.app/Contents/MacOS/${ident}"

# fix code signing of executable after adjusting shared library paths
codesign --force -s - "${oname}.app/Contents/MacOS/${ident}"
