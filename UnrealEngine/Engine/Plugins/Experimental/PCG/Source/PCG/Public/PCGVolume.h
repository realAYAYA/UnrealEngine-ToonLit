// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Volume.h"

#include "PCGVolume.generated.h"

class UPCGComponent;

UCLASS(BlueprintType)
class PCG_API APCGVolume : public AVolume
{
	GENERATED_BODY()

public:
	APCGVolume(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PCG)
	TObjectPtr<UPCGComponent> PCGComponent;
};
