// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Volume.h"

#include "PCGVolume.generated.h"

class UPCGComponent;

UCLASS(BlueprintType, DisplayName = "PCG Volume")
class PCG_API APCGVolume : public AVolume
{
	GENERATED_BODY()

public:
	APCGVolume(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	virtual FName GetCustomIconName() const { return NAME_None; }
#endif // WITH_EDITOR
	//~ End AActor Interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PCG)
	TObjectPtr<UPCGComponent> PCGComponent;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
