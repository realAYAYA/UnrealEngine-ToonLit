// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayBehaviorConfig.h"
#include "UObject/Package.h"
#include "GameplayBehaviorConfig_BehaviorTree.generated.h"

class UBehaviorTree;


UCLASS()
class GAMEPLAYBEHAVIORSMODULE_API UGameplayBehaviorConfig_BehaviorTree : public UGameplayBehaviorConfig
{
	GENERATED_BODY()
public:
	UGameplayBehaviorConfig_BehaviorTree(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UBehaviorTree* GetBehaviorTree() const;
	bool ShouldStorePreviousBT() const { return bRevertToPreviousBTOnFinish; }

protected:
	UPROPERTY(EditAnywhere, Category = SmartObject)
	mutable TSoftObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(EditAnywhere, Category = SmartObject)
	uint32 bRevertToPreviousBTOnFinish : 1;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "BehaviorTree/BehaviorTree.h"
#endif
