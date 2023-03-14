// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorMode.h"

class SCENEOUTLINER_API FActorFolderPickingMode : public FActorMode
{
public:
	FActorFolderPickingMode(SSceneOutliner* InSceneOutliner, FOnSceneOutlinerItemPicked InOnItemPicked, TWeakObjectPtr<UWorld> InSpecifiedWorldToDisplay = nullptr, const FFolder::FRootObject& InRootObject = FFolder::GetInvalidRootObject());
	virtual ~FActorFolderPickingMode() {}

	/* Begin ISceneOutlinerMode Implementation */
	virtual void OnFilterTextCommited(FSceneOutlinerItemSelection& Selection, ETextCommit::Type CommitType) override;
	virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;
	virtual void CreateViewContent(FMenuBuilder& MenuBuilder) override;
	virtual bool ShowViewButton() const override { return true; }
	virtual bool ShouldShowFolders() const { return true; }
protected:
	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;
	/* End ISceneOutlinerMode Implementation */

	/** Delegate to call when an item is picked */
	FOnSceneOutlinerItemPicked OnItemPicked;

	FFolder::FRootObject RootObject;
};