// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEventNodeSpawner.h"

#include "AnimNotifyEventNodeSpawner.generated.h"

UCLASS(Transient)
class BLUEPRINTGRAPH_API UAnimNotifyEventNodeSpawner : public UBlueprintEventNodeSpawner
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new UAnimNotifyEventNodeSpawner 
	 *
	 * @param  NotifyName	The name you want assigned to the event.
	 * @return A newly allocated instance of this class.
	 */
	static UAnimNotifyEventNodeSpawner* Create(const FSoftObjectPath& InSkeletonObjectPath, FName InNotifyName);

	/** @return the skeleton object path */
	const FSoftObjectPath& GetSkeletonObjectPath() const { return SkeletonObjectPath; }

private:
	/** The skeleton that supplied this notify, used for filtering */
	UPROPERTY()
	FSoftObjectPath SkeletonObjectPath;
};
