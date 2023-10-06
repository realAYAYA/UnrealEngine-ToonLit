// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "NiagaraStackSection.h"
#include "NiagaraStackFunctionInputCollection.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraStackFunctionInput;
class UNiagaraClipboardFunctionInput;
class UEdGraphPin;

/** A base class for value collections. Values can be all kinds of input data, such as module inputs, object properties etc.
 * This has base functionality for sections.
 */
UCLASS(MinimalAPI)
class UNiagaraStackValueCollection : public UNiagaraStackItemContent
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API void Initialize(FRequiredEntryData InRequiredEntryData, FString InOwningStackItemEditorDataKey, FString InStackEditorDataKey);
	
	bool GetShouldDisplayLabel() const { return bShouldDisplayLabel; }
	NIAGARAEDITOR_API void SetShouldDisplayLabel(bool bInShouldShowLabel);

	NIAGARAEDITOR_API const TArray<FText>& GetSections() const;

	NIAGARAEDITOR_API FText GetActiveSection() const;

	NIAGARAEDITOR_API void SetActiveSection(FText InActiveSection);

	NIAGARAEDITOR_API FText GetTooltipForSection(FString Section) const;

	void CacheLastActiveSection();
public:
	static NIAGARAEDITOR_API FText UncategorizedName;

	static NIAGARAEDITOR_API FText AllSectionName;
protected:
	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;
	virtual void GetSectionsInternal(TArray<FNiagaraStackSection>& OutStackSections) const { }
	NIAGARAEDITOR_API void UpdateCachedSectionData() const;
	NIAGARAEDITOR_API virtual bool FilterByActiveSection(const UNiagaraStackEntry& Child) const;
private:
	NIAGARAEDITOR_API virtual bool GetCanExpand() const override;
	NIAGARAEDITOR_API virtual bool GetShouldShowInStack() const override;
	NIAGARAEDITOR_API virtual int32 GetChildIndentLevel() const override;
private:
	mutable TOptional<TArray<FText>> SectionsCache;
	mutable TOptional<TMap<FString, TArray<FText>>> SectionToCategoryMapCache;
	mutable TOptional<TMap<FString, FText>> SectionToTooltipMapCache;
	mutable TOptional<FText> ActiveSectionCache;
	FText LastActiveSection;

	bool bShouldDisplayLabel;
};

UCLASS(MinimalAPI)
class UNiagaraStackFunctionInputCollection : public UNiagaraStackValueCollection
{
	GENERATED_BODY()

public:
	NIAGARAEDITOR_API UNiagaraStackFunctionInputCollection();

	NIAGARAEDITOR_API UNiagaraNodeFunctionCall* GetModuleNode() const;

	NIAGARAEDITOR_API UNiagaraNodeFunctionCall* GetInputFunctionCallNode() const;

	NIAGARAEDITOR_API void Initialize(
		FRequiredEntryData InRequiredEntryData,
		UNiagaraNodeFunctionCall& InModuleNode,
		UNiagaraNodeFunctionCall& InInputFunctionCallNode,
		FString InOwnerStackItemEditorDataKey);

	//~ UNiagaraStackEntry interface
	NIAGARAEDITOR_API virtual FText GetDisplayName() const override;
	NIAGARAEDITOR_API virtual bool GetIsEnabled() const;

	NIAGARAEDITOR_API void ToClipboardFunctionInputs(UObject* InOuter, TArray<const UNiagaraClipboardFunctionInput*>& OutClipboardFunctionInputs) const;

	NIAGARAEDITOR_API void SetValuesFromClipboardFunctionInputs(const TArray<const UNiagaraClipboardFunctionInput*>& ClipboardFunctionInputs);

	NIAGARAEDITOR_API void GetChildInputs(TArray<UNiagaraStackFunctionInput*>& OutResult) const;

	NIAGARAEDITOR_API void GetFilteredChildInputs(TArray<UNiagaraStackFunctionInput*>& OutFilteredChildInputs) const;

	NIAGARAEDITOR_API void GetCustomFilteredChildInputs(TArray<UNiagaraStackFunctionInput*>& OutResult, const TArray<FOnFilterChild>& CustomFilters) const;

	NIAGARAEDITOR_API TArray<UNiagaraStackFunctionInput*> GetInlineParameterInputs() const;

private:

	struct FInputData
	{
		FNiagaraVariable InputVariable;
		int32 SortKey;
		TOptional<FText> DisplayName;
		FText Category;
		bool bIsStatic;
		bool bIsHidden;

		UNiagaraNodeFunctionCall* ModuleNode;
		UNiagaraNodeFunctionCall* InputFunctionCallNode;
		
		TArray<FInputData*> Children;
		bool bIsChild = false;
	};

	struct FNiagaraParentData
	{
		FNiagaraVariable ParentVariable;
		TArray<int32> ChildIndices;
	};

	struct FFunctionCallNodesState
	{		
		TArray<FInputData> InputDataCollection;
		TMap<FName, FNiagaraParentData> ParentMapping;
	};
	
	void OnScriptApplied(UNiagaraScript* NiagaraScript, FGuid Guid);
	
	NIAGARAEDITOR_API virtual void FinalizeInternal() override;

	NIAGARAEDITOR_API virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	NIAGARAEDITOR_API virtual void GetSectionsInternal(TArray<FNiagaraStackSection>& OutStackSections) const override;
	
	void RefreshChildrenForFunctionCall(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues);
	
	void AppendInputsForFunctionCall(FFunctionCallNodesState& State, TArray<FStackIssue>& NewIssues);
	
	void ApplyAllFunctionInputsToChildren(FFunctionCallNodesState& State, const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues);

	void RefreshIssues(const TArray<FName>& DuplicateInputNames, const TArray<FName>& ValidAliasedInputNames, const TArray<FNiagaraVariable>& InputsWithInvalidTypes, const TMap<FName, UEdGraphPin*>& StaticSwitchInputs, TArray<FStackIssue>& NewIssues);

	void OnFunctionInputsChanged();

	FStackIssueFix GetNodeRemovalFix(UEdGraphPin* PinToRemove, FText FixDescription);

	FStackIssueFix GetResetPinFix(UEdGraphPin* PinToReset, FText FixDescription);

	FStackIssueFix GetUpgradeVersionFix(FText FixDescription);

	void AddInvalidChildStackIssue(FName PinName, TArray<FStackIssue>& OutIssues);
	
	void AddInputToCategory(const FInputData& InputData, const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren);
	
private:
	UNiagaraNodeFunctionCall* ModuleNode;
	UNiagaraNodeFunctionCall* InputFunctionCallNode;

	/** If this is set to true, no children will be resued when RefreshChildren is called */
	bool bForceCompleteRebuild = false;
};
