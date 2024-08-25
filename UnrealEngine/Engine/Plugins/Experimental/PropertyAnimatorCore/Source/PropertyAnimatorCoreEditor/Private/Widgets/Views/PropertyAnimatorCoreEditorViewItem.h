// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Properties/PropertyAnimatorCoreData.h"

struct FPropertiesViewControllerItem
{
	/** The controller containing the property */
	TWeakObjectPtr<UPropertyAnimatorCoreBase> ControllerWeak;

	/** The optional property linked to the controller */
	TSharedPtr<FPropertyAnimatorCoreData> Property;

	friend uint32 GetTypeHash(const FPropertiesViewControllerItem& InItem)
	{
		return HashCombine(GetTypeHash(InItem.ControllerWeak.Get()), GetTypeHash(InItem.Property.Get()));
	}

	bool operator==(const FPropertiesViewControllerItem& InOther) const
	{
		return ControllerWeak == InOther.ControllerWeak
			&& Property == InOther.Property;
	}
};

using FPropertiesViewControllerItemPtr = TSharedPtr<FPropertiesViewControllerItem>;
using FPropertiesViewControllerItemPtrWeak = TWeakPtr<FPropertiesViewControllerItem>;

struct FPropertiesViewItem
{
	/** The property displayed on the row */
	FPropertyAnimatorCoreData Property;

	/** Controllers this property is linked to */
	TSet<TWeakObjectPtr<UPropertyAnimatorCoreBase>> ControllersWeak;

	/** Nested properties below this property */
	TArray<TSharedPtr<FPropertiesViewItem>> Children;

	/** Parent property of this one */
	TWeakPtr<FPropertiesViewItem> ParentWeak;
};

using FPropertiesViewItemPtr = TSharedPtr<FPropertiesViewItem>;
using FPropertiesViewItemPtrWeak = TWeakPtr<FPropertiesViewItem>;

struct FControllersViewItem
{
	FPropertiesViewControllerItem ControlledProperty;

	TArray<TSharedPtr<FControllersViewItem>> Children;

	TWeakPtr<FControllersViewItem> ParentWeak;
};

using FControllersViewItemPtr = TSharedPtr<FControllersViewItem>;
using FControllersViewItemPtrWeak = TWeakPtr<FControllersViewItem>;