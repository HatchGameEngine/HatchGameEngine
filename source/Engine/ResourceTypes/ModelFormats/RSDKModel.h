#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_RSDKMODEL_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_RSDKMODEL_H

#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/IModel.h>

class RSDKModel {
public:
	static bool IsMagic(Stream* stream);
	static bool Convert(IModel* model, Stream* stream);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_RSDKMODEL_H */
