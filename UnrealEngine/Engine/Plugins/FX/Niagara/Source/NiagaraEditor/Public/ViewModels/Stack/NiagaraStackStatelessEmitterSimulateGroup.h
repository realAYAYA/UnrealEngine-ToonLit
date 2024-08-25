// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackObjectShared.h"

#include "NiagaraStackStatelessEmitterSimulateGroup.generated.h"

class FNiagaraStackItemPropertyHeaderValue;
class FNiagaraStatelessEmitterSimulateGroupAddUtilities;
class UNiagaraStatelessEmitter;
class UNiagaraStatelessModule;
class UNiagaraStackObject;

UCLASS(MinimalAPI)
class UNiagaraStackStatelessEmitterSimulateGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter);

	virtual EIconMode GetSupportedIconMode() const override { return EIconMode::Brush; }
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual bool GetShouldShowInStack() const override { return false; }

	UNiagaraStatelessEmitter* GetStatelessEmitter() const { return StatelessEmitterWeak.Get(); }

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	void ModuleAdded(UNiagaraStatelessModule* StatelessModule);
	void ModuleModifiedGroupItems();

private:
	TWeakObjectPtr<UNiagaraStatelessEmitter> StatelessEmitterWeak;
	TSharedPtr<FNiagaraStatelessEmitterSimulateGroupAddUtilities> AddUtilities;
};

UCLASS(MinimalAPI)
class UNiagaraStackStatelessModuleItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	static FString GenerateStackEditorDataKey(const UNiagaraStatelessModule* InStatelessModule);

	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessModule* InStatelessModule);

	virtual FText GetDisplayName() const override { return DisplayName; }

	virtual bool SupportsDelete() const override { return true; }
	virtual bool TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const override;
	virtual FText GetDeleteTransactionText() const override;
	virtual void Delete() override;

	virtual bool SupportsChangeEnabled() const override;
	virtual bool GetIsEnabled() const override;

	virtual bool SupportsHeaderValues() const override { return true; }
	virtual void GetHeaderValueHandlers(TArray<TSharedRef<INiagaraStackItemHeaderValueHandler>>& OutHeaderValueHandlers) const override;

	UNiagaraStatelessModule* GetStatelessModule() const { return StatelessModuleWeak.Get(); }

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual void SetIsEnabledInternal(bool bInIsEnabled) override;

private:
	static void FilterDetailNodes(const TArray<TSharedRef<IDetailTreeNode>>& InSourceNodes, TArray<TSharedRef<IDetailTreeNode>>& OutFilteredNodes);

	void OnHeaderValueChanged();

private:
	TWeakObjectPtr<UNiagaraStatelessModule> StatelessModuleWeak;
	FText DisplayName;
	TWeakObjectPtr<UNiagaraStackObject> ModuleObjectWeak;
	bool bGeneratedHeaderValueHandlers = false;
	TArray<TSharedRef<FNiagaraStackItemPropertyHeaderValue>> HeaderValueHandlers;
};

