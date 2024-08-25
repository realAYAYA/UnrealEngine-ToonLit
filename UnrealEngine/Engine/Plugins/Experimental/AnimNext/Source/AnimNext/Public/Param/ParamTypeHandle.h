// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Concepts/BaseStructureProvider.h"
#include "Param/ParamType.h"

class UAnimSequence;
class UCharacterMovementComponent;
class UAnimNextMeshComponent;
struct FAnimNextGraphLODPose;
struct FAnimNextGraphReferencePose;

namespace UE::AnimNext::Tests
{
	class FParamTypesTest;
}

namespace UE::AnimNext
{

/**
 * Representation of a parameter's type. Lightweight to compare and pass around, but not serializable as not stable
 * across runs.
 * Serialization can be performed on FAnimNextParamType. 
 */
struct ANIMNEXT_API FParamTypeHandle
{
	/**
	 * Byte enum representing built-in parameter types. Types beyond this are Custom and are accessed via an index into the
	 * global type array, which is slower
	 */
	enum class EParamType : uint8
	{
		None = 0,

		Bool,
		Byte,
		Int32,
		Int64,
		Float,
		Double,
		Name,
		String,
		Text,
		Vector,
		Vector4,
		Quat,
		Transform,

		// Common object types
		Object,
		CharacterMovementComponent,
		AnimNextMeshComponent,
		AnimSequence,

		// Common struct types
		AnimNextGraphLODPose,
		AnimNextGraphReferencePose,

		MaxBuiltIn = AnimNextGraphReferencePose,

		Custom = 0xff,

		Max = Custom,
	};

	friend struct ::FAnimNextParamType;
	friend struct FParamHelpers;
	friend struct FParamUtils;
	friend class UE::AnimNext::Tests::FParamTypesTest;

private:
	union
	{
		uint32 Value;

		struct
		{
			uint32 BuiltInType : 8;
			uint32 CustomTypeIndex : 24;
		} Fields;
	};

private:
	/** Set the built-in parameter type */
	void SetParameterType(EParamType InType)
	{
		Fields.BuiltInType = (uint32)InType;
	}

	/** Get the custom type index */
	uint32 GetCustomTypeIndex() const
	{
		return Fields.CustomTypeIndex;
	}

	/** Get the custom type index */
	void SetCustomTypeIndex(uint32 InIndex)
	{
		checkf(InIndex < (1 << 24), TEXT("FTypeHandle::SetCustomTypeIndex: Type Index out of range"));
		Fields.CustomTypeIndex = InIndex;
	}
	
#if WITH_DEV_AUTOMATION_TESTS
	// Used to isolate param type handles from automated tests
	static void BeginTestSandbox();
	static void EndTestSandbox();
#endif

	/** Get a custom type index for the passed-in type information */
	static uint32 GetOrAllocateCustomTypeIndex(FAnimNextParamType::EValueType InValueType, FAnimNextParamType::EContainerType InContainerType, const UObject* InValueTypeObject);

	/** Validate the passed-in custom type index */
	static bool ValidateCustomTypeIndex(uint32 InCustomTypeIndex);

