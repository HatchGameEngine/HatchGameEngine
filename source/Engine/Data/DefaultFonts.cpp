#include <Engine/Data/DefaultFonts.h>

// The following font(s) were generated using the bin2c tool, found under the tools/ directory.
// Some of these fonts may be under the Open Font License.
// If you are not using or do not wish to build with the default fonts, disable USE_DEFAULT_FONTS.

#ifdef USE_DEFAULT_FONTS
#include "OpenSans-Regular.ttf.inc"

void* GetDefaultFontData() {
	return (void*)OpenSans_Regular_ttf;
}
size_t GetDefaultFontDataLength() {
	return OpenSans_Regular_ttf_len;
}
#else
void* GetDefaultFontData() {
	return nullptr;
}
size_t GetDefaultFontDataLength() {
	return 0;
}
#endif
