// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FolderTreeItem.h"
#include "ActorFolder.h"

struct SCENEOUTLINER_API FActorFolderTreeItem : public FFolderTreeItem
{
public:
	/** Static type identifier for this tree item class */
	static const FSceneOutlinerTreeItemType Type;

	FActorFolderTreeItem(const FFolder& InFolder, const TWeakObjectPtr<UWorld>& InWorld);

	/** The world which this folder belongs to */
	TWeakObjectPtr<UWorld> World;

	/* Begin ISceneOutlinerTreeItem Implementation */
	virtual bool IsValid() const override { return World.IsValid(); }
	virtual void OnExpansionChanged() override;
	virtual void Delete(const FFolder& InNewParentFolder) override;
	virtual bool CanInteract() const override;
	virtual TSharedRef<SWidget> GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow) override;
	virtual bool ShouldShowPinnedState() const override { return false; }
	virtual bool ShouldShowVisibilityState() const override;
	virtual bool ShouldRemoveOnceLastChildRemoved() const override;
	virtual FFolder GetFolder() const override;
	/* End FFolderTreeItem Implementation */
		
	/* Begin FFolderTreeItem Implementation */
	virtual void MoveTo(const FFolder& InNewParentFolder) override;
	virtual void SetPath(const FName& InNewPath) override;
	
	bool CanChangeChildrenPinnedState() const;
	const UActorFolder* GetActorFolder() const { return ActorFolder.Get(); }
private:
	virtual void CreateSubFolder(TWeakPtr<SSceneOutliner> WeakOutliner) override;
	/* End FFolderTreeItem Implementation */

	/** The actor folder object (can be invalid) */
	TWeakObjectPtr<UActorFolder> ActorFolder;
};
