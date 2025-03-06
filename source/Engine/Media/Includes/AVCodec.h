#ifdef USING_LIBAV
extern "C" {
#include <libavcodec/avcodec.h>
}
#else
struct AVCodecContext;
struct AVFormatContext;
struct AVPacket;
struct AVFrame;
struct AVPixelFormat {};
struct AVSampleFormat {};
#endif
