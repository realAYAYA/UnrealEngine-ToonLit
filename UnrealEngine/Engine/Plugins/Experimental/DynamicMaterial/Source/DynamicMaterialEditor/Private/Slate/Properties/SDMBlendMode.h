// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Widgets/SCompoundWidget.h"

class UEnum;

DECLARE_DELEGATE_OneParam(FDMOnBlendModeChanged, const EBlendMode)

class SDMBlendMode : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMBlendMode)
		: _SelectedItem(BLEND_Translucent)
		{}
		SLATE_ATTRIBUTE(EBlendMode, SelectedItem)
		SLATE_EVENT(FDMOnBlendModeChanged, OnSelectedItemChanged)
	SLATE_END_ARGS()

	virtual ~SDMBlendMode() {}

	void Construct(const FArguments& InArgs);

protected:
	TAttribute<EBlendMode> SelectedItem;
	FDMOnBlendModeChanged OnSelectedItemChanged;

	TObjectPtr<UEnum> BlendModeEnum;

	TSharedRef<SWidget> OnGenerateWidget(const FName InItem);

	void OnSelectionChanged(const FName InNewItem, const ESelectInfo::Type InSelectInfoType);

	FText GetSelectedItemText() const;

	FText GetBlendModeDisplayName(const FName InBlendModeName) const;
};
