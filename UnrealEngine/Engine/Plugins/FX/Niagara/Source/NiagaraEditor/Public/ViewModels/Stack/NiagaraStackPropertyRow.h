// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraStackPropertyRow.generated.h"

class IDetailTreeNode;
class UNiagaraNode;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackPropertyRow : public UNiagaraStackItemContent
{
	GENERATED_BODY()
		
public:
	void Initialize(FRequiredEntryData InRequiredEntryData, TSharedRef<IDetailTreeNode> InDetailTreeNode, bool bInIsTopLevelProperty, FString InOwnerStackItemEditorDataKey, FString InOwnerStackEditorDataKey, UNiagaraNode* InOwningNiagaraNode);

	virtual EStackRowStyle GetStackRowStyle() const override;

	TSharedRef<IDetailTreeNode> GetDetailTreeNode() const;

	virtual bool GetIsEnabled() const override;

	virtual bool HasOverridenContent() const override;

	virtual bool IsExpandedByDefault() const override;

	virtual bool CanDrag() const override;

protected:
	virtual void FinalizeInternal() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual int32 GetChildIndentLevel() const override;

	virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const override;

	virtual TOptional<FDropRequestResponse> CanDropInternal(const FDropRequest& DropRequest) override;

	virtual TOptional<FDropRequestResponse> DropInternal(const FDropRequest& DropRequest) override;

private:
	TSharedPtr<IDetailTreeNode> DetailTreeNode;
	UNiagaraNode* OwningNiagaraNode;
	EStackRowStyle RowStyle;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSpacer> CategorySpacer;

	bool bCannotEditInThisContext;
	bool bIsTopLevelProperty;
};