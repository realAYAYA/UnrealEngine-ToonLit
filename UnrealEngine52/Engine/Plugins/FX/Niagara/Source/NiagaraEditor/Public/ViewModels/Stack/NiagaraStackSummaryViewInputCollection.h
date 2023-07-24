// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEmitter.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "PropertyEditorDelegates.h"
#include "NiagaraStackFunctionInputCollection.h"
#include "NiagaraStackSummaryViewInputCollection.generated.h"

class IPropertyRowGenerator;
class UNiagaraNode;
class IDetailTreeNode;
class UNiagaraStackFunctionInputCollection;
class UNiagaraNodeFunctionCall;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackSummaryViewObject : public UNiagaraStackFunctionInputCollectionBase
{
	GENERATED_BODY()
		
public:
	DECLARE_DELEGATE_TwoParams(FOnSelectRootNodes, TArray<TSharedRef<IDetailTreeNode>>, TArray<TSharedRef<IDetailTreeNode>>*);

public:
	UNiagaraStackSummaryViewObject();

	void Initialize(FRequiredEntryData InRequiredEntryData, FVersionedNiagaraEmitterWeakPtr InEmitter, FString InOwningStackItemEditorDataKey);
	virtual void FinalizeInternal() override;

	virtual FText GetDisplayName() const override;
	virtual bool GetIsEnabled() const;

protected:

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	void AppendEmitterCategory(FFunctionCallNodesState& State, TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, TArray<FStackIssue>& NewIssues);

	virtual void PostRefreshChildrenInternal() override;

	virtual void GetSectionsInternal(TArray<FNiagaraStackSection>& OutStackSections) const override;

private:

	void OnViewStateChanged();
private:

	FVersionedNiagaraEmitterWeakPtr Emitter;
	TMap<FGuid, UNiagaraStackFunctionInputCollection*> KnownInputCollections;
};
