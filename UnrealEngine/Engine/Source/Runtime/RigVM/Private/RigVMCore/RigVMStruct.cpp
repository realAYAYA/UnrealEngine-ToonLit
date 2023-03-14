// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVMRegistry.h"
#include "UObject/StructOnScope.h"
#include "RigVMModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMStruct)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FRigVMUnitNodeCreatedContext::IsValid() const
{
	return Reason != ERigVMNodeCreatedReason::Unknown &&
		AllExternalVariablesDelegate.IsBound() &&
		CreateExternalVariableDelegate.IsBound() &&
		BindPinToExternalVariableDelegate.IsBound();
}

TArray<FRigVMExternalVariable> FRigVMUnitNodeCreatedContext::GetExternalVariables() const
{
	TArray<FRigVMExternalVariable> ExternalVariables;

	if (AllExternalVariablesDelegate.IsBound())
	{
		ExternalVariables = AllExternalVariablesDelegate.Execute();
	}

	return ExternalVariables;
}

FName FRigVMUnitNodeCreatedContext::AddExternalVariable(const FRigVMExternalVariable& InVariableToCreate, FString InDefaultValue)
{
	if (CreateExternalVariableDelegate.IsBound())
	{
		return CreateExternalVariableDelegate.Execute(InVariableToCreate, InDefaultValue);
	}
	return NAME_None;
}

bool FRigVMUnitNodeCreatedContext::BindPinToExternalVariable(FString InPinPath, FString InVariablePath)
{
	if (BindPinToExternalVariableDelegate.IsBound())
	{
		const FString NodePinPath = FString::Printf(TEXT("%s.%s"), *NodeName.ToString(), *InPinPath);
		return BindPinToExternalVariableDelegate.Execute(NodePinPath, InVariablePath);
	}
	return false;
}

FRigVMExternalVariable FRigVMUnitNodeCreatedContext::FindVariable(FName InVariableName) const
{
	TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariables();
	for (FRigVMExternalVariable ExternalVariable : ExternalVariables)
	{
		if (ExternalVariable.Name == InVariableName)
		{
			return ExternalVariable;
		}
	}
	return FRigVMExternalVariable();
}

FName FRigVMUnitNodeCreatedContext::FindFirstVariableOfType(FName InCPPTypeName) const
{
	TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariables();
	for (FRigVMExternalVariable ExternalVariable : ExternalVariables)
	{
		if (ExternalVariable.TypeName == InCPPTypeName)
		{
			return ExternalVariable.Name;
		}
	}
	return NAME_None;
}

FName FRigVMUnitNodeCreatedContext::FindFirstVariableOfType(UObject* InCPPTypeObject) const
{
	TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariables();
	for (FRigVMExternalVariable ExternalVariable : ExternalVariables)
	{
		if (ExternalVariable.TypeObject == InCPPTypeObject)
		{
			return ExternalVariable.Name;
		}
	}
	return NAME_None;
}

FRigVMStructUpgradeInfo::FRigVMStructUpgradeInfo()
	: NodePath()
	, OldStruct(nullptr)
	, NewStruct(nullptr)
{
}

FRigVMStructUpgradeInfo FRigVMStructUpgradeInfo::MakeFromStructToFactory(UScriptStruct* InRigVMStruct, UScriptStruct* InFactoryStruct)
{
	check(InRigVMStruct);
	check(InFactoryStruct);
	
	const FRigVMDispatchFactory* Factory = FRigVMRegistry::Get().FindOrAddDispatchFactory(InFactoryStruct);
	check(Factory);
	const FRigVMTemplateTypeMap Types = GetTypeMapFromStruct(InRigVMStruct);
	const FString PermutationName = Factory->GetPermutationName(Types);

	FRigVMStructUpgradeInfo Info;
	Info.OldStruct = InRigVMStruct;
	Info.NewStruct = InFactoryStruct;
	Info.NewDispatchFunction = *PermutationName;
	return Info;
}

