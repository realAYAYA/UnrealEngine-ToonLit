// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Session/History/SSessionHistory.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SSessionHistory;
struct FConcertSessionActivity;

struct FCanPerformActionResult
{
	TOptional<FText> DeletionReason;

	static FCanPerformActionResult Yes() { return FCanPerformActionResult(); }
	static FCanPerformActionResult No(FText Reason) { return FCanPerformActionResult(MoveTemp(Reason)); }

	bool CanPerformAction() const { return !DeletionReason.IsSet(); }

	explicit FCanPerformActionResult(TOptional<FText> DeletionReason = {})
		: DeletionReason(DeletionReason)
	{}
};

/** Allows activities in the session history to be deleted. */
class CONCERTSHAREDSLATE_API SEditableSessionHistory : public SCompoundWidget
{
public:

	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SSessionHistory>, FMakeSessionHistory, SSessionHistory::FArguments)
	DECLARE_DELEGATE_RetVal_OneParam(FCanPerformActionResult, FCanPerformActionOnActivities, const TSet<TSharedRef<FConcertSessionActivity>>& /*Activities*/)
	DECLARE_DELEGATE_OneParam(FRequestActivitiesAction, const TSet<TSharedRef<FConcertSessionActivity>>&)

	SLATE_BEGIN_ARGS(SEditableSessionHistory)
	{}
		SLATE_EVENT(FMakeSessionHistory, MakeSessionHistory)
	
		/** Can selected activities be deleted? */
		SLATE_EVENT(FCanPerformActionOnActivities, CanDeleteActivities)
		/** Delete the selected activities */
		SLATE_EVENT(FRequestActivitiesAction, DeleteActivities)
		/** Can selected activities be muted? */
		SLATE_EVENT(FCanPerformActionOnActivities, CanMuteActivities)
		/** Mute the selected activities */
		SLATE_EVENT(FRequestActivitiesAction, MuteActivities)
		/** Can selected activities be unmuted? */
		SLATE_EVENT(FCanPerformActionOnActivities, CanUnmuteActivities)
		/** Unmute the selected activities */
		SLATE_EVENT(FRequestActivitiesAction, UnmuteActivities)
	
		SLATE_NAMED_SLOT(FArguments, StatusBar)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	
	//~ Begin SWidget Interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget Interface
	
private:

	TSharedPtr<SSessionHistory> SessionHistory;

	FCanPerformActionOnActivities CanDeleteActivitiesFunc;
	FRequestActivitiesAction DeleteActivitiesFunc;
	FCanPerformActionOnActivities CanMuteActivitiesFunc;
	FRequestActivitiesAction MuteActivitiesFunc;
	FCanPerformActionOnActivities CanUnmuteActivitiesFunc;
	FRequestActivitiesAction UnmuteActivitiesFunc;

	TSharedPtr<SWidget> OnContextMenuOpening();
	
	FReply OnClickDeleteActivitiesButton() const;
	FText GetDeleteActivitiesToolTip() const;
	bool IsDeleteButtonEnabled() const;
	
	FReply OnClickMuteActivitesButton() const;
	FText GetMuteActivitiesToolTip() const;
	bool IsMuteButtonEnabled() const;
	
	FReply OnClickUnmuteActivitesButton() const;
	FText GetUnmuteActivitiesToolTip() const;
	bool IsUnmuteButtonEnabled() const;

	FText GenerateTooltip(const FCanPerformActionOnActivities& CanPerformAction, FText SelectActivityToolTip, FText PerformActionToolTipFmt, FText CannotPerformActionToolTipFmt) const;
};
