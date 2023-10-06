// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSessionTableTreeView.h"

// Insights
#include "Insights/InsightsManager.h"

#define LOCTEXT_NAMESPACE "Insights::SSessionTableTreeView"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////
// SSessionTableTreeView
////////////////////////////////////////////////////////////////////////////////////////////////////

SSessionTableTreeView::~SSessionTableTreeView()
{
	// Remove ourselves from the Insights manager.
	if (FInsightsManager::Get().IsValid())
	{
		FInsightsManager::Get()->GetSessionChangedEvent().RemoveAll(this);
	}

	Session.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SSessionTableTreeView::ConstructWidget(TSharedPtr<FTable> InTablePtr)
{
	STableTreeView::ConstructWidget(InTablePtr);

	// Register ourselves with the Insights manager.
	FInsightsManager::Get()->GetSessionChangedEvent().AddSP(this, &SSessionTableTreeView::InsightsManager_OnSessionChanged);

	// Update the Session (i.e. when analysis session was already started).
	InsightsManager_OnSessionChanged();

	CreateGroupings();
	CreateSortings();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SSessionTableTreeView::InsightsManager_OnSessionChanged()
{
	TSharedPtr<const TraceServices::IAnalysisSession> NewSession = FInsightsManager::Get()->GetSession();
	if (NewSession != Session)
	{
		Session = NewSession;
		Reset();
	}
	else
	{
		UpdateTree();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