FRigVMTemplateTypeMap FRigVMStructUpgradeInfo::GetTypeMapFromStruct(UScriptStruct* InScriptStruct)
{
	FRigVMTemplateTypeMap Types;
#if WITH_EDITOR
	for (TFieldIterator<FProperty> It(InScriptStruct); It; ++It)
	{
		if(It->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		if(!It->HasMetaData(FRigVMStruct::InputMetaName) &&
			!It->HasMetaData(FRigVMStruct::OutputMetaName) &&
			!It->HasMetaData(FRigVMStruct::VisibleMetaName))
		{
			continue;
		}
		
		const FName PropertyName = It->GetFName();
		const TRigVMTypeIndex TypeIndex = FRigVMTemplateArgument(*It).GetTypeIndices()[0];
		Types.Add(PropertyName, TypeIndex);
	}
#endif
	return Types;
}

bool FRigVMStructUpgradeInfo::IsValid() const
{
	return NodePath.IsEmpty() && (OldStruct != nullptr) && (NewStruct != nullptr);
}

const FString& FRigVMStructUpgradeInfo::GetDefaultValueForPin(const FName& InPinName) const
{
	static const FString EmptyString = FString();
	if(const FString* DefaultValue = DefaultValues.Find(InPinName))
	{
		return *DefaultValue;
	}
	return EmptyString;
}

void FRigVMStructUpgradeInfo::SetDefaultValueForPin(const FName& InPinName, const FString& InDefaultValue)
{
	DefaultValues.FindOrAdd(InPinName) = InDefaultValue;
}

void FRigVMStructUpgradeInfo::AddRemappedPin(const FString& InOldPinPath, const FString& InNewPinPath, bool bAsInput,
	bool bAsOutput)
{
	if(bAsInput)
	{
		InputLinkMap.Add(InOldPinPath, InNewPinPath);
	}
	if(bAsOutput)
	{
		OutputLinkMap.Add(InOldPinPath, InNewPinPath);
	}
}

FString FRigVMStructUpgradeInfo::RemapPin(const FString& InPinPath, bool bIsInput, bool bContainsNodeName) const
{
	FString NodeName;
	FString PinPath = InPinPath;

	if(bContainsNodeName)
	{
		if(!PinPath.Split(TEXT("."), &NodeName, &PinPath, ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			return InPinPath;
		}
	}

	const TMap<FString, FString>& LinkMap = bIsInput ? InputLinkMap : OutputLinkMap;

	int32 FoundMaxLength = 0;
	FString FoundReplacement;

	for(const TPair<FString, FString>& Pair : LinkMap)
	{
		if(Pair.Key == PinPath)
		{
			PinPath = Pair.Value;
			FoundReplacement.Reset();
			break;
		}

		const FString KeyWithPeriod = Pair.Key + TEXT(".");
		if(PinPath.StartsWith(KeyWithPeriod))
		{
			if(FoundMaxLength < KeyWithPeriod.Len())
			{
				FoundMaxLength = KeyWithPeriod.Len();
				FoundReplacement = Pair.Value + PinPath.RightChop(Pair.Key.Len());
			}
		}
	}

	if(!FoundReplacement.IsEmpty())
	{
		PinPath = FoundReplacement;
	}

	if(bContainsNodeName)
	{
		PinPath = FString::Printf(TEXT("%s.%s"), *NodeName, *PinPath);
	}

	return PinPath;
}

FString FRigVMStructUpgradeInfo::AddAggregatePin(FString InPinName)
{
	if(InPinName.IsEmpty())
	{
		FString LastPinName;
		
		FStructOnScope StructOnScope(GetNewStruct());
		const FRigVMStruct* StructMemory = (const FRigVMStruct*)StructOnScope.GetStructMemory();

		if(AggregatePins.IsEmpty())
		{
#if WITH_EDITOR
			FString LastInputPinName;
			FString LastOutputPinName;
			for (TFieldIterator<FProperty> It(GetNewStruct()); It; ++It)
			{
				if(It->HasMetaData(FRigVMStruct::AggregateMetaName))
				{
					FString& LastDeterminedPinName = It->HasMetaData(FRigVMStruct::InputMetaName) ?
						LastInputPinName : LastOutputPinName;

					if(!LastDeterminedPinName.IsEmpty())
					{
						LastPinName = It->GetName();
						break;
					}

					LastDeterminedPinName = It->GetName();
				}
			}
#else
			LastPinName = TEXT("B");
#endif
		}
		else
		{
			LastPinName = AggregatePins.Last();
		}

		InPinName = StructMemory->GetNextAggregateName(*LastPinName).ToString();
	}
	AggregatePins.Add(InPinName);
	return AggregatePins.Last();
}

void FRigVMStructUpgradeInfo::SetDefaultValues(const FRigVMStruct* InNewStructMemory)
{
	check(NewStruct);
	check(InNewStructMemory);
	DefaultValues = InNewStructMemory->GetDefaultValues(NewStruct);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const FName FRigVMStruct::DeprecatedMetaName("Deprecated");
const FName FRigVMStruct::InputMetaName("Input");
const FName FRigVMStruct::OutputMetaName("Output");
const FName FRigVMStruct::IOMetaName("IO");
const FName FRigVMStruct::HiddenMetaName("Hidden");
const FName FRigVMStruct::VisibleMetaName("Visible");
const FName FRigVMStruct::DetailsOnlyMetaName("DetailsOnly");
const FName FRigVMStruct::AbstractMetaName("Abstract");
const FName FRigVMStruct::CategoryMetaName("Category");
const FName FRigVMStruct::DisplayNameMetaName("DisplayName");
const FName FRigVMStruct::MenuDescSuffixMetaName("MenuDescSuffix");
const FName FRigVMStruct::ShowVariableNameInTitleMetaName("ShowVariableNameInTitle");
const FName FRigVMStruct::CustomWidgetMetaName("CustomWidget");
const FName FRigVMStruct::ConstantMetaName("Constant");
const FName FRigVMStruct::TitleColorMetaName("TitleColor");
const FName FRigVMStruct::NodeColorMetaName("NodeColor");
const FName FRigVMStruct::IconMetaName("Icon");
const FName FRigVMStruct::KeywordsMetaName("Keywords");
const FName FRigVMStruct::TemplateNameMetaName = FRigVMRegistry::TemplateNameMetaName;
const FName FRigVMStruct::AggregateMetaName("Aggregate");
const FName FRigVMStruct::ExpandPinByDefaultMetaName("ExpandByDefault");
const FName FRigVMStruct::DefaultArraySizeMetaName("DefaultArraySize");
const FName FRigVMStruct::VaryingMetaName("Varying");
const FName FRigVMStruct::SingletonMetaName("Singleton");
const FName FRigVMStruct::SliceContextMetaName("SliceContext");
const FName FRigVMStruct::ExecuteName = TEXT("Execute");
const FName FRigVMStruct::ExecuteContextName = TEXT("ExecuteContext");
const FName FRigVMStruct::ForLoopCountPinName("Count");
const FName FRigVMStruct::ForLoopContinuePinName("Continue");
const FName FRigVMStruct::ForLoopCompletedPinName("Completed");
const FName FRigVMStruct::ForLoopIndexPinName("Index");

float FRigVMStruct::GetRatioFromIndex(int32 InIndex, int32 InCount)
{
	if (InCount <= 1)
	{
		return 0.f;
	}
	return ((float)FMath::Clamp<int32>(InIndex, 0, InCount - 1)) / ((float)(InCount - 1));
}

TArray<FRigVMUserWorkflow> FRigVMStruct::GetWorkflows(ERigVMUserWorkflowType InType, const UObject* InSubject) const
{
	return GetSupportedWorkflows(InSubject).FilterByPredicate([InType](const FRigVMUserWorkflow& InWorkflow) -> bool
	{
		return uint32(InWorkflow.GetType()) & uint32(InType) &&
			InWorkflow.IsValid();
	});
}

#if WITH_EDITOR

bool FRigVMStruct::ValidateStruct(UScriptStruct* InStruct, FString* OutErrorMessage)
{
	if (!InStruct->IsChildOf(StaticStruct()))
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = TEXT("Not a child of FRigVMStruct.");
		}
		return false;
	}

	FStructOnScope StructOnScope(InStruct);
	FRigVMStruct* StructMemory = (FRigVMStruct*)StructOnScope.GetStructMemory();

	if (StructMemory->IsForLoop())
	{
		if (!CheckPinExists(InStruct, ForLoopCountPinName, TEXT("int32"), OutErrorMessage))
		{
			return false;
		}
		else
		{
			if (!CheckPinDirection(InStruct, ForLoopCountPinName, InputMetaName) &&
				!CheckPinDirection(InStruct, ForLoopCountPinName, OutputMetaName) &&
				!CheckPinDirection(InStruct, ForLoopCountPinName, HiddenMetaName))
			{
				if (OutErrorMessage)
				{
					*OutErrorMessage = FString::Printf(TEXT("The '%s' pin needs to be either hidden, an input or an output."), *ForLoopCountPinName.ToString());
				}
				return false;
			}
			if (!CheckMetadata(InStruct, ForLoopCountPinName, SingletonMetaName, OutErrorMessage))
			{
				return false;
			}
		}

		if (!CheckPinExists(InStruct, ForLoopContinuePinName, TEXT("bool"), OutErrorMessage))
		{
			return false;
		}
		else
		{
			if (!CheckPinDirection(InStruct, ForLoopContinuePinName, HiddenMetaName))
			{
				if (OutErrorMessage)
				{
					*OutErrorMessage = FString::Printf(TEXT("The '%s' pin needs to be hidden."), *ForLoopContinuePinName.ToString());
				}
				return false;
			}
			if (!CheckMetadata(InStruct, ForLoopContinuePinName, SingletonMetaName, OutErrorMessage))
			{
				return false;
			}
		}

		if (!CheckPinExists(InStruct, ForLoopIndexPinName, TEXT("int32"), OutErrorMessage))
		{
			return false;
		}
		else
		{
			if (!CheckPinDirection(InStruct, ForLoopIndexPinName, HiddenMetaName) &&
				!CheckPinDirection(InStruct, ForLoopIndexPinName, OutputMetaName))
			{
				if (OutErrorMessage)
				{
					*OutErrorMessage = FString::Printf(TEXT("The '%s' pin needs to be hidden or an output."), *ForLoopIndexPinName.ToString());
				}
				return false;
			}
			if (!CheckMetadata(InStruct, ForLoopContinuePinName, SingletonMetaName, OutErrorMessage))
			{
				return false;
			}
		}
		
		if (!CheckPinExists(InStruct, ExecuteContextName, FString(), OutErrorMessage))
		{
			return false;
		}
		else
		{
			if (!CheckPinDirection(InStruct, ExecuteContextName, IOMetaName))
			{
				if (OutErrorMessage)
				{
					*OutErrorMessage = FString::Printf(TEXT("The '%s' pin needs to be IO."), *ExecuteContextName.ToString());
				}
				return false;
			}
		}

		if (!CheckPinExists(InStruct, ForLoopCompletedPinName, FString(), OutErrorMessage))
		{
			return false;
		}
		else
		{
			if (!CheckPinDirection(InStruct, ForLoopCompletedPinName, OutputMetaName))
			{
				if (OutErrorMessage)
				{
					*OutErrorMessage = FString::Printf(TEXT("The '%s' pin needs to be an output."), *ForLoopCompletedPinName.ToString());
				}
				return false;
			}
		}
	}

	return true;
}

bool FRigVMStruct::CheckPinDirection(UScriptStruct* InStruct, const FName& PinName, const FName& InDirectionMetaName)
{
	if (FProperty* Property = InStruct->FindPropertyByName(PinName))
	{
		if (InDirectionMetaName == IOMetaName)
		{
			return Property->HasMetaData(InputMetaName) && Property->HasMetaData(OutputMetaName);
		}
		else if (InDirectionMetaName == HiddenMetaName)
		{
			return !Property->HasMetaData(InputMetaName) && !Property->HasMetaData(OutputMetaName);
		}
		return Property->HasMetaData(InDirectionMetaName);
	}
	return true;
}

bool FRigVMStruct::CheckPinType(UScriptStruct* InStruct, const FName& PinName, const FString& ExpectedType, FString* OutErrorMessage)
{
	if (FProperty* Property = InStruct->FindPropertyByName(PinName))
	{
		if (Property->GetCPPType() != ExpectedType)
		{
			if (OutErrorMessage)
			{
				*OutErrorMessage = FString::Printf(TEXT("The '%s' property needs to be of type '%s'."), *ForLoopCountPinName.ToString(), *ExpectedType);;
			}
			return false;
		}
	}

	return true;
}

bool FRigVMStruct::CheckPinExists(UScriptStruct* InStruct, const FName& PinName, const FString& ExpectedType, FString* OutErrorMessage)
{
	if (FProperty* Property = InStruct->FindPropertyByName(PinName))
	{
		if (!ExpectedType.IsEmpty())
		{
			if (Property->GetCPPType() != ExpectedType)
			{
				if (OutErrorMessage)
				{
					*OutErrorMessage = FString::Printf(TEXT("The '%s' property needs to be of type '%s'."), *ForLoopCountPinName.ToString(), *ExpectedType);;
				}
				return false;
			}
		}
	}
	else
	{
		if (OutErrorMessage)
		{
			if (ExpectedType.IsEmpty())
			{
				*OutErrorMessage = FString::Printf(TEXT("Struct requires a '%s' property."), *ForLoopCountPinName.ToString());;
			}
			else
			{
				*OutErrorMessage = FString::Printf(TEXT("Struct requires a '%s' property of type '%s'."), *ForLoopCountPinName.ToString(), *ExpectedType);;
			}
		}
		return false;
	}

	return true;
}

bool FRigVMStruct::CheckMetadata(UScriptStruct* InStruct, const FName& PinName, const FName& InMetadataKey, FString* OutErrorMessage)
{
	if (FProperty* Property = InStruct->FindPropertyByName(PinName))
	{
		if (!Property->HasMetaData(InMetadataKey))
		{
			if (OutErrorMessage)
			{
				*OutErrorMessage = FString::Printf(TEXT("Property '%s' requires a '%s' metadata tag."), *PinName.ToString(), *InMetadataKey.ToString());;
			}
			return false;
		}
	}
	else
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = FString::Printf(TEXT("Struct requires a '%s' property."), *PinName.ToString());;
		}
		return false;
	}

	return true;
}

