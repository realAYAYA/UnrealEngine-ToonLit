// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchyCache.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigHierarchyCache)

bool FCachedRigElement::UpdateCache(const URigHierarchy* InHierarchy)
{
	if(InHierarchy)
	{
		if(!IsValid() || InHierarchy->GetTopologyVersion() != ContainerVersion)
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
		if(!IsValid() || !IsIdentical(InKey, InHierarchy))
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
						ContainerVersion = InHierarchy->GetTopologyVersion();
						return IsValid();
					}
				}
			}

			Reset();

			int32 Idx = InHierarchy->GetIndex(KeyToResolve);
			if(Idx != INDEX_NONE)
			{
				Key = KeyToResolve;
				Index = (uint16)Idx;
				Element = InHierarchy->Get(Index);
			}

			ContainerVersion = InHierarchy->GetTopologyVersion();
		}
		return IsValid();
	}
	return false;
}

bool FCachedRigElement::IsIdentical(const FRigElementKey& InKey, const URigHierarchy* InHierarchy)
{
	return InKey == Key && InHierarchy->GetTopologyVersion() == ContainerVersion;
}

