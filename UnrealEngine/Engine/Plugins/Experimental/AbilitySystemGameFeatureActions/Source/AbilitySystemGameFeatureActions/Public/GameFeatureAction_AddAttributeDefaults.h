// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
	virtual void OnGameFeatureUnregistering() override;
	//~End of UGameFeatureAction interface

	/** List of attribute default tables to add */
	UPROPERTY(EditAnywhere, Category = Attributes)
	TArray<FSoftObjectPath> AttribDefaultTableNames;

private:
	FName AttributeDefaultTablesOwnerName;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
