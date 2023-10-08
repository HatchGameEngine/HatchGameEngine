#if INTERFACE
#include <Engine/Includes/Standard.h>

class ImageFormat {
public:
    Uint32* Colors = NULL;
    Uint32* Data = NULL;
    Uint32  Width = 0;
    Uint32  Height = 0;
    bool    Paletted = false;
};
#endif

#include <Engine/ResourceTypes/ImageFormats/ImageFormat.h>

PUBLIC VIRTUAL bool ImageFormat::Save(const char* filename) {
    return false;
}
PUBLIC VIRTUAL      ImageFormat::~ImageFormat() {

}
