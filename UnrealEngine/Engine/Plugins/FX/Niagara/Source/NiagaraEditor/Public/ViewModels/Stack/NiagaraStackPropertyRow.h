// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackObjectShared.h"
#include "NiagaraStackPropertyRow.generated.h"

class IDetailTreeNode;
class UNiagaraNode;

UCLASS(MinimalAPI)
class UNiagaraStackPropertyRow : public UNiagaraStackItemContent
{
	GENERATED_BODY()
		
public:
	NIAGARAEDITOR_API void Initialize(
		FRequiredEntryData InRequiredEntryData,
		TSharedRef<IDetailTreeNode> InDetailTreeNode,
		bool bInIsTopLevelProperty,
		bool bInHideTopLevelCategories,
		FString InOwnerStackItemEditorDataKey,
		FString InOwnerStackEditorDataKey,
		UNiagaraNode* InOwningNiagaraNode);
	
	NIAGARAEDITOR_API void SetOnFilterDetailNodes(FNiagaraStackObjectShared::FOnFilterDetailNodes InOnFilterDetailNodes) { OnFilterDetailNodes = InOnFilterDetailNodes; }

	NIAGARAEDITOR_API virtual EStackRowStyle GetStackRowStyle() const override;

	NIAGARAEDITOR_API virtual bool GetShouldShowInStack() const override;

	NIAGARAEDITOR_API TSharedRef<IDetailTreeNode> GetDetailTreeNode() const;

	NIAGARAEDITOR_API virtual bool GetIsEnabled() const override;

	NIAGARAEDITOR_API virtual bool HasOverridenContent() const override;

	NIAGARAEDITOR_API virtual bool IsExpandedByDefault() const override;

	virtual bool SupportsStackNotes() override { return true; }
	
	NIAGARAEDITOR_API virtual bool CanDrag() const override;

	void SetOwnerGuid(TOptional<FGuid> InGuid) { OwnerGuid = InGuid; }
protected:
	NIAGARAEDITOR_API virtual void FinalizeInternal() override;

	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	NIAGARAEDITOR_API virtual int32 GetChildIndentLevel() const override;

	NIAGARAEDITOR_API virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const override;

	NIAGARAEDITOR_API virtual TOptional<FDropRequestResponse> CanDropInternal(const FDropRequest& DropRequest) override;

	NIAGARAEDITOR_API virtual TOptional<FDropRequestResponse> DropInternal(const FDropRequest& DropRequest) override;

	NIAGARAEDITOR_API virtual bool SupportsSummaryView() const override;
	NIAGARAEDITOR_API virtual FNiagaraHierarchyIdentity DetermineSummaryIdentity() const override;
private:
	TSharedPtr<IDetailTreeNode> DetailTreeNode;
	UNiagaraNode* OwningNiagaraNode;
	EStackRowStyle RowStyle;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSpacer> CategorySpacer;

	/** An optional guid that can be used to identify the parent object. The parent object is responsible for setting this. It is used to create the summary view identity */
	TOptional<FGuid> OwnerGuid;

	bool bCannotEditInThisContext;
	bool bIsTopLevelProperty;
	bool bHideTopLevelCategories;
	bool bIsHiddenCategory;

	FNiagaraStackObjectShared::FOnFilterDetailNodes OnFilterDetailNodes;
};
