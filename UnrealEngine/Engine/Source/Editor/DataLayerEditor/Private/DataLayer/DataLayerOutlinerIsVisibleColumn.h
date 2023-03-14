// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneOutlinerFwd.h"
#include "SceneOutlinerGutter.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class ISceneOutliner;
class SWidget;
template<typename ItemType> class STableRow;

class FDataLayerOutlinerIsVisibleColumn : public FSceneOutlinerGutter
{
public:
	FDataLayerOutlinerIsVisibleColumn(ISceneOutliner& SceneOutliner) : FSceneOutlinerGutter(SceneOutliner) {}
	virtual ~FDataLayerOutlinerIsVisibleColumn() {}
	static FName GetID();

	//////////////////////////////////////////////////////////////////////////
	// Begin ISceneOutlinerColumn Implementation
	virtual FName GetColumnID() override { return GetID(); }
	virtual const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override;
	virtual bool SupportsSorting() const override { return false; }
	// End ISceneOutlinerColumn Implementation
	//////////////////////////////////////////////////////////////////////////
};