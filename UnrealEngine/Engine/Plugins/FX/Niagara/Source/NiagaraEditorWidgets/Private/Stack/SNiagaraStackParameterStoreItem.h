// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNiagaraStackItem.h"

class UNiagaraStackParameterStoreItem;
class UNiagaraStackViewModel;

class SNiagaraStackParameterStoreItem : public SNiagaraStackItem
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackParameterStoreItem) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraStackParameterStoreItem& InParameterStoreItem, UNiagaraStackViewModel* InStackViewModel);

protected:
	virtual void AddCustomRowWidgets(TSharedRef<SHorizontalBox> HorizontalBox) override;

private:
	UNiagaraStackParameterStoreItem* ParameterStoreItem;
};