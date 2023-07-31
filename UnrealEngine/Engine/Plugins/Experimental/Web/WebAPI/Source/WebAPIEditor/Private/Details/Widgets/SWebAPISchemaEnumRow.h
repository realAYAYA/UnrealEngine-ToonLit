// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Details/ViewModels/WebAPIEnumViewModel.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"

class SWebAPISchemaEnumRow
	: public SWebAPISchemaTreeTableRow<FWebAPIEnumViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaEnumRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIEnumViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
