// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Details/Widgets/SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"
#include "Widgets/Views/ITableRow.h"

class FWebAPIEnumViewModel;

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
