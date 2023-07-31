// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"

namespace UE::Net
{

enum class ENetReferenceCollectorTraits : uint32
{
	None								= 0U,
	IncludeInvalidReferences			= 1U,
	OnlyCollectReferencesThatCanBeExported = IncludeInvalidReferences << 1U,
};
ENUM_CLASS_FLAGS(ENetReferenceCollectorTraits);

class FNetReferenceCollector
{
public:
	struct FReferenceInfo
	{
		FNetReferenceInfo Info;
		FNetObjectReference Reference;
		FNetSerializerChangeMaskParam ChangeMaskInfo;
	};

	typedef TArray<FReferenceInfo, TInlineAllocator<32>> FReferenceInfoArray;

public:
	FNetReferenceCollector() : Traits(ENetReferenceCollectorTraits::None) {}
	explicit FNetReferenceCollector(ENetReferenceCollectorTraits InTraits) : Traits(InTraits) {}
	
	inline void Add(const FNetReferenceInfo& ReferenceInfo, const FNetObjectReference& Reference, const FNetSerializerChangeMaskParam& ChangeMaskInfo);

	const FReferenceInfoArray& GetCollectedReferences() const { return ReferenceInfos; }

	void Reset() { ReferenceInfos.Reset(); }

private:
	FReferenceInfoArray ReferenceInfos;	
	const ENetReferenceCollectorTraits Traits;
};

void FNetReferenceCollector::Add(const FNetReferenceInfo& ReferenceInfo, const FNetObjectReference& Reference, const FNetSerializerChangeMaskParam& ChangeMaskInfo)
{
	if (!Reference.IsValid() && !EnumHasAnyFlags(Traits, ENetReferenceCollectorTraits::IncludeInvalidReferences))
	{
		return;
	}

	if (!Reference.CanBeExported() && EnumHasAnyFlags(Traits, ENetReferenceCollectorTraits::OnlyCollectReferencesThatCanBeExported))
	{
		return;
	}

	FReferenceInfo RefInfo;
	RefInfo.Info = ReferenceInfo;
	RefInfo.Reference = Reference;
	RefInfo.ChangeMaskInfo = ChangeMaskInfo;

	ReferenceInfos.Add(MoveTemp(RefInfo));
}

}
