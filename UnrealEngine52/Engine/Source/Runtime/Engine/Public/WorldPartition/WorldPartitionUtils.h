// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "UObject/WeakObjectPtr.h"

class UWorld;
class UWorldPartition;
class IWorldPartitionCell;
struct FWorldPartitionStreamingQuerySource;

struct FWorldPartitionUtils
{
	class FSimulateCookedSession
	{
	public:
		FSimulateCookedSession(UWorld* InWorld);
		~FSimulateCookedSession();

		bool IsValid() const { return bIsValid; }
		bool ForEachStreamingCells(TFunctionRef<void(const IWorldPartitionCell*)> Func);
		bool GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells);

	private:
		bool SimulateCook();

		bool bIsValid;
		TWeakObjectPtr<UWorldPartition> WorldPartition;
	};
};

#endif