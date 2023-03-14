// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "DebugUtils/DebugDraw.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "NavMesh/RecastHelpers.h"

struct FRecastInternalDebugData : public duDebugDraw
{
	duDebugDrawPrimitives CurrentPrim;
	int32 FirstVertexIndex;

	TArray<uint32> TriangleIndices;
	TArray<FVector> TriangleVertices;
	TArray<FColor> TriangleColors;

	TArray<FVector> LineVertices;
	TArray<FColor>  LineColors;

	TArray<FVector> PointVertices;
	TArray<FColor>  PointColors;

	FRecastInternalDebugData() {}
	virtual ~FRecastInternalDebugData() override {}

	virtual void depthMask(bool state) override { /*unused*/ };
	virtual void texture(bool state) override { /*unused*/ };

	virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f) override
	{
		CurrentPrim = prim;
		FirstVertexIndex = TriangleVertices.Num();
	}

	virtual void vertex(const FVector::FReal* pos, unsigned int color) override
	{
		vertex(pos[0], pos[1], pos[2], color, 0.0f, 0.0f);
	}

	virtual void vertex(const FVector::FReal x, const FVector::FReal y, const FVector::FReal z, unsigned int color) override
	{
		vertex(x, y, z, color, 0.0f, 0.0f);
	}

	virtual void vertex(const FVector::FReal* pos, unsigned int color, const FVector::FReal* uv) override
	{
		vertex(pos[0], pos[1], pos[2], color, uv[0], uv[1]);
	}

	virtual void vertex(const FVector::FReal x, const FVector::FReal y, const FVector::FReal z, unsigned int color, const FVector::FReal u, const FVector::FReal v) override;

	virtual void end() override;
};

