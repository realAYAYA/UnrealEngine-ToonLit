// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/Queue.h"
#include "SceneManagement.h"

/** Utility methods that allows Debug draw into the Chaos VD Editor */
class FChaosVDDebugDrawUtils
{
public:
	static void DrawArrowVector(FPrimitiveDrawInterface* PDI, const FVector& StartLocation, const FVector& EndLocation, FStringView DebugText, const FColor& Color, float ArrowSize = 10.0f);
	static void DrawPoint(FPrimitiveDrawInterface* PDI, const FVector& Location, FStringView DebugText, const FColor& Color, float Size = 10.0f);
	static void DrawText(FStringView StringToDraw, const FVector& Location, const FColor& Color);

	static void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas);

private:
	struct FChaosVDQueuedTextToDraw
	{
		FString Text;
		FVector WorldPosition;
		FLinearColor Color;
	};

	static TQueue<FChaosVDQueuedTextToDraw> TexToDrawQueue;
};
