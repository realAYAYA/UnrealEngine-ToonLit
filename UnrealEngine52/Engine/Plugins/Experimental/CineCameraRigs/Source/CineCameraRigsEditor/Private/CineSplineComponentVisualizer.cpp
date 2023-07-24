// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineSplineComponentVisualizer.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "SceneView.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "SplineComponentVisualizer"

void FCineSplineComponentVisualizer::OnRegister()
{
	FSplineComponentVisualizer::OnRegister();
}

void FCineSplineComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FSplineComponentVisualizer::DrawVisualizationHUD(Component, Viewport, View, Canvas);
	if (Canvas == nullptr || View == nullptr)
	{
		return;
	}

	const FIntRect CanvasRect = Canvas->GetViewRect();
	const float HalfX = CanvasRect.Width() / 2.f;
	const float HalfY = CanvasRect.Height() / 2.f;


	if (const UCineSplineComponent* SplineComp = Cast<const UCineSplineComponent>(Component))
	{
		if (UCineSplineMetadata* Metadata = SplineComp->CineSplineMetadata)
		{
			const int32 NumOfPoints = SplineComp->GetNumberOfSplinePoints();
			for (int32 i = 0; i < NumOfPoints; ++i)
			{
				FVector Location = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
				FPlane Plane(0, 0, 0, 0);
				Plane = View->Project(Location);
				const FVector Position(Plane);
				const float DrawPositionX = FMath::FloorToFloat(HalfX + Position.X * HalfX);
				const float DrawPositionY = FMath::FloorToFloat(HalfY + -1.f * Position.Y * HalfY);
				
				FNumberFormattingOptions FmtOptions;
				FmtOptions.SetMaximumFractionalDigits(4);
				FmtOptions.SetMinimumFractionalDigits(1);
				const FText Text = FText::AsNumber(Metadata->AbsolutePosition.Points[i].OutVal, &FmtOptions);
				FLinearColor Color = FMath::IsNearlyEqual(FMath::Frac(Metadata->AbsolutePosition.Points[i].OutVal), 0.0f) ? FLinearColor(1,1,0,1) : FLinearColor(0, 1, 1, 1);
				Canvas->DrawShadowedString(DrawPositionX, DrawPositionY, *Text.ToString(), GEngine->GetLargeFont(), Color, FLinearColor::Black);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE