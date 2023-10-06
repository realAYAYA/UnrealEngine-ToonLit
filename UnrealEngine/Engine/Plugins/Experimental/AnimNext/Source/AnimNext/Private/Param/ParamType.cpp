// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"
#include "Misc/StringBuilder.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"

FAnimNextParamType::FAnimNextParamType(EValueType InValueType, EContainerType InContainerType, const UObject* InValueTypeObject)
	: ValueTypeObject(InValueTypeObject)
	, ValueType(InValueType)
	, ContainerType(InContainerType)
{
}

bool FAnimNextParamType::IsValidObject() const
{
	switch(ValueType)
	{
	default:
	case EValueType::None:
	case EValueType::Bool:
	case EValueType::Byte:
	case EValueType::Int32:
	case EValueType::Int64:
	case EValueType::Float:
	case EValueType::Double:
	case EValueType::Name:
	case EValueType::String:
	case EValueType::Text:
		return false;
	case EValueType::Enum:
		{
			const UObject* ResolvedObject = ValueTypeObject.Get();
			return ResolvedObject && ResolvedObject->IsA(UEnum::StaticClass());
		}
	case EValueType::Struct:
		{
			const UObject* ResolvedObject = ValueTypeObject.Get();
			return ResolvedObject && ResolvedObject->IsA(UScriptStruct::StaticClass());
		}
	case EValueType::Object:
	case EValueType::SoftObject:
	case EValueType::Class:
	case EValueType::SoftClass:
		{
			const UObject* ResolvedObject = ValueTypeObject.Get();
			return ResolvedObject && ResolvedObject->IsA(UClass::StaticClass());
		}
	}
}

UE::AnimNext::FParamTypeHandle FAnimNextParamType::GetHandle() const
{
	using namespace UE::AnimNext;

	FParamTypeHandle Handle;
	
	switch(ContainerType)
	{
	case EPropertyBagContainerType::None:
		switch(ValueType)
		{
		default:
		case EValueType::None:
			Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			break;
		case EValueType::Bool:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Bool);
			break;
		case EValueType::Byte:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Byte);
			break;
		case EValueType::Int32:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Int32);
			break;
		case EValueType::Int64:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Int64);
			break;
		case EValueType::Float:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Float);
			break;
		case EValueType::Double:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Double);
			break;
		case EValueType::Name:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Name);
			break;
		case EValueType::String:
			Handle.SetParameterType(FParamTypeHandle::EParamType::String);
			break;
		case EValueType::Text:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Text);
			break;
		case EValueType::Struct:
			if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ValueTypeObject.Get()))
			{
				if(ScriptStruct == TBaseStructure<FVector>::Get())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::Vector);
				}
				else if(ScriptStruct == TBaseStructure<FVector4>::Get())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::Vector4);
				}
				else if(ScriptStruct == TBaseStructure<FQuat>::Get())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::Quat);
				}
				else if(ScriptStruct == TBaseStructure<FTransform>::Get())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::Transform);
				}
				else
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::Custom);
					Handle.SetCustomTypeIndex(FParamTypeHandle::GetOrAllocateCustomTypeIndex(EValueType::Struct, EContainerType::None, ScriptStruct));
				}
			}
			else
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			}
			break;
		case EValueType::Enum:
			if(const UEnum* Enum = Cast<UEnum>(ValueTypeObject.Get()))
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::Custom);
				Handle.SetCustomTypeIndex(FParamTypeHandle::GetOrAllocateCustomTypeIndex(ValueType, EContainerType::None, Enum));
			}
			else
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			}
			break;
		case EValueType::Object:
		case EValueType::SoftObject:
		case EValueType::Class:
		case EValueType::SoftClass:
			if(const UClass* Class = Cast<UClass>(ValueTypeObject.Get()))
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::Custom);
				Handle.SetCustomTypeIndex(FParamTypeHandle::GetOrAllocateCustomTypeIndex(ValueType, EContainerType::None, Class));
			}
			else
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			}
			break;
		}
		break;
	case EPropertyBagContainerType::Array:
		switch(ValueType)
		{
		default:
		case EValueType::None:
			Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			break;
		case EValueType::Bool:
		case EValueType::Byte:
		case EValueType::Int32:
		case EValueType::Int64:
		case EValueType::Float:
		case EValueType::Double:
		case EValueType::Name:
		case EValueType::String:
		case EValueType::Text:
			Handle.SetParameterType(FParamTypeHandle::EParamType::Custom);
			Handle.SetCustomTypeIndex(FParamTypeHandle::GetOrAllocateCustomTypeIndex(ValueType, ContainerType, nullptr));
			break;
		case EValueType::Struct:
			if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ValueTypeObject.Get()))
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::Custom);
				Handle.SetCustomTypeIndex(FParamTypeHandle::GetOrAllocateCustomTypeIndex(ValueType, ContainerType, ScriptStruct));
			}
			else
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			}
			break;
		case EValueType::Enum:
			if(const UEnum* Enum = Cast<UEnum>(ValueTypeObject.Get()))
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::Custom);
				Handle.SetCustomTypeIndex(FParamTypeHandle::GetOrAllocateCustomTypeIndex(ValueType, ContainerType, Enum));
			}
			else
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			}
			break;
		case EValueType::Object:
		case EValueType::SoftObject:
		case EValueType::Class:
		case EValueType::SoftClass:
			if(const UClass* Class = Cast<UClass>(ValueTypeObject.Get()))
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::Custom);
				Handle.SetCustomTypeIndex(FParamTypeHandle::GetOrAllocateCustomTypeIndex(ValueType, ContainerType, Class));
			}
			else
			{
				Handle.SetParameterType(FParamTypeHandle::EParamType::None);
			}
			break;
		}
		break;
	default:
		break;
	}

	return Handle;
}

