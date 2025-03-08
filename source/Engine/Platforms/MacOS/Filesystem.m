#ifdef __OBJC__
#include "Filesystem.h"
#endif

#if MACOSX_AAAAAAAA
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <NotificationCenter/NotificationCenter.h>
#endif

#include <string.h>

int MacOS_GetApplicationSupportDirectory(char* buffer, int maxSize) {
	@autoreleasepool {
#if MACOSX_AAAAAAAA
		/* clang-format off */
		NSBundle* bundle = [NSBundle mainBundle];
		const char* baseType =
			[[bundle bundlePath] fileSystemRepresentation];

		NSArray* paths = NSSearchPathForDirectoriesInDomains(
			NSApplicationSupportDirectory,
			NSUserDomainMask,
			YES);
		NSString* applicationSupportDirectory =
			[paths firstObject];
		/* NSLog(@"applicationSupportDirectory: '%@'",
		 * applicationSupportDirectory); */

		strncpy(buffer,
			[applicationSupportDirectory UTF8String],
			maxSize);
		if (baseType) {
			return !!StringUtils::StrCaseStr(
				baseType, ".app");
		}

/* clang-format on */
#endif
		return 0;
	}
}

int MacOS_GetSelfPath(char* buffer, int maxSize) {
#if MACOSX_AAAAAAAA
	char p[maxSize];
	uint32_t p_sz = sizeof p;

	memset(p, 0, maxSize);

	if (_NSGetExecutablePath(p, &p_sz) != 0) {
		return 1;
	}

	memcpy(buffer, p, maxSize);
#endif
	return 0;
}
