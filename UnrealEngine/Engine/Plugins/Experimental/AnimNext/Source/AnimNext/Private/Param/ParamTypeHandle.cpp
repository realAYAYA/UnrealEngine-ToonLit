// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamTypeHandle.h"
#include "Param/ParamType.h"
#include "Misc/ScopeRWLock.h"
#include "UObject/Class.h"
#include "UObject/TextProperty.h"

namespace UE::AnimNext
{

// Array of all non built-in types. Index into this array is (CustomTypeIndex - 1)
static TArray<FAnimNextParamType> GCustomTypes;

// Map of type to index in GNonBuiltInTypes array
static TMap<FAnimNextParamType, uint32> GTypeToIndexMap;

// RW lock for types array/map
FRWLock TypesLock;

void FParamTypeHandle::ResetCustomTypes()
{
	FRWScopeLock ScopeLock(TypesLock, SLT_Write);

	GCustomTypes.Empty();
	GTypeToIndexMap.Empty();
}

uint32 FParamTypeHandle::GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType InValueType, FAnimNextParamType::EContainerType InContainerType, const UObject* InValueTypeObject)
{
	FAnimNextParamType ParameterType;
	ParameterType.ValueTypeObject = InValueTypeObject;
	ParameterType.ValueType = InValueType;
	ParameterType.ContainerType = InContainerType;

	const uint32 Hash = GetTypeHash(ParameterType);

	// NOTE: It is tempting to try to acquire a only read lock here during the FindByHash, but doing so means that the
	// lazy-insert operation is no longer atomic as there is no way to upgrade a read lock into a write lock with
	// FRWScopeLock (@see FRWScopeLock comments), hence the write lock around the map/array access here.
	{
		// See if the type already exists in the map
		FRWScopeLock ScopeLock(TypesLock, SLT_Write);

		if(const uint32* IndexPtr = GTypeToIndexMap.FindByHash(Hash, ParameterType))
		{
			return *IndexPtr + 1;
		}

		// Add a new custom type
		const uint32 Index = GCustomTypes.Add(ParameterType);
		GTypeToIndexMap.AddByHash(Hash, ParameterType, Index);

		checkf((Index + 1) < (1 << 24), TEXT("FParamTypeHandle::GetCustomTypeIndex: Type index overflowed"));
		return Index + 1;
	}
}

bool FParamTypeHandle::ValidateCustomTypeIndex(uint32 InCustomTypeIndex)
{
	FRWScopeLock ScopeLock(TypesLock, SLT_ReadOnly);
	return GCustomTypes.IsValidIndex(InCustomTypeIndex - 1);
}

