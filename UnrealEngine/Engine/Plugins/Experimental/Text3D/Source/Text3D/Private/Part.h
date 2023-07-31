// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util.h"

/** Part represents point and it's next edge (counterclockwise one). */
struct FPart final
{
	static constexpr float CosMaxAngleSideTangent = 0.995f;
	static constexpr float CosMaxAngleSides = -0.9f;

	FPart();
	FPart(const FPartConstPtr& Other);


	/** Previous part. */
	FPartPtr Prev;

	/** Next part. */
	FPartPtr Next;


	/** Position, is equal to position of last vertex in paths (in coordinate system of glyph). */
	FVector2D Position;

	/** Offset in surface of front cap that this point already made. */
	float DoneExpand;


	FVector2D TangentX;


	/** Point normal, a bisector of angle. */
	FVector2D Normal;

	/** If true, previous and next edges are in one smoothing group. */
	bool bSmooth;


	FVector2D InitialPosition;


	// Paths along which triangulation is made. Values that are stored are indices of vertices. If bSmooth == true, both paths store one index (for same DoneExpand value). If not, indices are different.
	/** Path used for triangulation of previous edge. */
	TArray<int32> PathPrev;

	/** Path used for triangulation of next edge. */
	TArray<int32> PathNext;


	/** Offset needed for an IntersectionNear to happen. */
	float AvailableExpandNear;

	/** List of pairs (edge, offset) for IntersectionFar. */
	FAvailableExpandsFar AvailableExpandsFar;


	float TangentsDotProduct() const;
	float Length() const;

	void ResetDoneExpand();
	void ComputeTangentX();
	bool ComputeNormal();
	void ComputeSmooth();

	bool ComputeNormalAndSmooth();

	void ResetInitialPosition();
	void ComputeInitialPosition();

	void DecreaseExpandsFar(const float Delta);

	/**
	 * Compute position to which point will be expanded.
	 * @param Value - Offset in surface of front cap.
	 * @return Computed position.
	 */
	FVector2D Expanded(const float Value) const;
};
