// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"

#include "VisibilityBinding.generated.h"

enum class ESlateVisibility : uint8;

UCLASS()
class UMG_API UVisibilityBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UVisibilityBinding();

	virtual bool IsSupportedSource(FProperty* Property) const override;
	virtual bool IsSupportedDestination(FProperty* Property) const override;

	UFUNCTION()
	ESlateVisibility GetValue() const;
};
