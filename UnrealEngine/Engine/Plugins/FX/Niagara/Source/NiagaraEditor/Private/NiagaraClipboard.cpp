// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraClipboard.h"
#include "NiagaraDataInterface.h"
#include "NiagaraScript.h"

#include "Factories.h"
#include "UObject/UObjectMarks.h"
#include "UObject/PropertyPortFlags.h"
#include "Containers/UnrealString.h"
#include "UnrealExporter.h"
#include "Exporters/Exporter.h"
#include "HAL/PlatformApplicationMisc.h"
#include "NiagaraNodeFunctionCall.h"
#include "Engine/UserDefinedEnum.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraClipboard)

struct FNiagaraClipboardContentTextObjectFactory : public FCustomizableTextObjectFactory
{
public:
	UNiagaraClipboardContent* ClipboardContent;

public:
	FNiagaraClipboardContentTextObjectFactory()
		: FCustomizableTextObjectFactory(GWarn)
		, ClipboardContent(nullptr)
	{
	}

protected:
	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const override
	{
		return ObjectClass == UNiagaraClipboardContent::StaticClass();
	}

	virtual void ProcessConstructedObject(UObject* CreatedObject) override
	{
		if (CreatedObject->IsA<UNiagaraClipboardContent>())
		{
			ClipboardContent = CastChecked<UNiagaraClipboardContent>(CreatedObject);
		}
	}
};

UNiagaraClipboardFunctionInput* MakeNewInput(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, ENiagaraClipboardFunctionInputValueMode InValueMode)
{
	UNiagaraClipboardFunctionInput* NewInput = Cast<UNiagaraClipboardFunctionInput>(NewObject<UNiagaraClipboardFunctionInput>(InOuter));
	NewInput->InputName = InInputName;
	NewInput->InputType = InInputType;
	NewInput->bHasEditCondition = bInEditConditionValue.IsSet();
	NewInput->bEditConditionValue = bInEditConditionValue.Get(false);
	NewInput->ValueMode = InValueMode;
	return NewInput;
}

const UNiagaraClipboardFunctionInput* UNiagaraClipboardFunctionInput::CreateLocalValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, TArray<uint8>& InLocalValueData)
{
	checkf(InLocalValueData.Num() == InInputType.GetSize(), TEXT("Input data size didn't match type size."))
	UNiagaraClipboardFunctionInput* NewInput = MakeNewInput(InOuter, InInputName, InInputType, bInEditConditionValue, ENiagaraClipboardFunctionInputValueMode::Local);
	NewInput->Local = InLocalValueData;
	return NewInput;
}

const UNiagaraClipboardFunctionInput* UNiagaraClipboardFunctionInput::CreateLinkedValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, FName InLinkedValue)
{
	UNiagaraClipboardFunctionInput* NewInput = MakeNewInput(InOuter, InInputName, InInputType, bInEditConditionValue, ENiagaraClipboardFunctionInputValueMode::Linked);
	NewInput->Linked = InLinkedValue;
	return NewInput;
}

const UNiagaraClipboardFunctionInput* UNiagaraClipboardFunctionInput::CreateDataValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, UNiagaraDataInterface* InDataValue)
{
	UNiagaraClipboardFunctionInput* NewInput = MakeNewInput(InOuter, InInputName, InInputType, bInEditConditionValue, ENiagaraClipboardFunctionInputValueMode::Data);
	NewInput->Data = NewObject<UNiagaraDataInterface>(NewInput, InDataValue->GetClass());
	InDataValue->CopyTo(NewInput->Data);
	return NewInput;
}

const UNiagaraClipboardFunctionInput* UNiagaraClipboardFunctionInput::CreateExpressionValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, const FString& InExpressionValue)
{
	UNiagaraClipboardFunctionInput* NewInput = MakeNewInput(InOuter, InInputName, InInputType, bInEditConditionValue, ENiagaraClipboardFunctionInputValueMode::Expression);
	NewInput->Expression = InExpressionValue;
	return NewInput;
}