	/** Helper for GetHandle */
	template<typename ParamType>
	static constexpr void GetHandleInner(FAnimNextParamType::EValueType& OutValueType, UObject*& OutValueTypeObject)
	{
		using NonPtrParamType = std::remove_pointer_t<ParamType>;

		if constexpr (std::is_same_v<ParamType, bool>)
		{
			OutValueType = FAnimNextParamType::EValueType::Bool;
		}
		else if constexpr (std::is_same_v<ParamType, uint8>)
		{
			OutValueType = FAnimNextParamType::EValueType::Byte;
		}
		else if constexpr (std::is_same_v<ParamType, int32>)
		{
			OutValueType = FAnimNextParamType::EValueType::Int32;
		}
		else if constexpr (std::is_same_v<ParamType, int64>)
		{
			OutValueType = FAnimNextParamType::EValueType::Int64;
		}
		else if constexpr (std::is_same_v<ParamType, float>)
		{
			OutValueType = FAnimNextParamType::EValueType::Float;
		}
		else if constexpr (std::is_same_v<ParamType, double>)
		{
			OutValueType = FAnimNextParamType::EValueType::Double;
		}
		else if constexpr (std::is_same_v<ParamType, FName>)
		{
			OutValueType = FAnimNextParamType::EValueType::Name;
		}
		else if constexpr (std::is_same_v<ParamType, FString>)
		{
			OutValueType = FAnimNextParamType::EValueType::String;
		}
		else if constexpr (std::is_same_v<ParamType, FText>)
		{
			OutValueType = FAnimNextParamType::EValueType::Text;
		}
		else if constexpr (TIsUEnumClass<ParamType>::Value)
		{
			OutValueType = FAnimNextParamType::EValueType::Enum;
			OutValueTypeObject = StaticEnum<ParamType>();
		}
		else if constexpr (TModels<CStaticStructProvider, ParamType>::Value)
		{
			OutValueType = FAnimNextParamType::EValueType::Struct;
			OutValueTypeObject = ParamType::StaticStruct();
		}
		else if constexpr (TModels<CBaseStructureProvider, ParamType>::Value)
		{
			OutValueType = FAnimNextParamType::EValueType::Struct;
			OutValueTypeObject = TBaseStructure<ParamType>::Get();
		}
		else if constexpr (TModels<CStaticClassProvider, NonPtrParamType>::Value)
		{
			if constexpr (std::is_same_v<NonPtrParamType, UClass>)
			{
				OutValueType = FAnimNextParamType::EValueType::Class;
				OutValueTypeObject = NonPtrParamType::StaticClass();
			}
			else
			{
				OutValueType = FAnimNextParamType::EValueType::Object;
				OutValueTypeObject = NonPtrParamType::StaticClass();
			}
		}
		else if constexpr (TIsTObjectPtr<ParamType>::Value)
		{
			if constexpr (std::is_same_v<ParamType, TObjectPtr<UClass>>)
			{
				OutValueType = FAnimNextParamType::EValueType::Class;
				OutValueTypeObject = ParamType::ElementType::StaticClass();
			}
			else
			{
				OutValueType = FAnimNextParamType::EValueType::Object;
				OutValueTypeObject = ParamType::ElementType::StaticClass();
			}
		}
		else if constexpr (TIsTSubclassOf<ParamType>::Value)
		{
			OutValueType = FAnimNextParamType::EValueType::Class;
			OutValueTypeObject = ParamType::ElementType::StaticClass();
		}
		else if constexpr (Private::TIsSoftObjectPtr<ParamType>::Value)
		{
			OutValueType = FAnimNextParamType::EValueType::SoftObject;
			OutValueTypeObject = ParamType::ElementType::StaticClass();
		}
		else if constexpr (Private::TIsSoftClassPtr<ParamType>::Value)
		{
			OutValueType = FAnimNextParamType::EValueType::SoftClass;
			OutValueTypeObject = ParamType::ElementType::StaticClass();
		}
		else
		{
			static_assert(sizeof(ParamType) == 0, "Type is not expressible as a type handle, see FAnimNextParamType for available types.");
		}
	}

public:
	constexpr FParamTypeHandle()
		: Value(0)
	{
	}

	/** Get the built-in parameter type */
	EParamType GetParameterType() const
	{
		return (EParamType)Fields.BuiltInType;
	}

	/** Get the custom type info */
	void GetCustomTypeInfo(FAnimNextParamType::EValueType& OutValueType, FAnimNextParamType::EContainerType& OutContainerType, const UObject*& OutValueTypeObject) const;

	/** Check whether this describes a built-in type (i.e. not a custom type) */
	bool IsBuiltInType() const
	{
		return (EParamType)Fields.BuiltInType <= EParamType::MaxBuiltIn;
	}

