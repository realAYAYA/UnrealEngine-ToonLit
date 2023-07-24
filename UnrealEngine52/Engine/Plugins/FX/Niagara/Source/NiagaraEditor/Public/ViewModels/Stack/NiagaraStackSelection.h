// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackSelection.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSelection : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData);

	virtual bool GetCanExpand() const override;

	virtual bool GetShouldShowInStack() const override;

	void SetSelectedEntries(const TArray<UNiagaraStackEntry*>& InSelectedEntries);

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	TArray<TWeakObjectPtr<UNiagaraStackEntry>> SelectedEntries;
};