// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemTagTreeViewColumns
{
	//////////////////////////////////////////////////
	// Column identifiers

	static const FName NameColumnID;
	static const FName TypeColumnID;
	static const FName TrackerColumnID;
	static const FName InstanceCountColumnID;
	static const FName MinValueColumnID;
	static const FName MaxValueColumnID;
	static const FName AverageValueColumnID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Insights
{
	class FTableColumn;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FMemTagTreeViewColumnFactory
{
public:
	static void CreateMemTagTreeViewColumns(TArray<TSharedRef<Insights::FTableColumn>>& Columns);

	static TSharedRef<Insights::FTableColumn> CreateNameColumn();
	static TSharedRef<Insights::FTableColumn> CreateTypeColumn();
	static TSharedRef<Insights::FTableColumn> CreateTrackerColumn();
	static TSharedRef<Insights::FTableColumn> CreateInstanceCountColumn();
	static TSharedRef<Insights::FTableColumn> CreateMinValueColumn();
	static TSharedRef<Insights::FTableColumn> CreateMaxValueColumn();
	static TSharedRef<Insights::FTableColumn> CreateAverageValueColumn();

private:
	static constexpr float ValueColumnInitialWidth = 50.0f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