const UNiagaraClipboardFunctionInput* UNiagaraClipboardFunctionInput::CreateDynamicValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, TOptional<bool> bInEditConditionValue, FString InDynamicValueName, UNiagaraScript* InDynamicValue, const FGuid& InScriptVersion)
{
	UNiagaraClipboardFunctionInput* NewInput = MakeNewInput(InOuter, InInputName, InInputType, bInEditConditionValue, ENiagaraClipboardFunctionInputValueMode::Dynamic);
	NewInput->Dynamic = UNiagaraClipboardFunction::CreateScriptFunction(NewInput, InDynamicValueName, InDynamicValue, InScriptVersion);
	return NewInput;
}


bool UNiagaraClipboardFunctionInput::CopyValuesFrom(const UNiagaraClipboardFunctionInput* InOther)
{
	if (InputType != InOther->InputType)
		return false;

	ValueMode = InOther->ValueMode;
	Local = InOther->Local;
	Linked = InOther->Linked;

	Data = nullptr;
	if (InOther->Data)
		Data = Cast<UNiagaraDataInterface>(StaticDuplicateObject(InOther->Data, this));
	Expression = InOther->Expression;
	Dynamic = nullptr;
	if (InOther->Dynamic)
		Dynamic = Cast<UNiagaraClipboardFunction>(StaticDuplicateObject(InOther->Dynamic, this));

	return true;
}

UNiagaraClipboardFunction* UNiagaraClipboardFunction::CreateScriptFunction(UObject* InOuter, FString InFunctionName, UNiagaraScript* InScript, const FGuid& InScriptVersion, const TArray<FNiagaraStackMessage> InMessages)
{
	UNiagaraClipboardFunction* NewFunction = Cast<UNiagaraClipboardFunction>(NewObject<UNiagaraClipboardFunction>(InOuter));
	NewFunction->ScriptMode = ENiagaraClipboardFunctionScriptMode::ScriptAsset;
	NewFunction->FunctionName = InFunctionName;
	NewFunction->Script = InScript;
	NewFunction->ScriptVersion = InScriptVersion;
	NewFunction->Messages = InMessages;
	return NewFunction;
}

UNiagaraClipboardFunction* UNiagaraClipboardFunction::CreateAssignmentFunction(UObject* InOuter, FString InFunctionName, const TArray<FNiagaraVariable>& InAssignmentTargets, const TArray<FString>& InAssignmentDefaults)
{
	UNiagaraClipboardFunction* NewFunction = Cast<UNiagaraClipboardFunction>(NewObject<UNiagaraClipboardFunction>(InOuter));
	NewFunction->ScriptMode = ENiagaraClipboardFunctionScriptMode::Assignment;
	NewFunction->FunctionName = InFunctionName;
	NewFunction->AssignmentTargets = InAssignmentTargets;
	NewFunction->AssignmentDefaults = InAssignmentDefaults;
	return NewFunction;
}

UNiagaraClipboardContent* UNiagaraClipboardContent::Create()
{
	return NewObject<UNiagaraClipboardContent>(GetTransientPackage());
}

FNiagaraClipboard::FNiagaraClipboard()
{
}

void FNiagaraClipboard::SetClipboardContent(UNiagaraClipboardContent* ClipboardContent)
{
	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

	// Export the clipboard to text.
	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice(&Context, ClipboardContent, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ClipboardContent->GetOuter());
	FPlatformApplicationMisc::ClipboardCopy(*Archive);
}

const UNiagaraClipboardContent* FNiagaraClipboard::GetClipboardContent() const
{
	// Get the text from the clipboard.
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);

	// Try to create niagara clipboard content from that.
	FNiagaraClipboardContentTextObjectFactory ClipboardContentFactory;
	if (ClipboardContentFactory.CanCreateObjectsFromText(ClipboardText))
	{
		ClipboardContentFactory.ProcessBuffer(GetTransientPackage(), RF_Transactional, ClipboardText);
		return ClipboardContentFactory.ClipboardContent;
	}

	return nullptr;
}

