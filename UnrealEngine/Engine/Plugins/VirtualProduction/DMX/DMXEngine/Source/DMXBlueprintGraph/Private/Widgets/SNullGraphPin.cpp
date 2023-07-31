// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SNullGraphPin.h"

#include "Widgets/SNullWidget.h"

void SNullGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget> SNullGraphPin::GetDefaultValueWidget()
{
	//Create widget
	return SNullWidget::NullWidget;
}