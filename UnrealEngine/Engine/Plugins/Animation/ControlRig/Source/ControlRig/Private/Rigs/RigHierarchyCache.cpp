// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchyCache.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigHierarchyCache)

const FRigElementKey& FCachedRigElement::GetResolvedKey() const
{
	if(Element)
	{
		return Element->GetKey();
	}
	static FRigElementKey InvalidKey;
	return InvalidKey;
}

bool FCachedRigElement::UpdateCache(const URigHierarchy* InHierarchy)
{
	if(InHierarchy)
	{
		if(!IsValid() || InHierarchy->GetTopologyVersionHash() != ContainerVersion || Element != InHierarchy->Get(Index))
		{
			return UpdateCache(GetKey(), InHierarchy);
		}
		return IsValid();
	}
	return false;
}

bool FCachedRigElement::UpdateCache(const FRigElementKey& InKey, const URigHierarchy* InHierarchy)
{
	if(InHierarchy)
	{
		if(!IsValid() || !IsIdentical(InKey, InHierarchy) || Element != InHierarchy->Get(Index))
		{
			// have to create a copy since Reset below
			// potentially resets the InKey as well.
			const FRigElementKey KeyToResolve = InKey;

			// first try to resolve with the known index.
			// this happens a lot - where the topology version has
			// increased - but the known item is still valid
			if(InHierarchy->IsValidIndex(Index))
			{
				if(const FRigBaseElement* PreviousElement = InHierarchy->Get(Index))
				{
					if(PreviousElement->GetKey() == KeyToResolve)
					{
						Key = KeyToResolve;
						Element = PreviousElement;
						ContainerVersion = InHierarchy->GetTopologyVersionHash();
						return IsValid();
					}
				}
			}

			int32 Idx = InHierarchy->GetIndex(KeyToResolve);
			if(Idx != INDEX_NONE)
			{
				Key = KeyToResolve;
				Index = (uint16)Idx;
				Element = InHierarchy->Get(Index);
			}
			else
			{
				Reset();
				Key = KeyToResolve;
			}

			ContainerVersion = InHierarchy->GetTopologyVersionHash();
		}
		return IsValid();
	}
	return false;
}

bool FCachedRigElement::IsIdentical(const FRigElementKey& InKey, const URigHierarchy* InHierarchy)
{
	return InKey == Key && InHierarchy->GetTopologyVersionHash() == ContainerVersion;
}

FRigElementKeyRedirector::FRigElementKeyRedirector(const TMap<FRigElementKey, FRigElementKey>& InMap, const URigHierarchy* InHierarchy)
{
	check(InHierarchy);
	InternalKeyToExternalKey.Reserve(InMap.Num());
	ExternalKeys.Reserve(InMap.Num());

	Hash = 0;
	for(const TPair<FRigElementKey, FRigElementKey>& Pair : InMap)
	{
		check(Pair.Key.IsValid());
		Add(Pair.Key, Pair.Value, InHierarchy);
	}
}

FRigElementKeyRedirector::FRigElementKeyRedirector(const FRigElementKeyRedirector& InOther, const URigHierarchy* InHierarchy)
{
	check(InHierarchy);
	InternalKeyToExternalKey.Reserve(InOther.InternalKeyToExternalKey.Num());
	ExternalKeys.Reserve(InOther.ExternalKeys.Num());

	Hash = 0;
	for(const TPair<FRigElementKey, FCachedRigElement>& Pair : InOther.InternalKeyToExternalKey)
	{
		check(Pair.Key.IsValid());
		Add(Pair.Key, Pair.Value.GetKey(), InHierarchy);
	}
}

const FRigElementKey* FRigElementKeyRedirector::FindReverse(const FRigElementKey& InKey) const
{
	for(const TPair<FRigElementKey, FCachedRigElement>& Pair : InternalKeyToExternalKey)
	{
		if(Pair.Value.GetKey() == InKey)
		{
			return &Pair.Key;
		}
	}
	return nullptr;
}

void FRigElementKeyRedirector::Add(const FRigElementKey& InSource, const FRigElementKey& InTarget, const URigHierarchy* InHierarchy)
{
	if(!InSource.IsValid() || InSource == InTarget)
	{
		return;
	}

	if (InTarget.IsValid())
	{
		InternalKeyToExternalKey.Add(InSource, FCachedRigElement(InTarget, InHierarchy, true));
	}
	ExternalKeys.Add(InSource, InTarget);
	Hash = HashCombine(Hash, HashCombine(GetTypeHash(InSource), GetTypeHash(InTarget)));
}

