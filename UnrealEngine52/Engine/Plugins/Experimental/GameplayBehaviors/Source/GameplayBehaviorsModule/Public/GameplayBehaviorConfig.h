// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "GameplayBehaviorConfig.generated.h"

class UGameplayBehavior;



UCLASS(Blueprintable, BlueprintType, EditInlineNew, CollapseCategories)
class GAMEPLAYBEHAVIORSMODULE_API UGameplayBehaviorConfig : public UObject
{
	GENERATED_BODY()
public:
	//UGameplayBehavior(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Depending on the specific UGameplayBehavior class returns an instance or CDO of BehaviorClass. */
	virtual UGameplayBehavior* GetBehavior(UWorld& World) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = GameplayBehavior)
	TSubclassOf<UGameplayBehavior> BehaviorClass;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "GameplayBehavior.h"
#endif
