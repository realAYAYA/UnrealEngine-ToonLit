// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyBag.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Concepts/BaseStructureProvider.h"
#include "RigVMCore/RigVMTemplate.h"
#include "ParamType.generated.h"

namespace UE::AnimNext
{
	struct FParamTypeHandle;
	struct FParamHelpers;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Private
{
	template <typename T>
	struct TIsSoftObjectPtr
	{
		enum { Value = false };
	};

	template <typename T> struct TIsSoftObjectPtr<               TSoftObjectPtr<T>> { enum { Value = true }; };
	template <typename T> struct TIsSoftObjectPtr<const          TSoftObjectPtr<T>> { enum { Value = true }; };
	template <typename T> struct TIsSoftObjectPtr<      volatile TSoftObjectPtr<T>> { enum { Value = true }; };
	template <typename T> struct TIsSoftObjectPtr<const volatile TSoftObjectPtr<T>> { enum { Value = true }; };

	template <typename T>
	struct TIsSoftClassPtr
	{
		enum { Value = false };
	};

	template <typename T> struct TIsSoftClassPtr<               TSoftClassPtr<T>> { enum { Value = true }; };
	template <typename T> struct TIsSoftClassPtr<const          TSoftClassPtr<T>> { enum { Value = true }; };
	template <typename T> struct TIsSoftClassPtr<      volatile TSoftClassPtr<T>> { enum { Value = true }; };
	template <typename T> struct TIsSoftClassPtr<const volatile TSoftClassPtr<T>> { enum { Value = true }; };
}

/**
 * Representation of a parameter's type. Serializable, but fairly heavyweight to pass around and compare.
 * Faster comparisons and other operations can be performed on UE::AnimNext::FParamTypeHandle, but they cannot be
 * serialized as they are not stable across runs.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextParamType
{
public:
	GENERATED_BODY()

	using EValueType = ::EPropertyBagPropertyType;
	using EContainerType = ::EPropertyBagContainerType;

	friend struct UE::AnimNext::FParamTypeHandle;
	friend struct UE::AnimNext::FParamHelpers;
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	FAnimNextParamType() = default;

	/** Construct a parameter type from the passed in value, container and object type. */
	FAnimNextParamType(EValueType InValueType, EContainerType InContainerType = EContainerType::None, const UObject* InValueTypeObject = nullptr);

	/** Construct a parameter type from the passed in FRigVMTemplateArgumentType. */
	static FAnimNextParamType FromRigVMTemplateArgument(const FRigVMTemplateArgumentType& RigVMType);
private:
	/** Pointer to object that defines the Enum, Struct, or Class. */
	UPROPERTY()
	TObjectPtr<const UObject> ValueTypeObject = nullptr;

	/** Type of the value described by this parameter. */
	UPROPERTY()
	EPropertyBagPropertyType ValueType = EPropertyBagPropertyType::None;

	/** Type of the container described by this parameter. */
	UPROPERTY()
	EPropertyBagContainerType ContainerType = EPropertyBagContainerType::None;
	
private:
	/** Helper function for GetType */
	template<typename ParamType>
	static constexpr void GetTypeInner(FAnimNextParamType& ParameterType)
	{
		using NonPtrParamType = std::remove_pointer_t<ParamType>;

		if constexpr (std::is_same_v<ParamType, bool>)
		{
			ParameterType.ValueType = EValueType::Bool;
		}
		else if constexpr (std::is_same_v<ParamType, uint8>)
		{
			ParameterType.ValueType = EValueType::Byte;
		}
		else if constexpr (std::is_same_v<ParamType, int32>)
		{
			ParameterType.ValueType = EValueType::Int32;
		}
		else if constexpr (std::is_same_v<ParamType, int64>)
		{
			ParameterType.ValueType = EValueType::Int64;
		}
		else if constexpr (std::is_same_v<ParamType, float>)
		{
			ParameterType.ValueType = EValueType::Float;
		}
		else if constexpr (std::is_same_v<ParamType, double>)
		{
			ParameterType.ValueType = EValueType::Double;
		}
		else if constexpr (std::is_same_v<ParamType, FName>)
		{
			ParameterType.ValueType = EValueType::Name;
		}
		else if constexpr (std::is_same_v<ParamType, FString>)
		{
			ParameterType.ValueType = EValueType::String;
		}
		else if constexpr (std::is_same_v<ParamType, FText>)
		{
			ParameterType.ValueType = EValueType::Text;
		}
		else if constexpr (TIsUEnumClass<ParamType>::Value)
		{
			ParameterType.ValueType = EValueType::Enum;
			ParameterType.ValueTypeObject = StaticEnum<ParamType>();
		}
		else if constexpr (TModels<CStaticStructProvider, ParamType>::Value)
		{
			ParameterType.ValueType = EValueType::Struct;
			ParameterType.ValueTypeObject = ParamType::StaticStruct();
		}
		else if constexpr (TModels<CBaseStructureProvider, ParamType>::Value)
		{
			ParameterType.ValueType = EValueType::Struct;
			ParameterType.ValueTypeObject = TBaseStructure<ParamType>::Get();
		}
		else if constexpr (TModels<CStaticClassProvider, NonPtrParamType>::Value)
		{
			if constexpr (std::is_same_v<NonPtrParamType, UClass>)
			{
				ParameterType.ValueType = EValueType::Class;
				ParameterType.ValueTypeObject = NonPtrParamType::StaticClass();
			}
			else
			{
				ParameterType.ValueType = EValueType::Object;
				ParameterType.ValueTypeObject = NonPtrParamType::StaticClass();
			}
		}
		else if constexpr (TIsTObjectPtr<ParamType>::Value)
		{
			if constexpr (std::is_same_v<ParamType, TObjectPtr<UClass>>)
			{
				ParameterType.ValueType = EValueType::Class;
				ParameterType.ValueTypeObject = ParamType::ElementType::StaticClass();
			}
			else
			{
				ParameterType.ValueType = EValueType::Object;
				ParameterType.ValueTypeObject = ParamType::ElementType::StaticClass();
			}
		}
		else if constexpr (TIsTSubclassOf<ParamType>::Value)
		{
			ParameterType.ValueType = EValueType::Class;
			ParameterType.ValueTypeObject = ParamType::ElementType::StaticClass();
		}
		else if constexpr (UE::AnimNext::Private::TIsSoftObjectPtr<ParamType>::Value)
		{
			ParameterType.ValueType = EValueType::SoftObject;
			ParameterType.ValueTypeObject = ParamType::ElementType::StaticClass();
		}
		else if constexpr (UE::AnimNext::Private::TIsSoftClassPtr<ParamType>::Value)
		{
			ParameterType.ValueType = EValueType::SoftClass;
			ParameterType.ValueTypeObject = ParamType::ElementType::StaticClass();
		}
		else if constexpr (std::is_same_v<ParamType, uint32>)
		{
			ParameterType.ValueType = EValueType::UInt32;
		}
		else if constexpr (std::is_same_v<ParamType, uint64>)
		{
			ParameterType.ValueType = EValueType::UInt64;
		}
		else
		{
			static_assert(sizeof(ParamType) == 0, "Type is not expressible as a FAnimNextParamType for available types.");
		}
	}

