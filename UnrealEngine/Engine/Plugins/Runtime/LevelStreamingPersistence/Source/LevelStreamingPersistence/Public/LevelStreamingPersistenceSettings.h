// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LevelStreamingPersistenceSettings.generated.h"

USTRUCT()
struct LEVELSTREAMINGPERSISTENCE_API FLevelStreamingPersistentProperty
{
	GENERATED_BODY()

	UPROPERTY()
	FString Path;

	UPROPERTY()
	bool bIsPublic = false;
};

UCLASS(Config = Engine)
class LEVELSTREAMINGPERSISTENCE_API ULevelStreamingPersistenceSettings : public UObject
{
	GENERATED_BODY()

	UPROPERTY(Config)
	TArray<FLevelStreamingPersistentProperty> Properties;

	friend class ULevelStreamingPersistentPropertiesInfo;
};