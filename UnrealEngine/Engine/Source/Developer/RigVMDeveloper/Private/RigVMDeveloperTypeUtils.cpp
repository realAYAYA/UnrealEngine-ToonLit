// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMDeveloperTypeUtils.h"

#include "Internationalization/StringTableCore.h"
#include "RigVMModel/RigVMVariableDescription.h"

#if WITH_EDITOR

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"

FRigVMExternalVariable RigVMTypeUtils::ExternalVariableFromBPVariableDescription(
	const FBPVariableDescription& InVariableDescription)
{
	const bool bIsPublic = !((InVariableDescription.PropertyFlags & CPF_DisableEditOnInstance) == CPF_DisableEditOnInstance);
	const bool bIsReadOnly = ((InVariableDescription.PropertyFlags & CPF_BlueprintReadOnly) == CPF_BlueprintReadOnly);
	return ExternalVariableFromPinType(InVariableDescription.VarName, InVariableDescription.VarType, bIsPublic, bIsReadOnly);
}

FRigVMExternalVariable RigVMTypeUtils::ExternalVariableFromRigVMVariableDescription(const FRigVMGraphVariableDescription& InVariableDescription)
{	
	FRigVMExternalVariable ExternalVariable;
	ExternalVariable.Name = InVariableDescription.Name;

	if (IsArrayType(InVariableDescription.CPPType))
	{
		ExternalVariable.bIsArray = true;
		ExternalVariable.TypeName = *InVariableDescription.CPPType.Mid(7, InVariableDescription.CPPType.Len() - 8);
		ExternalVariable.TypeObject = InVariableDescription.CPPTypeObject;
	}
	else
	{
		ExternalVariable.bIsArray = false;
		ExternalVariable.TypeName = *InVariableDescription.CPPType;
		ExternalVariable.TypeObject = InVariableDescription.CPPTypeObject;
	}

	ExternalVariable.bIsPublic = false;
	ExternalVariable.bIsReadOnly = false;
	ExternalVariable.Memory = nullptr;
	return ExternalVariable;

}

FRigVMExternalVariable RigVMTypeUtils::ExternalVariableFromPinType(const FName& InName, const FEdGraphPinType& InPinType, bool bInPublic, bool bInReadonly)
{
	FRigVMExternalVariable ExternalVariable;
	ExternalVariable.Name = InName;
	ExternalVariable.bIsPublic = bInPublic;
	ExternalVariable.bIsReadOnly = bInReadonly;

	if (InPinType.ContainerType == EPinContainerType::None)
	{
		ExternalVariable.bIsArray = false;
	}
	else if (InPinType.ContainerType == EPinContainerType::Array)
	{
		ExternalVariable.bIsArray = true;
	}
	else
	{
		return FRigVMExternalVariable();
	}

	if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		ExternalVariable.TypeName = BoolTypeName;
		ExternalVariable.Size = sizeof(bool);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		ExternalVariable.TypeName = Int32TypeName;
		ExternalVariable.Size = sizeof(int32);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Enum ||
		InPinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		if (UEnum* Enum = Cast<UEnum>(InPinType.PinSubCategoryObject))
		{
			ExternalVariable.TypeName = Enum->GetFName();
			ExternalVariable.TypeObject = Enum;
		}
		else
		{
			ExternalVariable.TypeName = UInt8TypeName;
		}
		ExternalVariable.Size = sizeof(uint8);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			ExternalVariable.TypeName = FloatTypeName;
			ExternalVariable.Size = sizeof(float);
		}
		else if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			ExternalVariable.TypeName = DoubleTypeName;
			ExternalVariable.Size = sizeof(double);
		}
		else
		{
			checkf(false, TEXT("Unexpected subcategory for PC_Real pin type."));
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		ExternalVariable.TypeName = FNameTypeName;
		ExternalVariable.Size = sizeof(FName);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		ExternalVariable.TypeName = FStringTypeName;
		ExternalVariable.Size = sizeof(FString);
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (UScriptStruct* Struct = Cast<UScriptStruct>(InPinType.PinSubCategoryObject))
		{
			ExternalVariable.TypeName = *RigVMTypeUtils::GetUniqueStructTypeName(Struct);
			ExternalVariable.TypeObject = Struct;
			ExternalVariable.Size = Struct->GetStructureSize();
		}
	}

	return ExternalVariable;
}

