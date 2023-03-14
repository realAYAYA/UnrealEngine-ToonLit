// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"

class FWebAPIModelViewModel;

class SWebAPISchemaModelRow
	: public SWebAPISchemaTreeTableRow<FWebAPIModelViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaModelRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIModelViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
