// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/ScriptInterface.h"

namespace UE::AnimNext::Interface
{

struct FParamType;
struct FParamStorage;

// Parameter/result/state memory wrapper
// @TODO can we save memory here with some base/derived system for fundamental types vs struct types?
struct ANIMNEXTINTERFACE_API FParam
{
public:
	enum class EFlags : uint8
	{
		None		= 0,			// No flags
		Mutable		= 1 << 0,		// Parameter is mutable, so can be mutated at runtime
		Batched		= 1 << 1,		// Parameter is batched, so represents a number of values rather than a single value. Batch size is determined by NumElements and associated FContext
		Stored		= 1 << 2,		// Parameter has to be stored on context storage
		
		// --- Flags added for parameter storage prototype ---
		Value		= 1 << 3,		// Parameter will be stored as a Value
		Reference	= 1 << 4,		// Parameter will be stored as a Reference (pointer)
		Embedded	= 1 << 5,		// Parameter will be stored as a Value, but stored directly on the Data pointer
	};

	// Get the type of this param 
	const FParamType& GetType() const;

	// Check whether the supplied param can be written to by this param
	// Verifies that types are compatible, destination is mutable and batching matches
	bool CanAssignTo(const FParam& InParam) const;

	// Helper function for CanAssignTo
	bool CanAssignWith(const FParamType& InType, EFlags InFlags, int32 InNumelements, FStringBuilderBase* OutReasonPtr = nullptr) const;

	bool IsMutable() const { return EnumHasAnyFlags(Flags, EFlags::Mutable); }

	bool IsBatched() const { return EnumHasAnyFlags(Flags, EFlags::Batched); }

	int32 GetNumElements() const { return NumElements; }

	EFlags GetFlags() const { return Flags; }
	
	void* GetData() const { return Data; }

protected:
	friend struct FContext;
	friend struct FState;
	friend struct FKernel;
	friend struct FParamStorage;
	
	FParam(const FParam* InOtherParam);
	FParam(const FParamType& InType, void* InData, EFlags InFlags);
	FParam(const FParamType& InType, void* InData, int32 InNumElements, EFlags InFlags);
	FParam(const FParamType& InType, EFlags InFlags);

	void* Data = nullptr;
	int32 NumElements = 0;
	int16 TypeId = INDEX_NONE;
	EFlags Flags = EFlags::None;

public:
	// Internal use, but required for default constructed elements on containers
	FParam() = default;

	static FParam MakeParam_Internal(const FParamType& InType, void* InData, int32 InNumElements, EFlags InFlags)
	{
		return FParam(InType, InData, InNumElements, InFlags);
	}
};

ENUM_CLASS_FLAGS(FParam::EFlags);


// Simple type representation for anim interfaces
struct ANIMNEXTINTERFACE_API FParamType
{
	typedef FParam(*ParamCopyFunction)(const FParam& InSource, void* InMemory, int32 InAllocatedMemory);

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

	ParamCopyFunction GetParamCopyFunction() const
	{
		return ParamCopyFunctionPtr;
	}

	struct ANIMNEXTINTERFACE_API FRegistrar
	{
		// Helper for registration (via IMPLEMENT_DATA_INTERFACE_PARAM_TYPE).
		// Registers the type via deferred callback
		FRegistrar(TUniqueFunction<void(void)>&& InFunction);

		// Helper for registration (via IMPLEMENT_DATA_INTERFACE_PARAM_TYPE)
		static void RegisterType(FParamType& InType, const UScriptStruct* InStruct, FName InName, uint32 InSize, uint32 InAlignment, const ParamCopyFunction InParamCopyFunctionPtr);

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
	ParamCopyFunction ParamCopyFunctionPtr = nullptr;
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
		static_assert(sizeof(ValueType) > 0, "Unimplemented type - please include the file AnimNextInterfaceTypes.h or where DECLARE_DATA_INTERFACE_PARAM_TYPE is defined for your type");
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
public:
	FORCEINLINE_DEBUGGABLE bool IsValid() const
	{
		return ((Data != nullptr || EnumHasAnyFlags(Flags, FParam::EFlags::Embedded))
			&& TypeId != INDEX_NONE 
			&& NumElements > 0);
	}

	FORCEINLINE_DEBUGGABLE const ValueType& operator[](int32 InIndex) const
	{
		check(InIndex >= 0 && (InIndex < NumElements || NumElements == 1));
		check(Data != nullptr || EnumHasAnyFlags(Flags, FParam::EFlags::Embedded));

		const ValueType* ValueData = EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)
			? static_cast<const ValueType*>((void*)&Data)
			: static_cast<const ValueType*>(Data);
		return (NumElements > InIndex) ? ValueData[InIndex] : ValueData[0];
	}

	FORCEINLINE_DEBUGGABLE ValueType& operator[](int32 InIndex)
	{
		check(InIndex >= 0 && (InIndex < NumElements || NumElements == 1));
		check(Data != nullptr || EnumHasAnyFlags(Flags, FParam::EFlags::Embedded));

		ValueType* ValueData = EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)
			? static_cast<ValueType*>((void*) &Data)
			: static_cast<ValueType*>(Data);
		return (NumElements > InIndex) ? ValueData[InIndex] : ValueData[0];
	}

