// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/IToolTip.h"
#include "Widgets/SToolTip.h"

class SGridPanel;

namespace Insights
{
	class FTable;
	class FTableColumn;
}

class FNetStatsCounterNode;

////////////////////////////////////////////////////////////////////////////////////////////////////

/** NetStats View Tooltip */
class SNetStatsCountersViewTooltip
{
public:
	SNetStatsCountersViewTooltip() = delete;

	static TSharedPtr<SToolTip> GetTableTooltip(const Insights::FTable& Table);
	static TSharedPtr<SToolTip> GetColumnTooltip(const Insights::FTableColumn& Column);
	static TSharedPtr<SToolTip> GetRowTooltip(const TSharedPtr<FNetStatsCounterNode> TreeNodePtr);

private:
	static void AddAggregatedStatsRow(TSharedPtr<SGridPanel> Grid, int32& Row, const FText& Name, const FText& Value);
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class SNetStatsCounterTableRowToolTip : public IToolTip
{
public:
	SNetStatsCounterTableRowToolTip(const TSharedPtr<FNetStatsCounterNode> InTreeNodePtr) : TreeNodePtr(InTreeNodePtr) {}
	virtual ~SNetStatsCounterTableRowToolTip() { }

	virtual TSharedRef<class SWidget> AsWidget()
	{
		CreateToolTipWidget();
		return ToolTipWidget.ToSharedRef();
	}

	virtual TSharedRef<SWidget> GetContentWidget()
	{
		CreateToolTipWidget();
		return ToolTipWidget->GetContentWidget();
	}

	virtual void SetContentWidget(const TSharedRef<SWidget>& InContentWidget)
	{
		CreateToolTipWidget();
		ToolTipWidget->SetContentWidget(InContentWidget);
	}

	void InvalidateWidget()
	{
		ToolTipWidget.Reset();
	}

	virtual bool IsEmpty() const { return false; }
	virtual bool IsInteractive() const { return false; }
	virtual void OnOpening() {}
	virtual void OnClosed() {}

private:
	void CreateToolTipWidget()
	{
		if (!ToolTipWidget.IsValid())
		{
			ToolTipWidget = SNetStatsCountersViewTooltip::GetRowTooltip(TreeNodePtr);
		}
	}

private:
	TSharedPtr<SToolTip> ToolTipWidget;
	const TSharedPtr<FNetStatsCounterNode> TreeNodePtr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
