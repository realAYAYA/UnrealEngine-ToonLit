// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Param/ParameterPickerArgs.h"
#include "EdGraph/EdGraphPin.h"

struct FAnimNextParamType;

namespace UE::AnimNext::Editor
{
class SParameterPicker;
}

namespace UE::AnimNext::Editor
{

/** Retrieves the parameter name to display */
using FOnGetParameterName = TDelegate<FName(void)>;

/** Retrieves the parameter type to display */
using FOnGetParameterType = TDelegate<FAnimNextParamType(void)>;

class SParameterPickerCombo : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SParameterPickerCombo) {}

	/** Arguments for configuring the picker in the dropdown menu */
	SLATE_ARGUMENT(FParameterPickerArgs, PickerArgs)

	/** Retrieves the parameter name to display */
	SLATE_EVENT(FOnGetParameterName, OnGetParameterName)

	/** Retrieves the parameter type to display */
	SLATE_EVENT(FOnGetParameterType, OnGetParameterType)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RequestRefresh();

	// Retrieves the parameter name to display
	FOnGetParameterName OnGetParameterNameDelegate;

	// Retrieves the parameter type to display
	FOnGetParameterType OnGetParameterTypeDelegate;

	// Cached pin type
	FEdGraphPinType PinType;

	// Cached name
	FName ParameterName;

	// Cached display name
	FText ParameterNameText;

	// Cached parameter type
	FAnimNextParamType ParameterType;

	// Cached icon
	const FSlateBrush* Icon = nullptr;

	// Cached color
	FSlateColor IconColor = FLinearColor::Gray;

	// Picker widget used to focus after the popup is displayed
	TWeakPtr<SParameterPicker> PickerWidget;
};

}