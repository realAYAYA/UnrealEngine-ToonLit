// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPBookmarkLifecycleDelegates.h"


FVPBookmarkLifecycleDelegates::FVPBookmarkLifecycleDelegate& FVPBookmarkLifecycleDelegates::GetOnBookmarkCreated()
{
	static FVPBookmarkLifecycleDelegate OnBookmarkCreated;
	return OnBookmarkCreated;
}


FVPBookmarkLifecycleDelegates::FVPBookmarkLifecycleDelegate& FVPBookmarkLifecycleDelegates::GetOnBookmarkDestroyed()
{
	static FVPBookmarkLifecycleDelegate OnBookmarkDestroyed;
	return OnBookmarkDestroyed;
}


FVPBookmarkLifecycleDelegates::FVPBookmarkLifecycleDelegate& FVPBookmarkLifecycleDelegates::GetOnBookmarkCleared()
{
	static FVPBookmarkLifecycleDelegate OnBookmarkCleared;
	return OnBookmarkCleared;
}
