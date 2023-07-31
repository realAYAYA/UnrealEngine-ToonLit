// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayBehaviorConfig.h"
#include "BehaviorTree/BehaviorTree.h"
#include "GameplayBehaviorConfig_BehaviorTree.generated.h"


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
