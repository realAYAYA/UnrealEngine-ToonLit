// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataType.h"

#include "OptimusDataTypeRegistry.h"
#include "OptimusHelpers.h"



FOptimusDataTypeRef::FOptimusDataTypeRef(
	FOptimusDataTypeHandle InTypeHandle
	)
{
	Set(InTypeHandle);
}


void FOptimusDataTypeRef::Set(
	FOptimusDataTypeHandle InTypeHandle
	)
{
	if (InTypeHandle.IsValid())
	{
		TypeName = InTypeHandle->TypeName;
		TypeObject = InTypeHandle->TypeObject;
		checkSlow(FOptimusDataTypeRegistry::Get().FindType(TypeName) != nullptr);
	}
	else
	{
		TypeName = NAME_None;
		TypeObject.Reset();
	}
}


FOptimusDataTypeHandle FOptimusDataTypeRef::Resolve() const
{
	FOptimusDataTypeRegistry& Registry = FOptimusDataTypeRegistry::Get();

	FOptimusDataTypeHandle TypeHandle = Registry.FindType(TypeName);
	
	// This can happen during asset load, at which point the registry may not have been initialized yet
	// so we have to register these types on demand.
	if (!TypeHandle.IsValid())
	{
		if (TypeObject.IsValid())
		{
			if (Registry.RegisterStructType(Cast<UScriptStruct>(TypeObject.Get())))
			{
				TypeHandle = Registry.FindType(TypeName);
			}
		}
	}
	
	return TypeHandle;
}


void FOptimusDataTypeRef::PostSerialize(const FArchive& Ar)
{
	// Fix up data types so that the type points to the type object.
	if (Ar.IsLoading())
	{
		const FOptimusDataTypeHandle TypeHandle = FOptimusDataTypeRegistry::Get().FindType(TypeName);
		if (TypeHandle.IsValid())
		{
			TypeObject = TypeHandle->TypeObject;
		}
	}
}


FProperty* FOptimusDataType::CreateProperty(
	UStruct* InScope, 
	FName InName
	) const
{
	const FOptimusDataTypeRegistry::PropertyCreateFuncT PropertyCreateFunc =
		FOptimusDataTypeRegistry::Get().FindPropertyCreateFunc(TypeName);

	if (PropertyCreateFunc)
	{
		return PropertyCreateFunc(InScope, InName);
	}
	else
	{
		return nullptr;
	}
}


bool FOptimusDataType::ConvertPropertyValueToShader(
	TArrayView<const uint8> InValue,
	FShaderValueType::FValueView OutConvertedValue
	) const
{
	const FOptimusDataTypeRegistry::PropertyValueConvertFuncT PropertyConversionFunc =
		FOptimusDataTypeRegistry::Get().FindPropertyValueConvertFunc(TypeName);
	if (PropertyConversionFunc)
	{
		return PropertyConversionFunc(InValue, OutConvertedValue);
	}
	else
	{
		return false;
	}
}

FShaderValueType::FValue FOptimusDataType::MakeShaderValue() const
{
	return {ShaderValueSize, GetNumArrays()};
}

bool FOptimusDataType::CanCreateProperty() const
{
	return static_cast<bool>(FOptimusDataTypeRegistry::Get().FindPropertyCreateFunc(TypeName));
}

int32 FOptimusDataType::GetNumArrays() const
{
	const TArray<FOptimusDataTypeRegistry::FArrayMetadata>* ArrayMetadata =
		FOptimusDataTypeRegistry::Get().FindArrayMetadata(TypeName);
	
	if (ensure(ArrayMetadata))
	{
		return ArrayMetadata->Num();
	}

	return 0;
}

int32 FOptimusDataType::GetArrayShaderValueOffset(int32 InArrayIndex) const
{
	const TArray<FOptimusDataTypeRegistry::FArrayMetadata>* ArrayMetadata =
		FOptimusDataTypeRegistry::Get().FindArrayMetadata(TypeName);

	if (ensure(ArrayMetadata) && ensure(ArrayMetadata->IsValidIndex(InArrayIndex)))
	{
		return (*ArrayMetadata)[InArrayIndex].ShaderValueOffset;
	}

	return INDEX_NONE;
}

int32 FOptimusDataType::GetArrayElementShaderValueSize(int32 InArrayIndex) const
{
	const TArray<FOptimusDataTypeRegistry::FArrayMetadata>* ArrayMetadata =
		FOptimusDataTypeRegistry::Get().FindArrayMetadata(TypeName);

	if (ensure(ArrayMetadata && ArrayMetadata->IsValidIndex(InArrayIndex)))
	{
		return (*ArrayMetadata)[InArrayIndex].ElementShaderValueSize;
	}

	return INDEX_NONE;
}

