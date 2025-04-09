#ifndef ENGINE_RESOURCETYPES_MODELFORMATS_RSDKMODEL_H
#define ENGINE_RESOURCETYPES_MODELFORMATS_RSDKMODEL_H

#include <Engine/IO/Stream.h>
#include <Engine/ResourceTypes/IModel.h>

namespace RSDKModel {
//public:
	bool IsMagic(Stream* stream);
	bool Convert(IModel* model, Stream* stream);
};

#endif /* ENGINE_RESOURCETYPES_MODELFORMATS_RSDKMODEL_H */
