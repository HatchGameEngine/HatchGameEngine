#ifndef ENGINE_IO_MACOS_FILESYSTEM
#define ENGINE_IO_MACOS_FILESYSTEM

#ifdef __cplusplus
#define _externmacosfs extern "C"
#else
#define _externmacosfs extern
#endif
_externmacosfs int MacOS_GetApplicationSupportDirectory(char* buffer, int maxSize);
_externmacosfs int MacOS_GetSelfPath(char* buffer, int maxSize);

#endif /* ENGINE_IO_MACOS_FILESYSTEM */
