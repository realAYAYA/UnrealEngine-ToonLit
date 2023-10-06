// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Details/Widgets/SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"
#include "Widgets/Views/ITableRow.h"

class FWebAPIOperationRequestViewModel;

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
