// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraMessages.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraTypes.h"
#include "Engine/UserDefinedEnum.h"
#include "UObject/GCObject.h"
#include "UObject/SoftObjectPtr.h"
#include "NiagaraClipboard.generated.h"

class UNiagaraDataInterface;
class UNiagaraScript;
class UNiagaraRendererProperties;
class UNiagaraNodeFunctionCall;
class UNiagaraScriptVariable;

UENUM()
enum class ENiagaraClipboardFunctionInputValueMode
{
	Local,
	Linked,
	Data,
	Expression,
	Dynamic
};

class UNiagaraClipboardFunction;

UCLASS()
class NIAGARAEDITOR_API UNiagaraClipboardFunctionInput : public UObject
{
	GENERATED_BODY()

public:
	static const UNiagaraClipboardFunctionInput* CreateLocalValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, TArray<uint8>& InLocalValueData);

	static const UNiagaraClipboardFunctionInput* CreateLinkedValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, FName InLinkedValue);

	static const UNiagaraClipboardFunctionInput* CreateDataValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, UNiagaraDataInterface* InDataValue);

	static const UNiagaraClipboardFunctionInput* CreateExpressionValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, const FString& InExpressionValue);

	static const UNiagaraClipboardFunctionInput* CreateDynamicValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, FString InDynamicValueName, UNiagaraScript* InDynamicValue, const FGuid& InScriptVersion);

	UPROPERTY()
	FName InputName;

	UPROPERTY()
	FNiagaraTypeDefinition InputType;

	UPROPERTY()
	bool bHasEditCondition;

	UPROPERTY()
	bool bEditConditionValue;

	UPROPERTY()
	ENiagaraClipboardFunctionInputValueMode ValueMode;

	UPROPERTY()
	TArray<uint8> Local;

	UPROPERTY()
	FName Linked;

	UPROPERTY()
	TObjectPtr<UNiagaraDataInterface> Data;

	UPROPERTY()
	FString Expression;

	UPROPERTY()
	TObjectPtr<UNiagaraClipboardFunction> Dynamic;

	bool CopyValuesFrom(const UNiagaraClipboardFunctionInput* InOther);

	const FNiagaraTypeDefinition& GetTypeDef() const { return InputType; };

};

UENUM()
enum class ENiagaraClipboardFunctionScriptMode
{
	ScriptAsset,
	Assignment
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraClipboardFunction : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnPastedFunctionCallNode, UNiagaraNodeFunctionCall*, PastedFunctionCall);

	static UNiagaraClipboardFunction* CreateScriptFunction(UObject* InOuter, FString InFunctionName, UNiagaraScript* InScript, const FGuid& InScriptVersion = FGuid(), const TArray<FNiagaraStackMessage> InStackMessages = TArray<FNiagaraStackMessage>());

	static UNiagaraClipboardFunction* CreateAssignmentFunction(UObject* InOuter, FString InFunctionName, const TArray<FNiagaraVariable>& InAssignmentTargets, const TArray<FString>& InAssignmentDefaults);

	UPROPERTY()
	FString FunctionName;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	ENiagaraClipboardFunctionScriptMode ScriptMode;

	UPROPERTY()
	TSoftObjectPtr<UNiagaraScript> Script;

	UPROPERTY()
	TArray<FNiagaraVariable> AssignmentTargets;

	UPROPERTY()
	TArray<FString> AssignmentDefaults;

	UPROPERTY()
	TArray<TObjectPtr<const UNiagaraClipboardFunctionInput>> Inputs;

	UPROPERTY()
	FOnPastedFunctionCallNode OnPastedFunctionCallNodeDelegate;

	UPROPERTY()
	FGuid ScriptVersion;

	UPROPERTY()
	TArray<FNiagaraStackMessage> Messages;
};

USTRUCT()
struct NIAGARAEDITOR_API FNiagaraClipboardScriptVariable
{
	GENERATED_BODY()

	FNiagaraClipboardScriptVariable() : ScriptVariable(nullptr)
	{}
	FNiagaraClipboardScriptVariable(const UNiagaraScriptVariable& InScriptVariable) : ScriptVariable(&InScriptVariable), OriginalChangeId(InScriptVariable.GetChangeId())
	{}