size_t FAnimNextParamType::GetSize() const
{
	switch(ContainerType)
	{
	case EContainerType::None:
		return GetValueTypeSize();
	case EContainerType::Array:
		return sizeof(TArray<uint8>);
	default:
		break;
	}

	checkf(false, TEXT("Error: FParameterType::GetSize: Unknown Type Container %d, Value %d"), ContainerType, ValueType);
	return 0;
}

size_t FAnimNextParamType::GetValueTypeSize() const
{
	switch(ValueType)
	{
	case EValueType::None:
		return 0;
	case EValueType::Bool:
		return sizeof(bool);
	case EValueType::Byte:
		return sizeof(uint8);
	case EValueType::Int32:
		return sizeof(int32);
	case EValueType::Int64:
		return sizeof(int64);
	case EValueType::Float:
		return sizeof(float);
	case EValueType::Double:
		return sizeof(double);
	case EValueType::Name:
		return sizeof(FName);
	case EValueType::String:
		return sizeof(FString);
	case EValueType::Text:
		return sizeof(FText);
	case EValueType::Enum:
		// TODO: seems to be no way to find the size of a UEnum?
		return sizeof(uint8);
	case EValueType::Struct:
		if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ValueTypeObject.Get()))
		{
			return ScriptStruct->GetStructureSize();
		}
		else
		{
			checkf(false, TEXT("Error: FParameterType::GetValueTypeSize: Unknown Struct Type"));
			return 0;
		}
	case EValueType::Object:
		return sizeof(TObjectPtr<UObject>);
	case EValueType::SoftObject:
		return sizeof(TSoftObjectPtr<UObject>);
	case EValueType::Class:
		return sizeof(TSubclassOf<UObject>);
	case EValueType::SoftClass:
		return sizeof(TSoftClassPtr<UObject>);
	default:
		break;
	}

	checkf(false, TEXT("Error: FParameterType::GetValueTypeSize: Unknown Type Container %d, Value %d"), ContainerType, ValueType);
	return 0;
}

size_t FAnimNextParamType::GetAlignment() const
{
	switch(ContainerType)
	{
	case EContainerType::None:
		return GetValueTypeAlignment();
	case EContainerType::Array:
		return sizeof(TArray<uint8>);
	default:
		break;
	}

	checkf(false, TEXT("Error: FParameterType::GetAlignment: Unknown Type: Container %d, Value %d"), ContainerType, ValueType);
	return 0;
}

size_t FAnimNextParamType::GetValueTypeAlignment() const
{
	switch(ValueType)
	{
	case EValueType::None:
		return 0;
	case EValueType::Bool:
		return alignof(bool);
	case EValueType::Byte:
		return alignof(uint8);
	case EValueType::Int32:
		return alignof(int32);
	case EValueType::Int64:
		return alignof(int64);
	case EValueType::Float:
		return alignof(float);
	case EValueType::Double:
		return alignof(double);
	case EValueType::Name:
		return alignof(FName);
	case EValueType::String:
		return alignof(FString);
	case EValueType::Text:
		return alignof(FText);
	case EValueType::Enum:
		// TODO: seems to be no way to find the alignment of a UEnum?
		return alignof(uint8);
	case EValueType::Struct:
		if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ValueTypeObject.Get()))
		{
			return ScriptStruct->GetMinAlignment();
		}
		else
		{
			checkf(false, TEXT("Error: FParameterType::GetValueTypeAlignment: Unknown Struct Type"));
			return 0;
		}
	case EValueType::Object:
		return alignof(TObjectPtr<UObject>);
	case EValueType::SoftObject:
		return alignof(TSoftObjectPtr<UObject>);
	case EValueType::Class:
		return alignof(TSubclassOf<UObject>);
	case EValueType::SoftClass:
		return alignof(TSoftClassPtr<UObject>);
	default:
		break;
	}

	checkf(false, TEXT("Error: FParameterType::GetValueTypeAlignment: Unknown Type: Container %d, Value %d"), ContainerType, ValueType);
	return 0;
}

