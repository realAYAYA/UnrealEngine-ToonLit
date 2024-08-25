// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackObjectShared.h"

#include "NiagaraStackStatelessEmitterSpawnGroup.generated.h"

class FNiagaraStackItemPropertyHeaderValue;
struct FNiagaraStatelessSpawnInfo;
class UNiagaraStatelessEmitter;
class UNiagaraStackObject;

class FStructOnScope;

UCLASS(MinimalAPI)
class UNiagaraStackStatelessEmitterSpawnGroup : public UNiagaraStackItemGroup
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
	void OnSpawnInfoAdded(FGuid AddedItemId);
	void OnChildRequestDelete(FGuid DeleteItemId);

private:
	TWeakObjectPtr<UNiagaraStatelessEmitter> StatelessEmitterWeak;
	TSharedPtr<INiagaraStackItemGroupAddUtilities> AddUtilities;
};

UCLASS(MinimalAPI)
class UNiagaraStackStatelessEmitterSpawnItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnRequestDelete, FGuid /* AddedItemId */);

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStatelessEmitter* InStatelessEmitter, int32 InIndex);

	virtual FText GetDisplayName() const override;
	virtual FGuid GetSelectionId() const override;

	virtual bool SupportsDelete() const override { return true; }
	virtual bool TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const override;
	virtual FText GetDeleteTransactionText() const override;
	virtual void Delete() override;

	virtual bool SupportsHeaderValues() const override { return true; }
	virtual void GetHeaderValueHandlers(TArray<TSharedRef<INiagaraStackItemHeaderValueHandler>>& OutHeaderValueHandlers) const override;

	UNiagaraStatelessEmitter* GetStatelessEmitter() const { return StatelessEmitterWeak.Get(); }
	int32 GetIndex() const { return Index; }
	const FGuid& GetSourceId() const { return SourceId; }

	FOnRequestDelete& OnRequestDelete() { return OnRequestDeleteDelegate; }

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

private:
	FNiagaraStatelessSpawnInfo* GetSpawnInfo() const;

	static void FilterDetailNodes(const TArray<TSharedRef<IDetailTreeNode>>& InSourceNodes, TArray<TSharedRef<IDetailTreeNode>>& OutFilteredNodes);

	void OnHeaderValueChanged();
	void OnSpawnInfoModified(TArray<UObject*> Objects, ENiagaraDataObjectChange ChangeType);

private:
	TWeakObjectPtr<UNiagaraStatelessEmitter> StatelessEmitterWeak;
	int32 Index;
	FGuid SourceId;
	TSharedPtr<FStructOnScope> SpawnInfoStructOnScope;
	TWeakObjectPtr<UNiagaraStackObject> SpawnInfoObjectWeak;
	bool bGeneratedHeaderValueHandlers = false;
	TArray<TSharedRef<FNiagaraStackItemPropertyHeaderValue>> HeaderValueHandlers;
	FOnRequestDelete OnRequestDeleteDelegate;
};

