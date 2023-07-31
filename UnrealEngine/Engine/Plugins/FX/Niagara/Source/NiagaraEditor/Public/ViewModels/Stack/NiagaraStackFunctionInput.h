// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "NiagaraTypes.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCondition.h"
#include "NiagaraStackFunctionInput.generated.h"

class UNiagaraNodeFunctionCall;
class UNiagaraNodeCustomHlsl;
class UNiagaraNodeAssignment;
class UNiagaraNodeParameterMapSet;
class FStructOnScope;
class UNiagaraStackFunctionInputCollection;
class UNiagaraStackObject;
class UNiagaraScript;
class UEdGraphPin;
class UNiagaraDataInterface;
enum class EStackParameterBehavior;
class UNiagaraClipboardFunctionInput;
class UNiagaraClipboardFunction;
class UNiagaraScriptVariable;
class FNiagaraPlaceholderDataInterfaceHandle;

/** Represents a single module input in the module stack view model. */
UCLASS()
class NIAGARAEDITOR_API UNiagaraStackFunctionInput : public UNiagaraStackItemContent
{
	GENERATED_BODY()

public:
	/** Defines different modes which are used to provide the value for this function input. */
	enum class EValueMode
	{
		/** The value is set to a constant stored locally with this input. */
		Local,
		/** The value is linked to a parameter defined outside of this function. */
		Linked,
		/** The value is provided by a secondary dynamic input function. */
		Dynamic,
		/** The value is provided by a data interface object. */
		Data,
		/** The value is provided by an expression object. */
		Expression,
		/** The value is a default value provided by a function call. */
		DefaultFunction,
		/** This input is overridden in the stack graph, but the override is invalid. */
		InvalidOverride,
		/** This input has a default value set in it's graph which can't be displayed in the stack view. */
		UnsupportedDefault,
		/** This input has no value. */
		None
	};

	DECLARE_MULTICAST_DELEGATE(FOnValueChanged);

public:
	UNiagaraStackFunctionInput();

	/** 
	 * Sets the input data for this entry.
	 * @param InRequiredEntryData The required data for all stack entries.
	 * @param InStackEditorData The stack editor data for this input.
	 * @param InModuleNode The module function call which owns this input entry. NOTE: This input might not be an input to the module function call, it may be an input to a dynamic input function call which is owned by the module.
	 * @param InInputFunctionCallNode The function call which this entry is an input to. NOTE: This node can be a module function call node or a dynamic input node.
	 * @param InInputParameterHandle The input parameter handle for the function call.
	 * @param InInputType The type of this input.
	 * @param InParameterBehavior Determines how the parameter should behave in the stack
	 * @param InOwnerStackItemEditorDataKey The editor data key of the item that owns this input.
	 */
	void Initialize(
		FRequiredEntryData InRequiredEntryData,
		UNiagaraNodeFunctionCall& InModuleNode,
		UNiagaraNodeFunctionCall& InInputFunctionCallNode,
		FName InInputParameterHandle,
		FNiagaraTypeDefinition InInputType,
		EStackParameterBehavior InParameterBehavior,
		FString InOwnerStackItemEditorDataKey);

	/** Gets the function call node which owns this input. */
	const UNiagaraNodeFunctionCall& GetInputFunctionCallNode() const;

	/** Gets the script that the function call node was referencing when this input was initialized. */
	UNiagaraScript* GetInputFunctionCallInitialScript() const;

	/** Gets the current value mode */
	EValueMode GetValueMode() const;

	/** Gets the type of this input. */
	const FNiagaraTypeDefinition& GetInputType() const;

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual FText GetTooltipText() const override;
	virtual bool GetIsEnabled() const override;
	virtual UObject* GetExternalAsset() const override;
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
	virtual bool HasOverridenContent() const override;
	
	/** Gets the tooltip that should be shown for the value of this input. */
	FText GetValueToolTip() const;

	/** Gets the tooltip that should be shown for the value of this input. */
	FText GetCollapsedStateText() const;

	void SetSummaryViewDisiplayName(TOptional<FText> InDisplayName);

	/** Gets the path of parameter handles from the owning module to the function call which owns this input. */
	const TArray<FNiagaraParameterHandle>& GetInputParameterHandlePath() const;

	/** Gets the parameter handle which defined this input in the module. */
	const FNiagaraParameterHandle& GetInputParameterHandle() const;