FParamTypeHandle FParamTypeHandle::FromPropertyBagPropertyDesc(const FPropertyBagPropertyDesc& Desc)
{
	FParamTypeHandle Handle;
	
	if (Desc.ContainerTypes.Num() <= 1)
	{
		switch(Desc.ContainerTypes.GetFirstContainerType())
		{
		case EPropertyBagContainerType::None:
			switch(Desc.ValueType)
			{
			default:
			case EPropertyBagPropertyType::None:
				Handle.SetParameterType(EParamType::None);
				break;
			case EPropertyBagPropertyType::Bool:
				Handle.SetParameterType(EParamType::Bool);
				break;
			case EPropertyBagPropertyType::Byte:
				Handle.SetParameterType(EParamType::Byte);
				break;
			case EPropertyBagPropertyType::Int32:
				Handle.SetParameterType(EParamType::Int32);
				break;
			case EPropertyBagPropertyType::Int64:
				Handle.SetParameterType(EParamType::Int64);
				break;
			case EPropertyBagPropertyType::Float:
				Handle.SetParameterType(EParamType::Float);
				break;
			case EPropertyBagPropertyType::Double:
				Handle.SetParameterType(EParamType::Double);
				break;
			case EPropertyBagPropertyType::Name:
				Handle.SetParameterType(EParamType::Name);
				break;
			case EPropertyBagPropertyType::String:
				Handle.SetParameterType(EParamType::String);
				break;
			case EPropertyBagPropertyType::Text:
				Handle.SetParameterType(EParamType::Text);
				break;
			case EPropertyBagPropertyType::Struct:
				if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Desc.ValueTypeObject.Get()))
				{
					if(ScriptStruct == TBaseStructure<FVector>::Get())
					{
						Handle.SetParameterType(EParamType::Vector);
					}
					else if(ScriptStruct == TBaseStructure<FVector4>::Get())
					{
						Handle.SetParameterType(EParamType::Vector4);
					}
					else if(ScriptStruct == TBaseStructure<FQuat>::Get())
					{
						Handle.SetParameterType(EParamType::Quat);
					}
					else if(ScriptStruct == TBaseStructure<FTransform>::Get())
					{
						Handle.SetParameterType(EParamType::Transform);
					}
					else
					{
						Handle.SetParameterType(EParamType::Custom);
						Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(Desc.ValueType, Desc.ContainerTypes[0], ScriptStruct));
					}
				}
				else
				{
					Handle.SetParameterType(EParamType::None);
				}
				break;
			case EPropertyBagPropertyType::Enum:
				if(const UEnum* Enum = Cast<UEnum>(Desc.ValueTypeObject.Get()))
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(Desc.ValueType, Desc.ContainerTypes[0], Enum));
				}
				else
				{
					Handle.SetParameterType(EParamType::None);
				}
				break;
			case EPropertyBagPropertyType::Object:
			case EPropertyBagPropertyType::SoftObject:
			case EPropertyBagPropertyType::Class:
			case EPropertyBagPropertyType::SoftClass:
				if(const UClass* Class = Cast<UClass>(Desc.ValueTypeObject.Get()))
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(Desc.ValueType, Desc.ContainerTypes[0], Class));
				}
				else
				{
					Handle.SetParameterType(EParamType::None);
				}
				break;
			}
			break;
		case EPropertyBagContainerType::Array:
			switch(Desc.ValueType)
			{
			default:
			case EPropertyBagPropertyType::None:
				Handle.SetParameterType(EParamType::None);
				break;
			case EPropertyBagPropertyType::Bool:
			case EPropertyBagPropertyType::Byte:
			case EPropertyBagPropertyType::Int32:
			case EPropertyBagPropertyType::Int64:
			case EPropertyBagPropertyType::Float:
			case EPropertyBagPropertyType::Double:
			case EPropertyBagPropertyType::Name:
			case EPropertyBagPropertyType::String:
			case EPropertyBagPropertyType::Text:
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(Desc.ValueType, Desc.ContainerTypes[0], nullptr));
				break;
			case EPropertyBagPropertyType::Struct:
				if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Desc.ValueTypeObject.Get()))
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(Desc.ValueType, Desc.ContainerTypes[0], ScriptStruct));
				}
				else
				{
					Handle.SetParameterType(EParamType::None);
				}
				break;
			case EPropertyBagPropertyType::Enum:
				if(const UEnum* Enum = Cast<UEnum>(Desc.ValueTypeObject.Get()))
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(Desc.ValueType, Desc.ContainerTypes[0], Enum));
				}
				else
				{
					Handle.SetParameterType(EParamType::None);
				}
				break;
			case EPropertyBagPropertyType::Object:
			case EPropertyBagPropertyType::SoftObject:
			case EPropertyBagPropertyType::Class:
			case EPropertyBagPropertyType::SoftClass:
				if(const UClass* Class = Cast<UClass>(Desc.ValueTypeObject.Get()))
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(Desc.ValueType, Desc.ContainerTypes[0], Class));
				}
				else
				{
					Handle.SetParameterType(EParamType::None);
				}
				break;
			}
			break;
		default:
			break;
		}
	}

	return Handle;
}

