// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "IMessageContext.h"
#include "LiveLinkMessageBusFinder.h"
#include "Misc/App.h"
#include "Widgets/Views/SListView.h"

struct FLiveLinkPongMessage;
struct FMessageAddress;
struct FProviderPollResult;
class ILiveLinkClient;
class ITableRow;
class STableViewBase;

DECLARE_DELEGATE_OneParam(FOnLiveLinkMessageBusSourceSelected, FProviderPollResultPtr);

class SLiveLinkMessageBusSourceFactory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkMessageBusSourceFactory) {}
		SLATE_EVENT(FOnLiveLinkMessageBusSourceSelected, OnSourceSelected)
	SLATE_END_ARGS()

	~SLiveLinkMessageBusSourceFactory();

	void Construct(const FArguments& Args);

	FProviderPollResultPtr GetSelectedSource() const { return SelectedResult; }

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);

private:
	/** Represents a source with its latest time since it was polled successfully. */
	struct FLiveLinkSource
	{
		FProviderPollResultPtr PollResult;
		double LastTimeSincePong = 0.0;
		
		FLiveLinkSource(FProviderPollResultPtr InPollResult)
            : PollResult(MoveTemp(InPollResult))
			, LastTimeSincePong(FApp::GetCurrentTime()) 
		{
		}
	};

private:
	/** Create a widget entry in the source list based on a live link source. */
	TSharedRef<ITableRow> MakeSourceListViewWidget(TSharedPtr<FLiveLinkSource> Source, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Handles selection changing in the list. */
	void OnSourceListSelectionChanged(TSharedPtr<FLiveLinkSource> Source, ESelectInfo::Type SelectionType);

private:
	/** The widgets displayed in the list. */
	TSharedPtr<SListView<TSharedPtr<FLiveLinkSource>>> ListView;

	/** Latest sources pulled from the discovery manager. */
	TArray<FProviderPollResultPtr> PollData;

	/** The sources displayed by the list. */
	TArray<TSharedPtr<FLiveLinkSource>> Sources;

	/** The source that was selected. */
	FProviderPollResultPtr SelectedResult;

	/** The last source that was selected. */
	FOnLiveLinkMessageBusSourceSelected OnSourceSelected;

	/** The last time at which the UI was refreshed. */
	double LastUIUpdateSeconds = 0;

	/** Time before a source should disappear from the UI after its last pong. */
	double SecondsBeforeSourcesDisappear = 2.0;
};