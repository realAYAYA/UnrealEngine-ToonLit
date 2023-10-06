// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavAgentSelector.generated.h"

USTRUCT()
struct FNavAgentSelector
{
	GENERATED_USTRUCT_BODY()

	static const uint32 InitializedBit = 0x80000000;
	static const uint32 AllAgentsMask = 0x7fffffff;

#if CPP
	union
	{
		struct
		{
#endif
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent0 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent1 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent2 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent3 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent4 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent5 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent6 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent7 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent8 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent9 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent10 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent11 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent12 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent13 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent14 : 1;
			UPROPERTY(EditAnywhere, Category = Default)
			uint32 bSupportsAgent15 : 1;
#if CPP
		};
		uint32 PackedBits;
	};
#endif

	ENGINE_API explicit FNavAgentSelector(const uint32 InBits = AllAgentsMask);

	FORCEINLINE bool Contains(int32 AgentIndex) const
	{
		return (AgentIndex >= 0 && AgentIndex < 16) ? !!(PackedBits & (1 << AgentIndex)) : false;
	}

	FORCEINLINE bool ContainsAnyAgent() const
	{
		return PackedBits & AllAgentsMask;
	}


	FORCEINLINE void Set(int32 AgentIndex)
	{
		if (AgentIndex >= 0 && AgentIndex < 16)
		{
			PackedBits |= (1 << AgentIndex);
		}
	}

	FORCEINLINE bool IsInitialized() const
	{
		return (PackedBits & InitializedBit) != 0;
	}

	FORCEINLINE void MarkInitialized()
	{
		PackedBits |= InitializedBit;
	}

	FORCEINLINE void Empty()
	{
		PackedBits = 0;
	}

	bool IsSame(const FNavAgentSelector& Other) const
	{
		return (~InitializedBit & PackedBits) == (~InitializedBit & Other.PackedBits);
	}

	ENGINE_API bool Serialize(FArchive& Ar);

	uint32 GetAgentBits() const 
	{
		return (~InitializedBit & PackedBits);
	}
};

template<>
struct TStructOpsTypeTraits< FNavAgentSelector > : public TStructOpsTypeTraitsBase2< FNavAgentSelector >
{
	enum
	{
		WithSerializer = true,
	};
	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

