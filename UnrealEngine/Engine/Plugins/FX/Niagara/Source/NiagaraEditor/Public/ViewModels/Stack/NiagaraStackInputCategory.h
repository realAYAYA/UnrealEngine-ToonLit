// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "ViewModels/HierarchyEditor/NiagaraHierarchyViewModelBase.h"
#include "ViewModels/HierarchyEditor/NiagaraSummaryViewViewModel.h"
#include "NiagaraStackInputCategory.generated.h"

class UNiagaraStackFunctionInput;
class UNiagaraNodeFunctionCall;
class UNiagaraClipboardFunctionInput;

UENUM()
enum class EStackParameterBehavior
{
	Dynamic, Static
};

UCLASS(MinimalAPI)
class UNiagaraStackCategory : public UNiagaraStackItemContent
{
	GENERATED_BODY()

public:
	NIAGARAEDITOR_API void Initialize(FRequiredEntryData InRequiredEntryData, FString InOwningStackItemEditorDataKey, FString InStackEditorDataKey);

	//~ UNiagaraStackEntry interface
	NIAGARAEDITOR_API virtual FText GetDisplayName() const override;
	NIAGARAEDITOR_API virtual bool GetShouldShowInStack() const override;
	NIAGARAEDITOR_API virtual EStackRowStyle GetStackRowStyle() const override;
	NIAGARAEDITOR_API virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const override;

	virtual bool IsTopLevelCategory() const { return false; }
	NIAGARAEDITOR_API virtual int32 GetChildIndentLevel() const override;

protected:
	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
private:
private:
	NIAGARAEDITOR_API bool FilterForVisibleCondition(const UNiagaraStackEntry& Child) const;
	NIAGARAEDITOR_API bool FilterForIsInlineEditConditionToggle(const UNiagaraStackEntry& Child) const;
protected:
	bool bShouldShowInStack;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSpacer> CategorySpacer;
};

UCLASS(MinimalAPI)
class UNiagaraStackInputCategory : public UNiagaraStackCategory
{
	GENERATED_BODY() 

public:
	NIAGARAEDITOR_API void Initialize(
		FRequiredEntryData InRequiredEntryData,
		FString InputCategoryStackEditorDataKey,
		FText InCategoryName,
		bool bInIsTopLevelCategory,
		FString InOwnerStackItemEditorDataKey);
	
	NIAGARAEDITOR_API bool GetIsEnabled() const;

	NIAGARAEDITOR_API virtual FText GetDisplayName() const override;

	NIAGARAEDITOR_API void ResetInputs();

	NIAGARAEDITOR_API void AddInput(UNiagaraNodeFunctionCall* InModuleNode, UNiagaraNodeFunctionCall* InInputFunctionCallNode, FName InInputParameterHandle, FNiagaraTypeDefinition InInputType, EStackParameterBehavior InParameterBehavior, TOptional<FText> InOptionalDisplayName, bool bIsHidden, bool bIsChildInput);

	NIAGARAEDITOR_API void SetShouldShowInStack(bool bInShouldShowInStack);

	NIAGARAEDITOR_API void ToClipboardFunctionInputs(UObject* InOuter, TArray<const UNiagaraClipboardFunctionInput*>& OutClipboardFunctionInputs) const;

	NIAGARAEDITOR_API bool TrySetStaticSwitchValuesFromClipboardFunctionInput(const UNiagaraClipboardFunctionInput& ClipboardFunctionInput);

	NIAGARAEDITOR_API void SetStandardValuesFromClipboardFunctionInputs(const TArray<const UNiagaraClipboardFunctionInput*>& ClipboardFunctionInputs);

	NIAGARAEDITOR_API void GetFilteredChildInputs(TArray<UNiagaraStackFunctionInput*>& OutFilteredChildInputs) const;

protected:
	//~ UNiagaraStackEntry interface
	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	virtual bool IsTopLevelCategory() const override { return bIsTopLevelCategory; }
private:
	struct FInputParameterHandleAndType
	{
		UNiagaraNodeFunctionCall* ModuleNode;
		UNiagaraNodeFunctionCall* InputFunctionCallNode;
		FName ParameterHandle;
		FNiagaraTypeDefinition Type;
		EStackParameterBehavior ParameterBehavior;
		TOptional<FText> DisplayName;
		bool bIsHidden;
		bool bIsChildInput;
	};

	FText CategoryName;
	TOptional<FText> DisplayName;
	bool bIsTopLevelCategory;

	TArray<FInputParameterHandleAndType> Inputs;
};

void AddSummaryItem(UNiagaraHierarchyItemBase* HierarchyItem, UNiagaraStackEntry* Parent);

UCLASS(MinimalAPI)
class UNiagaraStackSummaryCategory : public UNiagaraStackCategory
{
	GENERATED_BODY()

public:
	UNiagaraStackSummaryCategory() {}

	NIAGARAEDITOR_API void Initialize(
		FRequiredEntryData InRequiredEntryData,
		TSharedPtr<FNiagaraHierarchyCategoryViewModel> InCategory,
		FString InOwnerStackItemEditorDataKey);

	//~ UNiagaraStackEntry interface
	NIAGARAEDITOR_API virtual FText GetDisplayName() const override;
	virtual bool GetIsEnabled() const override { return true; }
	
	TWeakPtr<FNiagaraHierarchyCategoryViewModel> GetHierarchyCategory() const { return CategoryViewModelWeakPtr; }

protected:
	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	NIAGARAEDITOR_API virtual bool IsTopLevelCategory() const override;
	NIAGARAEDITOR_API virtual FText GetTooltipText() const override;
	NIAGARAEDITOR_API virtual int32 GetChildIndentLevel() const override;

private:
	TWeakPtr<FNiagaraHierarchyCategoryViewModel> CategoryViewModelWeakPtr;
};
