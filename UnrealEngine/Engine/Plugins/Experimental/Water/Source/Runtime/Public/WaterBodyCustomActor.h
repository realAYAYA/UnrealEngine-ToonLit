// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterBodyActor.h"
#include "WaterBodyCustomActor.generated.h"

class UDEPRECATED_CustomMeshGenerator;
class UStaticMeshComponent;

// ----------------------------------------------------------------------------------

UCLASS(MinimalAPI)
class UDEPRECATED_CustomMeshGenerator : public UDEPRECATED_WaterBodyGenerator
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<UStaticMeshComponent> MeshComp;
};

// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API AWaterBodyCustom : public AWaterBody
{
	GENERATED_UCLASS_BODY()
protected:
	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<UDEPRECATED_CustomMeshGenerator> CustomGenerator_DEPRECATED;
#endif
};