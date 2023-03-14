// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "GameplayBehavior.h"
#include "GameplayBehaviorConfig.generated.h"



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
