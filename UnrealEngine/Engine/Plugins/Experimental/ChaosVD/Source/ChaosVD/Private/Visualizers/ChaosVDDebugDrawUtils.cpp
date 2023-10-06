// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDDebugDrawUtils.h"

#include "CanvasItem.h"
#include "ChaosVDEditorSettings.h"
#include "DebugRenderSceneProxy.h"
#include "Engine/Engine.h"

TQueue<FChaosVDDebugDrawUtils::FChaosVDQueuedTextToDraw> FChaosVDDebugDrawUtils::TexToDrawQueue = TQueue<FChaosVDQueuedTextToDraw>();

void FChaosVDDebugDrawUtils::DrawArrowVector(FPrimitiveDrawInterface* PDI, const FVector& StartLocation, const FVector& EndLocation, FStringView DebugText, const FColor& Color, float ArrowSize)
{
	const FDebugRenderSceneProxy::FArrowLine VelocityArrowLine = { StartLocation, EndLocation, Color};
	VelocityArrowLine.Draw(PDI, ArrowSize);

	if (!DebugText.IsEmpty())
	{
		// Draw the text in the middle of the vector line
		const FVector VectorToDraw = EndLocation - StartLocation;
		const FVector TextWorldPosition = StartLocation + VectorToDraw  * 0.5f;
		DrawText(DebugText.GetData(), TextWorldPosition , Color);
	}
}

void FChaosVDDebugDrawUtils::DrawPoint(FPrimitiveDrawInterface* PDI, const FVector& Location, FStringView DebugText, const FColor& Color, float Size)
{
	if (DebugText.IsEmpty())
	{
		return;
	}

	PDI->DrawPoint(Location, Color, Size, ESceneDepthPriorityGroup::SDPG_World);

	if (!DebugText.IsEmpty())
	{
		DrawText(DebugText, Location, Color);
	}
}

void FChaosVDDebugDrawUtils::DrawText(FStringView StringToDraw, const FVector& Location, const FColor& Color)
{
	if (const UChaosVDEditorSettings* CVDEditorSettings = GetDefault<UChaosVDEditorSettings>())
	{
		if (CVDEditorSettings->bShowDebugText)
		{
			TexToDrawQueue.Enqueue({StringToDraw.GetData(), Location, Color });
		}
	}
}

void FChaosVDDebugDrawUtils::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	while (!TexToDrawQueue.IsEmpty())
	{
		FChaosVDQueuedTextToDraw TextToDraw;
		if (TexToDrawQueue.Dequeue(TextToDraw))
		{
			FVector2D PixelLocation;
			if (View.WorldToPixel(TextToDraw.WorldPosition, PixelLocation))
			{
				FCanvasTextItem TextItem(PixelLocation, FText::AsCultureInvariant(TextToDraw.Text), GEngine->GetSmallFont(), TextToDraw.Color);
				TextItem.Scale = FVector2D::UnitVector;
				TextItem.EnableShadow(FLinearColor::Black);
				TextItem.Draw(&Canvas);
			}
		}
	}
}
