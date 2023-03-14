// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailTreeNode.h"
#include "Stack/SNiagaraStackTableRow.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class FMenuBuilder;

class FNiagaraStackPropertyRowUtilities
{
public:
	static SNiagaraStackTableRow::FOnFillRowContextMenu CreateOnFillRowContextMenu(TSharedPtr<IPropertyHandle> PropertyHandle, const FNodeWidgetActions& GeneratedPropertyNodeWidgetActions);

private:
	static void OnFillPropertyRowContextMenu(FMenuBuilder& MenuBuilder, TSharedPtr<IPropertyHandle> PropertyHandle, FNodeWidgetActions PropertyNodeWidgetActions);
};