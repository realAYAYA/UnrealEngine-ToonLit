// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "GenericPlatform/ICursor.h"
#include "Binding/PropertyBinding.h"
#include "MouseCursorBinding.generated.h"

UCLASS()
class UMG_API UMouseCursorBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UMouseCursorBinding();

	virtual bool IsSupportedSource(FProperty* Property) const override;
	virtual bool IsSupportedDestination(FProperty* Property) const override;

	UFUNCTION()
	EMouseCursor::Type GetValue() const;
};
