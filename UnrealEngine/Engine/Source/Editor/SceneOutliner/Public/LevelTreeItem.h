// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerTreeItem.h"

class ULevel;

/** A tree item that represents a level in the world */
struct SCENEOUTLINER_API FLevelTreeItem : ISceneOutlinerTreeItem
{
public:
	/** Static type identifier for this tree item class */
	static const FSceneOutlinerTreeItemType Type;

	DECLARE_DELEGATE_RetVal_OneParam(bool, FFilterPredicate, const ULevel*);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FInteractivePredicate, const ULevel*);

	bool Filter(FFilterPredicate Pred) const
	{
		return Pred.Execute(Level.Get());
	}

	bool GetInteractiveState(FInteractivePredicate Pred) const
	{
		return Pred.Execute(Level.Get());
	}

	/** The level this tree item is associated with. */
	mutable TWeakObjectPtr<ULevel> Level;

	/** Constant identifier for this tree item */
	const FObjectKey ID;

	/** Construct this item from a level */
	FLevelTreeItem(ULevel* InLevel);

	/* Begin ISceneOutlinerTreeItem Implementation */
	virtual bool IsValid() const override { return Level.IsValid(); }
	virtual FSceneOutlinerTreeItemID GetID() const override { return ID; }
	virtual FString GetDisplayString() const override;
	virtual bool CanInteract() const override;
	virtual TSharedRef<SWidget> GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow) override;
	virtual void GenerateContextMenu(UToolMenu* Menu, SSceneOutliner& Outliner) override;
	/* End ISceneOutlinerTreeItem Implementation */
};
