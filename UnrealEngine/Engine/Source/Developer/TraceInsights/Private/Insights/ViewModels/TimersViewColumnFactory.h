// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Insights/Table/ViewModels/TableColumn.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FTimersViewColumns
{
	//////////////////////////////////////////////////
	// Column identifiers

	static const FName NameColumnID;
	static const FName MetaGroupNameColumnID;
	static const FName TypeColumnID;
	static const FName InstanceCountColumnID;
	static const FName ChildInstanceCountColumnID;

	// Inclusive Time columns
	static const FName TotalInclusiveTimeColumnID;
	static const FName MaxInclusiveTimeColumnID;
	static const FName UpperQuartileInclusiveTimeColumnID;
	static const FName AverageInclusiveTimeColumnID;
	static const FName MedianInclusiveTimeColumnID;
	static const FName LowerQuartileInclusiveTimeColumnID;
	static const FName MinInclusiveTimeColumnID;

	// Exclusive Time columns
	static const FName TotalExclusiveTimeColumnID;
	static const FName MaxExclusiveTimeColumnID;
	static const FName UpperQuartileExclusiveTimeColumnID;
	static const FName AverageExclusiveTimeColumnID;
	static const FName MedianExclusiveTimeColumnID;
	static const FName LowerQuartileExclusiveTimeColumnID;
	static const FName MinExclusiveTimeColumnID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimersTableColumn : public Insights::FTableColumn
{
public:
	FTimersTableColumn(const FName InId)
		: Insights::FTableColumn(InId)
	{}

	FText GetDescription(ETraceFrameType InAggreagationMode) const
	{
		switch (InAggreagationMode)
		{
		case TraceFrameType_Game:
			return GameFrame_Description;
			break;
		case TraceFrameType_Rendering:
			return RenderingFrame_Description;
			break;
		default:
			return FTableColumn::GetDescription();
		}
	}

	void SetDescription(ETraceFrameType InAggreagationMode, FText InDescription)
	{
		switch (InAggreagationMode)
		{
		case TraceFrameType_Game:
			GameFrame_Description = InDescription;
			break;
		case TraceFrameType_Rendering:
			RenderingFrame_Description = InDescription;
			break;
		case TraceFrameType_Count:
			Insights::FTableColumn::SetDescription(InDescription);
			break;
		default:
			ensure(0);
		}
	}

private:

	FText GameFrame_Description;
	FText RenderingFrame_Description;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FTimersViewColumnFactory
{
public:
	static void CreateTimersViewColumns(TArray<TSharedRef<Insights::FTableColumn>>& Columns);
	static void CreateTimerTreeViewColumns(TArray<TSharedRef<Insights::FTableColumn>>& Columns);

	static TSharedRef<Insights::FTableColumn> CreateNameColumn();
	static TSharedRef<Insights::FTableColumn> CreateMetaGroupNameColumn();
	static TSharedRef<Insights::FTableColumn> CreateTypeColumn();
	static TSharedRef<Insights::FTableColumn> CreateInstanceCountColumn();
	static TSharedRef<Insights::FTableColumn> CreateChildInstanceCountColumn();

	static TSharedRef<Insights::FTableColumn> CreateTotalInclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMaxInclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateAverageInclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMedianInclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMinInclusiveTimeColumn();

	static TSharedRef<Insights::FTableColumn> CreateTotalExclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMaxExclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateAverageExclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMedianExclusiveTimeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMinExclusiveTimeColumn();

private:
	static constexpr float TotalTimeColumnInitialWidth = 60.0f;
	static constexpr float TimeMsColumnInitialWidth = 50.0f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
