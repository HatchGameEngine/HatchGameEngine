#ifndef ENGINE_NETWORK_HTTP_H
#define ENGINE_NETWORK_HTTP_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Bytecode/Types.h>

class HTTP {
public:
    static bool GET(const char* url, Uint8** outBuf, size_t* outLen, ObjBoundMethod* callback);
};

#endif /* ENGINE_NETWORK_HTTP_H */
