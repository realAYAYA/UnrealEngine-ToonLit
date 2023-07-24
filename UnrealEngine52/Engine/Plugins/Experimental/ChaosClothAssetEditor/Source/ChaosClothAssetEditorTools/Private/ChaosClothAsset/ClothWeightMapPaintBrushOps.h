// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sculpting/MeshBrushOpBase.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "MeshWeights.h"
#include "Async/ParallelFor.h"
#include "ClothWeightMapPaintBrushOps.generated.h"




class CHAOSCLOTHASSETEDITORTOOLS_API FMeshVertexWeightMapEditBrushOp : public FMeshSculptBrushOp
{
public:

	// not supported for this kind of brush op
	virtual void ApplyStamp(
		const FDynamicMesh3* Mesh,
		const FSculptBrushStamp& Stamp,
		const TArray<int32>& Vertices,
		TArray<FVector3d>& NewColorsOut) override
	{
		check(false);
	}


	virtual void ApplyStampByVertices(
		const FDynamicMesh3* Mesh,
		const FSculptBrushStamp& Stamp,
		const TArray<int32>& Vertices,
		TArray<double>& NewAttributesOut) = 0;

};






UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UWeightMapEraseBrushOpProps : public UMeshSculptBrushOpProps
{
	GENERATED_BODY()
public:
	/** Attribute to set as Erased value */
	UPROPERTY(EditAnywhere, Category = EraseBrush, meta = (DisplayName = "Erase Attribute", UIMin = 0))
	double AttributeValue = 0;

	virtual double GetAttribute() { return AttributeValue; }
};



class FWeightMapEraseBrushOp : public FMeshVertexWeightMapEditBrushOp
{
public:

	virtual void ApplyStampByVertices(
		const FDynamicMesh3* Mesh,
		const FSculptBrushStamp& Stamp,
		const TArray<int32>& Vertices,
		TArray<double>& NewAttributesOut) override
	{
		const FVector3d& StampPos = Stamp.LocalFrame.Origin;

		UWeightMapEraseBrushOpProps* Props = GetPropertySetAs<UWeightMapEraseBrushOpProps>();
		double EraseAttribute = (double)Props->GetAttribute();

		// TODO: Add something here to get the old value so we can subtract (clamped) the AttributeValue from it.

		// TODO: Handle the stamp's properties for fall off, etc..

		int32 NumVertices = Vertices.Num();
		for (int32 k = 0; k < NumVertices; ++k)
		{
			NewAttributesOut[k] = 0.0;
		}
	}
};




UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UWeightMapPaintBrushOpProps : public UMeshSculptBrushOpProps
{
	GENERATED_BODY()
public:
	/** The Attribute that will be assigned to triangles within the brush region */
	UPROPERTY(EditAnywhere, Category = PaintBrush, meta = (DisplayName = "Attribute", UIMin = 0))
	double AttributeValue = 1;

	virtual double GetAttribute() { return AttributeValue; }
};



class FWeightMapPaintBrushOp : public FMeshVertexWeightMapEditBrushOp
{
public:

	virtual void ApplyStampByVertices(
		const FDynamicMesh3* Mesh,
		const FSculptBrushStamp& Stamp,
		const TArray<int32>& Vertices,
		TArray<double>& NewAttributesOut) override
	{
		const FVector3d& StampPos = Stamp.LocalFrame.Origin;

		UWeightMapPaintBrushOpProps* Props = GetPropertySetAs<UWeightMapPaintBrushOpProps>();
		double SetToAttribute = (double)Props->GetAttribute();

		// TODO: Add something here to get the old value so we can add (clamped) the AttributeValue from it.

		// TODO: Handle the stamp's properties for fall off, etc..

		int32 NumVertices = Vertices.Num();
		for ( int32 k = 0; k < NumVertices; ++k)
		{
			NewAttributesOut[k] = FMath::Clamp(SetToAttribute, 0.0, 1.0);
		}
	}
};
