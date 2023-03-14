// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CustomizableObjectPopulationConstraint.h"

#include "CustomizableObjectPopulationCharacteristic.generated.h"

USTRUCT()
struct FCustomizableObjectPopulationCharacteristic
{
public:

	GENERATED_USTRUCT_BODY()
	
	/** Name of the parameter to add contraints to */
	UPROPERTY(Category = "CustomizablePopulationClass", EditAnywhere, meta = (ToolTip = "Name of the Customizable Object Parameter that will be specified by this characteristic"))
	FString ParameterName;
	
	/** List of constraints */
	UPROPERTY(Category = "CustomizablePopulationClass", EditAnywhere, meta = (ToolTip = "Constraints applied to this characteristic"))
	TArray<FCustomizableObjectPopulationConstraint> Constraints;

};