	/** Helper function for IsValid */
	bool IsValidObject() const;

public:
	/** Get a parameter type based on the passed-in built-in type */
	template<typename ParamType>
	static FAnimNextParamType GetType()
	{
		using NonConstType = std::remove_const_t<ParamType>;

		FAnimNextParamType ParameterType;

		if constexpr (TIsTArray<NonConstType>::Value)
		{
			GetTypeInner<typename NonConstType::ElementType>(ParameterType);
			ParameterType.ContainerType = EContainerType::Array;
		}
		else
		{
			GetTypeInner<NonConstType>(ParameterType);
		}

		return ParameterType;
	}

	/** Get a parameter type handle that represents this type */
	UE::AnimNext::FParamTypeHandle GetHandle() const;

	/** Get the pointer to the object that defines the Enum, Struct, or Class. */
	const UObject* GetValueTypeObject() const
	{
		return ValueTypeObject;
	}

	/** Get the type of the value described by this parameter. */
	EValueType GetValueType() const
	{
		return ValueType;
	}

	/** Get the type of the container described by this parameter. */
	EContainerType GetContainerType() const
	{
		return ContainerType;
	}

	/** 
	 * Helper function returning the size of the type.
	 *
	 * @returns Size in bytes of the type.
	 */
	size_t GetSize() const;

	/** 
	 * Helper function returning the size of the value type.
	 * This is identical to GetSize for non containers
	 *
	 * @returns Size in bytes of the value type.
	 */	
	size_t GetValueTypeSize() const;
	
	/** 
	 * Helper function returning the alignment of the type.
	 *
	 * @returns Alignment of the type.
	 */	
	size_t GetAlignment() const;

	/** 
	 * Helper function returning the alignment of the value type.
	 * This is identical to GetAlignment for non containers
	 *
	 * @returns Alignment of the value type.
	 */	
	size_t GetValueTypeAlignment() const;

	/** Append a string representing this type to the supplied string builder */
	void ToString(FStringBuilderBase& InStringBuilder) const;

	/** Get a string representing this type */
	FString ToString() const;

	/** Get a type from a string */
	static FAnimNextParamType FromString(const FString& InString);

	/** Equality operator */
	friend bool operator==(const FAnimNextParamType& InLHS, const FAnimNextParamType& InRHS)
	{
		return InLHS.ValueType == InRHS.ValueType && InLHS.ContainerType == InRHS.ContainerType && InLHS.ValueTypeObject == InRHS.ValueTypeObject;
	}

	/** Inequality operator */
	friend bool operator!=(const FAnimNextParamType& InLHS, const FAnimNextParamType& InRHS)
	{
		return InLHS.ValueType != InRHS.ValueType || InLHS.ContainerType != InRHS.ContainerType || InLHS.ValueTypeObject != InRHS.ValueTypeObject;
	}

	/** Type hash for TMap storage */
	friend uint32 GetTypeHash(const FAnimNextParamType& InType)
	{
		return HashCombineFast(GetTypeHash((uint32)InType.ValueType | ((uint32)InType.ContainerType << 8)), GetTypeHash(InType.ValueTypeObject));
	}
	
	/** @return whether this type actually describes a type */
	bool IsValid() const
	{
		const bool bHasValidValueType = ValueType != EValueType::None;
		const bool bHasValidContainerType = (ContainerType == EContainerType::None) || (bHasValidValueType && ContainerType != EContainerType::None);
		const bool bHasValidObjectType = (ValueType < EValueType::Enum) || (ValueType >= EValueType::Enum && ValueType <= EValueType::SoftClass && IsValidObject());
		return bHasValidValueType && bHasValidContainerType && bHasValidObjectType;
	}

	/** @return whether this type represents an object (object/class/softobject/softclass) */
	bool IsObjectType() const;
};