void UNiagaraClipboardEditorScriptingUtilities::TryGetInputByName(const TArray<UNiagaraClipboardFunctionInput*>& InInputs, FName InInputName, bool& bOutSucceeded, UNiagaraClipboardFunctionInput*& OutInput)
{
	for (UNiagaraClipboardFunctionInput* Input : InInputs)
	{
		if (Input->InputName == InInputName)
		{
			bOutSucceeded = true;
			OutInput = const_cast<UNiagaraClipboardFunctionInput*>(Input);
		}
	}
	bOutSucceeded = false;
	OutInput = nullptr;
}

void UNiagaraClipboardEditorScriptingUtilities::TryGetLocalValueAsFloat(const UNiagaraClipboardFunctionInput* InInput, bool& bOutSucceeded, float& OutValue)
{
	if (InInput->ValueMode == ENiagaraClipboardFunctionInputValueMode::Local && InInput->InputType == FNiagaraTypeDefinition::GetFloatDef() && InInput->Local.Num() == InInput->InputType.GetSize())
	{
		bOutSucceeded = true;
		OutValue = *((float*)InInput->Local.GetData());
	}
	else
	{
		bOutSucceeded = false;
		OutValue = 0.0f;
	}
}

void UNiagaraClipboardEditorScriptingUtilities::TryGetLocalValueAsInt(const UNiagaraClipboardFunctionInput* InInput, bool& bOutSucceeded, int32& OutValue)
{
	bool bCompatibleType = InInput->InputType == FNiagaraTypeDefinition::GetIntDef() || (InInput->InputType.IsEnum() && InInput->InputType.GetSize() == sizeof(int32)) ||
		InInput->InputType == FNiagaraTypeDefinition::GetBoolDef();
	if (InInput->ValueMode == ENiagaraClipboardFunctionInputValueMode::Local && bCompatibleType && InInput->Local.Num() == InInput->InputType.GetSize())
	{
		bOutSucceeded = true;
		OutValue = *((int32*)InInput->Local.GetData());
	}
	else
	{
		bOutSucceeded = false;
		OutValue = 0;
	}
}

void UNiagaraClipboardEditorScriptingUtilities::TrySetLocalValueAsInt(UNiagaraClipboardFunctionInput* InInput, bool& bOutSucceeded, int32 InValue,bool bLooseTyping)
{
	bool bCompatibleType = InInput->InputType == FNiagaraTypeDefinition::GetIntDef() || (bLooseTyping && InInput->InputType.IsEnum() && InInput->InputType.GetSize() == sizeof(int32)) ||
		(bLooseTyping && InInput->InputType == FNiagaraTypeDefinition::GetBoolDef() && InInput->InputType.GetSize() == sizeof(int32));
	if (InInput->ValueMode == ENiagaraClipboardFunctionInputValueMode::Local && bCompatibleType && InInput->Local.Num() == InInput->InputType.GetSize())
	{
		bOutSucceeded = true;
		*((int32*)InInput->Local.GetData()) = InValue;
	}
	else
	{
		bOutSucceeded = false;
	}
}

FName UNiagaraClipboardEditorScriptingUtilities::GetTypeName(const UNiagaraClipboardFunctionInput* InInput)
{
	return InInput->InputType.GetFName();
}

const UNiagaraClipboardFunctionInput* CreateLocalValue(UObject* InOuter, FName InInputName, FNiagaraTypeDefinition InInputType, bool bInHasEditCondition, bool bInEditConditionValue, TArray<uint8>& InLocalValueData)
{
	return UNiagaraClipboardFunctionInput::CreateLocalValue(
		InOuter != nullptr ? InOuter : GetTransientPackage(),
		InInputName,
		InInputType,
		bInHasEditCondition ? TOptional<bool>(bInEditConditionValue) : TOptional<bool>(),
		InLocalValueData);
}

