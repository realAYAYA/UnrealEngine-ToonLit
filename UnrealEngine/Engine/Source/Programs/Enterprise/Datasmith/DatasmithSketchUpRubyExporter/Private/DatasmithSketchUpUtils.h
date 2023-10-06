// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithSketchUpCommon.h"


namespace DatasmithSketchUpUtils
{
	namespace FromSketchUp
	{
		static FORCEINLINE FVector ConvertDirection(const SUVector3D& V)
		{
			return FVector(float(V.x), float(-V.y), float(V.z));
		}

		static FORCEINLINE FVector3f ConvertPosition(double X, double Y, double Z)
		{
			const float UnitScaleSketchupToUnreal = 2.54; // centimeters per inch
			return FVector3f(float(X * UnitScaleSketchupToUnreal), float(-Y * UnitScaleSketchupToUnreal), float(Z * UnitScaleSketchupToUnreal));
		}

		static FORCEINLINE FVector3f ConvertPosition(const SUPoint3D& V)
		{
			return ConvertPosition(V.x, V.y, V.z);
		}

		static FORCEINLINE float ConvertDistance(const float& D)
		{
			const float UnitScaleSketchupToUnreal = 2.54; // centimeters per inch
			return D * UnitScaleSketchupToUnreal;
		}
	}

	DatasmithSketchUp::FEntityIDType GetEntityID(
		SUEntityRef InSuEntityRef // valid SketckUp component definition
	);

	DatasmithSketchUp::FComponentDefinitionIDType GetComponentID(
		SUComponentDefinitionRef InSComponentDefinitionRef // valid SketckUp component definition
	);

	DatasmithSketchUp::FComponentInstanceIDType GetComponentInstanceID(
		SUComponentInstanceRef ComponentInstanceRef
	);

	DatasmithSketchUp::FComponentInstanceIDType GetGroupID(
		SUGroupRef GroupRef
	);

	// Get the face ID of a SketckUp face.
	int32 GetFaceID(
		SUFaceRef InSFaceRef // valid SketckUp face
	);

	// Get the edge ID of a SketckUp edge.
	int32 GetEdgeID(
		SUEdgeRef InSEdgeRef // valid SketckUp edge
	);

	// Get the material ID of a SketckUp material.
	DatasmithSketchUp::FMaterialIDType GetMaterialID(
		SUMaterialRef InSMaterialRef // valid SketckUp material
	);

	// Get the camera ID of a SketckUp scene.
	DatasmithSketchUp::FEntityIDType GetSceneID(
		SUSceneRef InSSceneRef // valid SketckUp scene
	);

	// Get the component persistent ID of a SketckUp component instance.
	int64 GetComponentPID(
		SUComponentInstanceRef InSComponentInstanceRef // valid SketckUp component instance
	);

	// Return the effective layer of a SketckUp component instance.
	SULayerRef GetEffectiveLayer(
		SUComponentInstanceRef InSComponentInstanceRef, // valid SketckUp component instance
		SULayerRef             InSInheritedLayerRef     // SketchUp inherited layer
	);
	SULayerRef GetEffectiveLayer(SUDrawingElementRef DrawingElementRef, SULayerRef InInheritedLayerRef);

	// Return whether or not a SketckUp component instance is visible in the current SketchUp scene.
	bool IsVisible(
		SUComponentInstanceRef InSComponentInstanceRef, // valid SketckUp component instance
		SULayerRef             InSEffectiveLayerRef     // SketchUp component instance effective layer
	);

	// Return whether or not a SketckUp layer is visible in the current SketchUp scene taking into account folder visibility
	bool IsLayerVisible(
		SULayerRef LayerRef
	);

	// Get the material of a SketckUp component instance.
	SUMaterialRef GetMaterial(
		SUComponentInstanceRef InSComponentInstanceRef // valid SketckUp component instance
	);

	// Set the world transform of a Datasmith actor.
	void SetActorTransform(
		TSharedPtr<IDatasmithActorElement> IODActorPtr,      // Datasmith actor to transform
		SUTransformation const& InSWorldTransform // SketchUp world transform to apply
	);

	SUTransformation GetComponentInstanceTransform(SUComponentInstanceRef InSComponentInstanceRef, SUTransformation const& InSWorldTransform);
}