FParamTypeHandle FParamTypeHandle::FromProperty(const FProperty* InProperty)
{
	FParamTypeHandle Handle;

	if (InProperty->IsA<FBoolProperty>())
	{
		Handle.SetParameterType(EParamType::Bool);
	}
	else if (InProperty->IsA<FByteProperty>())
	{
		Handle.SetParameterType(EParamType::Byte);
	}
	else if (InProperty->IsA<FIntProperty>())
	{
		Handle.SetParameterType(EParamType::Int32);
	}
	else if (InProperty->IsA<FInt64Property>())
	{
		Handle.SetParameterType(EParamType::Int64);
	}
	else if (InProperty->IsA<FFloatProperty>())
	{
		Handle.SetParameterType(EParamType::Float);
	}
	else if (InProperty->IsA<FDoubleProperty>())
	{
		Handle.SetParameterType(EParamType::Double);
	}
	else if (InProperty->IsA<FNameProperty>())
	{
		Handle.SetParameterType(EParamType::Name);
	}
	else if (InProperty->IsA<FStrProperty>())
	{
		Handle.SetParameterType(EParamType::String);
	}
	else if (InProperty->IsA<FTextProperty>())
	{
		Handle.SetParameterType(EParamType::Text);
	}
	else if (InProperty->IsA<FStructProperty>())
	{
		UScriptStruct* ScriptStruct = CastField<FStructProperty>(InProperty)->Struct;
		if (ScriptStruct == TBaseStructure<FVector>::Get())
		{
			Handle.SetParameterType(EParamType::Vector);
		}
		else if (ScriptStruct == TBaseStructure<FVector4>::Get())
		{
			Handle.SetParameterType(EParamType::Vector4);
		}
		else if (ScriptStruct == TBaseStructure<FQuat>::Get())
		{
			Handle.SetParameterType(EParamType::Quat);
		}
		else if (ScriptStruct == TBaseStructure<FTransform>::Get())
		{
			Handle.SetParameterType(EParamType::Transform);
		}
	}

	// Not found - custom type
	if (!Handle.IsValid())
	{
		if (InProperty->IsA<FStructProperty>())
		{
			UScriptStruct* ScriptStruct = CastField<FStructProperty>(InProperty)->Struct;
			Handle.SetParameterType(EParamType::Custom);
			Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Struct, FAnimNextParamType::EContainerType::None, ScriptStruct));
		}
		else if (InProperty->IsA<FObjectPropertyBase>())
		{
			UClass* Class = CastField<FObjectPropertyBase>(InProperty)->PropertyClass;
			if (InProperty->IsA<FObjectProperty>() || InProperty->IsA<FObjectPtrProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Object, FAnimNextParamType::EContainerType::None, Class));
			}
			else if (InProperty->IsA<FSoftObjectProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::SoftObject, FAnimNextParamType::EContainerType::None, Class));
			}
			else if (InProperty->IsA<FSoftClassProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::SoftClass, FAnimNextParamType::EContainerType::None, Class));
			}
		}
		else if(InProperty->IsA<FArrayProperty>())
		{
			FProperty* InnerProperty = CastField<FArrayProperty>(InProperty)->Inner;
			if (InnerProperty->IsA<FBoolProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Bool, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FByteProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Byte, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FIntProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Int32, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FInt64Property>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Int64, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FFloatProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Float, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FDoubleProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Double, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FNameProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Name, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FStrProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::String, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FTextProperty>())
			{
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Text, FAnimNextParamType::EContainerType::Array, nullptr));
			}
			else if (InnerProperty->IsA<FStructProperty>())
			{
				UScriptStruct* ScriptStruct = CastField<FStructProperty>(InnerProperty)->Struct;
				Handle.SetParameterType(EParamType::Custom);
				Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Struct, FAnimNextParamType::EContainerType::Array, ScriptStruct));
			}
			else if (InnerProperty->IsA<FObjectPropertyBase>())
			{
				UClass* Class = CastField<FObjectPropertyBase>(InnerProperty)->PropertyClass;
				if (InnerProperty->IsA<FObjectProperty>() || InnerProperty->IsA<FObjectPtrProperty>())
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::Object, FAnimNextParamType::EContainerType::Array, Class));
				}
				else if (InnerProperty->IsA<FSoftObjectProperty>())
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::SoftObject, FAnimNextParamType::EContainerType::Array, Class));
				}
				else if (InnerProperty->IsA<FSoftClassProperty>())
				{
					Handle.SetParameterType(EParamType::Custom);
					Handle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType::SoftClass, FAnimNextParamType::EContainerType::Array, Class));
				}
			}
		}
	}

	return Handle;
}

