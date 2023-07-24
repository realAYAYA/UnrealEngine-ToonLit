// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorMode.h"

class SCENEOUTLINER_API FActorPickingMode : public FActorMode
{
public:
	FActorPickingMode(const FActorModeParams& Params, FOnSceneOutlinerItemPicked OnItemPickedDelegate);

	virtual ~FActorPickingMode() {};
public:
	virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;

	/** Allow the user to commit their selection by pressing enter if it is valid */
	virtual void OnFilterTextCommited(FSceneOutlinerItemSelection& Selection, ETextCommit::Type CommitType) override;

	virtual void CreateViewContent(FMenuBuilder& MenuBuilder) override;

	// Don't synchronize world selection
	virtual void SynchronizeSelection() override {};
public:
	virtual bool ShowViewButton() const override { return true; }
private:
	FOnSceneOutlinerItemPicked OnItemPicked;
};