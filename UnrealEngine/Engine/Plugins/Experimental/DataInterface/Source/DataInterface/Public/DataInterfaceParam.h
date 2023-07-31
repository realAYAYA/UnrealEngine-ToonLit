// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/ScriptInterface.h"

namespace UE::DataInterface
{

struct FParamType;

// Parameter/result/state memory wrapper
// @TODO can we save memory here with some base/derived system for fundamental types vs struct types?
struct DATAINTERFACE_API FParam
{
public:
	enum class EFlags : uint8
	{
		None	= 0,		// No flags
		Mutable	= 1,		// Parameter is mutable, so can be mutated at runtime
		Chunked = 2,		// Parameter is chunked, so represents a number of values rather than a single value. Chunk size is determined by an associated FContext
	};

	// Get the type of this param 
	const FParamType& GetType() const;

	// Check whether the supplied param can be written to by this param
	// Verifies that types are compatible, destination is mutable and chunking matches
	bool CanAssignTo(const FParam& InParam) const;

	// Helper function for CanAssignTo
	bool CanAssignWith(const FParamType& InType, EFlags InFlags, FStringBuilderBase* OutReasonPtr = nullptr) const;

	bool bIsMutable() const { return EnumHasAnyFlags(Flags, EFlags::Mutable); }

	bool bIsChunked() const { return EnumHasAnyFlags(Flags, EFlags::Chunked); }

protected:
	friend struct FContext;
	friend struct FState;
	friend struct FKernel;

	FParam() = default;
	
	FParam(const FParam* InOtherParam);
	FParam(const FParamType& InType, void* InData, EFlags InFlags);

	void* Data = nullptr;
	int16 TypeId = INDEX_NONE;
	EFlags Flags = EFlags::None;
};

ENUM_CLASS_FLAGS(FParam::EFlags);

// Simple type representation for data interfaces
struct DATAINTERFACE_API FParamType
{
	FParamType() = default;

	inline const UScriptStruct* GetStruct() const
	{
		return Struct;
	}
		
	inline FName GetName() const
	{
		return Name;
	}
		
	inline int32 GetSize() const
	{
		return Size;
	}
		
	int32 GetAlignment() const
	{
		return Alignment;
	}
		
	int16 GetTypeId() const
	{
		return TypeId;
	}

	struct DATAINTERFACE_API FRegistrar
	{
		// Helper for registration (via IMPLEMENT_DATA_INTERFACE_PARAM_TYPE).
		// Registers the type via deferred callback
		FRegistrar(TUniqueFunction<void(void)>&& InFunction);

		// Helper for registration (via IMPLEMENT_DATA_INTERFACE_PARAM_TYPE)
		static void RegisterType(FParamType& InType, const UScriptStruct* InStruct, FName InName, uint32 InSize, uint32 InAlignment);

		// Helper for registration
		static void RegisterDeferredTypes();

		// Find a type by name. If the type was not found, returns the default (invalid) type
		static const FParamType& FindTypeByName(FName InName);

		// Get a type by ID. If the type is invalid, returns the default (invalid) type
		static const FParamType& GetTypeById(uint16 InTypeId);
	};

private:
	const UScriptStruct* Struct = nullptr;
	FName Name = NAME_None;
	uint32 Size = 0;
	uint16 Alignment = 0;
	uint16 TypeId = INDEX_NONE;
};

// A type-erased param that wraps FProperty memory
struct FPropertyParam : FParam
{
	FPropertyParam(const FProperty* InProperty, UObject* InContainer);
};

namespace Private
{

template<typename ValueType> 
struct TParamTypeImpl
{
	FORCEINLINE static const FParamType& GetType()
	{
		static_assert(sizeof(ValueType) > 0, "Unimplemented type - please include the file DataInterfaceTypes.h or where DECLARE_DATA_INTERFACE_PARAM_TYPE is defined for your type");
		static FParamType Default;
		return Default;
	}
};

// Regular types need to have GetType function for this to work
template<typename ValueType> 
struct TParamType
{
	FORCEINLINE static const FParamType& GetType()
	{
		return TParamTypeImpl<typename TRemoveConst<ValueType>::Type>::GetType();
	}
};

template<typename ValueType>
static bool CheckParam(const FParam& InParam)
{
	return InParam.GetType().GetTypeId() == Private::TParamType<ValueType>::GetType().GetTypeId();
}

}

namespace Private
{

// Concept used to decide whether to use container-based construction of wrapped results
struct CSizedContainerWithAccessibleDataAsRawPtr
{
	template<typename ContainerType>
	auto Requires(ContainerType& Container) -> decltype(
		Container.Num(),
		Container.GetData()
	);
};

}

// A typed result which wraps the type-erased underlying param
template<typename ValueType>
struct TParam : FParam
{
protected:
	using TValueType = ValueType;
	
	friend struct FContext;
	friend struct FState;
	friend struct FKernel;
	
	TParam(void* InData, EFlags InFlags)
		: FParam(Private::TParamType<ValueType>::GetType(), InData, InFlags)
	{
	}

	TParam(const void* InData, EFlags InFlags)
		: FParam(Private::TParamType<ValueType>::GetType(), const_cast<void*>(InData), InFlags)
	{
	}

	TParam(FParam& InParam, EFlags InAdditionalFlags = EFlags::None)
		: FParam(&InParam)
	{
#if DO_CHECK
		TStringBuilder<64> Error;
		checkf(InParam.CanAssignWith(Private::TParamType<ValueType>::GetType(), GetTypeFlags(InAdditionalFlags), &Error),
			TEXT("Cannot assign type: %s"), Error.ToString());
#endif
	}

	static EFlags GetTypeFlags(EFlags InAdditionalFlags)
	{
		EFlags Flags = InAdditionalFlags;

		// Add const flags or not
		if constexpr (!TIsConst<ValueType>::Value)
		{
			Flags |= FParam::EFlags::Mutable;
		}

		return Flags;
	}
};

namespace Private
{

struct CHasStaticStruct
{
	template<typename ValueType>
	auto Requires(UScriptStruct* Result) -> decltype(
		Result = ValueType::StaticStruct()
	);
};

template<typename ValueType> 
struct TParamTypeHelper
{
	FORCEINLINE static const UScriptStruct* GetStruct()
	{
		if constexpr (TModels<CHasStaticStruct, ValueType>::Value)
		{
			return TBaseStructure<ValueType>::Get();
		}
		else
		{
			return nullptr;
		}
	}

	FORCEINLINE static uint32 GetSize()
	{
		return sizeof(ValueType);
	}

	FORCEINLINE static uint32 GetAlignment()
	{
		return alignof(ValueType);
	}
};

}

}