bool FRigVMStruct::CheckFunctionExists(UScriptStruct* InStruct, const FName& FunctionName, FString* OutErrorMessage)
{
	FString Key = FString::Printf(TEXT("%s::%s"), *InStruct->GetStructCPPName(), *FunctionName.ToString());
	if (FRigVMRegistry::Get().FindFunction(*Key) == nullptr)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = FString::Printf(TEXT("Function '%s' not found, required for this type of struct."), *Key);
		}
		return false;
	}
	return true;
}

ERigVMPinDirection FRigVMStruct::GetPinDirectionFromProperty(FProperty* InProperty)
{
	bool bIsInput = InProperty->HasMetaData(InputMetaName);
	bool bIsOutput = InProperty->HasMetaData(OutputMetaName);
	bool bIsVisible = InProperty->HasMetaData(VisibleMetaName);

	if (bIsVisible)
	{
		return ERigVMPinDirection::Visible;
	}
	
	if (bIsInput)
	{
		return bIsOutput ? ERigVMPinDirection::IO : ERigVMPinDirection::Input;
	} 
	
	if(bIsOutput)
	{
		return ERigVMPinDirection::Output;
	}

	return ERigVMPinDirection::Hidden;
}

#endif

FString FRigVMStruct::ExportToFullyQualifiedText(const FProperty* InMemberProperty, const uint8* InMemberMemoryPtr, bool bUseQuotes)
{
	check(InMemberProperty);
	check(InMemberMemoryPtr);

	FString DefaultValue;

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(InMemberProperty))
	{
		DefaultValue = ExportToFullyQualifiedText(StructProperty->Struct, InMemberMemoryPtr);
	}
	else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InMemberProperty))
	{
		FScriptArrayHelper ScriptArrayHelper(ArrayProperty, InMemberMemoryPtr);

		TArray<FString> ElementValues;
		for (int32 ElementIndex = 0; ElementIndex < ScriptArrayHelper.Num(); ElementIndex++)
		{
			const uint8* ElementMemoryPtr = ScriptArrayHelper.GetRawPtr(ElementIndex);
			ElementValues.Add(ExportToFullyQualifiedText(ArrayProperty->Inner, ElementMemoryPtr));
		}

		if (ElementValues.Num() == 0)
		{
			DefaultValue = TEXT("()");
		}
		else
		{
			DefaultValue = FString::Printf(TEXT("(%s)"), *FString::Join(ElementValues, TEXT(",")));
		}
	}
	else
	{
		InMemberProperty->ExportTextItem_Direct(DefaultValue, InMemberMemoryPtr, nullptr, nullptr, PPF_None);

		if (CastField<FNameProperty>(InMemberProperty) != nullptr ||
			CastField<FStrProperty>(InMemberProperty) != nullptr)
		{
			if(bUseQuotes)
			{
				if (DefaultValue.IsEmpty())
				{
					DefaultValue = TEXT("\"\"");
				}
				else
				{
					DefaultValue = FString::Printf(TEXT("\"%s\""), *DefaultValue);
				}
			}
		}
	}

	return DefaultValue;
}

