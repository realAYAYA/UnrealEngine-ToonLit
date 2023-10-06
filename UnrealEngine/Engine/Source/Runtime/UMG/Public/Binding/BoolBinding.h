// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"
#include "BoolBinding.generated.h"

UCLASS(MinimalAPI)
class UBoolBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UMG_API UBoolBinding();

	UMG_API virtual bool IsSupportedSource(FProperty* Property) const override;
	UMG_API virtual bool IsSupportedDestination(FProperty* Property) const override;

	UFUNCTION()
	UMG_API bool GetValue() const;
};
