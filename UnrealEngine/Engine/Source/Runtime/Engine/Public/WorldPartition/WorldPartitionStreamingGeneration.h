// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "OverrideVoidReturnInvoker.h"
#include "WorldPartition/WorldPartitionActorDescView.h"

class ENGINE_API FActorDescViewMap
{
	friend class FWorldPartitionStreamingGenerator;

private:
	template <class Func>
	void ForEachActorDescView(Func InFunc)
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		for (TUniquePtr<FWorldPartitionActorDescView>& ActorDescView : ActorDescViewList)
		{
			if (!Invoker(*ActorDescView))
			{
				return;
			}
		}
	}

	FWorldPartitionActorDescView* FindByGuid(const FGuid& InGuid)
	{
		if (FWorldPartitionActorDescView** ActorDescViewPtr = ActorDescViewsByGuid.Find(InGuid))
		{
			return *ActorDescViewPtr;
		}
		return nullptr;
	}

	FWorldPartitionActorDescView& FindByGuidChecked(const FGuid& InGuid)
	{
		return *ActorDescViewsByGuid.FindChecked(InGuid);
	}

public:
	FActorDescViewMap();

	// Non-copyable but movable
	FActorDescViewMap(const FActorDescViewMap&) = delete;
	FActorDescViewMap(FActorDescViewMap&&) = default;
	FActorDescViewMap& operator=(const FActorDescViewMap&) = delete;
	FActorDescViewMap& operator=(FActorDescViewMap&&) = default;

	FWorldPartitionActorDescView* Emplace(const FGuid& InActorGuid, const FWorldPartitionActorDescView& InActorDescView);

	FORCEINLINE int32 Num() const
	{
		return ActorDescViewList.Num();
	}

	template <class Func>
	void ForEachActorDescView(Func InFunc) const
	{
		TOverrideVoidReturnInvoker Invoker(true, InFunc);

		for (const TUniquePtr<FWorldPartitionActorDescView>& ActorDescView : ActorDescViewList)
		{
			if (!Invoker(*ActorDescView))
			{
				return;
			}
		}
	}

	const FWorldPartitionActorDescView* FindByGuid(const FGuid& InGuid) const
	{
		if (const FWorldPartitionActorDescView* const* ActorDescViewPtr = ActorDescViewsByGuid.Find(InGuid))
		{
			return *ActorDescViewPtr;
		}
		return nullptr;
	}

	const FWorldPartitionActorDescView& FindByGuidChecked(const FGuid& InGuid) const
	{
		return *ActorDescViewsByGuid.FindChecked(InGuid);
	}

	template <class ClassType>
	TArray<const FWorldPartitionActorDescView*> FindByExactNativeClass() const
	{
		return FindByExactNativeClass(ClassType::StaticClass());
	}

	TArray<const FWorldPartitionActorDescView*> FindByExactNativeClass(UClass* InExactNativeClass) const;

	const TMap<FGuid, FWorldPartitionActorDescView*>& GetActorDescViewsByGuid() const { return ActorDescViewsByGuid; }

protected:
	TArray<TUniquePtr<FWorldPartitionActorDescView>> ActorDescViewList;

	TMap<FGuid, FWorldPartitionActorDescView*> ActorDescViewsByGuid;
	TMultiMap<FName, const FWorldPartitionActorDescView*> ActorDescViewsByClass;
};

#endif // WITH_EDITOR