FAnimNextParamType FParamTypeHandle::GetType() const
{
	FAnimNextParamType ParameterType;

	switch(GetParameterType())
	{
	default:
	case EParamType::None:
		break;
	case EParamType::Bool:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Bool;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Byte:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Byte;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Int32:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Int32;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Int64:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Int64;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Float:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Float;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Double:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Double;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Name:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Name;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::String:
		ParameterType.ValueType = FAnimNextParamType::EValueType::String;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Text:
		ParameterType.ValueType = FAnimNextParamType::EValueType::Text;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Vector:
		ParameterType.ValueTypeObject = TBaseStructure<FVector>::Get();
		ParameterType.ValueType = FAnimNextParamType::EValueType::Struct;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Vector4:
		ParameterType.ValueTypeObject = TBaseStructure<FVector4>::Get();
		ParameterType.ValueType = FAnimNextParamType::EValueType::Struct;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Quat:
		ParameterType.ValueTypeObject = TBaseStructure<FQuat>::Get();
		ParameterType.ValueType = FAnimNextParamType::EValueType::Struct;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Transform:
		ParameterType.ValueTypeObject = TBaseStructure<FTransform>::Get();
		ParameterType.ValueType = FAnimNextParamType::EValueType::Struct;
		ParameterType.ContainerType = FAnimNextParamType::EContainerType::None;
		break;
	case EParamType::Custom:
		{
			FRWScopeLock ScopeLock(TypesLock, SLT_ReadOnly);
			ParameterType = GCustomTypes[GetCustomTypeIndex() - 1];
		}
		break;
	}

	return ParameterType;
}


size_t FParamTypeHandle::GetSize() const
{
	switch(GetParameterType())
	{
	default:
	case EParamType::None:
		return 0;
	case EParamType::Bool:
		return sizeof(bool);
	case EParamType::Byte:
		return sizeof(uint8);
	case EParamType::Int32:
		return sizeof(int32);
	case EParamType::Int64:
		return sizeof(int64);
	case EParamType::Float:
		return sizeof(float);
	case EParamType::Double:
		return sizeof(double);
	case EParamType::Name:
		return sizeof(FName);
	case EParamType::String:
		return sizeof(FString);
	case EParamType::Text:
		return sizeof(FText);
	case EParamType::Vector:
		return sizeof(FVector);
	case EParamType::Vector4:
		return sizeof(FVector4);
	case EParamType::Quat:
		return sizeof(FQuat);
	case EParamType::Transform:
		return sizeof(FTransform);
	case EParamType::Custom:
		return GetType().GetSize();
	}

	return 0;
}

size_t FParamTypeHandle::GetValueTypeSize() const
{
	return GetSize();
}

size_t FParamTypeHandle::GetAlignment() const
{
	switch(GetParameterType())
	{
	default:
	case EParamType::None:
		return 0;
	case EParamType::Bool:
		return alignof(bool);
	case EParamType::Byte:
		return alignof(uint8);
	case EParamType::Int32:
		return alignof(int32);
	case EParamType::Int64:
		return alignof(int64);
	case EParamType::Float:
		return alignof(float);
	case EParamType::Double:
		return alignof(double);
	case EParamType::Name:
		return alignof(FName);
	case EParamType::String:
		return alignof(FString);
	case EParamType::Text:
		return alignof(FText);
	case EParamType::Vector:
		return alignof(FVector);
	case EParamType::Vector4:
		return alignof(FVector4);
	case EParamType::Quat:
		return alignof(FQuat);
	case EParamType::Transform:
		return alignof(FTransform);
	case EParamType::Custom:
		return GetType().GetAlignment();
	}

	return 0;
}

size_t FParamTypeHandle::GetValueTypeAlignment() const
{
	return GetAlignment();
}

FString FParamTypeHandle::ToString() const
{
	return GetType().ToString();
}

}
