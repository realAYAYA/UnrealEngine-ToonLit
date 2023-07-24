// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "Containers/SparseArray.h"
#include "PointSetComponent.generated.h"

class FPrimitiveSceneProxy;


struct FRenderablePoint
{
	FRenderablePoint()
		: Position(ForceInitToZero)
		, Color(ForceInitToZero)
		, Size(0.f)
		, DepthBias(0.f)
	{}

	FRenderablePoint(const FVector& InPosition, const FColor& InColor, const float InSize, const float InDepthBias = 0.0f)
		: Position(InPosition)
		, Color(InColor)
		, Size(InSize)
		, DepthBias(InDepthBias)
	{}

	FVector Position;
	FColor Color;
	float Size;
	float DepthBias;
};


/**
 * UPointSetComponent is a Component that draws a set of points, as small squares.
 * Per-point Color and (view-space) Size is supported. Normals are not supported.
 * 
 * Points are inserted with an externally-defined ID, internally this is done via
 * a TSparseArray. This class allocates a contiguous TArray large enugh to hold the 
 * largest ID. Using ReservePoints() may be beneficial for huge arrays.
 *
 * The points are drawn as two triangles (ie a square) orthogonal to the view direction. 
 * The actual point size is calculated in the shader, and so a custom material must be used.
 */
UCLASS()
class MODELINGCOMPONENTS_API UPointSetComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UPointSetComponent();

	/** Specify material which handles points */
	void SetPointMaterial(UMaterialInterface* InPointMaterial);

	/** Clear all primitives */
	void Clear();

	/** Reserve enough memory for up to the given ID */
	void ReservePoints(const int32 MaxID);

	/** Add a point to be rendered using the component. */
	int32 AddPoint(const FRenderablePoint& OverlayPoint);

	/** Create and add a point to be rendered using the component. */
	int32 AddPoint(const FVector& InPosition, const FColor& InColor, const float InSize, const float InDepthBias = 0.0f)
	{
		// This is just a convenience function to avoid client code having to know about FRenderablePoint.
		return AddPoint(FRenderablePoint(InPosition, InColor, InSize, InDepthBias));
	}

	/** Insert a point with the given ID into the set. */
	void InsertPoint(const int32 ID, const FRenderablePoint& OverlayPoint);

	/** Retrieve a point with the given id. */
	const FRenderablePoint& GetPoint(const int32 ID);

	/** Sets the position of a point (assumes its existence). */
	void SetPointPosition(const int32 ID, const FVector& NewPosition);

	/** Sets the color of a point */
	void SetPointColor(const int32 ID, const FColor& NewColor);

	/** Sets the size of a point */
	void SetPointSize(const int32 ID, const float NewSize);

	/** Sets the color of all points currently in the set. */
	void SetAllPointsColor(const FColor& NewColor);

	/** Remove a point from the set. */
	void RemovePoint(const int32 ID);

	/** Queries whether a point with the given ID exists */
	bool IsPointValid(const int32 ID) const;

private:

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

	UPROPERTY()
	TObjectPtr<const UMaterialInterface> PointMaterial;

	UPROPERTY()
	mutable FBoxSphereBounds Bounds;

	UPROPERTY()
	mutable bool bBoundsDirty;

	TSparseArray<FRenderablePoint> Points;

	friend class FPointSetSceneProxy;
};
