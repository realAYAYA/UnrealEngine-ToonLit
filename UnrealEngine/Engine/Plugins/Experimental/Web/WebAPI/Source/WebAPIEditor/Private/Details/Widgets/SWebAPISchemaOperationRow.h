// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Details/ViewModels/WebAPIOperationViewModel.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"

class SWebAPISchemaOperationRow
	: public SWebAPISchemaTreeTableRow<FWebAPIOperationViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaOperationRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIOperationViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
