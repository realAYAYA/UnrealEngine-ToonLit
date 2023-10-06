// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Details/Widgets/SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"
#include "Widgets/Views/ITableRow.h"

class FWebAPIServiceViewModel;

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
