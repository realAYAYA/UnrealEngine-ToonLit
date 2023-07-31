// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Details/ViewModels/WebAPIModelViewModel.h"
#include "SWebAPISchemaTreeTableRow.h"
#include "SWebAPISchemaTreeTableRow.inl"

class SWebAPISchemaPropertyRow
	: public SWebAPISchemaTreeTableRow<FWebAPIPropertyViewModel>
{
public:
	SLATE_BEGIN_ARGS(SWebAPISchemaPropertyRow)
	{ }
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FWebAPIPropertyViewModel>& InViewModel, const TSharedRef<STableViewBase>& InOwnerTableView);
};
