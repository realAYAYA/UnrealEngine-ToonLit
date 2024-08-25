// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "UObject/WeakObjectPtr.h"

class UWorld;
class UWorldPartition;
class IWorldPartitionCell;
class FWorldPartitionCookPackageContext;
struct FWorldPartitionStreamingQuerySource;

struct FWorldPartitionUtils
{
	class ENGINE_API FSimulateCookedSession
	{
	public:
		FSimulateCookedSession(UWorld* InWorld);
		~FSimulateCookedSession();

		bool IsValid() const { return !!CookContext; }
		bool ForEachStreamingCells(TFunctionRef<void(const IWorldPartitionCell*)> Func);
		bool GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells);

	private:
		bool SimulateCook();

		FWorldPartitionCookPackageContext* CookContext;
		TWeakObjectPtr<UWorldPartition> WorldPartition;
	};
};

#endif