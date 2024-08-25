// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Item/AvaOutlinerItemProxy.h"
#include "Textures/SlateIcon.h"

class AActor;
class UActorModifierCoreBase;
class UActorModifierCoreStack;

/** Creates Modifier Items based on all the Modifiers found in the Root Stack of an Actor */
class AVALANCHEMODIFIERSEDITOR_API FAvaOutlinerModifierProxy : public FAvaOutlinerItemProxy
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaOutlinerModifierProxy, FAvaOutlinerItemProxy);

	FAvaOutlinerModifierProxy(IAvaOutliner& InOutliner, const FAvaOutlinerItemPtr& InParentItem);

	/** Gets the Modifier Stack to use (e.g. for an Actor it would be the Root Modifier Stack) */
	UActorModifierCoreStack* GetModifierStack() const;
	AActor* GetActor() const;

	//~ Begin IAvaOutlinerItem
	virtual void OnItemRegistered() override;
	virtual void OnItemUnregistered() override;
	virtual void Select(FAvaOutlinerScopedSelection& InSelection) const override;
	virtual FText GetDisplayName() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual FText GetIconTooltipText() const override;
	//~ End IAvaOutlinerItem

	//~ Begin FAvaOutlinerItemProxy
	virtual void GetProxiedItems(const TSharedRef<IAvaOutlinerItem>& InParent, TArray<FAvaOutlinerItemPtr>& OutChildren, bool bInRecursive) override;
	//~ End FAvaOutlinerItemProxy

protected:
	void BindDelegates();
	void UnbindDelegates();

	void OnModifierStackUpdated(UActorModifierCoreBase* ItemChanged);

	FSlateIcon ModifierIcon;
};
