// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Details/ViewModels/WebAPIOperationRequestViewModel.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"

class FWebAPIEnumViewModel;

class SWebAPISchemaOperationRequestRow
	: public SWebAPISchemaTreeTableRow<FWebAPIOperationRequestViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaOperationRequestRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIOperationRequestViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
