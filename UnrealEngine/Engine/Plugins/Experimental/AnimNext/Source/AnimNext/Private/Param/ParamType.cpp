// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamType.h"

#include "Component/AnimNextMeshComponent.h"
#include "Graph/AnimNext_LODPose.h"
#include "Param/ParamTypeHandle.h"
#include "Misc/StringBuilder.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "RigVMCore/RigVMTemplate.h"

FAnimNextParamType::FAnimNextParamType(EValueType InValueType, EContainerType InContainerType, const UObject* InValueTypeObject)
	: ValueTypeObject(InValueTypeObject)
	, ValueType(InValueType)
	, ContainerType(InContainerType)
{
}

FAnimNextParamType FAnimNextParamType::FromRigVMTemplateArgument(const FRigVMTemplateArgumentType& RigVMType)
{
	FAnimNextParamType Type;	
	const FString CPPTypeString = RigVMType.CPPType.ToString();
	
	if (RigVMTypeUtils::IsArrayType(CPPTypeString))
	{
		Type.ContainerType = EPropertyBagContainerType::Array;
	}

	static const FName IntTypeName(TEXT("int")); // type used by some engine tests
	static const FName Int64TypeName(TEXT("Int64"));
	static const FName UInt64TypeName(TEXT("UInt64"));

	const FName CPPType = *CPPTypeString;
	
	if (CPPType == RigVMTypeUtils::BoolTypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::Bool;
	}
	else if (CPPType == RigVMTypeUtils::Int32TypeName || CPPType == IntTypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::Int32;
	}
	else if (CPPType == RigVMTypeUtils::UInt32TypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::UInt32;
	}
	else if (CPPType == Int64TypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::Int64;
	}
	else if (CPPType == UInt64TypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::UInt64;
	}
	else if (CPPType == RigVMTypeUtils::FloatTypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::Float;
	}
	else if (CPPType == RigVMTypeUtils::DoubleTypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::Double;
	}
	else if (CPPType == RigVMTypeUtils::FNameTypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::Name;
	}
	else if (CPPType == RigVMTypeUtils::FStringTypeName)
	{
		Type.ValueType = EPropertyBagPropertyType::String;
	}
	else if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(RigVMType.CPPTypeObject))
	{
		Type.ValueType = EPropertyBagPropertyType::Struct;
		Type.ValueTypeObject = ScriptStruct;
	}
	else if (UEnum* Enum = Cast<UEnum>(RigVMType.CPPTypeObject))
	{
		Type.ValueType = EPropertyBagPropertyType::Enum;
		Type.ValueTypeObject = Enum;
	}
	else if (UObject* Object = Cast<UObject>(RigVMType.CPPTypeObject))
	{
		Type.ValueType = EPropertyBagPropertyType::Object;	
		Type.ValueTypeObject = Object;
	}
	else
	{
		ensureMsgf(false, TEXT("Unsupported type : %s"), *CPPTypeString);
		Type.ValueType = EPropertyBagPropertyType::None;
	}

	return Type;
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
				else if(ScriptStruct == FAnimNextGraphLODPose::StaticStruct())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::AnimNextGraphLODPose);
				}
				else if(ScriptStruct == FAnimNextGraphReferencePose::StaticStruct())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::AnimNextGraphReferencePose);
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
			if(const UClass* Class = Cast<UClass>(ValueTypeObject.Get()))
			{
				if (Class == UObject::StaticClass())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::Object);
					break;
				}
				else if (Class == UCharacterMovementComponent::StaticClass())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::CharacterMovementComponent);
					break;
				}
				else if (Class == UAnimNextMeshComponent::StaticClass())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::AnimNextMeshComponent);
					break;
				}
				else if (Class == UAnimSequence::StaticClass())
				{
					Handle.SetParameterType(FParamTypeHandle::EParamType::AnimSequence);
					break;
				}
			}
			// fall through
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

void FAnimNextParamType::ToString(FStringBuilderBase& InStringBuilder) const
{
	auto GetTypeString = [this, &InStringBuilder]()
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

	switch(ContainerType)
	{
	case EContainerType::None:
		GetTypeString();
		break;
	case EContainerType::Array:
		{
			InStringBuilder.Append(TEXT("TArray<"));
			GetTypeString();
			InStringBuilder.Append(TEXT(">"));
		}
		break;
	default:
		InStringBuilder.Append(TEXT("Error: Unknown container type"));
		break;
	}
}