	FORCEINLINE_DEBUGGABLE operator const ValueType& () const
	{
		check((Data != nullptr || EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)) && NumElements == 1);

		const ValueType* ValueData = EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)
			? static_cast<const ValueType*>((void*)&Data)
			: static_cast<const ValueType*>(Data);

		return *ValueData;
	}

	FORCEINLINE_DEBUGGABLE operator ValueType& ()
	{
		check((Data != nullptr || EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)) && NumElements == 1);

		ValueType* ValueData = EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)
			? static_cast<ValueType*>((void*)&Data)
			: static_cast<ValueType*>(Data);

		return *ValueData;
	}

	FORCEINLINE_DEBUGGABLE ValueType& operator*() const
	{
		check((Data != nullptr || EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)) && NumElements == 1);

		ValueType* ValueData = EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)
			? static_cast<ValueType*>((void*)&Data)
			: static_cast<ValueType*>(Data);

		return *ValueData;
	}

	FORCEINLINE_DEBUGGABLE ValueType* operator->() const
	{
		check((Data != nullptr || EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)) && NumElements == 1);

		ValueType* ValueData = EnumHasAnyFlags(Flags, FParam::EFlags::Embedded)
			? static_cast<ValueType*>((void*)&Data)
			: static_cast<ValueType*>(Data);

		return ValueData;
	}

protected:
	using TValueType = ValueType;
	
	friend struct FContext;
	friend struct FState;
	friend struct FKernel;
	friend struct FParamStorage;
	
	TParam(EFlags InFlags)
		: FParam(Private::TParamType<ValueType>::GetType(), InFlags)
	{
	}

	TParam(void* InData, EFlags InFlags)
		: FParam(Private::TParamType<ValueType>::GetType(), InData, InFlags)
	{
	}

	TParam(const void* InData, EFlags InFlags)
		: FParam(Private::TParamType<ValueType>::GetType(), const_cast<void*>(InData), InFlags)
	{
	}

	TParam(void* InData, int32 InNumElements, EFlags InFlags)
		: FParam(Private::TParamType<ValueType>::GetType(), InData, InNumElements, InFlags)
	{
	}

	TParam(const void* InData, int32 InNumElements, EFlags InFlags)
		: FParam(Private::TParamType<ValueType>::GetType(), const_cast<void*>(InData), InNumElements, InFlags)
	{
	}

	TParam(FParam& InParam, EFlags InAdditionalFlags = EFlags::None)
		: FParam(&InParam)
	{
#if DO_CHECK
		TStringBuilder<64> Error;
		checkf(InParam.CanAssignWith(Private::TParamType<ValueType>::GetType(), GetTypeFlags(InAdditionalFlags), NumElements, &Error),
			TEXT("Cannot assign type: %s"), Error.ToString());
#endif
	}

	TParam(const FParam& InParam, EFlags InAdditionalFlags = EFlags::None)
		: FParam(&InParam)
	{
#if DO_CHECK
		TStringBuilder<64> Error;
		checkf(InParam.CanAssignWith(Private::TParamType<ValueType>::GetType(), GetTypeFlags(InAdditionalFlags), NumElements, &Error),
			TEXT("Cannot assign type: %s"), Error.ToString());
#endif
	}

protected:
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

template<typename ValueType>
struct TParamCopyBaseTemplate
{
	static FParam CopyParam(const FParam& InSource, void* InTargetMemory, int32 InTargetAllocatedSize)
	{
		const FParamType& ParamType = InSource.GetType();
		
		const int32 NumElem = InSource.GetNumElements();
		const UScriptStruct* ScriptStruct = ParamType.GetStruct();

		if (ScriptStruct != nullptr)
		{
			const int32 StructSize = ScriptStruct->GetStructureSize();
			check(NumElem * StructSize <= InTargetAllocatedSize);

			ScriptStruct->CopyScriptStruct(InTargetMemory, InSource.GetData(), NumElem);
		}
		else
		{
			const int32 ParamAlignment = ParamType.GetAlignment();
			const int32 ParamSize = ParamType.GetSize();
			const int32 ParamAllocSize = Align(ParamSize, ParamAlignment);

			// TODO : check if this is safe to do for any type not containing a script struct, else we can specialize for POD types
			FMemory::Memcpy(InTargetMemory, InSource.GetData(), NumElem * ParamAllocSize);
		}

		return FParam::MakeParam_Internal(ParamType, InTargetMemory, NumElem, InSource.GetFlags());
	}
};

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

	FORCEINLINE static const FParamType::ParamCopyFunction GetParamCloneFunction()
	{
		return &TParamCopyBaseTemplate<ValueType>::CopyParam;
	}
};

}

// A typed param that wraps a user ptr to single or batched elements
template<typename ValueType>
struct TWrapParam : TParam<ValueType>
{
public:
	TWrapParam(ValueType* InValuePtrToWrap)
		: TParam<ValueType>(GetDataForConstructor(InValuePtrToWrap), GetFlagsForConstructor(InValuePtrToWrap))
	{
	}

