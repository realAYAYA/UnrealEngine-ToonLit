// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "Containers/SparseArray.h"
#include "Materials/MaterialInterface.h"

#include "LineSetComponent.generated.h"

class FPrimitiveSceneProxy;

struct FRenderableLine
{
	FRenderableLine()
		: Start(ForceInitToZero)
		, End(ForceInitToZero)
		, Color(ForceInitToZero)
		, Thickness(0.0f)
		, DepthBias(0.0f)
	{}

	FRenderableLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor, const float InThickness, const float InDepthBias = 0.0f)
		: Start(InStart)
		, End(InEnd)
		, Color(InColor)
		, Thickness(InThickness)
		, DepthBias(InDepthBias)
	{}

	FVector Start;
	FVector End;
	FColor Color;
	float Thickness;
	float DepthBias;
};

UCLASS()
class MODELINGCOMPONENTS_API ULineSetComponent : public UMeshComponent
{
	GENERATED_BODY()

public:

	ULineSetComponent();

	/** Specify material which handles lines */
	void SetLineMaterial(UMaterialInterface* InLineMaterial);

	/** Clear the line set */
	void Clear();

	/** Reserve enough memory for up to the given ID (for inserting via ID) */
	void ReserveLines(const int32 MaxID);

	/** Add a line to be rendered using the component. */
	int32 AddLine(const FRenderableLine& OverlayLine);

	/** Create and add a line to be rendered using the component. */
	inline int32 AddLine(const FVector& InStart, const FVector& InEnd, const FColor& InColor, const float InThickness, const float InDepthBias = 0.0f)
	{
		// This is just a convenience function to avoid client code having to know about FRenderableLine.
		return AddLine(FRenderableLine(InStart, InEnd, InColor, InThickness, InDepthBias));
	}

	/** Insert a line with the given ID to the overlay */
	void InsertLine(const int32 ID, const FRenderableLine& OverlayLine);

	/** Changes the start coordinates of a line */
	void SetLineStart(const int32 ID, const FVector& NewPostion);

	/** Changes the end coordinates of a line */
	void SetLineEnd(const int32 ID, const FVector& NewPostion);

	/** Sets the color of a line */
	void SetLineColor(const int32 ID, const FColor& NewColor);

	/** Sets the thickness of a line */
	void SetLineThickness(const int32 ID, const float NewThickness);


	/** Sets the color of all existing lines */
	void SetAllLinesColor(const FColor& NewColor);

	/** Sets the thickness of all existing lines */
	void SetAllLinesThickness(const float NewThickness);

	/** Rescales each line assuming that vertex 0 is the origin */
	void SetAllLinesLength(const float NewLength, bool bUpdateBounds = false);

	/** Remove a line from the set */
	void RemoveLine(const int32 ID);

	/** Queries whether a line with the given ID exists */
	bool IsLineValid(const int32 ID) const;


	// utility construction functions

	/**
	 * Add a set of lines for each index in a sequence
	 * @param NumIndices iterate from 0...NumIndices and call LineGenFunc() for each value
	 * @param LineGenFunc called to fetch the lines for an index, callee filles LinesOut array (reset before each call)
	 * @param LinesPerIndexHint if > 0, will reserve space for NumIndices*LinesPerIndexHint new lines
	 */
	void AddLines(
		int32 NumIndices,
		TFunctionRef<void(int32 Index, TArray<FRenderableLine>& LinesOut)> LineGenFunc,
		int32 LinesPerIndexHint = -1,
		bool bDeferRenderStateDirty = false);

private:

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	UPROPERTY()
	TObjectPtr<const UMaterialInterface> LineMaterial;

	UPROPERTY()
	mutable FBoxSphereBounds Bounds;

	UPROPERTY()
	mutable bool bBoundsDirty;

	TSparseArray<FRenderableLine> Lines;

	int32 AddLineInternal(const FRenderableLine& Line);

	friend class FLineSetSceneProxy;
};