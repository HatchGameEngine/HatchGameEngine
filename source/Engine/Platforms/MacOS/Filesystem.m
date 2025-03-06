#ifdef __OBJC__
#include "Filesystem.h"
#endif

#if MACOSX_AAAAAAAA
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <NotificationCenter/NotificationCenter.h>
#endif

int MacOS_GetApplicationSupportDirectory(char* buffer, int maxSize) {
	@autoreleasepool {
#if MACOSX_AAAAAAAA
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

#endif
		return 0;
	}
}
