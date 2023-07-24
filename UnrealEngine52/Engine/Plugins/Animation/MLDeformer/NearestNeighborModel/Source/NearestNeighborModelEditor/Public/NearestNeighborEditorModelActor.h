// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerGeomCacheActor.h"
#include "MLDeformerComponent.h"

class UMLDeformerComponent;
class UGeometryCacheComponent;

namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;

	using namespace UE::MLDeformer;

	enum : int32
	{
		ActorID_NearestNeighborActors = 6
	};

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborEditorModelActor
		: public FMLDeformerGeomCacheActor
	{
	public:
		FNearestNeighborEditorModelActor(const FConstructSettings& Settings);

		friend class FNearestNeighborEditorModel; 

	private:
		void SetGeometryCacheComponent(UGeometryCacheComponent* Component) { GeomCacheComponent = Component; }
		UGeometryCacheComponent* GetGeometryCacheComponent() const { return GeomCacheComponent; }

		void InitNearestNeighborActor(const int32 InPartId, const UMLDeformerComponent* InComponent);
		void TickNearestNeighborActor();

		int32 PartId = INDEX_NONE;
		const UMLDeformerComponent* MLDeformerComponent = nullptr;
	};
}	// namespace UE::NearestNeighborModel