	/** Gets the handle to the linked value for this input if there is one. */
	const FNiagaraParameterHandle& GetLinkedValueHandle() const;

	/** Sets the value of this input to a linked parameter handle. */
	void SetLinkedValueHandle(const FNiagaraParameterHandle& InParameterHandle);

	/** Gets the current set of available parameter handles which can be assigned to this input. Optionally returns possible conversion scripts. */
	void GetAvailableParameterHandles(TArray<FNiagaraParameterHandle>& AvailableParameterHandles, TMap<FNiagaraVariable, UNiagaraScript*>& AvailableConversionHandles, bool bIncludeConversionScripts = true) const;

	/** Gets the function node form the script graph if the current value mode is DefaultFunction. */
	UNiagaraNodeFunctionCall* GetDefaultFunctionNode() const;

	/** Gets the dynamic input node providing the value for this input, if one is available. */
	UNiagaraNodeFunctionCall* GetDynamicInputNode() const;

	/** Gets the dynamic inputs available for this input. */
	void GetAvailableDynamicInputs(TArray<UNiagaraScript*>& AvailableDynamicInputs, bool bIncludeNonLibraryInputs = false);

	/** Sets the dynamic input script for this input. */
	void SetDynamicInput(UNiagaraScript* DynamicInput, FString SuggestedName = FString(), const FGuid& InScriptVersion = FGuid());

	/** Gets the expression providing the value for this input, if one is available. */
	FText GetCustomExpressionText() const;

	/** Sets the dynamic custom expression script for this input. */
	void SetCustomExpression(const FString& InCustomExpression);

	/** Create a new scratch pad dynamic inputs and set this input to use it. */
	void SetScratch();

	/** Gets the current struct value of this input is there is one. */
	TSharedPtr<const FStructOnScope> GetLocalValueStruct();

	/** Gets the current data object value of this input is there is one. */
	UNiagaraDataInterface* GetDataValueObject();

	/** Called to notify the input that an ongoing change to it's value has begun. */
	void NotifyBeginLocalValueChange();

	/** Called to notify the input that an ongoing change to it's value has ended. */
	void NotifyEndLocalValueChange();

	/** Is this pin editable or should it show as disabled?*/
	bool IsEnabled() const;

	/** Sets this input's local value. */
	void SetLocalValue(TSharedRef<FStructOnScope> InLocalValue);
	
	/** Returns whether or not the value or handle of this input has been overridden and can be reset. */
	bool CanReset() const;

	/** Resets the value and handle of this input to the value and handle defined in the module. */
	void Reset();

	/** Determine if this field is editable */
	bool IsEditable() const;

	/** If true the parameter can only be set to local constant values */
	bool IsStaticParameter() const;

	/** Whether or not this input has a base value.  This is true for emitter instances in systems. */
	bool EmitterHasBase() const;

	/** Whether or not this input can be reset to a base value. */
	bool CanResetToBase() const;

	/** Resets this input to its base value. */
	void ResetToBase();

	/** Returns whether or not this input can be renamed. */
	virtual bool SupportsRename() const override;

	/** Renames this input to the name specified. */
	virtual void OnRenamed(FText NewName) override;

	/** Returns whether or not this input can be deleted. */
	bool CanDeleteInput() const;

	/** Deletes this input */
	void DeleteInput();

	/** Gets the namespaces which new parameters for this input can be read from. */
	void GetNamespacesForNewReadParameters(TArray<FName>& OutNamespacesForNewParameters) const;

	/** Gets the namespaces which new parameters for this input can write to. */
	void GetNamespacesForNewWriteParameters(TArray<FName>& OutNamespacesForNewParameters) const;

	/** Gets a multicast delegate which is called whenever the value on this input changes. */
	FOnValueChanged& OnValueChanged();

	/** Gets the variable that serves as an edit condition for this input. */
	TOptional<FNiagaraVariable> GetEditConditionVariable() const;
	
	/** Gets whether or not this input has an associated edit condition input. */
	bool GetHasEditCondition() const;

	/** Gets whether or not to show a control inline for the edit condition input associated with this input. */
	bool GetShowEditConditionInline() const;

	/** Gets the enabled value of the edit condition input associated with this input. */
	bool GetEditConditionEnabled() const;

