// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ISceneOutlinerTreeItem.h"
#include "UObject/ObjectKey.h"
#include "Components/SceneComponent.h"

/** A tree item that represents an Component in the world */
struct SCENEOUTLINER_API FComponentTreeItem : ISceneOutlinerTreeItem
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(bool, FFilterPredicate, const UActorComponent*);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FInteractivePredicate, const UActorComponent*);

	bool Filter(FFilterPredicate Pred) const
	{
		return Pred.Execute(Component.Get());
	}

	bool GetInteractiveState(FInteractivePredicate Pred) const
	{
		return Pred.Execute(Component.Get());
	}

	/** The Component this tree item is associated with. */
	mutable TWeakObjectPtr<UActorComponent> Component;

	/** Constant identifier for this tree item */
	const FObjectKey ID;

	/** Static type identifier for this tree item class */
	static const FSceneOutlinerTreeItemType Type;

	/** Construct this item from an Component */
	FComponentTreeItem(UActorComponent* InComponent);

	/* Begin ISceneOutlinerTreeItem Implementation */
	virtual bool IsValid() const override { return Component.IsValid(); }
	virtual FSceneOutlinerTreeItemID GetID() const override;
	virtual FString GetDisplayString() const override;
	virtual bool CanInteract() const override;
	virtual TSharedRef<SWidget> GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow) override;
	/* End ISceneOutlinerTreeItem Implementation */
public:
	/** true if this item exists in both the current world and PIE. */
	bool bExistsInCurrentWorldAndPIE;
	/** Cache the string displayed */
	FString CachedDisplayString;
};
