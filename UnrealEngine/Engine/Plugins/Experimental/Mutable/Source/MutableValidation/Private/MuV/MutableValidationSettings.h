// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MutableValidationSettings.generated.h"

UCLASS(config = Editor)
class UMutableValidationSettings : public UObject
{
	GENERATED_BODY()

public:

	/** If true, validation of referenced COs from asset subject to data validation, will be run. */
	UPROPERTY(config, EditAnywhere, Category = Validation)
	bool bEnableIndirectValidation = false;
};