FString FRigVMStruct::ExportToFullyQualifiedText(const UScriptStruct* InStruct, const uint8* InStructMemoryPtr, bool bUseQuotes)
{
	check(InStruct);
	check(InStructMemoryPtr);

	TArray<FString> FieldValues;
	for (TFieldIterator<FProperty> It(InStruct); It; ++It)
	{
		if(It->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}
		
		FString PropertyName = It->GetName();
		const uint8* StructMemberMemoryPtr = It->ContainerPtrToValuePtr<uint8>(InStructMemoryPtr);
		FString DefaultValue = ExportToFullyQualifiedText(*It, StructMemberMemoryPtr, bUseQuotes);
		FieldValues.Add(FString::Printf(TEXT("%s=%s"), *PropertyName, *DefaultValue));
	}

	if (FieldValues.Num() == 0)
	{
		return TEXT("()");
	}

	return FString::Printf(TEXT("(%s)"), *FString::Join(FieldValues, TEXT(",")));
}

FString FRigVMStruct::ExportToFullyQualifiedText(const UScriptStruct* InScriptStruct, const FName& InPropertyName, const uint8* InStructMemoryPointer, bool bUseQuotes) const
{
	check(InScriptStruct);
	if(InStructMemoryPointer == nullptr)
	{
		InStructMemoryPointer = (const uint8*)this;
	}
	
	const FProperty* Property = InScriptStruct->FindPropertyByName(InPropertyName);
	if(Property == nullptr)
	{
		return FString();
	}

	const uint8* StructMemberMemoryPtr = Property->ContainerPtrToValuePtr<uint8>(InStructMemoryPointer);
	return ExportToFullyQualifiedText(Property, StructMemberMemoryPtr, bUseQuotes);
}

