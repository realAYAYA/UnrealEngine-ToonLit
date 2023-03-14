// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "PropertyEditorDelegates.h"
#include "NiagaraStackFunctionInputCollection.h"
#include "NiagaraStackFilteredObject.generated.h"

class IPropertyRowGenerator;
class UNiagaraNode;
class IDetailTreeNode;
class UNiagaraStackFunctionInputCollection;
class UNiagaraNodeFunctionCall;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackFilteredObject : public UNiagaraStackFunctionInputCollectionBase
{
	GENERATED_BODY()
		
public:
	UNiagaraStackFilteredObject();

	void Initialize(FRequiredEntryData InRequiredEntryData, FString InOwningStackItemEditorDataKey);
	void FinalizeInternal() override;

	virtual FText GetDisplayName() const override;
	virtual bool GetShouldShowInStack() const override;
	virtual bool GetIsEnabled() const override;
protected:

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	void AppendEmitterCategory(TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, TArray<UNiagaraStackEntry*>& NewChildren, const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<FStackIssue>& NewIssues);

	virtual void PostRefreshChildrenInternal() override;

private:
	void ProcessInputsForModule(TMap<FGuid, UNiagaraStackFunctionInputCollection*>& NewKnownInputCollections, TArray<UNiagaraStackEntry*>& NewChildren, UNiagaraNodeFunctionCall* ModuleNode);


	void OnViewStateChanged();
private:

	FVersionedNiagaraEmitter VersionedEmitter;

	TMap<FGuid, UNiagaraStackFunctionInputCollection*> KnownInputCollections;

};
