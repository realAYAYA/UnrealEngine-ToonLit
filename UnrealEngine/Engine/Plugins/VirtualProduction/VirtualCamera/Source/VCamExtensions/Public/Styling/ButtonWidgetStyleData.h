// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetStyleData.h"
#include "Styling/SlateTypes.h"
#include "ButtonWidgetStyleData.generated.h"

/** Associates a button style for widgets that want to display a button for a modifier / connection point. */
UCLASS()
class VCAMEXTENSIONS_API UButtonWidgetStyleData : public UWidgetStyleData
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Virtual Camera")
	FButtonStyle ButtonStyle;
};