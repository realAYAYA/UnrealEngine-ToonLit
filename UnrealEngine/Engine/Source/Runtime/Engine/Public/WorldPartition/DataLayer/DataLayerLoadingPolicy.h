// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DataLayerLoadingPolicy.generated.h"

class UDataLayerInstance;

UCLASS()
class ENGINE_API UDataLayerLoadingPolicy : public UObject
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	virtual bool ResolveIsLoadedInEditor(TArray<const UDataLayerInstance*>& InDataLayers) const;
#endif
};