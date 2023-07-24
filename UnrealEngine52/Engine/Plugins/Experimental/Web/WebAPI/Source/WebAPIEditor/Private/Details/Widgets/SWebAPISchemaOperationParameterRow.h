// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Details/Widgets/SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"
#include "Widgets/Views/ITableRow.h"

class FWebAPIOperationParameterViewModel;

class SWebAPISchemaOperationParameterRow
	: public SWebAPISchemaTreeTableRow<FWebAPIOperationParameterViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaOperationParameterRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIOperationParameterViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
