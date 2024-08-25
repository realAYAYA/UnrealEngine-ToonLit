// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "MaterialDomain.h"
#include "Widgets/SCompoundWidget.h"

class UEnum;

DECLARE_DELEGATE_OneParam(FDMOnDomainChanged, const EMaterialDomain)

class SDMDomain : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMDomain)
		: _SelectedItem(EMaterialDomain::MD_Surface)
		{}
		SLATE_ATTRIBUTE(EMaterialDomain, SelectedItem)
		SLATE_EVENT(FDMOnDomainChanged, OnSelectedItemChanged)
	SLATE_END_ARGS()

	virtual ~SDMDomain() {}

	void Construct(const FArguments& InArgs);

protected:
	TAttribute<EMaterialDomain> SelectedItem;
	FDMOnDomainChanged OnSelectedItemChanged;

	TObjectPtr<UEnum> DomainEnum;

	TSharedRef<SWidget> OnGenerateWidget(const FName InItem);

	void OnSelectionChanged(const FName InNewItem, const ESelectInfo::Type InSelectInfoType);

	FText GetSelectedItemText() const;

	FText GetDomainDisplayName(const FName InDomainName) const;
};
