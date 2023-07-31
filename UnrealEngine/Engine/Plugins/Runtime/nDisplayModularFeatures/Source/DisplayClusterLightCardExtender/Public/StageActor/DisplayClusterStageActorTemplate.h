// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterStageActorTemplate.generated.h"

UCLASS(Abstract, PerObjectConfig, config=EditorPerProjectUserSettings)
class DISPLAYCLUSTERLIGHTCARDEXTENDER_API UDisplayClusterStageActorTemplate : public UObject
{
	GENERATED_BODY()

public:
	virtual AActor* GetTemplateActor() const { return nullptr; }
	
public:
	/** If the user has marked this a favorite template. */
	UPROPERTY(Config)
	bool bIsFavorite;
};