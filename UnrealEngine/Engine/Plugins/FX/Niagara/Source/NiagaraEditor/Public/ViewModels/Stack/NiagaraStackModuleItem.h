// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "NiagaraStackModuleItem.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackModuleItemLinkedInputCollection;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackModuleItemOutputCollection;
class UNiagaraScript;
class INiagaraStackItemGroupAddUtilities;
struct FAssetData;
class UNiagaraClipboardFunctionInput;
class INiagaraMessage;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackModuleItem : public UNiagaraStackItem
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnRequestDeprecationRecommended, UNiagaraStackModuleItem*);
	DECLARE_DELEGATE_OneParam(FOnNoteModeSet, bool);

	UNiagaraStackModuleItem();

	UNiagaraNodeFunctionCall& GetModuleNode() const;

	void Initialize(FRequiredEntryData InRequiredEntryData, INiagaraStackItemGroupAddUtilities* GroupAddUtilities, UNiagaraNodeFunctionCall& InFunctionCallNode);

	virtual FText GetDisplayName() const override;
	virtual UObject* GetDisplayedObject() const override;
	virtual FText GetTooltipText() const override;

	INiagaraStackItemGroupAddUtilities* GetGroupAddUtilities();

	bool CanMoveAndDelete() const;
	bool CanRefresh() const;
	void Refresh();

	virtual bool SupportsRename() const override { return true; }

	virtual bool SupportsChangeEnabled() const override { return true; }
	virtual bool GetIsEnabled() const override;

	int32 GetModuleIndex() const;
	
	UObject* GetExternalAsset() const override;

	virtual bool CanDrag() const override;

	/** Gets the output node of this module. */
	class UNiagaraNodeOutput* GetOutputNode() const;

	bool CanAddInput(FNiagaraVariable InputParameter) const;

	void AddInput(FNiagaraVariable InputParameter);

	/** Gets whether or not a module script reassignment is pending.  This can happen when trying to fix modules which are missing their scripts. */
	bool GetIsModuleScriptReassignmentPending() const;

	/** Gets whether or not a module script reassignment should be be pending. */
	void SetIsModuleScriptReassignmentPending(bool bIsPending);

	/** Reassigns the function script for the module without resetting the inputs. */
	void ReassignModuleScript(UNiagaraScript* ModuleScript);
	
	void ChangeScriptVersion(FGuid NewScriptVersion);

	void SetInputValuesFromClipboardFunctionInputs(const TArray<const UNiagaraClipboardFunctionInput*>& ClipboardFunctionInputs);

	void GetParameterInputs(TArray<class UNiagaraStackFunctionInput*>& OutResult) const;
	TArray<class UNiagaraStackFunctionInput*> GetInlineParameterInputs() const;

	virtual bool SupportsCut() const override { return true; }
	virtual bool TestCanCutWithMessage(FText& OutMessage) const override;
	virtual FText GetCutTransactionText() const override;
	virtual void CopyForCut(UNiagaraClipboardContent* ClipboardContent) const override;
	virtual void RemoveForCut() override;

	virtual bool SupportsCopy() const override { return true; }
	virtual bool TestCanCopyWithMessage(FText& OutMessage) const override;
	virtual void Copy(UNiagaraClipboardContent* ClipboardContent) const override;

	virtual bool SupportsPaste() const override { return true; }
	virtual bool TestCanPasteWithMessage(const UNiagaraClipboardContent* ClipboardContent, FText& OutMessage) const override;
	virtual FText GetPasteTransactionText(const UNiagaraClipboardContent* ClipboardContent) const override;
	virtual void Paste(const UNiagaraClipboardContent* ClipboardContent, FText& OutPasteWarning) override;

	virtual bool SupportsDelete() const override { return true; }
	virtual bool TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const override;
	virtual FText GetDeleteTransactionText() const override;
	virtual void Delete() override;

	virtual bool SupportsInheritance() const override { return true; }
	virtual bool GetIsInherited() const override;
	virtual FText GetInheritanceMessage() const override;

	bool IsScratchModule() const;

	void SetOnRequestDeprecationRecommended(FOnRequestDeprecationRecommended InOnRequest)
	{
		DeprecationDelegate = InOnRequest;
	}

	void SetEnabled(bool bEnabled)
	{
		SetIsEnabledInternal(bEnabled);
	}

	void SetNoteMode(bool bEnabled)
	{
		bIsNoteModeActive = bEnabled;
		OnNoteModeSet().ExecuteIfBound(bIsNoteModeActive);
	}

	bool GetNoteMode() const
	{
		return bIsNoteModeActive;
	}

	bool IsDebugDrawEnabled() const;
	void SetDebugDrawEnabled(bool bInEnabled);

	FOnNoteModeSet& OnNoteModeSet() { return OnNoteModeSetDelegate;}

	bool OpenSourceAsset() const;

protected:
	FOnRequestDeprecationRecommended DeprecationDelegate;

	virtual void FinalizeInternal() override;
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	virtual void SetIsEnabledInternal(bool bInIsEnabled) override;

	virtual TOptional<FDropRequestResponse> CanDropInternal(const FDropRequest& DropRequest) override;
	virtual TOptional<FDropRequestResponse> DropInternal(const FDropRequest& DropRequest) override;

	virtual const FCollectedUsageData& GetCollectedUsageData() const override;

private:
	bool FilterOutputCollection(const UNiagaraStackEntry& Child) const;
	bool FilterOutputCollectionChild(const UNiagaraStackEntry& Child) const;
	bool FilterLinkedInputCollection(const UNiagaraStackEntry& Child) const;
	bool FilterLinkedInputCollectionChild(const UNiagaraStackEntry& Child) const;
	void RefreshIssues(TArray<FStackIssue>& NewIssues);
	void RefreshIsEnabled();
	void OnMessageManagerRefresh(const TArray<TSharedRef<const INiagaraMessage>>& NewMessages);
	FStackIssueFixDelegate GetUpgradeVersionFix();

private:
	UNiagaraNodeOutput* OutputNode;
	UNiagaraNodeFunctionCall* FunctionCallNode;
	mutable TOptional<bool> bCanMoveAndDeleteCache;
	mutable TOptional<FText> DisplayNameCache;
	bool bIsEnabled;
	bool bCanRefresh;

	UPROPERTY()
	TObjectPtr<UNiagaraStackModuleItemLinkedInputCollection> LinkedInputCollection;

	UPROPERTY()
	TObjectPtr<UNiagaraStackFunctionInputCollection> InputCollection;

	UPROPERTY()
	TObjectPtr<UNiagaraStackModuleItemOutputCollection> OutputCollection;

	INiagaraStackItemGroupAddUtilities* GroupAddUtilities;

	mutable TOptional<bool> bIsScratchModuleCache;

	bool bIsNoteModeActive = false;
	
	bool bIsModuleScriptReassignmentPending;

	FGuid MessageManagerRegistrationKey;

	//** Issues created outside of the RefreshChildren call that will be committed the next time the UI state is refreshed. */
	TArray<FStackIssue> MessageManagerIssues;

	FGuid MessageLogGuid;
	
	FOnNoteModeSet OnNoteModeSetDelegate;
};
