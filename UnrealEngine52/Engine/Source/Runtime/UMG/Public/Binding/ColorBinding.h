// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Styling/SlateColor.h"
#include "Binding/PropertyBinding.h"
#include "ColorBinding.generated.h"

UCLASS()
class UMG_API UColorBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UColorBinding();

	virtual bool IsSupportedSource(FProperty* Property) const override;
	virtual bool IsSupportedDestination(FProperty* Property) const override;

	virtual void Bind(FProperty* Property, FScriptDelegate* Delegate) override;

	UFUNCTION()
	FSlateColor GetSlateValue() const;

	UFUNCTION()
	FLinearColor GetLinearValue() const;

private:
	mutable TOptional<bool> bNeedsConversion;
};
