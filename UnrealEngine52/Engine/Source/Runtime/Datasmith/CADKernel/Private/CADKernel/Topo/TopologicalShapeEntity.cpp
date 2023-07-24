// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Topo/TopologicalShapeEntity.h"

namespace UE::CADKernel
{

void FTopologicalShapeEntity::CompleteMetadata()
{
	if (HostedBy != nullptr)
	{
		Dictionary.CompleteDictionary(HostedBy->GetMetadataDictionary());
	}
}


#ifdef CADKERNEL_DEV
FInfoEntity& FTopologicalShapeEntity::GetInfo(FInfoEntity& Info) const
{
	return FTopologicalEntity::GetInfo(Info)
		.Add(TEXT("Hosted by"), (FEntity*) HostedBy)
		.Add(Dictionary);
}
#endif

}
