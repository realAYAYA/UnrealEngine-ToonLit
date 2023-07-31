// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Details/ViewModels/WebAPIServiceViewModel.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"

class SWebAPISchemaServiceRow
	: public SWebAPISchemaTreeTableRow<FWebAPIServiceViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaServiceRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIServiceViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