	/** Get a parameter type based on the passed-in built-in type */
	template<typename ParamType>
	static constexpr FParamTypeHandle GetHandle()
	{
		using NonConstType = std::remove_const_t<ParamType>;

		FParamTypeHandle TypeHandle;

		// Check against built-in types statically
		if constexpr (std::is_same_v<NonConstType, bool>)
		{
			TypeHandle.SetParameterType(EParamType::Bool);
		}
		else if constexpr (std::is_same_v<NonConstType, uint8>)
		{
			TypeHandle.SetParameterType(EParamType::Byte);
		}
		else if constexpr (std::is_same_v<NonConstType, int32>)
		{
			TypeHandle.SetParameterType(EParamType::Int32);
		}
		else if constexpr (std::is_same_v<NonConstType, int64>)
		{
			TypeHandle.SetParameterType(EParamType::Int64);
		}
		else if constexpr (std::is_same_v<NonConstType, float>)
		{
			TypeHandle.SetParameterType(EParamType::Float);
		}
		else if constexpr (std::is_same_v<NonConstType, double>)
		{
			TypeHandle.SetParameterType(EParamType::Double);
		}
		else if constexpr (std::is_same_v<NonConstType, FName>)
		{
			TypeHandle.SetParameterType(EParamType::Name);
		}
		else if constexpr (std::is_same_v<NonConstType, FString>)
		{
			TypeHandle.SetParameterType(EParamType::String);
		}
		else if constexpr (std::is_same_v<NonConstType, FText>)
		{
			TypeHandle.SetParameterType(EParamType::Text);
		}
		else if constexpr (std::is_same_v<NonConstType, FVector>)
		{
			TypeHandle.SetParameterType(EParamType::Vector);
		}
		else if constexpr (std::is_same_v<NonConstType, FVector4>)
		{
			TypeHandle.SetParameterType(EParamType::Vector4);
		}
		else if constexpr (std::is_same_v<NonConstType, FQuat>)
		{
			TypeHandle.SetParameterType(EParamType::Quat);
		}
		else if constexpr (std::is_same_v<NonConstType, FTransform>)
		{
			TypeHandle.SetParameterType(EParamType::Transform);
		}
		else if constexpr (std::is_same_v<NonConstType, TObjectPtr<UObject>> || std::is_same_v<NonConstType, UObject*>)
		{
			TypeHandle.SetParameterType(EParamType::Object);
		}
		else if constexpr (std::is_same_v<NonConstType, TObjectPtr<UCharacterMovementComponent>> || std::is_same_v<NonConstType, UCharacterMovementComponent*>)
		{
			TypeHandle.SetParameterType(EParamType::CharacterMovementComponent);
		}
		else if constexpr (std::is_same_v<NonConstType, TObjectPtr<UAnimNextMeshComponent>> || std::is_same_v<NonConstType, UAnimNextMeshComponent*>)
		{
			TypeHandle.SetParameterType(EParamType::AnimNextMeshComponent);
		}
		else if constexpr (std::is_same_v<NonConstType, TObjectPtr<UAnimSequence>> || std::is_same_v<NonConstType, UAnimSequence*>)
		{
			TypeHandle.SetParameterType(EParamType::AnimSequence);
		}
		else if constexpr (std::is_same_v<NonConstType, FAnimNextGraphLODPose>)
		{
			TypeHandle.SetParameterType(EParamType::AnimNextGraphLODPose);
		}
		else if constexpr (std::is_same_v<NonConstType, FAnimNextGraphReferencePose>)
		{
			TypeHandle.SetParameterType(EParamType::AnimNextGraphReferencePose);
		}
		else
		{
			// Not a built-in-type, so we need to do some work
			FAnimNextParamType::EValueType ValueType = FAnimNextParamType::EValueType::None;
			FAnimNextParamType::EContainerType ContainerType = FAnimNextParamType::EContainerType::None;
			UObject* ValueTypeObject = nullptr;

			if constexpr (TIsTArray<NonConstType>::Value)
			{
				GetHandleInner<typename NonConstType::ElementType>(ValueType, ValueTypeObject);
				ContainerType = FAnimNextParamType::EContainerType::Array;
			}
			else
			{
				GetHandleInner<NonConstType>(ValueType, ValueTypeObject);
			}

			if(ValueType != FAnimNextParamType::EValueType::None)
			{
				TypeHandle.SetCustomTypeIndex(GetOrAllocateCustomTypeIndex(ValueType, ContainerType, ValueTypeObject));
				TypeHandle.SetParameterType(EParamType::Custom);
			}
		}

		check(TypeHandle.IsValid());

		return TypeHandle;
	}

	/** Make a parameter handle from a raw uint32 */
	static FParamTypeHandle FromRaw(uint32 InRawHandle)
	{
		FParamTypeHandle Handle;
		Handle.Value = InRawHandle;
		return Handle;
	}

	/** Make a parameter type handle from a property bag property desc */
	static FParamTypeHandle FromPropertyBagPropertyDesc(const FPropertyBagPropertyDesc& Desc);
	
	/** Make a parameter type handle from a FProperty */
	static FParamTypeHandle FromProperty(const FProperty* InProperty);

	/** Make a parameter type handle from a UObject */
	static FParamTypeHandle FromObject(const UObject* InObject);
	
	/** Return the raw value that this handle uses to represent itself */
	uint32 ToRaw() const
	{
		return Value;
	}

	/** Get the parameter type that this handle corresponds to */
	FAnimNextParamType GetType() const;

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
	
	/** Get a string representing this type */
	FString ToString() const;

	/** Equality operator */
	friend bool operator==(const FParamTypeHandle& InLHS, const FParamTypeHandle& InRHS)
	{
		return InLHS.Value == InRHS.Value;
	}

	/** Inequality operator */
	friend bool operator!=(const FParamTypeHandle& InLHS, const FParamTypeHandle& InRHS)
	{
		return InLHS.Value != InRHS.Value;
	}

	/** Type hash for TMap storage */
	friend uint32 GetTypeHash(const FParamTypeHandle& InHandle)
	{
		return GetTypeHash(InHandle.Value);
	}

	/**
	 * Check type invariants - type handle is either:
	 * - Built-in and in known ranges
	 * - Custom and with a non-zero custom type index
	 * @return whether this type actually describes a type
	 */
	bool IsValid() const
	{
		const EParamType ParameterType = GetParameterType();
		const uint32 CustomTypeIndex = GetCustomTypeIndex();
		checkSlow(ParameterType != EParamType::Custom || ValidateCustomTypeIndex(CustomTypeIndex));
		const bool bIsValidCustomType = (ParameterType == EParamType::Custom && CustomTypeIndex != 0);
		const bool bIsValidBuiltInType = ParameterType > EParamType::None && ParameterType <= EParamType::MaxBuiltIn;
		return bIsValidBuiltInType || bIsValidCustomType;
	}
};

}