#ifdef USING_LIBAV
extern "C" {
#include <libswscale/swscale.h>
}
#else
struct SwsContext;
#endif
