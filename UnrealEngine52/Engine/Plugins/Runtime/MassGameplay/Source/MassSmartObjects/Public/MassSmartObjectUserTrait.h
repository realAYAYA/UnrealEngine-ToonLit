// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "GameplayTagContainer.h"
#include "MassSmartObjectUserTrait.generated.h"

/**
 * Trait to allow an entity to interact with SmartObjects
 */
UCLASS(meta = (DisplayName = "SmartObject User"))
class MASSSMARTOBJECTS_API UMassSmartObjectUserTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/** Tags describing the SmartObject user. Used when searching smart objects. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer UserTags;
};
