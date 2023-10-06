// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassSettings.h"
#include "MassLODTypes.h"
#include "MassBehaviorSettings.generated.h"

UCLASS(config = Mass, defaultconfig, meta = (DisplayName = "Mass Behavior"))
class MASSAIBEHAVIOR_API UMassBehaviorSettings : public UMassModuleSettings
{
	GENERATED_BODY()

public:
	UMassBehaviorSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	UPROPERTY(EditAnywhere, Category = "Mass|LOD", config)
	int32 MaxActivationsPerLOD[EMassLOD::Max];
};
