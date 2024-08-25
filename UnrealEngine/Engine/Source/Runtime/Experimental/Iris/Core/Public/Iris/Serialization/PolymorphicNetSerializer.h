// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetSerializer.h"
#include "UObject/ObjectMacros.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Containers/Array.h"

#include "PolymorphicNetSerializer.generated.h"

class UScriptStruct;
namespace UE::Net
{
	struct FReplicationStateDescriptor;
}

namespace UE::Net
{

struct FPolymorphicNetSerializerScriptStructCache
{
	enum : uint32 { RegisteredTypeBits = 5U };
	enum : uint32 { MaxRegisteredTypeCount = (1U << RegisteredTypeBits) - 1U };
	enum : uint32 { InvalidTypeIndex = 0U };

	struct FTypeInfo
	{
		const UScriptStruct* ScriptStruct;
		TRefCountPtr<const FReplicationStateDescriptor> Descriptor;
	};

	IRISCORE_API void InitForType(const UScriptStruct* InScriptStruct);
	inline const uint32 GetTypeIndex(const UScriptStruct* ScriptStruct) const;
	inline const FTypeInfo* GetTypeInfo(uint32 TypeIndex) const;
	inline bool CanHaveNetReferences() const { return EnumHasAnyFlags(CommonTraits, EReplicationStateTraits::HasObjectReference); }

private:
	TArray<FTypeInfo> RegisteredTypes;
	EReplicationStateTraits CommonTraits = EReplicationStateTraits::None;
};

}

USTRUCT()
struct FPolymorphicStructNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()

	UE::Net::FPolymorphicNetSerializerScriptStructCache RegisteredTypes;
};

USTRUCT()
struct FPolymorphicArrayStructNetSerializerConfig : public FPolymorphicStructNetSerializerConfig
{
	GENERATED_BODY()
};

namespace UE::Net
{

const FPolymorphicNetSerializerScriptStructCache::FTypeInfo* FPolymorphicNetSerializerScriptStructCache::GetTypeInfo(uint32 TypeIndex) const
{
	static_assert(InvalidTypeIndex == 0U, "Expected InvalidTypeIndex to be 0");
	if ((TypeIndex - 1U) >= static_cast<uint32>(RegisteredTypes.Num()))
	{
		return nullptr;
	}
	return &RegisteredTypes.GetData()[TypeIndex - 1U];
}

const uint32 FPolymorphicNetSerializerScriptStructCache::GetTypeIndex(const UScriptStruct* ScriptStruct) const
{
	if (ScriptStruct)
	{
		const int32 ArrayIndex =  RegisteredTypes.IndexOfByPredicate([&ScriptStruct](const FTypeInfo& TypeInfo) { return TypeInfo.ScriptStruct == ScriptStruct; } );
		if (ArrayIndex != INDEX_NONE)
		{
			return static_cast<uint32>(ArrayIndex + 1);
		}
	}

	return InvalidTypeIndex;
}

}
