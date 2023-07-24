// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
struct FObjectPostCDOCompiledContext
{
	/** True if the Blueprint is currently being regenerated for the first time after being loaded (aka, compile-on-load) */
	bool bIsRegeneratingOnLoad = false;
	/** True if this notification was from a 'skeleton-only' compile */
	bool bIsSkeletonOnly = false;
};
#endif
