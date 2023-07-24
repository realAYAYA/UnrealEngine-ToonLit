// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "ISceneOutliner.h"
#include "SceneOutlinerPublicTypes.h"
#include "ISceneOutlinerColumn.h"

template<typename ItemType> class STableRow;
class SUnsavedActorWidget;

/** A column for the SceneOutliner that displays if the actor is unsaved */
class FSceneOutlinerActorUnsavedColumn : public ISceneOutlinerColumn
{
public:
	FSceneOutlinerActorUnsavedColumn(ISceneOutliner& SceneOutliner) : WeakOutliner(StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared())) {}
	virtual ~FSceneOutlinerActorUnsavedColumn() = default;

	static FName GetID() { return FSceneOutlinerBuiltInColumnTypes::Unsaved(); }

	//////////////////////////////////////////////////////////////////////////
	// Begin ISceneOutlinerColumn Implementation
	virtual FName GetColumnID() override;
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override;
	virtual bool SupportsSorting() const override { return true; }
	virtual void SortItems(TArray<FSceneOutlinerTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const override;
	// End ISceneOutlinerColumn Implementation
	//////////////////////////////////////////////////////////////////////////

private:
	TWeakPtr<ISceneOutliner> WeakOutliner;

	// Map of asset path -> widgets for each asset we encounter
	TMap<FString, TSharedRef<SUnsavedActorWidget>> UnsavedActorWidgets;
	
};