	/** Sets the enabled value of the edit condition input associated with this input. */
	void SetEditConditionEnabled(bool bIsEnabled);

	/** Gets whether or not this input has an associated visible condition input. */
	bool GetHasVisibleCondition() const;

	/** Gets the enabled value of the visible condition input associated with this input. */
	bool GetVisibleConditionEnabled() const;

	/** Gets whether or not this input is used as an edit condition for another input and should be hidden. */
	bool GetIsInlineEditConditionToggle() const;

	/** Gets whether or not a dynamic input script reassignment is pending.  This can happen when trying to fix dynamic inputs which are missing their scripts. */
	bool GetIsDynamicInputScriptReassignmentPending() const;

	/** Gets whether or not a dynamic input script reassignment should be be pending. */
	void SetIsDynamicInputScriptReassignmentPending(bool bIsPending);

	/** Reassigns the function script for the current dynamic input without resetting the sub-inputs. */
	void ReassignDynamicInputScript(UNiagaraScript* DynamicInputScript);

	/** Gets whether or not this input is filtered from search results and appearing in stack due to visibility metadata*/
	bool GetShouldPassFilterForVisibleCondition() const;
	
	TArray<UNiagaraScript*> GetPossibleConversionScripts(const FNiagaraTypeDefinition& FromType) const;
	static TArray<UNiagaraScript*> GetPossibleConversionScripts(const FNiagaraTypeDefinition& FromType, const FNiagaraTypeDefinition& ToType);

	void SetLinkedInputViaConversionScript(const FName& LinkedInputName, const FNiagaraTypeDefinition& FromType);
	void SetLinkedInputViaConversionScript(const FNiagaraVariable& LinkedInput, UNiagaraScript* ConversionScript);
	void SetClipboardContentViaConversionScript(const UNiagaraClipboardFunctionInput& ClipboardFunctionInput);

	void ChangeScriptVersion(FGuid NewScriptVersion);

	const UNiagaraClipboardFunctionInput* ToClipboardFunctionInput(UObject* InOuter) const;

	void SetValueFromClipboardFunctionInput(const UNiagaraClipboardFunctionInput& ClipboardFunctionInput);

	bool IsScratchDynamicInput() const;

	bool ShouldDisplayInline() const;
	
	TArray<UNiagaraStackFunctionInput*> GetChildInputs() const;

	TOptional<FNiagaraVariableMetaData> GetInputMetaData() const;
	
	virtual bool IsSemanticChild() const override;
	void SetSemanticChild(bool IsSemanticChild);

	//~ UNiagaraStackEntry interface
	virtual void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const override;

	TOptional<FNiagaraVariableMetaData> GetMetadata() const;
	TOptional<FGuid> GetMetadataGuid() const;


	virtual const FCollectedUsageData& GetCollectedUsageData() const override;

	bool OpenSourceAsset() const;

	bool SupportsCustomExpressions() const;

protected:
	//~ UNiagaraStackEntry interface
	virtual void FinalizeInternal() override;
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	bool UpdateRapidIterationParametersForAffectedScripts(const uint8* Data);
	bool RemoveRapidIterationParametersForAffectedScripts(bool bUpdateGraphGuidsForAffected = false);
	FString ResolveDisplayNameArgument(const FString& InArg) const;
	FStackIssueFixDelegate GetUpgradeDynamicInputVersionFix();

private:
	struct FInputValues
	{
		FInputValues()
			: Mode(EValueMode::None)
		{
		}

		bool HasEditableData() const
		{
			return Mode != EValueMode::None && Mode != EValueMode::InvalidOverride && Mode != EValueMode::UnsupportedDefault;
		}

		EValueMode Mode;
		TSharedPtr<FStructOnScope> LocalStruct;
		FNiagaraParameterHandle LinkedHandle;
		TWeakObjectPtr<UNiagaraNodeFunctionCall> DynamicNode;
		TWeakObjectPtr<UNiagaraNodeCustomHlsl> ExpressionNode;
		TWeakObjectPtr<UNiagaraDataInterface> DataObject;
		TWeakObjectPtr<UNiagaraNodeFunctionCall> DefaultFunctionNode;
	};

private:
	/** Refreshes the current values for this input from the state of the graph. */
	void RefreshValues();

