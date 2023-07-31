// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"
#include "CheckedStateBinding.generated.h"

enum class ECheckBoxState : uint8;

UCLASS()
class UMG_API UCheckedStateBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UCheckedStateBinding();

	virtual bool IsSupportedSource(FProperty* Property) const override;
	virtual bool IsSupportedDestination(FProperty* Property) const override;

	UFUNCTION()
	ECheckBoxState GetValue() const;

private:
	enum class EConversion : uint8
	{
		None,
		Bool
	};

	mutable TOptional<EConversion> bConversion;
};
