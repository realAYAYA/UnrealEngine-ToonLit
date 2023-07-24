// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/SKeyNavigationButtons.h"

#include "Containers/UnrealString.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "MVVM/Extensions/IHoveredExtension.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "Math/NumericLimits.h"
#include "Misc/FrameNumber.h"
#include "Misc/FrameTime.h"
#include "Misc/Optional.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateColor.h"
#include "Templates/TypeHash.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

struct FSlateBrush;

namespace UE
{
namespace Sequencer
{

void SKeyNavigationButtons::Construct(const FArguments& InArgs, const TSharedPtr<FViewModel>& InModel)
{
	WeakModel = InModel;

	GetNavigatableTimesEvent = InArgs._GetNavigatableTimes;
	AddKeyEvent = InArgs._OnAddKey;
	SetTimeEvent = InArgs._OnSetTime;
	TimeAttribute = InArgs._Time;

	const FSlateBrush* NoBorder = FAppStyle::GetBrush( "NoBorder" );

	TAttribute<FLinearColor> HoverTint(this, &SKeyNavigationButtons::GetHoverTint);

	ChildSlot
	[
		SNew(SHorizontalBox)
		
		// Previous key slot
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(3, 0, 0, 0)
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(NoBorder)
			.ColorAndOpacity(HoverTint)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ToolTipText(InArgs._PreviousKeyToolTip)
				.OnClicked(this, &SKeyNavigationButtons::OnPreviousKeyClicked)
				.ForegroundColor( FSlateColor::UseForeground() )
				.ContentPadding(2)
				.IsFocusable(false)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.7"))
					.Text(FText::FromString(FString(TEXT("\xf060"))) /*fa-arrow-left*/)
				]
			]
		]
		// Add key slot
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(NoBorder)
			.ColorAndOpacity(HoverTint)
			.IsEnabled(InArgs._IsEnabled)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ToolTipText(InArgs._AddKeyToolTip)
				.OnClicked(this, &SKeyNavigationButtons::OnAddKeyClicked)
				.ForegroundColor( FSlateColor::UseForeground() )
				.ContentPadding(2)
				.IsFocusable(false)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.7"))
					.Text(FText::FromString(FString(TEXT("\xf055"))) /*fa-plus-circle*/)
				]
			]
		]
		// Next key slot
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(NoBorder)
			.ColorAndOpacity(HoverTint)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ToolTipText(InArgs._NextKeyToolTip)
				.OnClicked(this, &SKeyNavigationButtons::OnNextKeyClicked)
				.ContentPadding(2)
				.ForegroundColor( FSlateColor::UseForeground() )
				.IsFocusable(false)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.7"))
					.Text(FText::FromString(FString(TEXT("\xf061"))) /*fa-arrow-right*/)
				]
			]
		]
	];
}

FLinearColor SKeyNavigationButtons::GetHoverTint() const
{
	IHoveredExtension* HoveredExtension = ICastable::CastWeakPtr<IHoveredExtension>(WeakModel);
	return HoveredExtension && HoveredExtension->IsHovered() ? FLinearColor(1,1,1,0.9f) : FLinearColor(1,1,1,0.4f);
}

FReply SKeyNavigationButtons::OnPreviousKeyClicked()
{
	FFrameTime ClosestPreviousKeyDistance = FFrameTime(TNumericLimits<int32>::Max(), 0.99999f);
	FFrameTime CurrentTime = TimeAttribute.Get();
	TOptional<FFrameTime> PreviousTime;

	TArray<FFrameNumber> AllTimes;
	GetNavigatableTimesEvent.ExecuteIfBound(AllTimes);
	if (AllTimes.IsEmpty())
	{
		return FReply::Handled();
	}

	for (FFrameNumber Time : AllTimes)
	{
		if (Time < CurrentTime && CurrentTime - Time < ClosestPreviousKeyDistance)
		{
			PreviousTime = Time;
			ClosestPreviousKeyDistance = CurrentTime - Time;
		}
	}

	if (!PreviousTime.IsSet() && AllTimes.Num() > 0)
	{
		AllTimes.Sort();

		PreviousTime = AllTimes.Last();
	}

	if (PreviousTime.IsSet())
	{
		SetTimeEvent.ExecuteIfBound(PreviousTime.GetValue());
	}
	return FReply::Handled();
}

FReply SKeyNavigationButtons::OnNextKeyClicked()
{
	FFrameTime ClosestNextKeyDistance = FFrameTime(TNumericLimits<int32>::Max(), 0.99999f);
	FFrameTime CurrentTime = TimeAttribute.Get();
	TOptional<FFrameTime> NextTime;

	TArray<FFrameNumber> AllTimes;
	GetNavigatableTimesEvent.ExecuteIfBound(AllTimes);
	if (AllTimes.IsEmpty())
	{
		return FReply::Handled();
	}

	for (FFrameNumber Time : AllTimes)
	{
		if (Time > CurrentTime && Time - CurrentTime < ClosestNextKeyDistance)
		{
			NextTime = Time;
			ClosestNextKeyDistance = Time - CurrentTime;
		}
	}

	if (!NextTime.IsSet() && AllTimes.Num() > 0)
	{
		AllTimes.Sort();

		NextTime = AllTimes[0];
	}

	if (NextTime.IsSet())
	{
		SetTimeEvent.ExecuteIfBound(NextTime.GetValue());
	}

	return FReply::Handled();
}

FReply SKeyNavigationButtons::OnAddKeyClicked()
{
	if (TSharedPtr<FViewModel> DataModel = WeakModel.Pin())
	{
		FFrameTime CurrentTime = TimeAttribute.Get();
		AddKeyEvent.ExecuteIfBound(CurrentTime, DataModel);
	}

	return FReply::Handled();
}

} // namespace Sequencer
} // namespace UE

