// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FStatsViewColumns
{
	// Column identifiers
	static const FName NameColumnID;
	static const FName MetaGroupNameColumnID;
	static const FName TypeColumnID;
	static const FName DataTypeColumnID;
	static const FName CountColumnID;
	static const FName SumColumnID;
	static const FName MaxColumnID;
	static const FName UpperQuartileColumnID;
	static const FName AverageColumnID;
	static const FName MedianColumnID;
	static const FName LowerQuartileColumnID;
	static const FName MinColumnID;
	static const FName DiffColumnID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{
	class FTableColumn;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FStatsViewColumnFactory
{
public:
	static void CreateStatsViewColumns(TArray<TSharedRef<Insights::FTableColumn>>& Columns);

	static TSharedRef<Insights::FTableColumn> CreateNameColumn();
	static TSharedRef<Insights::FTableColumn> CreateMetaGroupNameColumn();
	static TSharedRef<Insights::FTableColumn> CreateTypeColumn();
	static TSharedRef<Insights::FTableColumn> CreateDataTypeColumn();
	static TSharedRef<Insights::FTableColumn> CreateCountColumn();
	static TSharedRef<Insights::FTableColumn> CreateSumColumn();
	static TSharedRef<Insights::FTableColumn> CreateMaxColumn();
	static TSharedRef<Insights::FTableColumn> CreateUpperQuartileColumn();
	static TSharedRef<Insights::FTableColumn> CreateAverageColumn();
	static TSharedRef<Insights::FTableColumn> CreateMedianColumn();
	static TSharedRef<Insights::FTableColumn> CreateLowerQuartileColumn();
	static TSharedRef<Insights::FTableColumn> CreateMinColumn();
	static TSharedRef<Insights::FTableColumn> CreateDiffColumn();

private:
	static constexpr float AggregatedStatsColumnInitialWidth = 80.0f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
