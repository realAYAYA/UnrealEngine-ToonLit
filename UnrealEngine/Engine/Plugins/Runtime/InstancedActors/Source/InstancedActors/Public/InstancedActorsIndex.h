// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Serialization/StructuredArchive.h"
#include "InstancedActorsIndex.generated.h"


class UInstancedActorsData;
class AInstancedActorsManager;
struct FInstancedActorsIterationContext;

/** This type is only valid to be used with the instance of UInstancedActorsData it applies to. */
USTRUCT()
struct INSTANCEDACTORS_API FInstancedActorsInstanceIndex
{
	GENERATED_BODY()
	
	FInstancedActorsInstanceIndex() = default;
	explicit FInstancedActorsInstanceIndex(const int32 InIndex) : Index((uint16)InIndex) 
	{ 
		check((InIndex >= INDEX_NONE) && (InIndex < (MAX_uint16 - 1))); // -1 for INDEX_NONE
	}

	friend FArchive& operator<<(FArchive& Ar, FInstancedActorsInstanceIndex& InstanceIndex)
	{
		Ar << InstanceIndex.Index;
		return Ar;
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, FInstancedActorsInstanceIndex& InstanceIndex)
	{
		Slot << InstanceIndex.Index;
	}

	friend uint32 GetTypeHash(const FInstancedActorsInstanceIndex& InstanceIndex)
	{
		return uint32(InstanceIndex.Index);
	}

	bool IsValid() const { return Index != INDEX_NONE; }

	/** Returns a string suitable for debug logging to identify this instance. */
	FString GetDebugName() const;

	bool operator==(const FInstancedActorsInstanceIndex&) const = default;

	int32 GetIndex() const { return Index; }

	static constexpr FORCEINLINE int32 BuildCompositeIndex(const uint16 InstanceDataID, const int32 InstanceIndex)
	{
		const uint32 HighBits = InstanceDataID;
		const uint32 LowBits = InstanceIndex;
		check(LowBits <= MAX_uint16);
		return static_cast<int32>((HighBits << InstanceIndexBits) | LowBits);
	}

	static constexpr FORCEINLINE int32 ExtractInstanceDataID(const int32 CompositeIndex)
	{
		return CompositeIndex >> InstanceIndexBits;
	}

	static constexpr FORCEINLINE int32 ExtractInternalInstanceIndex(const int32 CompositeIndex)
	{
		return (CompositeIndex & InstanceIndexMask);
	}

private:
	/** Stable(consistent between client and server) instance index into UInstancedActorsData */
	UPROPERTY()
	uint16 Index = INDEX_NONE;

	static constexpr int32 InstanceIndexBits = 16;
	static constexpr int32 InstanceIndexMask = (1 << InstanceIndexBits) - 1;
};

template<>
struct TStructOpsTypeTraits<FInstancedActorsInstanceIndex> : public TStructOpsTypeTraitsBase2<FInstancedActorsInstanceIndex>
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = true,
		WithIdenticalViaEquality = true,
	};
};

USTRUCT(BlueprintType)
struct INSTANCEDACTORS_API FInstancedActorsInstanceHandle
{
	GENERATED_BODY()

	FInstancedActorsInstanceHandle() = default;
	FInstancedActorsInstanceHandle(UInstancedActorsData& InInstancedActorData, FInstancedActorsInstanceIndex InIndex);

	UInstancedActorsData* GetInstanceActorData() const { return InstancedActorData; }
	UInstancedActorsData& GetInstanceActorDataChecked() const;

	AInstancedActorsManager* GetManager() const;
	AInstancedActorsManager& GetManagerChecked() const;

	bool IsValid() const;

	/** Returns a string suitable for debug logging to identify this instance. */
	FString GetDebugName() const;

	bool operator==(const FInstancedActorsInstanceHandle&) const = default;
	FInstancedActorsInstanceIndex GetInstanceIndex() const { return Index; }
	int32 GetIndex() const { return Index.GetIndex(); }

	void Reset()
	{
		InstancedActorData = nullptr;
		Index = FInstancedActorsInstanceIndex();
	}

private:

	friend INSTANCEDACTORS_API uint32 GetTypeHash(const FInstancedActorsInstanceHandle& InstanceHandle);
	friend UInstancedActorsData;
	friend AInstancedActorsManager;
	friend FInstancedActorsIterationContext;

	/** Specific UInstancedActorsData responsible for this instance.Can be used to get to it's owning Manager. */
	UPROPERTY()
	TObjectPtr<UInstancedActorsData> InstancedActorData = nullptr;

	/** Stable(consistent between client and server) instance index into UInstancedActorsData. */
	UPROPERTY()
	FInstancedActorsInstanceIndex Index;
};

