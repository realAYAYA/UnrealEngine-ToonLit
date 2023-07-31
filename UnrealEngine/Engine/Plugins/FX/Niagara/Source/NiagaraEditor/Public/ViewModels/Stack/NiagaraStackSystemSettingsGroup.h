// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "NiagaraCommon.h"
#include "NiagaraParameterStore.h"
#include "ViewModels/Stack/NiagaraStackParameterStoreEntry.h"
#include "Delegates/IDelegateInstance.h"
#include "NiagaraStackSystemSettingsGroup.generated.h"

class FNiagaraScriptViewModel;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSystemPropertiesGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()
		
public:
	void Initialize(FRequiredEntryData InRequiredEntryData);

	virtual EIconMode GetSupportedIconMode() const override { return UNiagaraStackEntry::EIconMode::Brush; }
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool GetCanExpandInOverview() const override { return false; }
	virtual bool GetShouldShowInStack() const override { return false; }

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSystemUserParametersGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, UObject* InOwner, FNiagaraParameterStore* InParameterStore);

	virtual EIconMode GetSupportedIconMode() const override { return UNiagaraStackEntry::EIconMode::Text; }
	virtual FText GetIconText() const override;
	virtual bool GetCanExpandInOverview() const override { return false; }
	virtual bool GetShouldShowInStack() const override { return false; }
private:
	TWeakObjectPtr<UObject> Owner;
	FDelegateHandle ParameterStoreChangedHandle;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackParameterStoreItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, UObject* InOwner, FNiagaraParameterStore* InParameterStore, INiagaraStackItemGroupAddUtilities* InGroupAddUtilities);

	virtual FText GetDisplayName() const override;

	virtual FText GetTooltipText() const override;

	virtual bool GetShouldShowInOverview() const override { return false; }

	INiagaraStackItemGroupAddUtilities* GetGroupAddUtilities() const { return GroupAddUtilities; }

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	virtual void FinalizeInternal() override;

private:
	void ParameterStoreChanged();

private:
	TWeakObjectPtr<UObject> Owner;
	FNiagaraParameterStore* ParameterStore;
	INiagaraStackItemGroupAddUtilities* GroupAddUtilities;
	FDelegateHandle ParameterStoreChangedHandle;
};