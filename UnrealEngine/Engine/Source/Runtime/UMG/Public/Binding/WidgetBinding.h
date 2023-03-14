// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"
#include "WidgetBinding.generated.h"

class UWidget;

UCLASS()
class UMG_API UWidgetBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UWidgetBinding();

	virtual bool IsSupportedSource(FProperty* Property) const override;
	virtual bool IsSupportedDestination(FProperty* Property) const override;

	UFUNCTION()
	UWidget* GetValue() const;
};
