// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "KismetPins/SGraphPinNumSlider.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

class SWidget;
class UEdGraphPin;

class GRAPHEDITOR_API SGraphPinIntegerSlider : public SGraphPinNumSlider<int32>
{
public:
	SLATE_BEGIN_ARGS(SGraphPinIntegerSlider) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, FProperty* InProperty);

protected:
	//~ Begin SGraphPinString Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPinString Interface
};
