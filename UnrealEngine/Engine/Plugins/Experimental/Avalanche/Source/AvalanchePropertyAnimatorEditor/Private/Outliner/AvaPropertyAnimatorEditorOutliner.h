// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Item/AvaOutlinerObject.h"
#include "Textures/SlateIcon.h"
#include "UObject/WeakObjectPtr.h"

class UPropertyAnimatorCoreBase;

/** Item representing a property animator item */
class FAvaPropertyAnimatorEditorOutliner : public FAvaOutlinerObject
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaPropertyAnimatorEditorOutliner, FAvaOutlinerObject);

	FAvaPropertyAnimatorEditorOutliner(IAvaOutliner& InOutliner, UPropertyAnimatorCoreBase* InObject);

	UPropertyAnimatorCoreBase* GetPropertyAnimator() const
	{
		return PropertyAnimator.Get();
	}

	//~ Begin IAvaOutlinerItem
	virtual void Select(FAvaOutlinerScopedSelection& InSelection) const override;
	virtual FText GetDisplayName() const override;
	virtual FText GetIconTooltipText() const override;
	virtual FSlateIcon GetIcon() const override;
	virtual bool ShowVisibility(EAvaOutlinerVisibilityType InVisibilityType) const override;
	virtual bool GetVisibility(EAvaOutlinerVisibilityType InVisibilityType) const override;
	virtual void OnVisibilityChanged(EAvaOutlinerVisibilityType InVisibilityType, bool bInNewVisibility) override;
	//~ End IAvaOutlinerItem

protected:
	//~ Begin FAvaOutlinerObjectItem
	virtual void SetObject_Impl(UObject* InObject) override;
	//~ End FAvaOutlinerObjectItem

	TWeakObjectPtr<UPropertyAnimatorCoreBase> PropertyAnimator;

	FText ItemName;
	FSlateIcon ItemIcon;
	FText ItemTooltip;
};