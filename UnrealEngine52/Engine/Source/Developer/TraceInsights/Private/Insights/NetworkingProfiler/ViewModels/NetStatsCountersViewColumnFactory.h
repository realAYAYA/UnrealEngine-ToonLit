// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FNetStatsCountersViewColumns
{
	//////////////////////////////////////////////////
	// Column identifiers

	static const FName NameColumnID;
	static const FName TypeColumnID;
	static const FName InstanceCountColumnID;

	static const FName SumColumnID;
	static const FName MaxCountColumnID;
	static const FName AverageCountColumnID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{
	class FTableColumn;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FNetStatsCountersViewColumnFactory
{
public:
	static void CreateNetStatsCountersViewColumns(TArray<TSharedRef<Insights::FTableColumn>>& Columns);

	static TSharedRef<Insights::FTableColumn> CreateNameColumn();
	static TSharedRef<Insights::FTableColumn> CreateTypeColumn();
	static TSharedRef<Insights::FTableColumn> CreateInstanceCountColumn();

	static TSharedRef<Insights::FTableColumn> CreateSumColumn();
	static TSharedRef<Insights::FTableColumn> CreateMaxCountColumn();
	static TSharedRef<Insights::FTableColumn> CreateAverageCountColumn();

private:
	static constexpr float SumColumnInitialWidth = 60.0f;
	static constexpr float CountColumnInitialWidth = 50.0f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
