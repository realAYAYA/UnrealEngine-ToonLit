// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/Attribute.h"
#include "Misc/FrameTime.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

struct FFrameNumber;

namespace UE
{
namespace Sequencer
{

class FViewModel;

/**
 * A widget for navigating between keys on a sequencer track
 */
class SEQUENCERCORE_API SKeyNavigationButtons
	: public SCompoundWidget
{
public:

	DECLARE_DELEGATE_OneParam(FOnSetTime, FFrameTime)
	DECLARE_DELEGATE_OneParam(FOnGetNavigatableTimes, TArray<FFrameNumber>&)
	DECLARE_DELEGATE_TwoParams(FOnAddKey, FFrameTime, TSharedPtr<FViewModel>)

	SLATE_BEGIN_ARGS(SKeyNavigationButtons) : _IsEnabled(true) {}
		SLATE_ATTRIBUTE(bool, IsEnabled)

		SLATE_ARGUMENT(FText, PreviousKeyToolTip)
		SLATE_ARGUMENT(FText, NextKeyToolTip)

		SLATE_ATTRIBUTE(FFrameTime, Time)
		SLATE_EVENT(FOnSetTime, OnSetTime)

		SLATE_EVENT(FOnAddKey, OnAddKey)
		SLATE_ARGUMENT(FText, AddKeyToolTip)

		SLATE_EVENT(FOnGetNavigatableTimes, GetNavigatableTimes)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FViewModel>& InModel);

	FLinearColor GetHoverTint() const;
	FReply OnPreviousKeyClicked();
	FReply OnNextKeyClicked();
	FReply OnAddKeyClicked();

protected:

	TAttribute<FFrameTime> TimeAttribute;
	FOnSetTime SetTimeEvent;
	FOnAddKey AddKeyEvent;
	FOnGetNavigatableTimes GetNavigatableTimesEvent;

	TWeakPtr<FViewModel> WeakModel;
};

} // namespace Sequencer
} // namespace UE

