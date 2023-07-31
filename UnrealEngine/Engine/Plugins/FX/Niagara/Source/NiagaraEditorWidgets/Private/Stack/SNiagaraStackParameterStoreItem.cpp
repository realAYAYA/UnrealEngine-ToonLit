// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackParameterStoreItem.h"

#include "Stack/SNiagaraStackItemGroupAddButton.h"
#include "ViewModels/Stack/INiagaraStackItemGroupAddUtilities.h"
#include "ViewModels/Stack/NiagaraStackSystemSettingsGroup.h"

void SNiagaraStackParameterStoreItem::Construct(const FArguments& InArgs, UNiagaraStackParameterStoreItem& InParameterStoreItem, UNiagaraStackViewModel* InStackViewModel)
{
	ParameterStoreItem = &InParameterStoreItem;
	SNiagaraStackItem::Construct(SNiagaraStackItem::FArguments(), InParameterStoreItem, InStackViewModel);
}

void SNiagaraStackParameterStoreItem::AddCustomRowWidgets(TSharedRef<SHorizontalBox> HorizontalBox)
{
	INiagaraStackItemGroupAddUtilities* GroupAddUtilities = ParameterStoreItem->GetGroupAddUtilities();
	if(GroupAddUtilities != nullptr)
	{
		HorizontalBox->AddSlot()
			.AutoWidth()
			[
				SNew(SNiagaraStackItemGroupAddButton, ParameterStoreItem, GroupAddUtilities)
			];
	}
}