FRigVMExternalVariable RigVMTypeUtils::ExternalVariableFromCPPTypePath(const FName& InName, const FString& InCPPTypePath, bool bInPublic, bool bInReadonly)
{
	FRigVMExternalVariable Variable;
	if (InCPPTypePath.StartsWith(TEXT("TMap<")))
	{
		return Variable;
	}
	
	Variable.Name = InName;
	Variable.bIsPublic = bInPublic;
	Variable.bIsReadOnly = bInReadonly;

	FString CPPTypePath = InCPPTypePath;
	Variable.bIsArray = RigVMTypeUtils::IsArrayType(CPPTypePath);
	if (Variable.bIsArray)
	{
		CPPTypePath = BaseTypeFromArrayType(CPPTypePath);
	}

	if (CPPTypePath == BoolType)
	{
		Variable.TypeName = *CPPTypePath;
		Variable.Size = sizeof(bool);
	}
	else if (CPPTypePath == FloatType)
	{
		Variable.TypeName = *CPPTypePath;
		Variable.Size = sizeof(float);
	}
	else if (CPPTypePath == DoubleType)
	{
		Variable.TypeName = *CPPTypePath;
		Variable.Size = sizeof(double);
	}
	else if (CPPTypePath == Int32Type)
	{
		Variable.TypeName = *CPPTypePath;
		Variable.Size = sizeof(int32);
	}
	else if (CPPTypePath == FStringType)
	{
		Variable.TypeName = *CPPTypePath;
		Variable.Size = sizeof(FString);
	}
	else if (CPPTypePath == FNameType)
	{
		Variable.TypeName = *CPPTypePath;
		Variable.Size = sizeof(FName);
	}
	else if(UScriptStruct* ScriptStruct = URigVMPin::FindObjectFromCPPTypeObjectPath<UScriptStruct>(CPPTypePath))
	{
		Variable.TypeName = *RigVMTypeUtils::GetUniqueStructTypeName(ScriptStruct);
		Variable.TypeObject = ScriptStruct;
		Variable.Size = ScriptStruct->GetStructureSize();
	}
	else if (UEnum* Enum= URigVMPin::FindObjectFromCPPTypeObjectPath<UEnum>(CPPTypePath))
	{
		Variable.TypeName = *CPPTypeFromEnum(Enum);
		Variable.TypeObject = Enum;
		Variable.Size = Enum->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
	}
	else
	{
		check(false);
	}

	return Variable;
}

