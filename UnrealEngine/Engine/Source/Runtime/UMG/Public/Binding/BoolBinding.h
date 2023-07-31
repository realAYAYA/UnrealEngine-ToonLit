// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"
#include "BoolBinding.generated.h"

UCLASS()
class UMG_API UBoolBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UBoolBinding();

	virtual bool IsSupportedSource(FProperty* Property) const override;
	virtual bool IsSupportedDestination(FProperty* Property) const override;

	UFUNCTION()
	bool GetValue() const;
};
