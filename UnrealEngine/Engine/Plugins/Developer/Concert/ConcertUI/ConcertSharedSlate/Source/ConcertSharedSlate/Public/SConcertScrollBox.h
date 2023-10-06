// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"

class SScrollBox;
class SButton;
class SScrollBar;


class CONCERTSHAREDSLATE_API SConcertScrollBox : public SCompoundWidget
{
public:

	class FSlot : public TSlotBase<FSlot>
	{
	public:
		using TSlotBase<FSlot>::TSlotBase;

		SLATE_SLOT_BEGIN_ARGS(FSlot, TSlotBase<FSlot>)
		SLATE_SLOT_END_ARGS()
	};

	SLATE_BEGIN_ARGS(SConcertScrollBox) {}
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
	SLATE_END_ARGS();

	/**
	* Construct a custom ScrollBox that automatically scrolls to the bottom if the user is not currently scrolling.
	*
	* @param InArgs The Slate argument list.
	* @param ConstructUnderMajorTab The major tab which will contain the Session History widget.
	*/
	void Construct(const FArguments& InArgs);

	/** SWidget interface. */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** End of SWidget interface. */

	/**
	 * @return a new slot. Slots contain children for SScrollBox 
	 */
	static FSlot::FSlotArguments Slot();

private:
	
	/** Delegate called to handle a user scrolling. */
	void HandleUserScrolled(float);

	/** Checks if the scrollbar is currently at the bottom of its track. */
	bool IsAtBottom();

	/** Callback for scrolling to the start of the list. */
	FReply HandleScrollToStart();

	/** Callback for scrolling to the end of the list. */
	FReply HandleScrollToEnd();

	/** Callback for getting the ScrollBar buttons visibility. */
	EVisibility HandleScrollButtonsVisibility() const;

	/** Creates a button with the FontAwesome font. */
	TSharedRef<SButton> CreateScrollBarButton(const FText& InToolTip, const FString& InIcon, FOnClicked OnClickedDelegate) const;

private:
	
	/** Whether the scrollbar is locked to the bottom. */
	bool bIsLocked;

	/** Whether locking should be prevented. */
	bool bPreventLock;

	/** Holds the ScrollBar. */
	TSharedPtr<SScrollBar> ScrollBar;

	/** Holds the inner ScrollBox. */
	TSharedPtr<SScrollBox> ScrollBox;
};