	UPROPERTY();
	TObjectPtr<const UNiagaraScriptVariable> ScriptVariable;

	/** We cache the original change Id here since deserialization of the clipboard will cause the change id to update.
	 *  Using the original change id, we can identify during pasting whether we have already pasted this script variable before. 
	 */
	UPROPERTY()
	FGuid OriginalChangeId;

	// since the contained variables are typically copies, we compare their change IDs instead
	bool operator==(const FNiagaraClipboardScriptVariable& OtherVariable) const
	{
		return OriginalChangeId == OtherVariable.OriginalChangeId;
	}
};
UCLASS()
class NIAGARAEDITOR_API UNiagaraClipboardContent : public UObject
{
	GENERATED_BODY()

public:
	static UNiagaraClipboardContent* Create();

	UPROPERTY()
	TArray<TObjectPtr<const UNiagaraClipboardFunction>> Functions;

	UPROPERTY()
	TArray<TObjectPtr<const UNiagaraClipboardFunctionInput>> FunctionInputs;

	UPROPERTY()
	TArray<TObjectPtr<const UNiagaraRendererProperties>> Renderers;

	UPROPERTY()
	TArray<TObjectPtr<const UNiagaraScript>> Scripts;

	UPROPERTY()
	TArray<FNiagaraClipboardScriptVariable> ScriptVariables;

	/** We expect nodes to be exported into this string using FEdGraphUtilities::ExportNodesToText */
	UPROPERTY()
	FString ExportedNodes;

	/** Markup MetaData to specify that if scripts are pasted from the clipboard to automatically fixup their order in the stack to satisfy dependencies. */
	UPROPERTY()
	mutable bool bFixupPasteIndexForScriptDependenciesInStack = false;
};

class NIAGARAEDITOR_API FNiagaraClipboard
{
public:
	FNiagaraClipboard();

	void SetClipboardContent(UNiagaraClipboardContent* ClipboardContent);

	const UNiagaraClipboardContent* GetClipboardContent() const;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraClipboardEditorScriptingUtilities : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Input")
	static void TryGetInputByName(const TArray<UNiagaraClipboardFunctionInput*>& InInputs, FName InInputName, bool& bOutSucceeded, UNiagaraClipboardFunctionInput*& OutInput);

	UFUNCTION(BlueprintPure, Category = "Input")
	static void TryGetLocalValueAsFloat(const UNiagaraClipboardFunctionInput* InInput, bool& bOutSucceeded, float& OutValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static void TryGetLocalValueAsInt(const UNiagaraClipboardFunctionInput* InInput, bool& bOutSucceeded, int32& OutValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static void TrySetLocalValueAsInt(UNiagaraClipboardFunctionInput* InInput, bool& bOutSucceeded, int32 InValue, bool bLooseTyping = true);

	UFUNCTION(BlueprintPure, Category = "Input")
	static FName GetTypeName(const UNiagaraClipboardFunctionInput* InInput);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateFloatLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, float InLocalValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateVec2LocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, FVector2D InVec2Value);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateVec3LocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, FVector InVec3Value);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateIntLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, int32 InLocalValue);
	
	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateBoolLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, bool InBoolValue);
	
	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateStructLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, UUserDefinedStruct* InStructValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateEnumLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditCoditionValue, UUserDefinedEnum* InEnumType, int32 InEnumValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateLinkedValueInput(UObject* InOuter, FName InInputName, FName InInputTypeName, bool bInHasEditCondition, bool bInEditConditionValue, FName InLinkedValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateDataValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, UNiagaraDataInterface* InDataValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateExpressionValueInput(UObject* InOuter, FName InInputName, FName InInputTypeName, bool bInHasEditCondition, bool bInEditConditionValue, const FString& InExpressionValue);

	UFUNCTION(BlueprintPure, Category = "Input")
	static UNiagaraClipboardFunctionInput* CreateDynamicValueInput(UObject* InOuter, FName InInputName, FName InInputTypeName, bool bInHasEditCondition, bool bInEditConditionValue, FString InDynamicValueName, UNiagaraScript* InDynamicValue);

	static FNiagaraTypeDefinition GetRegisteredTypeDefinitionByName(FName InTypeName);
};
