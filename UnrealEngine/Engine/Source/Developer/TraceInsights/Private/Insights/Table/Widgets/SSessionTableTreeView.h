// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/Table/Widgets/STableTreeView.h"

namespace TraceServices
{
	class IAnalysisSession;
}

namespace Insights
{

class FTable;

////////////////////////////////////////////////////////////////////////////////////////////////////

class SSessionTableTreeView : public STableTreeView
{
public:
	virtual ~SSessionTableTreeView() override;

protected:
	virtual void ConstructWidget(TSharedPtr<FTable> InTablePtr) override;

	/** Called when the analysis session has changed. */
	void InsightsManager_OnSessionChanged();

	/** The analysis session used to populate this widget. */
	TSharedPtr<const TraceServices::IAnalysisSession> Session;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
