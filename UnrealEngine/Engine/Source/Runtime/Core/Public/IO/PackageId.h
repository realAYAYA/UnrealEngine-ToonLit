// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "UObject/NameTypes.h"

class FArchive;
class FStructuredArchiveSlot;

#ifndef WITH_PACKAGEID_NAME_MAP
#define WITH_PACKAGEID_NAME_MAP WITH_EDITOR
#endif

class FPackageId
{
	static constexpr uint64 InvalidId = 0;
	uint64 Id = InvalidId;

	inline explicit FPackageId(uint64 InId): Id(InId) {}

public:
	FPackageId() = default;

	CORE_API static FPackageId FromName(const FName& Name);

	static FPackageId FromValue(const uint64 Value)
	{
		return FPackageId(Value);
	}

	inline bool IsValid() const
	{
		return Id != InvalidId;
	}

	inline uint64 Value() const
	{
		check(Id != InvalidId);
		return Id;
	}

	inline uint64 ValueForDebugging() const
	{
		return Id;
	}

	inline bool operator<(FPackageId Other) const
	{
		return Id < Other.Id;
	}

	inline bool operator==(FPackageId Other) const
	{
		return Id == Other.Id;
	}
	
	inline bool operator!=(FPackageId Other) const
	{
		return Id != Other.Id;
	}

	inline friend uint32 GetTypeHash(const FPackageId& In)
	{
		return uint32(In.Id);
	}

	CORE_API friend FArchive& operator<<(FArchive& Ar, FPackageId& Value);

	CORE_API friend void operator<<(FStructuredArchiveSlot Slot, FPackageId& Value);

#if WITH_PACKAGEID_NAME_MAP
	CORE_API FName GetName() const;
#endif
};
