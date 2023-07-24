// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentInstanceDataCache.h"
#include "ObjectSnapshotData.h"
#include "Misc/ObjectDependencyCallback.h"
#include "ComponentSnapshotData.generated.h"

class UPackage;
struct FWorldSnapshotData;
struct FPropertySelection;

USTRUCT()
struct LEVELSNAPSHOTS_API FComponentSnapshotData
{
	GENERATED_BODY()
	
	/** Describes how the component was created */
	UPROPERTY()
	EComponentCreationMethod CreationMethod {};
};