FRigVMExternalVariable RigVMTypeUtils::ExternalVariableFromCPPType(const FName& InName, const FString& InCPPType, UObject* InCPPTypeObject, bool bInPublic, bool bInReadonly)
{
	FRigVMExternalVariable Variable;
	if (InCPPType.StartsWith(TEXT("TMap<")))
	{
		return Variable;
	}
	
	Variable.Name = InName;
	Variable.bIsPublic = bInPublic;
	Variable.bIsReadOnly = bInReadonly;

	FString CPPType = InCPPType;
	Variable.bIsArray = RigVMTypeUtils::IsArrayType(CPPType);
	if (Variable.bIsArray)
	{
		CPPType = BaseTypeFromArrayType(CPPType);
	}

	if (CPPType == BoolType)
	{
		Variable.TypeName = *CPPType;
		Variable.Size = sizeof(bool);
	}
	else if (CPPType == FloatType)
	{
		Variable.TypeName = *CPPType;
		Variable.Size = sizeof(float);
	}
	else if (CPPType == DoubleType)
	{
		Variable.TypeName = *CPPType;
		Variable.Size = sizeof(double);
	}
	else if (CPPType == Int32Type)
	{
		Variable.TypeName = *CPPType;
		Variable.Size = sizeof(int32);
	}
	else if (CPPType == FStringType)
	{
		Variable.TypeName = *CPPType;
		Variable.Size = sizeof(FString);
	}
	else if (CPPType == FNameType)
	{
		Variable.TypeName = *CPPType;
		Variable.Size = sizeof(FName);
	}
	else if(UScriptStruct* ScriptStruct = URigVMPin::FindObjectFromCPPTypeObjectPath<UScriptStruct>(CPPType))
	{
		Variable.TypeName = *RigVMTypeUtils::GetUniqueStructTypeName(ScriptStruct);
		Variable.TypeObject = ScriptStruct;
		Variable.Size = ScriptStruct->GetStructureSize();
	}
	else if (UEnum* Enum= URigVMPin::FindObjectFromCPPTypeObjectPath<UEnum>(CPPType))
	{
		Variable.TypeName = *CPPTypeFromEnum(Enum);
		Variable.TypeObject = Enum;
		Variable.Size = Enum->GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal);
	}
	else
	{
		Variable.TypeName = *CPPType;
		Variable.TypeObject = InCPPTypeObject;
		Variable.Size = InCPPTypeObject->StaticClass()->GetStructureSize();
	}

	return Variable;
}

FEdGraphPinType RigVMTypeUtils::PinTypeFromCPPType(const FName& InCPPType, UObject* InCPPTypeObject)
{
	FEdGraphPinType PinType;
	PinType.ResetToDefaults();
	PinType.PinCategory = NAME_None;

	FName BaseCPPType = InCPPType;
	PinType.ContainerType = EPinContainerType::None;
	if (RigVMTypeUtils::IsArrayType(InCPPType.ToString()))
	{
		BaseCPPType = *RigVMTypeUtils::BaseTypeFromArrayType(InCPPType.ToString());
		PinType.ContainerType = EPinContainerType::Array;
	}

	if (BaseCPPType == BoolTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (BaseCPPType == Int32TypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (BaseCPPType == FloatTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (BaseCPPType == DoubleTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (BaseCPPType == FNameTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (BaseCPPType == FStringTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Cast<UScriptStruct>(InCPPTypeObject))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = InCPPTypeObject;
	}
	else if (Cast<UEnum>(InCPPTypeObject))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		PinType.PinSubCategoryObject = InCPPTypeObject;
	}

	return PinType;
}

FEdGraphPinType RigVMTypeUtils::PinTypeFromExternalVariable(const FRigVMExternalVariable& InExternalVariable)
{
	FEdGraphPinType PinType;
	PinType.ResetToDefaults();
	PinType.PinCategory = NAME_None;

	if (InExternalVariable.TypeName == BoolTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (InExternalVariable.TypeName == Int32TypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (InExternalVariable.TypeName == FloatTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (InExternalVariable.TypeName == DoubleTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (InExternalVariable.TypeName == FNameTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (InExternalVariable.TypeName == FStringTypeName)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Cast<UScriptStruct>(InExternalVariable.TypeObject))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = InExternalVariable.TypeObject;
	}
	else if (Cast<UEnum>(InExternalVariable.TypeObject))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		PinType.PinSubCategoryObject = InExternalVariable.TypeObject;
	}

	if (InExternalVariable.bIsArray)
	{
		PinType.ContainerType = EPinContainerType::Array;
	}
	else
	{
		PinType.ContainerType = EPinContainerType::None;
	}

	return PinType;
}

FEdGraphPinType RigVMTypeUtils::PinTypeFromRigVMVariableDescription(
	const FRigVMGraphVariableDescription& InVariableDescription)
{
	FEdGraphPinType PinType;
	PinType.ResetToDefaults();
	PinType.PinCategory = NAME_None;

	FString CurrentCPPType = InVariableDescription.CPPType;
	if (IsArrayType(CurrentCPPType))
	{
		PinType.ContainerType = EPinContainerType::Array;
		CurrentCPPType = BaseTypeFromArrayType(CurrentCPPType);
	}
	else
	{
		PinType.ContainerType = EPinContainerType::None;
	}

	if (CurrentCPPType == BoolType)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (CurrentCPPType == Int32Type)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (CurrentCPPType == FloatType)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
	}
	else if (CurrentCPPType == DoubleType)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (CurrentCPPType == FNameType)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (CurrentCPPType == FStringType)
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Cast<UScriptStruct>(InVariableDescription.CPPTypeObject))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = InVariableDescription.CPPTypeObject;
	}
	else if (Cast<UEnum>(InVariableDescription.CPPTypeObject))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		PinType.PinSubCategoryObject = InVariableDescription.CPPTypeObject;
	}
	else if (Cast<UClass>(InVariableDescription.CPPTypeObject))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = InVariableDescription.CPPTypeObject;
	}

	return PinType;
}

