// Copyright Epic Games, Inc. All Rights Reserved.

#if USE_USD_SDK

#pragma warning(pop)
#pragma pop_macro("check")

#if PLATFORM_LINUX && PLATFORM_EXCEPTIONS_DISABLED
	#undef try
	#undef catch
	#pragma pop_macro("try")
	#pragma pop_macro("catch")
#endif

THIRD_PARTY_INCLUDES_END

#endif // #if USE_USD_SDK
