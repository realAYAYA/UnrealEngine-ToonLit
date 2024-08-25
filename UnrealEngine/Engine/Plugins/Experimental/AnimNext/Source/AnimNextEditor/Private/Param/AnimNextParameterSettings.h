// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "UObject/Object.h"
#include "Param/ParamType.h"
#include "AnimNextParameterSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class UAnimNextParameterSettings : public UObject
{
	GENERATED_BODY()

	UAnimNextParameterSettings();

public:
	// Get the type of the last parameter type that we created
	const FAnimNextParamType& GetLastParameterType() const;

	// Set the type of the last parameter type that we created
	void SetLastParameterType(const FAnimNextParamType& InLastParameterType);
private:
	UPROPERTY(Transient)
	FAnimNextParamType LastParameterType = FAnimNextParamType::GetType<bool>();
};