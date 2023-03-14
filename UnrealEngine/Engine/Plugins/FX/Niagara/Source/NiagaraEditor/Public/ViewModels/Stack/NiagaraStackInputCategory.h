// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "NiagaraStackInputCategory.generated.h"

class UNiagaraStackFunctionInput;
class UNiagaraNodeFunctionCall;
class UNiagaraClipboardFunctionInput;

UENUM()
enum class EStackParameterBehavior
{
	Dynamic, Static
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackInputCategory : public UNiagaraStackItemContent
{
	GENERATED_BODY() 

public:
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		FString InputCategoryStackEditorDataKey,
		FText InCategoryName,
		bool bInIsTopLevelCategory,
		FString InOwnerStackItemEditorDataKey);
	
	const FText& GetCategoryName() const;

	void ResetInputs();

	void AddInput(UNiagaraNodeFunctionCall* InModuleNode, UNiagaraNodeFunctionCall* InInputFunctionCallNode, FName InInputParameterHandle, FNiagaraTypeDefinition InInputType, EStackParameterBehavior InParameterBehavior, TOptional<FText> InOptionalDisplayName, bool bIsHidden, bool bIsChildInput);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual bool GetShouldShowInStack() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual bool GetIsEnabled() const override;
	virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const override;

	void SetShouldShowInStack(bool bInShouldShowInStack);

	void ToClipboardFunctionInputs(UObject* InOuter, TArray<const UNiagaraClipboardFunctionInput*>& OutClipboardFunctionInputs) const;

	void SetStaticSwitchValuesFromClipboardFunctionInputs(const TArray<const UNiagaraClipboardFunctionInput*>& ClipboardFunctionInputs);

	void SetStandardValuesFromClipboardFunctionInputs(const TArray<const UNiagaraClipboardFunctionInput*>& ClipboardFunctionInputs);

protected:
	//~ UNiagaraStackEntry interface
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	virtual int32 GetChildIndentLevel() const override;

private:
	bool FilterForVisibleCondition(const UNiagaraStackEntry& Child) const;
	bool FilterForIsInlineEditConditionToggle(const UNiagaraStackEntry& Child) const;

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

	TOptional<FText> DisplayName;
	FText CategoryName;
	TArray<FInputParameterHandleAndType> Inputs;

	UPROPERTY()
	TObjectPtr<UNiagaraStackSpacer> CategorySpacer;

	bool bShouldShowInStack;
	bool bIsTopLevelCategory;
};