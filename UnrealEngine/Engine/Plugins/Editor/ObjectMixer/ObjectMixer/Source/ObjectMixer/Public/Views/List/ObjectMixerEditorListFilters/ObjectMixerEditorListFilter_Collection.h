// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IObjectMixerEditorListFilter.h"

#define LOCTEXT_NAMESPACE "ObjectMixerEditor"

class OBJECTMIXEREDITOR_API FObjectMixerEditorListFilter_Collection : public IObjectMixerEditorListFilter
{
public:

	FObjectMixerEditorListFilter_Collection(const FName InCollectionName)
	{
		CollectionName = InCollectionName;
		SetFilterMatchType(EObjectMixerEditorListFilterMatchType::MatchAll);
	}

	virtual FString GetFilterName() const override
	{
		return "ObjectMixerCollectionListFilter_" + CollectionName.ToString();
	}

	virtual bool IsUserToggleable() const override
	{
		return false;
	}

	virtual FText GetFilterButtonLabel() const override
	{
		return LOCTEXT("ShowSourceTextFilterFormat", "Show Collections");
	}

	virtual FText GetFilterButtonToolTip() const override
	{
		return LOCTEXT("ShowSourceTextFilterTooltipFormat", "Show rows that are assigned to the selected collections");
	}

	virtual bool DoesItemPassFilter(const FObjectMixerEditorListRowPtr& InItem) const override
	{
		if (InItem.IsValid())
		{
			return InItem->IsObjectRefInCollection(CollectionName);
		}

		return false;
	}

	FName CollectionName;

};

#undef LOCTEXT_NAMESPACE