	/** Refreshes additional state for this input which comes from input metadata. */
	void RefreshFromMetaData(TArray<FStackIssue>& NewIssues);

	/** Called whenever the graph which generated this input changes. */
	void OnGraphChanged(const struct FEdGraphEditAction& InAction);

	/** Called whenever rapid iteration parameters are changed for the script that owns the function that owns this input. */
	void OnRapidIterationParametersChanged();

	/** Called whenever the script source that owns the function that owns this input changes. */
	void OnScriptSourceChanged();

	/** Gets the graph node which owns the local overrides for the module that owns this input if it exists. */
	UNiagaraNodeParameterMapSet* GetOverrideNode() const;

	/** Gets the graph node which owns the local overrides for the module that owns this input this input.  
	  * This will create the node and add it to the graph if it doesn't exist. */
	UNiagaraNodeParameterMapSet& GetOrCreateOverrideNode();

	/** Gets the pin on the override node which is associated with this input if it exists. */
	UEdGraphPin* GetOverridePin() const;

	/** Gets the pin on the override node which is associated with this input.  If either the override node or
	  * pin don't exist, they will be created. */
	UEdGraphPin& GetOrCreateOverridePin();

	void GetDefaultDataInterfaceValueFromDefaultPin(UEdGraphPin* DefaultPin, UNiagaraStackFunctionInput::FInputValues& InInputValues) const;

	void GetDefaultLocalValueFromDefaultPin(UEdGraphPin* DefaultPin, UNiagaraStackFunctionInput::FInputValues& InInputValues) const;

	void GetDefaultLinkedHandleOrLinkedFunctionFromDefaultPin(UEdGraphPin* DefaultPin, UNiagaraStackFunctionInput::FInputValues& InInputValues) const;

	void UpdateValuesFromScriptDefaults(FInputValues& InInputValues) const;

	void UpdateValuesFromOverridePin(const FInputValues& OldInputValues, FInputValues& NewInputValues, UEdGraphPin& InOverridePin) const;

	/** Removes all nodes connected to the override pin which provide it's value. */
	void RemoveNodesForOverridePin(UEdGraphPin& OverridePin);

	/** Remove the override pin and all nodes connected to it. */
	void RemoveOverridePin();

	/** Determine if the values in this input are possibly under the control of the rapid iteration array on the script.*/
	bool IsRapidIterationCandidate() const;

	FNiagaraVariable CreateRapidIterationVariable(const FName& InName);

	/** Handles the message manager refreshing messages. */
	void OnMessageManagerRefresh(const TArray<TSharedRef<const INiagaraMessage>>& NewMessages);

	void GetCurrentChangeIds(FGuid& OutOwningGraphChangeId, FGuid& OutFunctionGraphChangeId) const;

	UNiagaraScript* FindConversionScript(const FNiagaraTypeDefinition& FromType, TMap<FNiagaraTypeDefinition, UNiagaraScript*>& ConversionScriptCache, bool bIncludeConversionScripts) const;

private:
	/** The module function call which owns this input entry. NOTE: This input might not be an input to the module function
		call, it may be an input to a dynamic input function call which is owned by the module. */
	TWeakObjectPtr<UNiagaraNodeFunctionCall> OwningModuleNode;

	/** The function call which this entry is an input to. NOTE: This node can be a module function call node or a dynamic input node. */
	TWeakObjectPtr<UNiagaraNodeFunctionCall> OwningFunctionCallNode;

	/** The script which the owning function call is referencing. */
	TWeakObjectPtr<UNiagaraScript> OwningFunctionCallInitialScript;

	/** The assignment node which owns this input.  This is only valid for inputs of assignment modules. */
	TWeakObjectPtr<UNiagaraNodeAssignment> OwningAssignmentNode;

	/** The niagara type definition for this input. */
	FNiagaraTypeDefinition InputType;

	/** The meta data for this input, defined in the owning function's script. */
	TOptional<FNiagaraVariableMetaData> InputMetaData;

	// Stack issues generated when updating from metadata.
	TArray<FStackIssue> InputMetaDataIssues;

	/** A unique key for this input for looking up editor only UI data. */
	FString StackEditorDataKey;

	/** An array representing the path of Namespace.Name handles starting from the owning module to this function input. */
	TArray<FNiagaraParameterHandle> InputParameterHandlePath;

