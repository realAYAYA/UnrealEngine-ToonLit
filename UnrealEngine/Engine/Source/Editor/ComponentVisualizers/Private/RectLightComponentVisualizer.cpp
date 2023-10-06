// Copyright Epic Games, Inc. All Rights Reserved.

#include "RectLightComponentVisualizer.h"

#include "Components/ActorComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/EngineTypes.h"
#include "Math/Box.h"
#include "Math/Color.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "Templates/Casts.h"


void FRectLightComponentVisualizer::DrawVisualization( const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI )
{
	if(View->Family->EngineShowFlags.LightRadius)
	{
		const URectLightComponent* RectLightComp = Cast<const URectLightComponent>(Component);
		if(RectLightComp != NULL)
		{
			FTransform LightTM = RectLightComp->GetComponentTransform();
			LightTM.RemoveScaling();

			// Draw light radius
			DrawWireSphereAutoSides(PDI, LightTM, FColor(200, 255, 255), RectLightComp->AttenuationRadius, SDPG_World);

			FBox Box(
				FVector( 0.0f, -0.5f * RectLightComp->SourceWidth, -0.5f * RectLightComp->SourceHeight ),
				FVector( 0.0f,  0.5f * RectLightComp->SourceWidth,  0.5f * RectLightComp->SourceHeight )
			);

			const FColor ElementColor(231, 239, 0, 255);
			auto DrawBarnRect = [&](const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3)
			{
				FVector TP0 = LightTM.TransformPosition(P0);
				FVector TP1 = LightTM.TransformPosition(P1);
				FVector TP2 = LightTM.TransformPosition(P2);
				FVector TP3 = LightTM.TransformPosition(P3);
				PDI->DrawLine(TP0, TP1, ElementColor, SDPG_World);
				PDI->DrawLine(TP1, TP2, ElementColor, SDPG_World);
				PDI->DrawLine(TP2, TP3, ElementColor, SDPG_World);
				PDI->DrawLine(TP3, TP0, ElementColor, SDPG_World);
			};

			const float BarnMaxAngle = GetRectLightBarnDoorMaxAngle();
			const float AngleRad	= FMath::DegreesToRadians(FMath::Clamp(RectLightComp->BarnDoorAngle, 0.f, BarnMaxAngle));
			const float BarnDepth	= FMath::Cos(AngleRad) * RectLightComp->BarnDoorLength;
			const float BarnExtent	= FMath::Sin(AngleRad) * RectLightComp->BarnDoorLength;

			FVector Corners[8];

			// +SourceWidth
			{
				FVector P0(0.0f,		+0.5f * RectLightComp->SourceWidth,					-0.5f * RectLightComp->SourceHeight);
				FVector P1(0.0f,		+0.5f * RectLightComp->SourceWidth,					+0.5f * RectLightComp->SourceHeight);
				FVector P2(BarnDepth, 	+0.5f * RectLightComp->SourceWidth + BarnExtent,	+0.5f * RectLightComp->SourceHeight + BarnExtent);
				FVector P3(BarnDepth, 	+0.5f * RectLightComp->SourceWidth + BarnExtent,	-0.5f * RectLightComp->SourceHeight - BarnExtent);
				Corners[0] =  P3;
				Corners[1] =  P2;

				DrawBarnRect(P0, P1, P2, P3);
			}
			// +SourceHeight
			{
				FVector P0(0.0f,		-0.5f * RectLightComp->SourceWidth,	+0.5f * RectLightComp->SourceHeight);
				FVector P1(0.0f,		+0.5f * RectLightComp->SourceWidth,	+0.5f * RectLightComp->SourceHeight);
				FVector P2(BarnDepth,	+0.5f * RectLightComp->SourceWidth + BarnExtent,	+0.5f * RectLightComp->SourceHeight + BarnExtent);
				FVector P3(BarnDepth,	-0.5f * RectLightComp->SourceWidth - BarnExtent,	+0.5f * RectLightComp->SourceHeight + BarnExtent);
				Corners[2] =  P2;
				Corners[3] =  P3;

				DrawBarnRect(P0, P1, P2, P3);
			}
			// -SourceWidth
			{
				FVector P0(0.0f,		-0.5f * RectLightComp->SourceWidth,					-0.5f * RectLightComp->SourceHeight);
				FVector P1(0.0f,		-0.5f * RectLightComp->SourceWidth,					+0.5f * RectLightComp->SourceHeight);
				FVector P2(BarnDepth,	-0.5f * RectLightComp->SourceWidth - BarnExtent,	+0.5f * RectLightComp->SourceHeight + BarnExtent);
				FVector P3(BarnDepth,	-0.5f * RectLightComp->SourceWidth - BarnExtent,	-0.5f * RectLightComp->SourceHeight - BarnExtent);
				Corners[4] =  P2;
				Corners[5] =  P3;

				DrawBarnRect(P0, P1, P2, P3);
			}
			// -SourceHeight
			{
				FVector P0(0.0f,		-0.5f * RectLightComp->SourceWidth,	-0.5f * RectLightComp->SourceHeight);
				FVector P1(0.0f,		+0.5f * RectLightComp->SourceWidth,	-0.5f * RectLightComp->SourceHeight);
				FVector P2(BarnDepth,	+0.5f * RectLightComp->SourceWidth + BarnExtent,	-0.5f * RectLightComp->SourceHeight - BarnExtent);
				FVector P3(BarnDepth,	-0.5f * RectLightComp->SourceWidth - BarnExtent,	-0.5f * RectLightComp->SourceHeight - BarnExtent);
				Corners[6] =  P3;
				Corners[7] =  P2;

				DrawBarnRect(P0, P1, P2, P3);
			
			}

			DrawWireBox(PDI, LightTM.ToMatrixNoScale(), Box, ElementColor, SDPG_World);

		}
	}
}
