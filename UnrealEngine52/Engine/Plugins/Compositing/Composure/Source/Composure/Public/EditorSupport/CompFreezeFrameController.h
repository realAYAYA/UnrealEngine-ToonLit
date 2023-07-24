// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Misc/CoreMiscDefines.h" // for EForceInit

enum class ETargetUsageFlags : uint8;
typedef FGuid FFreezeFrameControlHandle;

struct COMPOSURE_API FCompFreezeFrameController
{
public:
	FCompFreezeFrameController(int32& FreezeFlagsRef)
		: FreezeFlags(FreezeFlagsRef)
	{}

	FORCEINLINE bool IsLocked() const 
	{ 
		return LockKey.IsValid(); 
	}
	
	FORCEINLINE FFreezeFrameControlHandle Lock()
	{ 
		if (ensure(!IsLocked()))
		{
			LockKey = FGuid::NewGuid();
			return LockKey;
		}
		return FGuid();
	}

	FORCEINLINE bool Unlock(const FFreezeFrameControlHandle& InLockKey)
	{ 
		if (InLockKey == LockKey)
		{
			LockKey.Invalidate();
		}
		return !IsLocked();
	}

	FORCEINLINE ETargetUsageFlags GetFreezeFlags() const { return (ETargetUsageFlags)FreezeFlags; }

	bool SetFreezeFlags(ETargetUsageFlags InFreezeFlags, bool bClearOthers = false, const FFreezeFrameControlHandle& LockKey = FFreezeFrameControlHandle());
	bool ClearFreezeFlags(ETargetUsageFlags InFreezeFlags, const FFreezeFrameControlHandle& LockKey = FFreezeFrameControlHandle());
	bool ClearFreezeFlags(const FFreezeFrameControlHandle& LockKey = FFreezeFrameControlHandle());

	FORCEINLINE operator ETargetUsageFlags() const { return GetFreezeFlags(); }
	FORCEINLINE operator int32()                   { return (int32)GetFreezeFlags(); }

	bool HasAnyFlags(ETargetUsageFlags InFreezeFlags);
	bool HasAllFlags(ETargetUsageFlags InFreezeFlags);

private: 
	FFreezeFrameControlHandle LockKey;
	int32& FreezeFlags;

public:

	/** DO NOT USE - For UObject construction only */
	FCompFreezeFrameController(EForceInit Default = ForceInit);
};

