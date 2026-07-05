#ifndef ENGINE_DATA_DEFAULTFONTS_H
#define ENGINE_DATA_DEFAULTFONTS_H

#include <Engine/Includes/Standard.h>

void* GetDefaultFontData();
size_t GetDefaultFontDataLength();

#ifdef USE_DEFAULT_FONTS
extern unsigned char OpenSans_Regular_ttf[];
extern unsigned int OpenSans_Regular_ttf_len;
#endif

#endif /* ENGINE_DATA_DEFAULTFONTS_H */
