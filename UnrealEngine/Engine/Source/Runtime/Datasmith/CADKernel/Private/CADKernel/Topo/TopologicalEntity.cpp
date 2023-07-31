// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Topo/TopologicalEntity.h"

namespace UE::CADKernel
{

#ifdef CADKERNEL_DEV
FInfoEntity& FTopologicalEntity::GetInfo(FInfoEntity& Info) const
{
	return FEntity::GetInfo(Info)
		.Add(TEXT("IsDeleted"), IsDeleted())
		.Add(TEXT("IsMesh"), IsMeshed());
}
#endif

}