FNiagaraTypeDefinition UNiagaraClipboardEditorScriptingUtilities::GetRegisteredTypeDefinitionByName(FName InTypeName)
{
	for (const FNiagaraTypeDefinition& RegisteredType : FNiagaraTypeRegistry::GetRegisteredTypes())
	{
		if (RegisteredType.GetFName() == InTypeName)
		{
			return RegisteredType;
		}
	}
	return FNiagaraTypeDefinition();
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateFloatLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, float InFloatValue)
{
	FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition::GetFloatDef();
	TArray<uint8> FloatValue;
	FloatValue.AddUninitialized(InputType.GetSize());
	FMemory::Memcpy(FloatValue.GetData(), &InFloatValue, InputType.GetSize());

	return const_cast<UNiagaraClipboardFunctionInput*>(CreateLocalValue(InOuter, InInputName, InputType, bInHasEditCondition, bInEditConditionValue, FloatValue));
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateVec2LocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, FVector2D InVec2Value)
{
	FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition::GetVec2Def();
	const FVector2f InVec2ValueFloat = FVector2f(InVec2Value);	// LWC_TODO: Precision loss
	TArray<uint8> Vec2ValueFloat;
	Vec2ValueFloat.AddUninitialized(InputType.GetSize());
	FMemory::Memcpy(Vec2ValueFloat.GetData(), &InVec2ValueFloat, InputType.GetSize());

	return const_cast<UNiagaraClipboardFunctionInput*>(CreateLocalValue(InOuter, InInputName, InputType, bInHasEditCondition, bInEditConditionValue, Vec2ValueFloat));
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateVec3LocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, FVector InVec3Value)
{
	FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition::GetVec3Def();
	const FVector3f InVec3ValueFloat = (FVector3f)InVec3Value;
	TArray<uint8> Vec3ValueFloat;
	Vec3ValueFloat.AddUninitialized(InputType.GetSize());
	FMemory::Memcpy(Vec3ValueFloat.GetData(), &InVec3ValueFloat, InputType.GetSize());

	return const_cast<UNiagaraClipboardFunctionInput*>(CreateLocalValue(InOuter, InInputName, InputType, bInHasEditCondition, bInEditConditionValue, Vec3ValueFloat));
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateIntLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, int32 InIntValue)
{
	FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition::GetIntDef();
	TArray<uint8> IntValue;
	IntValue.AddUninitialized(InputType.GetSize());
	FMemory::Memcpy(IntValue.GetData(), &InIntValue, InputType.GetSize());

	return const_cast<UNiagaraClipboardFunctionInput*>(CreateLocalValue(InOuter, InInputName, InputType, bInHasEditCondition, bInEditConditionValue, IntValue));
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateBoolLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, bool InBoolValue)
{
	FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition::GetBoolDef();
	const int32 BoolAsIntValue = InBoolValue ? 1 : 0;
	TArray<uint8> IntValue;
	IntValue.AddUninitialized(InputType.GetSize());
	FMemory::Memcpy(IntValue.GetData(), &BoolAsIntValue, InputType.GetSize());

	return const_cast<UNiagaraClipboardFunctionInput*>(CreateLocalValue(InOuter, InInputName, InputType, bInHasEditCondition, bInEditConditionValue, IntValue));
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateStructLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, UUserDefinedStruct* InStructValue)
{
	FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition(InStructValue);
	FStructOnScope StructOnScope = FStructOnScope(InStructValue);
	TArray<uint8> StructValue;
	const int32 StructSize = InStructValue->GetStructureSize();
	StructValue.AddUninitialized(StructSize);
	FMemory::Memcpy(StructValue.GetData(), StructOnScope.GetStructMemory(), StructSize);
	
	return const_cast<UNiagaraClipboardFunctionInput*>(UNiagaraClipboardFunctionInput::CreateLocalValue(
		InOuter != nullptr ? InOuter : GetTransientPackage(),
		InInputName,
		InputType,
		bInHasEditCondition ? TOptional<bool>(bInEditConditionValue) : TOptional<bool>(),
		StructValue)
	);
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateEnumLocalValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditCoditionValue, UUserDefinedEnum* InEnumType, int32 InEnumValue)
{
	FNiagaraTypeDefinition InputType = FNiagaraTypeDefinition(Cast<UEnum>(InEnumType));
	TArray<uint8> EnumValue;
	EnumValue.AddUninitialized(sizeof(int32));
	FMemory::Memcpy(EnumValue.GetData(), &InEnumValue, sizeof(int32));

	return const_cast<UNiagaraClipboardFunctionInput*>(UNiagaraClipboardFunctionInput::CreateLocalValue(
		InOuter != nullptr ? InOuter : GetTransientPackage(),
		InInputName,
		InputType,
		bInHasEditCondition ? TOptional<bool>(bInEditCoditionValue) : TOptional<bool>(),
		EnumValue)
	);
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateLinkedValueInput(UObject* InOuter, FName InInputName, FName InInputTypeName, bool bInHasEditCondition, bool bInEditConditionValue, FName InLinkedValue)
{
	FNiagaraTypeDefinition InputType = GetRegisteredTypeDefinitionByName(InInputTypeName);
	if (InputType.IsValid())
	{
		return const_cast<UNiagaraClipboardFunctionInput*>(UNiagaraClipboardFunctionInput::CreateLinkedValue(
			InOuter != nullptr ? InOuter : GetTransientPackage(),
			InInputName,
			InputType,
			bInHasEditCondition ? TOptional<bool>(bInEditConditionValue) : TOptional<bool>(),
			InLinkedValue));
	}
	return nullptr;
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateDataValueInput(UObject* InOuter, FName InInputName, bool bInHasEditCondition, bool bInEditConditionValue, UNiagaraDataInterface* InDataValue)
{
	if (InDataValue != nullptr)
	{ 
		return const_cast<UNiagaraClipboardFunctionInput*>(UNiagaraClipboardFunctionInput::CreateDataValue(
			InOuter != nullptr ? InOuter : GetTransientPackage(),
			InInputName,
			FNiagaraTypeDefinition(InDataValue->GetClass()),
			bInHasEditCondition ? TOptional<bool>(bInEditConditionValue) : TOptional<bool>(),
			InDataValue));
	}
	return nullptr;
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateExpressionValueInput(UObject* InOuter, FName InInputName, FName InInputTypeName, bool bInHasEditCondition, bool bInEditConditionValue, const FString& InExpressionValue)
{
	FNiagaraTypeDefinition InputType = GetRegisteredTypeDefinitionByName(InInputTypeName);
	if (InputType.IsValid())
	{
		return const_cast<UNiagaraClipboardFunctionInput*>(UNiagaraClipboardFunctionInput::CreateExpressionValue(
			InOuter != nullptr ? InOuter : GetTransientPackage(),
			InInputName,
			InputType,
			bInHasEditCondition ? TOptional<bool>(bInEditConditionValue) : TOptional<bool>(),
			InExpressionValue));
	}
	return nullptr;
}

UNiagaraClipboardFunctionInput* UNiagaraClipboardEditorScriptingUtilities::CreateDynamicValueInput(UObject* InOuter, FName InInputName, FName InInputTypeName, bool bInHasEditCondition, bool bInEditConditionValue, FString InDynamicValueName, UNiagaraScript* InDynamicValue)
{
	FNiagaraTypeDefinition InputType = GetRegisteredTypeDefinitionByName(InInputTypeName);
	if (InputType.IsValid())
	{
		return const_cast<UNiagaraClipboardFunctionInput*>(UNiagaraClipboardFunctionInput::CreateDynamicValue(
			InOuter != nullptr ? InOuter : GetTransientPackage(),
			InInputName,
			InputType,
			bInHasEditCondition ? TOptional<bool>(bInEditConditionValue) : TOptional<bool>(),
			InDynamicValueName,
			InDynamicValue,
			FGuid()));
	}
	return nullptr;
}