FEdGraphPinType RigVMTypeUtils::SubPinType(const FEdGraphPinType& InPinType, const FString& InSegmentPath)
{
	FEdGraphPinType Result;
	if (InSegmentPath.IsEmpty())
	{
		return InPinType;
	}

	if (InPinType.PinSubCategoryObject.IsValid())
	{
		if (UStruct* Struct = Cast<UStruct>(InPinType.PinSubCategoryObject))
		{
			int32 PartIndex = 0; 
			TArray<FString> Parts;
			if (!URigVMPin::SplitPinPath(InSegmentPath, Parts))
			{
				Parts.Add(InSegmentPath);
			}
			
			FProperty* Property = Struct->FindPropertyByName(*Parts[PartIndex++]);
			while (PartIndex < Parts.Num() && Property != nullptr)
			{
				if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					Property = ArrayProperty->Inner;
					PartIndex++;
					continue;
				}

				if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					Struct = StructProperty->Struct;
					Property = Struct->FindPropertyByName(*Parts[PartIndex++]);
					continue;
				}

				break;
			}

			if (PartIndex == Parts.Num() && Property)
			{
				UEdGraphSchema_K2::StaticClass()->GetDefaultObject<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, Result);
			}
		}
	}

	return Result;
}

bool RigVMTypeUtils::CPPTypeFromPin(URigVMPin* InPin, FString& OutCPPType, UObject** OutCPPTypeObject, bool bGetBaseCPPType)
{
	check(InPin);
	OutCPPType = InPin->GetCPPType();
	if(bGetBaseCPPType && IsArrayType(OutCPPType))
	{
		OutCPPType = BaseTypeFromArrayType(OutCPPType);
	}
	if(OutCPPTypeObject)
	{
		*OutCPPTypeObject = InPin->GetCPPTypeObject();
	}
	return true;
}

bool RigVMTypeUtils::CPPTypeFromPin(URigVMPin* InPin, FString& OutCPPType, FName& OutCPPTypeObjectPath, bool bGetBaseCPPType)
{
	OutCPPType = FString();
	OutCPPTypeObjectPath = NAME_None;
	UObject* CPPTypeObject = nullptr;
	if (RigVMTypeUtils::CPPTypeFromPin(InPin, OutCPPType, &CPPTypeObject, bGetBaseCPPType))
	{
		if (CPPTypeObject)
		{
			OutCPPTypeObjectPath = *CPPTypeObject->GetPathName();
		}
		return true;
	}
	return false;
}

bool RigVMTypeUtils::CPPTypeFromPin(URigVMPin* InPin, FString& OutCPPType, FString& OutCPPTypeObjectPath, bool bGetBaseCPPType)
{
	OutCPPType = FString();
	OutCPPTypeObjectPath.Reset();
	FName CPPTypeObjectPath(NAME_None);
	if (RigVMTypeUtils::CPPTypeFromPin(InPin, OutCPPType, CPPTypeObjectPath, bGetBaseCPPType))
	{
		if (!CPPTypeObjectPath.IsNone())
		{
			OutCPPTypeObjectPath = CPPTypeObjectPath.ToString();
		}
		return true;
	}
	return false;
}

