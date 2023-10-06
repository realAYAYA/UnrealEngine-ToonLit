// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Details/Widgets/SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"
#include "Widgets/Views/ITableRow.h"

class FWebAPIParameterViewModel;

class SWebAPISchemaParameterRow
	: public SWebAPISchemaTreeTableRow<FWebAPIParameterViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaParameterRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIParameterViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
