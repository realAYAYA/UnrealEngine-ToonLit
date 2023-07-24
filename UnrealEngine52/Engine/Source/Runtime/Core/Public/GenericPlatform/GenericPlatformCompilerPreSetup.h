// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#ifndef PRAGMA_DISABLE_DEPRECATION_WARNINGS
	#define PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif

#ifndef PRAGMA_ENABLE_DEPRECATION_WARNINGS
	#define PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

#ifndef EMIT_CUSTOM_WARNING_AT_LINE
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning)
#endif // EMIT_CUSTOM_WARNING_AT_LINE

#ifndef EMIT_CUSTOM_WARNING
	#define EMIT_CUSTOM_WARNING(Warning) \
		EMIT_CUSTOM_WARNING_AT_LINE(__LINE__, Warning)
#endif // EMIT_CUSTOM_WARNING

#ifndef UE_DEPRECATED_MACRO
	#define UE_DEPRECATED_MACRO(Version, Message) EMIT_CUSTOM_WARNING(Message " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.")
#endif

#ifndef DEPRECATED_MACRO
	#define DEPRECATED_MACRO(Version, Message) UE_DEPRECATED_MACRO(5.1, "The DEPRECATED_MACRO macro has been deprecated in favor of UE_DEPRECATED_MACRO.") UE_DEPRECATED_MACRO(Version, Message)
#endif

#ifndef PRAGMA_DEFAULT_VISIBILITY_START
	#define PRAGMA_DEFAULT_VISIBILITY_START
#endif

#ifndef PRAGMA_DEFAULT_VISIBILITY_END
	#define PRAGMA_DEFAULT_VISIBILITY_END
#endif
