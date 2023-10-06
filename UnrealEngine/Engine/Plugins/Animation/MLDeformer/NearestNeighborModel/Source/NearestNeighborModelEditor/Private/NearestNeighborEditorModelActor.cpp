// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborEditorModelActor.h"
#include "NearestNeighborModelInstance.h"
#include "GeometryCacheComponent.h"

namespace UE::NearestNeighborModel
{
	using namespace UE::MLDeformer;

	FNearestNeighborEditorModelActor::FNearestNeighborEditorModelActor(const FConstructSettings& Settings)
		: FMLDeformerGeomCacheActor(Settings)
	{
	}

	void FNearestNeighborEditorModelActor::InitNearestNeighborActor(const int32 InPartId, const UMLDeformerComponent* InComponent)
	{
		PartId = InPartId;
		MLDeformerComponent = InComponent;
	}

	void FNearestNeighborEditorModelActor::TickNearestNeighborActor()
	{
		if (GeomCacheComponent && GeomCacheComponent->GetGeometryCache() && MLDeformerComponent)
		{
			const UNearestNeighborModelInstance* ModelInstance = static_cast<UNearestNeighborModelInstance*>(MLDeformerComponent->GetModelInstance());
			if (ModelInstance && PartId < ModelInstance->NeighborIdNum())
			{
				GeomCacheComponent->SetManualTick(true);
				GeomCacheComponent->TickAtThisTime(GeomCacheComponent->GetTimeAtFrame(ModelInstance->NearestNeighborId(PartId)), false, false, false);
			}
		}
	}
}	// namespace UE::NearestNeighborModel