FString FAnimNextParamType::ToString() const
{
	TStringBuilder<128> StringBuilder;
	ToString(StringBuilder);
	return StringBuilder.ToString();
}

FAnimNextParamType FAnimNextParamType::FromString(const FString& InString)
{
	auto GetInnerType = [](const FString& InTypeString, FAnimNextParamType& OutType)
	{
		static TMap<FString, FAnimNextParamType> BasicTypes =
		{
			{ TEXT("bool"),		GetType<bool>() },
			{ TEXT("uint8"),		GetType<uint8>() },
			{ TEXT("int32"),		GetType<int32>() },
			{ TEXT("int64"),		GetType<int64>() },
			{ TEXT("float"),		GetType<float>() },
			{ TEXT("double"),		GetType<double>() },
			{ TEXT("FName"),		GetType<FName>() },
			{ TEXT("FString"),		GetType<FString>() },
			{ TEXT("FText"),		GetType<FText>() },
			{ TEXT("uint32"),		GetType<uint32>() },
			{ TEXT("uint64"),		GetType<uint64>() },
		};

		if(const FAnimNextParamType* BasicType = BasicTypes.Find(InTypeString))
		{
			OutType = *BasicType;
			return true;
		}

		// Check for object/struct/enum
		EValueType ObjectValueType = EValueType::None;
		FString ObjectInnerString;
		if(InTypeString.StartsWith(TEXT("U"), ESearchCase::CaseSensitive))
		{
			ObjectInnerString = InTypeString.RightChop(1).TrimStartAndEnd();
			ObjectValueType = EValueType::Object;
		}
		else if(InTypeString.StartsWith(TEXT("TObjectPtr<U"), ESearchCase::CaseSensitive))
		{
			ObjectInnerString = InTypeString.RightChop(12).LeftChop(1).TrimStartAndEnd();
			ObjectValueType = EValueType::Object;
		}
		else if(InTypeString.StartsWith(TEXT("TSubClassOf<U"), ESearchCase::CaseSensitive))
		{
			ObjectInnerString = InTypeString.RightChop(13).LeftChop(1).TrimStartAndEnd();
			ObjectValueType = EValueType::Class;
		}
		else if(InTypeString.StartsWith(TEXT("F"), ESearchCase::CaseSensitive))
		{
			ObjectInnerString = InTypeString.RightChop(1).TrimStartAndEnd();
			ObjectValueType = EValueType::Struct;
		}
		else if(InTypeString.StartsWith(TEXT("E"), ESearchCase::CaseSensitive))
		{
			ObjectInnerString = InTypeString.RightChop(1).TrimStartAndEnd();
			ObjectValueType = EValueType::Enum;
		}
		
		if(UObject* ObjectType = FindFirstObject<UObject>(*ObjectInnerString, EFindFirstObjectOptions::NativeFirst))
		{
			OutType.ValueType = ObjectValueType;
			OutType.ValueTypeObject = nullptr;

			switch(ObjectValueType)
			{
			case EPropertyBagPropertyType::Enum:
				OutType.ValueTypeObject = Cast<UEnum>(ObjectType);
				break;
			case EPropertyBagPropertyType::Struct:
				OutType.ValueTypeObject = Cast<UScriptStruct>(ObjectType);
				break;
			case EPropertyBagPropertyType::Object:
				OutType.ValueTypeObject = Cast<UClass>(ObjectType);
				break;
			case EPropertyBagPropertyType::Class:
				OutType.ValueTypeObject = Cast<UClass>(ObjectType);
				break;
			default:
				break;
			}

			return OutType.ValueTypeObject != nullptr;
		}

		return false;
	};

	{
		FAnimNextParamType Type;
		if(GetInnerType(InString, Type))
		{
			return Type;
		}
	}
	
	if(InString.StartsWith(TEXT("TArray<"), ESearchCase::CaseSensitive))
	{
		const FString InnerTypeString = InString.RightChop(7).LeftChop(1).TrimStartAndEnd();
		FAnimNextParamType Type;
		if(GetInnerType(InnerTypeString, Type))
		{
			Type.ContainerType = EContainerType::Array;
			return Type;
		}
	}

	return FAnimNextParamType();
}

bool FAnimNextParamType::IsObjectType() const
{
	switch (ValueType)
	{
	case EValueType::Object:
	case EValueType::SoftObject:
	case EValueType::Class:
	case EValueType::SoftClass:
		return true;
	default:
		return false;
	}
}