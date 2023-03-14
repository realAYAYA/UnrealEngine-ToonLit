// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NameTypes.h"
#include "Misc/EnumClassFlags.h"

#include "OptimusDataType.generated.h"


/** These flags govern how the data type can be used */
UENUM(meta = (Bitflags))
enum class EOptimusDataTypeUsageFlags : uint8
{
	None				= 0,
	
	Resource			= 1 << 0,		/** This type can be used in a resource */
	Variable			= 1 << 1,		/** This type can be used in a variable */
	AnimAttributes      = 1 << 2,       /** This type can be used to query an anim attribute*/
};
ENUM_CLASS_FLAGS(EOptimusDataTypeUsageFlags)


/** These flags are for indicating type behaviour */
UENUM(meta = (Bitflags))
enum class EOptimusDataTypeFlags : uint8
{
	None = 0,
	
	IsStructType		= 1 << 0,		/** This is a UScriptStruct-based type. */
	ShowElements		= 1 << 1,		/** If a struct type, show the struct elements. */
};
ENUM_CLASS_FLAGS(EOptimusDataTypeFlags)


USTRUCT()
struct OPTIMUSCORE_API FOptimusDataType
{
	GENERATED_BODY()

	FOptimusDataType() = default;

	// Create an FProperty with the given scope and name, but only if the UsageFlags contains 
	// EOptimusDataTypeUsageFLags::Variable. Otherwise it returns a nullptr.
	FProperty* CreateProperty(
		UStruct *InScope,
		FName InName
		) const;

	// Convert an FProperty value to a value compatible with the shader parameter data layout.
	// The InValue parameter should point at the memory location governed by the FProperty for
	// this data type, and OutConvertedValue is an array to store the bytes for the converted
	// value. If the function failed, a nullptr is returned and the state of the OutConvertedValue
	// is undefined. Upon success, the return value is a pointer to the value following the
	// converted input value, and the converted output value array will have been grown to
	// accommodate the newly converted value.
	bool ConvertPropertyValueToShader(
		TArrayView<const uint8> InValue,
		FShaderValueType::FValueView OutConvertedValue
		) const;

	// Return a value struct that can hold raw shader value of this type
	FShaderValueType::FValue MakeShaderValue() const;
	
	// Returns true if the data type can create a FProperty object to represent it.
	bool CanCreateProperty() const;

	// Returns the total number of array members (recursive) in the shader type
	int32 GetNumArrays() const;
	
	// Returns the offset from the beginning of shader type of each array typed shader struct member
	int32 GetArrayShaderValueOffset(int32 InArrayIndex) const;
	
	// Returns the element size of each array typed shader struct member
	int32 GetArrayElementShaderValueSize(int32 InArrayIndex) const;

	UPROPERTY()
	FName TypeName;

	UPROPERTY()
	FText DisplayName;

	// Shader value type that goes with this Optimus pin type.
	UPROPERTY()
	FShaderValueTypeHandle ShaderValueType;

	// Size of the shader value that can hold a value of this type. If this type is not a
	// shader value, then this value is zero.
	UPROPERTY()
	int32 ShaderValueSize = 0;
	
	UPROPERTY()
	FName TypeCategory;

	UPROPERTY()
	TWeakObjectPtr<UObject> TypeObject;

	UPROPERTY()
	bool bHasCustomPinColor = false;

	UPROPERTY()
	FLinearColor CustomPinColor = FLinearColor::Black;
	
	UPROPERTY()
	EOptimusDataTypeUsageFlags UsageFlags = EOptimusDataTypeUsageFlags::None;

	UPROPERTY()
	EOptimusDataTypeFlags TypeFlags = EOptimusDataTypeFlags::None;
};


using FOptimusDataTypeHandle = TSharedPtr<const FOptimusDataType>;


/** A reference object for an Optimus data type to use in UObjects and other UStruct-like things */
USTRUCT(BlueprintType)
struct OPTIMUSCORE_API FOptimusDataTypeRef
{
	GENERATED_BODY()

	FOptimusDataTypeRef(FOptimusDataTypeHandle InTypeHandle = {});

	bool IsValid() const
	{
		return !TypeName.IsNone();
	}

	explicit operator bool() const
	{
		return IsValid();
	}

	void Set(FOptimusDataTypeHandle InTypeHandle);

	FOptimusDataTypeHandle Resolve() const;

	const FOptimusDataType* operator->() const
	{
		return Resolve().Get();
	}

	const FOptimusDataType& operator*() const
	{
		return *Resolve().Get();
	}

	bool operator==(const FOptimusDataTypeRef& InOther) const
	{
		return TypeName == InOther.TypeName && TypeObject == InOther.TypeObject;
	}

	bool operator!=(const FOptimusDataTypeRef& InOther) const
	{
		return TypeName != InOther.TypeName || TypeObject != InOther.TypeObject;
	}

	UPROPERTY(EditAnywhere, Category=Type)
	FName TypeName;

	// A weak pointer to the type object helps enforce asset dependency
	UPROPERTY(EditAnywhere, Category=Type)
	TWeakObjectPtr<UObject> TypeObject;
	
	void PostSerialize(const FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FOptimusDataTypeRef> : public TStructOpsTypeTraitsBase2<FOptimusDataTypeRef>
{
	enum 
	{
		WithPostSerialize = true,
	};
};
