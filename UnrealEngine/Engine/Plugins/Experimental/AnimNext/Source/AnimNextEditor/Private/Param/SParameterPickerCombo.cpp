// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterPickerCombo.h"
#include "SParameterPicker.h"
#include "UncookedOnlyUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SSearchBox.h"

namespace UE::AnimNext::Editor
{

void SParameterPickerCombo::Construct(const FArguments& InArgs)
{
	OnGetParameterNameDelegate = InArgs._OnGetParameterName;
	OnGetParameterTypeDelegate = InArgs._OnGetParameterType;

	FParameterPickerArgs PickerArgs = InArgs._PickerArgs;
	PickerArgs.OnParameterPicked = FOnParameterPicked::CreateLambda([this, OrigPickerArgs = InArgs._PickerArgs](const FParameterBindingReference& InReference)
	{
		FSlateApplication::Get().DismissAllMenus();

		// Forward to the original delegate
		OrigPickerArgs.OnParameterPicked.ExecuteIfBound(InReference);

		RequestRefresh();
	});
	PickerArgs.bFocusSearchWidget = true;

	ChildSlot
	[
		SNew(SComboButton)
		.OnGetMenuContent_Lambda([PickerArgs, this]()
		{
			return
				SNew(SBox)
				.WidthOverride(300.0f)
				.HeightOverride(300.0f)
				[
					SNew(SParameterPicker)
					.Args(PickerArgs)
				];
		})
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(0.0f, 2.0f, 2.0f, 2.0f)
			[
				SNew(SImage)
				.Image_Lambda([this]()
				{
					return Icon;
				})
				.ColorAndOpacity_Lambda([this]()
				{
					return IconColor;
				})
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(0.0f, 2.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>( "SmallText" ))
				.Text_Lambda([this]()
				{
					return ParameterNameText;
				})
			]
		]
	];

	RequestRefresh();
}

void SParameterPickerCombo::RequestRefresh()
{
	RegisterActiveTimer(1.0f/60.0f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
	{
		if(OnGetParameterNameDelegate.IsBound())
		{
			ParameterName = OnGetParameterNameDelegate.Execute();
		}

		if(OnGetParameterTypeDelegate.IsBound())
		{
			ParameterType = OnGetParameterTypeDelegate.Execute();
		}

		ParameterNameText = UE::AnimNext::UncookedOnly::FUtils::GetParameterDisplayNameText(ParameterName);
		PinType = UncookedOnly::FUtils::GetPinTypeFromParamType(ParameterType);
		Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
		IconColor = GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);

		return EActiveTimerReturnType::Stop;
	}));
}

}