FName FRigVMStruct::GetNextAggregateName(const FName& InLastAggregatePinName) const
{
	if(InLastAggregatePinName.IsNone())
	{
		return InLastAggregatePinName;
	}

	const FString PinName = InLastAggregatePinName.ToString();
	if(PinName.IsNumeric())
	{
		const int32 Index = FCString::Atoi(*PinName);
		return *FString::FormatAsNumber(Index+1);
	}

	if(PinName.Len() == 1)
	{
		const TCHAR C = PinName[0];
		if((C >='a' && C < 'z') || (C >='A' && C <'Z'))
		{
			FString Result;
			Result.AppendChar(C + 1);
			return *Result;
		}
	}

	if(PinName.Contains(TEXT("_")))
	{
		const FString Left = PinName.Left(PinName.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd));
		const FString Right = PinName.Mid(PinName.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)+1);
		return *FString::Printf(TEXT("%s_%s"), *Left, *GetNextAggregateName(*Right).ToString());
	}

	return *FString::Printf(TEXT("%s_1"), *PinName);
}

TMap<FName, FString> FRigVMStruct::GetDefaultValues(UScriptStruct* InScriptStruct) const
{
	check(InScriptStruct);
	
	TMap<FName, FString> DefaultValues;
	for (TFieldIterator<FProperty> It(InScriptStruct); It; ++It)
	{
		if(It->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

#if WITH_EDITOR
		if(!It->HasMetaData(FRigVMStruct::InputMetaName) &&
			!It->HasMetaData(FRigVMStruct::OutputMetaName) &&
			!It->HasMetaData(FRigVMStruct::VisibleMetaName))
		{
			continue;
		}
#endif
		
		const FName PropertyName = It->GetFName();
		const uint8* StructMemberMemoryPtr = It->ContainerPtrToValuePtr<uint8>(this);
		const FString DefaultValue = ExportToFullyQualifiedText(*It, StructMemberMemoryPtr, false);
		DefaultValues.Add(PropertyName, DefaultValue);
	}

	return DefaultValues;
}

class FRigVMStructApplyUpgradeInfoErrorContext : public FOutputDevice
{
public:

	int32 NumErrors;
	UScriptStruct* OldStruct;
	UScriptStruct* NewStruct;
	FName PropertyName;

	FRigVMStructApplyUpgradeInfoErrorContext(UScriptStruct* InOldStruct, UScriptStruct* InNewStruct, const FName& InPropertyName)
		: FOutputDevice()
		, NumErrors(0)
		, OldStruct(InOldStruct)
		, NewStruct(InNewStruct)
		, PropertyName(InPropertyName)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if(Verbosity == ELogVerbosity::Error)
		{
			UE_LOG(LogRigVM, Warning, TEXT("Error when applying upgrade data from %s to %s, property %s: %s"),
				*OldStruct->GetStructCPPName(),
				*NewStruct->GetStructCPPName(),
				*PropertyName.ToString(),
				V);
			NumErrors++;
		}
		else if(Verbosity == ELogVerbosity::Warning)
		{
			UE_LOG(LogRigVM, Warning, TEXT("Warning when applying upgrade data from %s to %s, property %s: %s"),
				*OldStruct->GetStructCPPName(),
				*NewStruct->GetStructCPPName(),
				*PropertyName.ToString(),
				V);
		}
	}
};

bool FRigVMStruct::ApplyUpgradeInfo(const FRigVMStructUpgradeInfo& InUpgradeInfo)
{
	check(InUpgradeInfo.IsValid());

	for(const TPair<FName, FString>& Pair : InUpgradeInfo.GetDefaultValues())
	{
		const FName& PropertyName = Pair.Key;
		const FString& DefaultValue = Pair.Value;
		
		const FProperty* Property = InUpgradeInfo.GetNewStruct()->FindPropertyByName(PropertyName);
		if(Property == nullptr)
		{
			return false;
		}

		uint8* MemberMemory = Property->ContainerPtrToValuePtr<uint8>(this);

		FRigVMStructApplyUpgradeInfoErrorContext ErrorPipe(InUpgradeInfo.GetOldStruct(), InUpgradeInfo.GetNewStruct(), PropertyName);
        Property->ImportText_Direct(*DefaultValue, MemberMemory, nullptr, PPF_None, &ErrorPipe);

		if(ErrorPipe.NumErrors > 0)
		{
			return false;
		}
	}

	return true;
}

