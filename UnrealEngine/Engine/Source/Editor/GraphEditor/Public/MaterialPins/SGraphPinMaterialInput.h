// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UEdGraphPin;

class GRAPHEDITOR_API SGraphPinMaterialInput : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinMaterialInput) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	//~ Begin SGraphPin Interface
	virtual FSlateColor GetPinColor() const override;
	//~ End SGraphPin Interface

};