	TWrapParam(ValueType& InValueToWrap)
		: TParam<ValueType>(GetDataForConstructor(&InValueToWrap), GetFlagsForConstructor(&InValueToWrap))
	{
	}

	TWrapParam(ValueType* InValuePtrToWrap, const int32 NumElements)
		: TParam<ValueType>(InValuePtrToWrap, NumElements, NumElements > 1 ? FParam::EFlags::Batched : FParam::EFlags::None)
	{
	}

	TWrapParam(const TArrayView<ValueType>& InArrayToWrap)
		: TParam<ValueType>(
			InArrayToWrap.GetData(), 
			InArrayToWrap.Num(), 
			((InArrayToWrap.Num()) > 1 ? FParam::EFlags::Batched : FParam::EFlags::None) | ((InArrayToWrap.Num() > 0) ? GetFlagsForConstructor(&InArrayToWrap[0]) : FParam::EFlags::None))
	{
	}

protected:
	void* GetDataForConstructor(ValueType* InValuePtrToWrap) const
	{
		using MutableValueType = typename TRemoveConst<ValueType>::Type;

		// Support containers with Num() and GetData()
		if constexpr (TModels<Private::CSizedContainerWithAccessibleDataAsRawPtr, ValueType>::Value)
		{
			return (void*)(InValuePtrToWrap->GetData());
		}
		else
		{
			return const_cast<MutableValueType*>(InValuePtrToWrap);
		}
	}

	FParam::EFlags GetFlagsForConstructor(const ValueType* InValuePtrToWrap) const
	{
		FParam::EFlags NewFlags = FParam::EFlags::None;

		// Add batched flags or not
		if constexpr (TModels<Private::CSizedContainerWithAccessibleDataAsRawPtr, ValueType>::Value)
		{
			NewFlags |= (InValuePtrToWrap->Num() > 1) ? FParam::EFlags::Batched : FParam::EFlags::None;
		}

		// Add const flags or not
		if constexpr (!TIsConst<ValueType>::Value)
		{
			NewFlags |= FParam::EFlags::Mutable;
		}

		return NewFlags;
	}
};

// A typed param that owns it's own memory with size defined at compile time
template<typename ValueType, int32 NumElem = 1>
struct TParamValue : TParam<ValueType>
{
public:
	TParamValue()
		: TParam<ValueType>(&ValueArray[0], NumElem, GetFlagsForConstructor())
	{
	}

	TParamValue(FParam::EFlags InFlags)
		: TParam<ValueType>(&ValueArray[0], NumElem, InFlags)
	{
	}

private:

	FParam::EFlags GetFlagsForConstructor() const
	{
		FParam::EFlags NewFlags = FParam::EFlags::None;

		NewFlags |= (NumElem > 1) ? FParam::EFlags::Batched : FParam::EFlags::None;

		// Add const flags or not
		if constexpr (!TIsConst<ValueType>::Value)
		{
			NewFlags |= FParam::EFlags::Mutable;
		}

		return NewFlags;
	}

	ValueType ValueArray[NumElem];
};

template<typename ValueType>
struct TContextStorageParam : TWrapParam<ValueType>
{
public:
	TContextStorageParam(ValueType* InValuePtrToWrap)
		: TWrapParam<ValueType>(InValuePtrToWrap)
	{
		FParam::Flags |= FParam::EFlags::Stored;
	}

	TContextStorageParam(ValueType& InValueToWrap)
		: TWrapParam<ValueType>(&InValueToWrap)
	{
		FParam::Flags |= FParam::EFlags::Stored;
	}

	TContextStorageParam(ValueType* InValuePtrToWrap, const int32 NumElements)
		: TWrapParam<ValueType>(InValuePtrToWrap, NumElements)
	{
		FParam::Flags |= FParam::EFlags::Stored;
	}

	TContextStorageParam(const TArrayView<ValueType>& InArrayToWrap)
		: TWrapParam<ValueType>(InArrayToWrap)
	{
		FParam::Flags |= FParam::EFlags::Stored;
	}
};

// --- ---

typedef int32 FParamHandle;
constexpr FParamHandle InvalidParamHandle = -1;

struct ANIMNEXTINTERFACE_API FHParam
{
	FHParam() = default;
	~FHParam();

	FHParam(FHParam&& Other)
		: FHParam()
	{
		Swap(*this, Other);
	}

	FHParam& operator= (FHParam&& Other)
	{
		Swap(*this, Other);
		return *this;
	}

	FHParam(const FHParam& Other);

	FHParam& operator= (const FHParam& Other)
	{
		FHParam Temp(Other); // note that the ref count is incremented here

		Swap(*this, Temp);

		return *this;
	}

protected:
	friend struct FContext;
	friend struct FParamStorage;

	FHParam(FParamStorage* InOwnerStorage, FParamHandle InHandle);

	FParamStorage* OwnerStorage = nullptr;
	FParamHandle ParamHandle = InvalidParamHandle;

private:
};

} // end namespace UE::AnimNext::Interface
