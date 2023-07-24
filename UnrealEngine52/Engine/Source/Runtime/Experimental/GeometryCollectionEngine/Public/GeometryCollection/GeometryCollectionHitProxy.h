// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCollectionEditorSelection.h"

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
#include "EngineUtils.h"

class UGeometryCollectionComponent;

/** HitProxy with transform index information.
 * @todo: These are never actually drawn because the shader is always in per-vertex mode, where it is passed HGeometryCollectionBone (or the whole-object HActor)
 */
struct HGeometryCollection : public HActor
{
	DECLARE_HIT_PROXY(GEOMETRYCOLLECTIONENGINE_API)
	int32 TransformIndex;

	HGeometryCollection(AActor* InActor, const UPrimitiveComponent* InPrimitiveComponent, int32 InSectionIndex, int32 InMaterialIndex, int32 InTransformIndex)
		: HActor(InActor, InPrimitiveComponent, InSectionIndex, InMaterialIndex)
		, TransformIndex(InTransformIndex)
		{}
};


/** HitProxy with transform index information. */
// @todo FractureTools - This is temp, we should merge it with HGeometryCollection
struct HGeometryCollectionBone : public HHitProxy
{
	DECLARE_HIT_PROXY(GEOMETRYCOLLECTIONENGINE_API)

	UGeometryCollectionComponent* Component;
	int32 BoneIndex;

	HGeometryCollectionBone(UGeometryCollectionComponent* InGeometryCollectionComponent, int32 InBoneIndex)
		: Component(InGeometryCollectionComponent)
		, BoneIndex(InBoneIndex)
	{}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Component);
	}
};
#endif  // #if GEOMETRYCOLLECTION_EDITOR_SELECTION
