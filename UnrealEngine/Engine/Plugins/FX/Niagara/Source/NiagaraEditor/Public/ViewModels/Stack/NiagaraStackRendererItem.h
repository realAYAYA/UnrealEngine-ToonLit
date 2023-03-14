// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraRendererProperties.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "NiagaraStackRendererItem.generated.h"

struct FVersionedNiagaraEmitterData;
class UNiagaraEmitter;
class UNiagaraRendererProperties;
class UNiagaraStackObject;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackRendererItem : public UNiagaraStackItem
{
	GENERATED_BODY()
		
public:
	UNiagaraStackRendererItem();

	void Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraRendererProperties* InRendererProperties);

	UNiagaraRendererProperties* GetRendererProperties();

	virtual FText GetDisplayName() const override;

	virtual bool SupportsCut() const override { return true; }
	virtual bool TestCanCutWithMessage(FText& OutMessage) const override;
	virtual FText GetCutTransactionText() const override;
	virtual void CopyForCut(UNiagaraClipboardContent* ClipboardContent) const override;
	virtual void RemoveForCut() override;

	virtual bool SupportsCopy() const override { return true; }
	virtual bool TestCanCopyWithMessage(FText& OutMessage) const override;
	virtual void Copy(UNiagaraClipboardContent* ClipboardContent) const override;

	virtual bool SupportsPaste() const { return true; }
	virtual bool TestCanPasteWithMessage(const UNiagaraClipboardContent* ClipboardContent, FText& OutMessage) const override;
	virtual FText GetPasteTransactionText(const UNiagaraClipboardContent* ClipboardContent) const override;
	virtual void Paste(const UNiagaraClipboardContent* ClipboardContent, FText& OutPasteWarning) override;

	virtual bool SupportsDelete() const override { return true; }
	virtual bool TestCanDeleteWithMessage(FText& OutCanDeleteMessage) const;
	virtual FText GetDeleteTransactionText() const override;
	virtual void Delete();

	virtual bool SupportsInheritance() const override { return true; }
	virtual bool GetIsInherited() const override;
	virtual FText GetInheritanceMessage() const override;
	
	virtual bool SupportsRename() const override { return true; }

	bool HasBaseRenderer() const;

	virtual bool SupportsChangeEnabled() const override { return true; }
	virtual bool GetIsEnabled() const override;

	virtual EIconMode GetSupportedIconMode() const override { return EIconMode::Brush; }
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual bool SupportsResetToBase() const override { return true; }
	virtual bool TestCanResetToBaseWithMessage(FText& OutCanResetToBaseMessage) const override;
	virtual void ResetToBase() override;

	static TArray<FNiagaraVariable> GetMissingVariables(UNiagaraRendererProperties* RendererProperties, const FVersionedNiagaraEmitterData* EmitterData);
	static bool AddMissingVariable(const FVersionedNiagaraEmitterData* EmitterData, const FNiagaraVariable& Variable);

	bool IsExcludedFromScalability() const;
	bool IsOwningEmitterExcludedFromScalability() const;

	virtual const FCollectedUsageData& GetCollectedUsageData() const override;
protected:
	virtual void FinalizeInternal() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual void SetIsEnabledInternal(bool bInIsEnabled) override;

private:
	void RendererChanged();
	void RefreshIssues(TArray<FStackIssue>& NewIssues);
	void ProcessRendererIssues(const TArray<FNiagaraRendererFeedback>& InIssues, EStackIssueSeverity Severity, TArray<FStackIssue>& OutIssues);

private:
	TWeakObjectPtr<UNiagaraRendererProperties> RendererProperties;

	mutable TOptional<bool> bHasBaseRendererCache;

	mutable TOptional<bool> bCanResetToBaseCache;

	mutable TOptional<FText> DisplayNameCache;

	TArray<FNiagaraVariable> MissingAttributes;

	UPROPERTY()
	TObjectPtr<UNiagaraStackObject> RendererObject;
};
