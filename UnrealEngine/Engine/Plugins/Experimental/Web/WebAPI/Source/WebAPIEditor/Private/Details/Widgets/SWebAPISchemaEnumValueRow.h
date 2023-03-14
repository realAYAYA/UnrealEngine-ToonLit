// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"
#include "Details/ViewModels/WebAPIEnumViewModel.h"

class SWebAPISchemaEnumValueRow
	: public SWebAPISchemaTreeTableRow<FWebAPIEnumValueViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaEnumValueRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIEnumValueViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
