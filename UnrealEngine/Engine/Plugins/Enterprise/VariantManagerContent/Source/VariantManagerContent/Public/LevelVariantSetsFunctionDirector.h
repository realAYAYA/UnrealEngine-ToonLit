// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "LevelVariantSetsFunctionDirector.generated.h"

class ULevelVariantSetsFunctionDirector;

DECLARE_MULTICAST_DELEGATE_OneParam(OnDirectorDestroyed, ULevelVariantSetsFunctionDirector*);

UCLASS(Blueprintable)
class VARIANTMANAGERCONTENT_API ULevelVariantSetsFunctionDirector : public UObject
{
public:
	GENERATED_BODY()

	~ULevelVariantSetsFunctionDirector()
	{
		OnDestroy.Broadcast(this);
	}

	OnDirectorDestroyed& GetOnDestroy()
	{
		return OnDestroy;
	}

	virtual UWorld* GetWorld() const override;

	// Called from our destructor
	// Mainly used by levelvariantsets to keep track of when a director becomes invalid and
	// we need to create a new one for that world
	OnDirectorDestroyed OnDestroy;
};