FString FAnimNextParamType::ToString() const
{
	auto GetTypeString = [this](TStringBuilder<128>& InStringBuilder)
	{
		switch(ValueType)
		{
		case EValueType::None:
			InStringBuilder.Append(TEXT("None"));
			break;
		case EValueType::Bool:
			InStringBuilder.Append(TEXT("bool"));
			break;
		case EValueType::Byte:
			InStringBuilder.Append(TEXT("uint8"));
			break;
		case EValueType::Int32:
			InStringBuilder.Append(TEXT("int32"));
			break;
		case EValueType::Int64:
			InStringBuilder.Append(TEXT("int64"));
			break;
		case EValueType::Float:
			InStringBuilder.Append(TEXT("float"));
			break;
		case EValueType::Double:
			InStringBuilder.Append(TEXT("double"));
			break;
		case EValueType::Name:
			InStringBuilder.Append(TEXT("FName"));
			break;
		case EValueType::String:
			InStringBuilder.Append(TEXT("FString"));
			break;
		case EValueType::Text:
			InStringBuilder.Append(TEXT("FText"));
			break;
		case EValueType::Enum:
			if(const UEnum* Enum = Cast<UEnum>(ValueTypeObject.Get()))
			{
				InStringBuilder.Append(TEXT("U"));
				InStringBuilder.Append(Enum->GetName());
			}
			else
			{
				InStringBuilder.Append(TEXT("Error: Unknown Enum"));
			}
			break;
		case EValueType::Struct:
			if(const UScriptStruct* Struct = Cast<UScriptStruct>(ValueTypeObject.Get()))
			{
				InStringBuilder.Append(TEXT("F"));
				InStringBuilder.Append(Struct->GetName());
			}
			else
			{
				InStringBuilder.Append(TEXT("Error: Unknown Struct"));
			}
			break;
		case EValueType::Object:
			if(const UClass* Class = Cast<UClass>(ValueTypeObject.Get()))
			{
				InStringBuilder.Append(TEXT("TObjectPtr<U"));
				InStringBuilder.Append(Class->GetName());
				InStringBuilder.Append(TEXT(">"));
			}
			else
			{
				InStringBuilder.Append(TEXT("Error: TObjectPtr of Unknown Class"));
			}
			break;
		case EValueType::SoftObject:
			if(const UClass* Class = Cast<UClass>(ValueTypeObject.Get()))
			{
				InStringBuilder.Append(TEXT("TSoftObjectPtr<U"));
				InStringBuilder.Append(Class->GetName());
				InStringBuilder.Append(TEXT(">"));
			}
			else
			{
				InStringBuilder.Append(TEXT("Error: TSoftObjectPtr of Unknown Class"));
			}
			break;
		case EValueType::Class:
			if(const UClass* Class = Cast<UClass>(ValueTypeObject.Get()))
			{
				InStringBuilder.Append(TEXT("TSubClassOf<U"));
				InStringBuilder.Append(Class->GetName());
				InStringBuilder.Append(TEXT(">"));
			}
			else
			{
				InStringBuilder.Append(TEXT("Error: TSubClassOf of Unknown Class"));
			}
			break;
		case EValueType::SoftClass:
			if(const UClass* Class = Cast<UClass>(ValueTypeObject.Get()))
			{
				InStringBuilder.Append(TEXT("TSoftClassPtr<U"));
				InStringBuilder.Append(Class->GetName());
				InStringBuilder.Append(TEXT(">"));
			}
			else
			{
				InStringBuilder.Append(TEXT("Error: TSoftClassPtr of Unknown Class"));
			}
			break;
		default:
			InStringBuilder.Append(TEXT("Error: Unknown value type"));
			break;
		}
	};

	TStringBuilder<128> StringBuilder;

	switch(ContainerType)
	{
	case EContainerType::None:
		GetTypeString(StringBuilder);
		break;
	case EContainerType::Array:
		{
			StringBuilder.Append(TEXT("TArray<"));
			GetTypeString(StringBuilder);
			StringBuilder.Append(TEXT(">"));
		}
		break;
	default:
		StringBuilder.Append(TEXT("Error: Unknown container type"));
		break;
	}

	return StringBuilder.ToString();
}
