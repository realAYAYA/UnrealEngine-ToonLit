// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "NiagaraCommon.h"
#include "AssetRegistry/AssetData.h"
#include "NiagaraStackScriptItemGroup.generated.h"

class FNiagaraScriptViewModel;
class UNiagaraNodeOutput;
class UNiagaraNodeFunctionCall;
class FScriptItemGroupAddUtilities;
class UEdGraph;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackScriptItemGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		FText InDisplayName,
		FText InToolTip,
		TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
		ENiagaraScriptUsage InScriptUsage,
		FGuid InScriptUsageId = FGuid());

	ENiagaraScriptUsage GetScriptUsage() const { return ScriptUsage; }
	FGuid GetScriptUsageId() const { return ScriptUsageId; }
	UNiagaraNodeOutput* GetScriptOutputNode() const;

	virtual UObject* GetDisplayedObject() const override;

	virtual bool SupportsPaste() const override { return true; }
	virtual bool TestCanPasteWithMessage(const UNiagaraClipboardContent* ClipboardContent, FText& OutMessage) const override;
	virtual FText GetPasteTransactionText(const UNiagaraClipboardContent* ClipboardContent) const override;
	virtual void Paste(const UNiagaraClipboardContent* ClipboardContent, FText& OutPasteWarning) override;

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual void FinalizeInternal() override;

	virtual TOptional<FDropRequestResponse> CanDropInternal(const FDropRequest& DropRequest) override;

	virtual TOptional<FDropRequestResponse> DropInternal(const FDropRequest& DropRequest) override;

	virtual TOptional<FDropRequestResponse> ChildRequestCanDropInternal(const UNiagaraStackEntry& TargetChild, const FDropRequest& DropRequest) override;

	virtual TOptional<FDropRequestResponse> ChildRequestDropInternal(const UNiagaraStackEntry& TargetChild, const FDropRequest& DropRequest) override;

private:
	void ItemAdded(UNiagaraNodeFunctionCall* AddedModule);

	void ChildModifiedGroupItems();

	bool ChildRequestCanPaste(const UNiagaraClipboardContent* ClipboardContent, FText& OutCanPasteMessage);

	void ChildRequestPaste(const UNiagaraClipboardContent* ClipboardContent, int32 PasteIndex, FText& OutPasteWarning);

	void ChildRequestDeprecatedRecommendation(class UNiagaraStackModuleItem* TargetChild);

	void OnScriptGraphChanged(const struct FEdGraphEditAction& InAction);

	void OnSystemScriptCompiled(UNiagaraScript* InScript, const FGuid& ScriptVersion);

	void OnParticleScriptCompiled(UNiagaraScript* InScript, const FGuid& ScriptVersion);

	TOptional<FDropRequestResponse> CanDropOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> CanDropEntriesOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> CanDropAssetsOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> CanDropScriptsOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> CanDropParameterOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> DropOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> DropEntriesOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> DropAssetsOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> DropScriptsOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

	TOptional<FDropRequestResponse> DropParameterOnTarget(const UNiagaraStackEntry& TargetEntry, const FDropRequest& DropRequest);

protected:
	TWeakPtr<FNiagaraScriptViewModel> ScriptViewModel;
	void RefreshIssues(TArray<FStackIssue>& NewIssues);

private:
	void PasteModules(const UNiagaraClipboardContent* ClipboardContent, int32 PasteIndex, FText& OutPasteWarning);

private:
	TSharedPtr<FScriptItemGroupAddUtilities> AddUtilities;

	ENiagaraScriptUsage ScriptUsage;

	FGuid ScriptUsageId;
	bool bIsValidForOutput;

	TWeakObjectPtr<UEdGraph> ScriptGraph;

	TWeakObjectPtr<UNiagaraSystem> OwningSystemWeak;

	TWeakObjectPtr<UNiagaraScript> OwningParticleScriptWeak;

	FDelegateHandle OnGraphChangedHandle;
};
