// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITORONLY_DATA
#include "Misc/HashBuilder.h"

/**
 * FPartitionInstanceHash
 * Used for editor locality of placed instances in a partition actor to control density and avoid overlapping instances
 */
struct FInstancedPlacementHash
{
private:
	static constexpr int32 MaxHashCellBits = 9;	// 512x512x512 grid
	struct FKey
	{
		int64 X;
		int64 Y;
		int64 Z;

		FKey() = default;

		FKey(int64 InX, int64 InY, int64 InZ)
			: X(InX), Y(InY), Z(InZ) {}

		bool operator==(const FKey& Other) const
		{
			return (X == Other.X) && (Y == Other.Y) && (Z == Other.Z);
		}

		friend uint32 GetTypeHash(const FKey& Key)
		{
			FHashBuilder HashBuilder;
			HashBuilder << Key.X << Key.Y << Key.Z;
			return HashBuilder.GetHash();
		}

		friend FArchive& operator<<(FArchive& Ar, FKey& Key)
		{
			Ar << Key.X;
			Ar << Key.Y;
			Ar << Key.Z;
			return Ar;
		}
	};

	const int32 HashCellBits;
	TMap<FKey, TSet<int32>> CellMap;

	FKey MakeKey(const FVector& Location) const
	{
		return FKey(FMath::FloorToInt64(Location.X) >> HashCellBits, FMath::FloorToInt64(Location.Y) >> HashCellBits, FMath::FloorToInt64(Location.Z) >> HashCellBits);
	}

	FVector MakeLocation(FKey CellKey) const
	{
		return FVector(FVector::FReal(CellKey.X << HashCellBits), FVector::FReal(CellKey.Y << HashCellBits), FVector::FReal(CellKey.Z << HashCellBits));
	}

public:
	FInstancedPlacementHash(int32 InHashCellBits = MaxHashCellBits)
		: HashCellBits(InHashCellBits)
	{}

	~FInstancedPlacementHash() = default;

	void InsertInstance(const FVector& InstanceLocation, int32 InstanceIndex)
	{
		FKey Key = MakeKey(InstanceLocation);

		CellMap.FindOrAdd(Key).Add(InstanceIndex);
	}

	void RemoveInstance(const FVector& InstanceLocation, int32 InstanceIndex, bool bChecked = true)
	{
		FKey Key = MakeKey(InstanceLocation);

		if (bChecked)
		{
			int32 RemoveCount = CellMap.FindChecked(Key).Remove(InstanceIndex);
			check(RemoveCount == 1);
		}
		else if (TSet<int32>* Value = CellMap.Find(Key))
		{
			Value->Remove(InstanceIndex);
		}
	}

	void SwapInstance(const FVector& InstanceLocation, int32 OldIndex, int32 NewIndex, bool bCheckedRemoval = true)
	{
		RemoveInstance(InstanceLocation, OldIndex, bCheckedRemoval);
		InsertInstance(InstanceLocation, NewIndex);
	}

	void GetInstancesOverlappingBox(const FBox& InBox, TArray<int32>& OutInstanceIndices) const
	{
		FKey MinKey = MakeKey(InBox.Min);
		FKey MaxKey = MakeKey(InBox.Max);

		for (int64 z = MinKey.Z; z <= MaxKey.Z; ++z)
		{
			for (int64 y = MinKey.Y; y <= MaxKey.Y; y++)
			{
				for (int64 x = MinKey.X; x <= MaxKey.X; x++)
				{
					auto* SetPtr = CellMap.Find(FKey(x, y, z));
					if (SetPtr)
					{
						OutInstanceIndices.Append(SetPtr->Array());
					}
				}
			}
		}
	}

	TArray<int32> GetInstancesOverlappingBox(const FBox& InBox) const
	{
		TArray<int32> Result;
		GetInstancesOverlappingBox(InBox, Result);
		return Result;
	}

	void CheckInstanceCount(int32 InCount) const
	{
		int32 HashCount = 0;
		for (const auto& Pair : CellMap)
		{
			HashCount += Pair.Value.Num();
		}

		check(HashCount == InCount);
	}

	FBox GetBounds() const
	{
		FBox HashBounds(ForceInit);
		for (const auto& Pair : CellMap)
		{
			HashBounds += MakeLocation(Pair.Key);
		}

		return HashBounds;
	}

	void Empty()
	{
		CellMap.Empty();
	}

	friend FArchive& operator<<(FArchive& Ar, FInstancedPlacementHash& Hash)
	{
		Ar << Hash.CellMap;
		return Ar;
	}
};
#endif
