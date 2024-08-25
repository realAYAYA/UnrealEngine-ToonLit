// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamTypeHandle.h"
#include "Param/ParamId.h"

class UAnimNextComponent;

namespace UE::AnimNext
{
	struct FParamStack;
	struct FUObjectLayer;
	struct FInstancedPropertyBagLayer;
	struct FParamStackLayerHandle;
	struct FRemappedLayer;
	struct FParamResult;
	struct FParamCompatibility;
}

namespace UE::AnimNext
{
	struct FParamStackLayer;
}

namespace UE::AnimNext::Private
{

enum class EParamFlags : uint8
{
	None = 0,				// No flags
	Mutable = 1 << 0,		// Parameter is mutable, so can be mutated after it has been created
	Embedded = 1 << 1,		// Parameter will be stored as a Value, but stored directly on the Data pointer
	Reference = 1 << 2,		// Parameter is a reference, so will not be embdedded even if it fits in the Data pointer
};

ENUM_CLASS_FLAGS(EParamFlags);

// Parameter memory wrapper used in the param stack
struct FParamEntry
{
	FParamEntry() = default;
	ANIMNEXT_API ~FParamEntry();
	ANIMNEXT_API FParamEntry(const FParamEntry& InOtherParam);
	ANIMNEXT_API FParamEntry& operator=(const FParamEntry& InOtherParam);
	ANIMNEXT_API FParamEntry(FParamEntry&& InOtherParam) noexcept;
	ANIMNEXT_API FParamEntry& operator=(FParamEntry&& InOtherParam) noexcept;

private:
	friend struct UE::AnimNext::FParamStack;
	friend struct UE::AnimNext::FParamStackLayer;
	friend struct UE::AnimNext::FUObjectLayer;
	friend struct UE::AnimNext::FInstancedPropertyBagLayer;
	friend struct UE::AnimNext::FParamStackLayerHandle;
	friend struct UE::AnimNext::FRemappedLayer;
	friend class ::UAnimNextComponent;
	template<typename ElementType, typename AllocatorType> friend class ::TArray;

	ANIMNEXT_API FParamEntry(const FParamId& InId, const FParamTypeHandle& InTypeHandle, TArrayView<uint8> InData, bool bInIsReference, bool bInIsMutable);

	// Get the type handle of this param 
	FParamTypeHandle GetTypeHandle() const { return TypeHandle; }

	// Check whether this parameter is able to be mutated
	bool IsMutable() const { return EnumHasAnyFlags(Flags, EParamFlags::Mutable); }

	// Check whether this parameter is of reference type (i.e. the parameter refers to user data owned outside of the parameter)
	bool IsReference() const { return EnumHasAnyFlags(Flags, EParamFlags::Reference); }

	// Check whether this parameter is embedded in the parameter (stored internally rather than as a ptr)
	bool IsEmbedded() const { return EnumHasAnyFlags(Flags, EParamFlags::Embedded); }

	// Get an immutable view of the parameter's data
	TConstArrayView<uint8> GetData() const
	{
		if (EnumHasAnyFlags(Flags, EParamFlags::Embedded))
		{
			return TConstArrayView<uint8>(reinterpret_cast<const uint8*>(&Data), Size);
		}
		else
		{
			return TConstArrayView<uint8>(static_cast<const uint8*>(Data), Size);
		}
	}

	FParamResult GetParamData(FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData) const;

	FParamResult GetParamData(FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const;

	// Get an mutable view of the parameter's data, returns an empty array view if this parameter is immutable
	TArrayView<uint8> GetMutableData()
	{
		if (IsMutable())
		{
			if (EnumHasAnyFlags(Flags, EParamFlags::Embedded))
			{
				return TArrayView<uint8>(reinterpret_cast<uint8*>(&Data), Size);
			}
			else
			{
				return TArrayView<uint8>(static_cast<uint8*>(Data), Size);
			}
		}
		return TArrayView<uint8>();
	}

	FParamResult GetMutableParamData(FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData);

	FParamResult GetMutableParamData(FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility);

	// Check whether this param represents valid data. Note that this doesn't check the type hande for validity.
	bool IsValid() const
	{
		return Size > 0;
	}

	// Get the name of this parameter
	FName GetName() const
	{
		return Id.GetName();
	}

	// Get the hash of this parameter
	uint32 GetHash() const
	{
		return Id.GetHash();
	}

	// Get the Id of this parameter
	const FParamId& GetId() const
	{
		return Id;
	}
	
	// Raw ptr to the data, or the data itself if we have EFlags::Embedded
	void* Data = nullptr;

	// ID of the parameter
	FParamId Id;
	
	// The type of the param
	FParamTypeHandle TypeHandle;

	// Size of the data
	uint16 Size = 0;

	// Internal flags
	EParamFlags Flags = EParamFlags::None;
};

}