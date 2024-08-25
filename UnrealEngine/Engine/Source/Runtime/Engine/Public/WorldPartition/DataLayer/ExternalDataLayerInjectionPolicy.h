// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreFwd.h"
#include "UObject/ObjectMacros.h"

#include "ExternalDataLayerInjectionPolicy.generated.h"

class UWorld;
class UExternalDataLayerAsset;

UCLASS()
class ENGINE_API UExternalDataLayerInjectionPolicy : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
public:
	virtual bool CanInject(const UWorld* InWorld, const UExternalDataLayerAsset* InExternalDataLayerAsset, const UObject* InClient, FText* OutFailureReason = nullptr) const;
#endif
};
