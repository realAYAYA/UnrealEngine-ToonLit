// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "ISceneOutliner.h"
#include "SceneOutlinerPublicTypes.h"
#include "ISceneOutlinerColumn.h"
#include "SSourceControlWidget.h"

template<typename ItemType> class STableRow;

/** A column for the SceneOutliner that displays the SCC Information */
class FSceneOutlinerActorSCCColumn : public ISceneOutlinerColumn
{

public:
	FSceneOutlinerActorSCCColumn(ISceneOutliner& SceneOutliner) : WeakSceneOutliner(StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared())) {}

	virtual ~FSceneOutlinerActorSCCColumn() {}

	static FName GetID() { return FSceneOutlinerBuiltInColumnTypes::SourceControl(); }

	//////////////////////////////////////////////////////////////////////////
	// Begin ISceneOutlinerColumn Implementation

	virtual FName GetColumnID() override;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;

	virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override;

	virtual bool SupportsSorting() const override { return false; }

	// End ISceneOutlinerColumn Implementation
	//////////////////////////////////////////////////////////////////////////
private:
	const FSlateBrush* GetHeaderIconBadge() const;

	TWeakPtr<ISceneOutliner> WeakSceneOutliner;
};
