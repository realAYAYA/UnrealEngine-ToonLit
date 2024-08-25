// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "COIBakingTestCommandlet.generated.h"

class UCustomizableObjectInstance;
struct FUpdateContext;

/**
 * Commandlet designed to test the baking of instances.
 * It expects an instance UAsset being provided to it so it can later compile it's CO, update the instance itself, and then bake it.
 * EX : -Run=COIBakingTest -EnablePlugins=MutableTesting -CustomizableObjectInstance="/MutableTesting/Cyborg_Character/Character_Inst" -AllowCommandletRendering
 */
UCLASS()
class MUTABLEVALIDATION_API UCOIBakingTestCommandlet : public UCommandlet
{
	GENERATED_BODY()
	
public:
	virtual int32 Main(const FString& Params) override;
	
private:
	
	/** Instance targeted for baking */
	UPROPERTY()
	TObjectPtr<UCustomizableObjectInstance> TargetInstance = nullptr;
};