	/** The parameter handle which defined this input in the module graph. */
	FNiagaraParameterHandle InputParameterHandle;

	/** Defines the type of the stack parameter (e.g. 'static' if it should only allow constant compile-time values). */
	EStackParameterBehavior ParameterBehavior;

	/** The parameter handle which defined this input in the module graph, aliased for use in the current emitter
	  * graph.  This only affects parameter handles which are local module handles. */
	FNiagaraParameterHandle AliasedInputParameterHandle;

	/** The a rapid iteration variable that could potentially drive this entry..*/
	FNiagaraVariable RapidIterationParameter;

	/** The name of this input for display in the UI. */
	FText DisplayName;

	/** Optional override for the display name*/
	TOptional<FText> DisplayNameOverride;
	TOptional<FText> SummaryViewDisplayNameOverride;

	/** The default value for this input defined in the defining script. */
	FInputValues DefaultInputValues;

	/** Pointers and handles to the various values this input can have. */
	FInputValues InputValues;

	TSharedPtr<FNiagaraPlaceholderDataInterfaceHandle> PlaceholderDataInterfaceHandle;

	/** A cached pointer to the override node for this input if it exists.  This value is cached here since the
	  * UI reads this value every frame due to attribute updates. */
	mutable TOptional<UNiagaraNodeParameterMapSet*> OverrideNodeCache;

	/** A cached pointer to the override pin for this input if it exists.  This value is cached here since the
	  * UI reads this value every frame due to attribute updates. */
	mutable TOptional<UEdGraphPin*> OverridePinCache;

	/** Whether or not this input can be reset to its default value. */
	mutable TOptional<bool> bCanResetCache;

	/** Whether or not this input can be reset to a base value defined by a parent emitter. */
	mutable TOptional<bool> bCanResetToBaseCache;

	/** A tooltip to show for the value of this input. */
	mutable TOptional<FText> ValueToolTipCache;

	/** Text to display on a collapsed node. */
	mutable TOptional<FText> CollapsedTextCache;

	mutable TOptional<bool> bIsScratchDynamicInputCache;

	/** A flag to prevent handling graph changes when it's being updated directly by this object. */
	bool bUpdatingGraphDirectly;

	/** A flag to prevent handling changes to the local value when it's being set directly by this object. */
	bool bUpdatingLocalValueDirectly;

	/** A handle for removing the graph changed delegate. */
	FDelegateHandle GraphChangedHandle;
	FDelegateHandle OnRecompileHandle;

	/** A handle for removing the changed delegate for the rapid iteration parameters for the script
	   that owns the function that owns this input. */
	FDelegateHandle RapidIterationParametersChangedHandle;

	/** A multicast delegate which is called when the value of this input is changed. */
	FOnValueChanged ValueChangedDelegate;

	/** The script which owns the function which owns this input.  This is also the autoritative version of the rapid iteration parameters. */
	TWeakObjectPtr<UNiagaraScript> SourceScript;

	/** An array of scripts which this input affects. */
	TArray<TWeakObjectPtr<UNiagaraScript>> AffectedScripts;

	/** An input condition handler for the edit condition. */
	FNiagaraStackFunctionInputCondition EditCondition;

	/** An input condition handler for the visible condition. */
	FNiagaraStackFunctionInputCondition VisibleCondition;
	
	/** Whether or not to show an inline control for the edit condition input. */
	bool bShowEditConditionInline;

	/** Whether or not this input is an edit condition toggle. */
	bool bIsInlineEditConditionToggle;

	/** Whether or not the dynamic input for this input has a function script reassignment pending due to a request to fix a missing script. */
	bool bIsDynamicInputScriptReassignmentPending;

	/** A key to the message manager registration and its delegate binding. */
	FGuid MessageManagerRegistrationKey;

	//** Issues created outside of the RefreshChildren call that will be committed the next time the UI state is refreshed. */
	TArray<FStackIssue> MessageManagerIssues;

	FGuid MessageLogGuid;

	// If true then this stack entry is the semantic child of another stack entry
	bool bIsSemanticChild = false;

	// The value of the change id for the graph which owns the function call that owns this input as of the last refresh.
	TOptional<FGuid> LastOwningGraphChangeId;

	// The value of the change id for the called graph of the function call which owns this input as of the last refresh.
	TOptional<FGuid> LastFunctionGraphChangeId;
};
