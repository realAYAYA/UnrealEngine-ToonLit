// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"

#include "GameFeatureAction_AddAttributeDefaults.generated.h"

//////////////////////////////////////////////////////////////////////
// UGameFeatureAction_AddAttributeDefaults

/**
 * Adds ability system attribute defaults from this game feature
 */
UCLASS(MinimalAPI, meta = (DisplayName = "Add Attribute Defaults"))
class UGameFeatureAction_AddAttributeDefaults final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~UGameFeatureAction interface
	virtual void OnGameFeatureRegistering() override;
	//~End of UGameFeatureAction interface

	/** List of attribute default tables to add */
	UPROPERTY(EditAnywhere, Category = Attributes)
	TArray<FSoftObjectPath> AttribDefaultTableNames;
};
