#ifdef USING_LIBAV
extern "C" {
#include <libswresample/swresample.h>
}
#else
struct SwrContext;
#endif
