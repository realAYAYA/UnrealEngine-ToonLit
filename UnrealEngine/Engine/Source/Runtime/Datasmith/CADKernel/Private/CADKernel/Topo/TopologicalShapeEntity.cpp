// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Topo/TopologicalShapeEntity.h"

namespace UE::CADKernel
{

void FTopologicalShapeEntity::CompleteMetaDataWithHostMetaData()
{
	if (HostedBy != nullptr)
	{
		Dictionary.CompleteDictionary(HostedBy->GetMetaDataDictionary());
	}
}

}
