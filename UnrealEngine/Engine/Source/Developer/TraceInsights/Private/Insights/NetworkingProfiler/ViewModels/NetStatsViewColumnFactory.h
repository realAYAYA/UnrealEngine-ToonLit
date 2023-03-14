// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FNetStatsViewColumns
{
	//////////////////////////////////////////////////
	// Column identifiers

	static const FName NameColumnID;
	static const FName TypeColumnID;
	static const FName LevelColumnID;
	static const FName InstanceCountColumnID;

	// Inclusive  columns
	static const FName TotalInclusiveSizeColumnID;
	static const FName MaxInclusiveSizeColumnID;
	static const FName AverageInclusiveSizeColumnID;

	// Exclusive  columns
	static const FName TotalExclusiveSizeColumnID;
	static const FName MaxExclusiveSizeColumnID;
	static const FName AverageExclusiveSizeColumnID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{
	class FTableColumn;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FNetStatsViewColumnFactory
{
public:
	static void CreateNetStatsViewColumns(TArray<TSharedRef<Insights::FTableColumn>>& Columns);

	static TSharedRef<Insights::FTableColumn> CreateNameColumn();
	static TSharedRef<Insights::FTableColumn> CreateTypeColumn();
	static TSharedRef<Insights::FTableColumn> CreateLevelColumn();
	static TSharedRef<Insights::FTableColumn> CreateInstanceCountColumn();

	static TSharedRef<Insights::FTableColumn> CreateTotalInclusiveSizeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMaxInclusiveSizeColumn();
	static TSharedRef<Insights::FTableColumn> CreateAverageInclusiveSizeColumn();

	static TSharedRef<Insights::FTableColumn> CreateTotalExclusiveSizeColumn();
	static TSharedRef<Insights::FTableColumn> CreateMaxExclusiveSizeColumn();
	static TSharedRef<Insights::FTableColumn> CreateAverageExclusiveSizeColumn();

private:
	static constexpr float TotalSizeColumnInitialWidth = 60.0f;
	static constexpr float SizeColumnInitialWidth = 50.0f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
