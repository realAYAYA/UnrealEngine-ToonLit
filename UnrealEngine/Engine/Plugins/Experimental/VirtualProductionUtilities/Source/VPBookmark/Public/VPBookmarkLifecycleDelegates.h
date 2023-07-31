// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class UVPBookmark;


class VPBOOKMARK_API FVPBookmarkLifecycleDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FVPBookmarkLifecycleDelegate, UVPBookmark*);

	static FVPBookmarkLifecycleDelegate& GetOnBookmarkCreated();
	static FVPBookmarkLifecycleDelegate& GetOnBookmarkDestroyed();
	static FVPBookmarkLifecycleDelegate& GetOnBookmarkCleared();
};