bool RigVMTypeUtils::CPPTypeFromPinType(const FEdGraphPinType& InPinType, FString& OutCPPType, UObject** OutCPPTypeObject)
{
	FString Prefix = "";
	FString Suffix = "";
	if (InPinType.ContainerType == EPinContainerType::Array)
	{
		Prefix = TEXT("TArray<");
		Suffix = TEXT(">");
	}

	OutCPPType = FString();
	*OutCPPTypeObject = nullptr;
	if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		OutCPPType = Prefix + BoolType + Suffix;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		OutCPPType = Prefix + Int32Type + Suffix;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			OutCPPType = Prefix + FloatType + Suffix;
		}
		else if (InPinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			OutCPPType = Prefix + DoubleType + Suffix;
		}
		else
		{
			checkf(false, TEXT("Unexpected subcategory for PC_Real pin type."));
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Float)
	{
		OutCPPType = Prefix + FloatType + Suffix;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Double)
	{
		OutCPPType = Prefix + DoubleType + Suffix;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		OutCPPType = Prefix + FNameType + Suffix;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		OutCPPType = Prefix + FStringType + Suffix;
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (UScriptStruct* Struct = Cast<UScriptStruct>(InPinType.PinSubCategoryObject))
		{
			OutCPPType = Prefix + GetUniqueStructTypeName(Struct) + Suffix;
			*OutCPPTypeObject = Struct;
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Object ||
		InPinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject ||
		InPinType.PinCategory == UEdGraphSchema_K2::AllObjectTypes)
	{
		if (UClass* Class = Cast<UClass>(InPinType.PinSubCategoryObject))
		{
			OutCPPType = Prefix + FString::Printf(TEXT("TObjectPtr<%s%s>"), Class->GetPrefixCPP(), *Class->GetName()) + Suffix;
			*OutCPPTypeObject = Class;
		}
	}
	else if (InPinType.PinCategory == UEdGraphSchema_K2::PC_Byte ||
		InPinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
	{
		if (UEnum* Enum = Cast<UEnum>(InPinType.PinSubCategoryObject))
		{
			OutCPPType = Prefix + Enum->GetFName().ToString() + Suffix;
			*OutCPPTypeObject = Enum;
		}
		else
		{
			OutCPPType = Prefix + UInt8Type + Suffix;
		}
	}
	else
	{
		return false;
	}
		
	return true;
}

bool RigVMTypeUtils::CPPTypeFromPinType(const FEdGraphPinType& InPinType, FString& OutCPPType, FName& OutCPPTypeObjectPath)
{
	OutCPPType = FString();
	OutCPPTypeObjectPath = NAME_None;
	UObject* CPPTypeObject = nullptr;
	if (RigVMTypeUtils::CPPTypeFromPinType(InPinType, OutCPPType, &CPPTypeObject))
	{
		if (CPPTypeObject)
		{
			OutCPPTypeObjectPath = *CPPTypeObject->GetPathName();
			if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject))
			{
				FString StructName = GetUniqueStructTypeName(ScriptStruct);
				while (IsArrayType(OutCPPType))
				{
					OutCPPType = BaseTypeFromArrayType(OutCPPType);
					StructName = ArrayTypeFromBaseType(StructName);
				}
				OutCPPType = StructName;
			}
		}

		return true;
	}

	return false;
}

bool RigVMTypeUtils::CPPTypeFromPinType(const FEdGraphPinType& InPinType, FString& OutCPPType, FString& OutCPPTypeObjectPath)
{
	OutCPPType = FString();
	OutCPPTypeObjectPath.Reset();
	FName CPPTypeObjectPath(NAME_None);
	if (RigVMTypeUtils::CPPTypeFromPinType(InPinType, OutCPPType, CPPTypeObjectPath))
	{
		if(!CPPTypeObjectPath.IsNone())
		{
			OutCPPTypeObjectPath = CPPTypeObjectPath.ToString();
		}
		return true;
	}

	return false;
}

