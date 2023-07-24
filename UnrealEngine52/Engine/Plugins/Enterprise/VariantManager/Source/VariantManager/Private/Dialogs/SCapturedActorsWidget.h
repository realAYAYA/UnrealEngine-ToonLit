// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class ITableRow;
class STableViewBase;
struct FCapturableProperty;

class SCapturedActorsWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCapturedActorsWidget) {}
	SLATE_ARGUMENT(const TArray<UObject*>*, Actors)
	SLATE_END_ARGS()

	SCapturedActorsWidget()
	{
	}

	~SCapturedActorsWidget()
	{
	}

	void Construct(const FArguments& InArgs);
	TArray<UObject*> GetCurrentCheckedActors();

private:

	TSharedRef<ITableRow> MakeRow(UObject* Item, const TSharedRef<STableViewBase>& OwnerTable);

	TArray<UObject*> AllActors;
	TMap<UObject*, bool> ActorChecked;
};