bool RigVMTypeUtils::CPPTypeFromExternalVariable(const FRigVMExternalVariable& InExternalVariable, FString& OutCPPType, UObject** OutCPPTypeObject)
{
	FString Prefix = "";
	FString Suffix = "";
	if (InExternalVariable.bIsArray)
	{
		Prefix = TEXT("TArray<");
		Suffix = TEXT(">");
	}

	*OutCPPTypeObject = nullptr;
	if (InExternalVariable.TypeName == BoolTypeName)
	{
		OutCPPType = Prefix + BoolType + Suffix;
	}
	else if (InExternalVariable.TypeName == Int32TypeName)
	{
		OutCPPType = Prefix + Int32Type + Suffix;
	}
	else if (InExternalVariable.TypeName == FloatTypeName)
	{
		OutCPPType = Prefix + FloatType + Suffix;
	}
	else if (InExternalVariable.TypeName == DoubleTypeName)
	{
		OutCPPType = Prefix + DoubleType + Suffix;
	}
	else if (InExternalVariable.TypeName == FNameTypeName)
	{
		OutCPPType = Prefix + FNameType + Suffix;
	}
	else if (InExternalVariable.TypeName == FStringTypeName)
	{
		OutCPPType = Prefix + FStringType + Suffix;
	}
	else if (UScriptStruct* Struct = Cast<UScriptStruct>(InExternalVariable.TypeObject))
	{
		OutCPPType = Prefix + *GetUniqueStructTypeName(Struct) + Suffix;
		*OutCPPTypeObject = Struct;	
	}
	else if (UEnum* Enum = Cast<UEnum>(InExternalVariable.TypeObject))
	{
		OutCPPType = Prefix + Enum->GetFName().ToString() + Suffix;
		*OutCPPTypeObject = Enum;
	}	
	else
	{
		check(false);
		return false;
	}
		
	return true;
}

bool RigVMTypeUtils::AreCompatible(const FName& InCPPTypeA, UObject* InCPPTypeObjectA, const FName& InCPPTypeB,	UObject* InCPPTypeObjectB)
{
	return AreCompatible(PinTypeFromCPPType(InCPPTypeA, InCPPTypeObjectA), PinTypeFromCPPType(InCPPTypeB, InCPPTypeObjectB));
}

bool RigVMTypeUtils::AreCompatible(const FRigVMExternalVariable& InTypeA, const FRigVMExternalVariable& InTypeB, const FString& InSegmentPathA, const FString& InSegmentPathB)
{
	return AreCompatible(PinTypeFromExternalVariable(InTypeA), PinTypeFromExternalVariable(InTypeB), InSegmentPathA, InSegmentPathB);
}

bool RigVMTypeUtils::AreCompatible(const FEdGraphPinType& InTypeA, const FEdGraphPinType& InTypeB, const FString& InSegmentPathA, const FString& InSegmentPathB)
{
	FEdGraphPinType SubPinTypeA = SubPinType(InTypeA, InSegmentPathA);
	FEdGraphPinType SubPinTypeB = SubPinType(InTypeB, InSegmentPathB);

	// We allow connectiongs between floats and doubles, while EdGraphSchema_K2 does not
	// Every other case is evaluated by UEdGraphSchema_K2::ArePinTypesCompatible
	if (SubPinTypeA.ContainerType == SubPinTypeB.ContainerType)
	{
		if ((SubPinTypeA.PinCategory == FloatTypeName && SubPinTypeB.PinCategory == DoubleTypeName) ||
			(SubPinTypeA.PinCategory == DoubleTypeName && SubPinTypeB.PinCategory == FloatTypeName))
		{
			return true;
		}
	}
	
	return UEdGraphSchema_K2::StaticClass()->GetDefaultObject<UEdGraphSchema_K2>()->ArePinTypesCompatible(SubPinTypeA, SubPinTypeB);	
}
#endif
