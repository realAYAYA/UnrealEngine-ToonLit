// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineUtil.h"

#include "Components/SplineComponent.h"
#include "SceneManagement.h" // FPrimitiveDrawInterface
#include "SceneView.h"
#include "Styling/StyleColors.h"
#include "ToolContextInterfaces.h" // IToolsContextRenderAPI

UE::Geometry::SplineUtil::FDrawSplineSettings::FDrawSplineSettings()
	: RegularColor(FStyleColors::White.GetSpecifiedColor().ToFColor(true))
	, SelectedColor(FStyleColors::AccentOrange.GetSpecifiedColor().ToFColor(true))
{
}

// Mostly copied from the editor-only FSplineComponentVisualizer
void UE::Geometry::SplineUtil::DrawSpline(const USplineComponent& SplineComp, IToolsContextRenderAPI& RenderAPI, const FDrawSplineSettings& Settings)
{
	const FSceneView* View = RenderAPI.GetSceneView();
	auto GetDashSize = [View](const FVector& Start, const FVector& End, float Scale) -> double
	{
		const double StartW = View->WorldToScreen(Start).W;
		const double EndW = View->WorldToScreen(End).W;

		const double WLimit = 10.0f;
		if (StartW > WLimit || EndW > WLimit)
		{
			return FMath::Max(StartW, EndW) * Scale;
		}

		return 0;
	};

	FPrimitiveDrawInterface* PDI = RenderAPI.GetPrimitiveDrawInterface();

	const FInterpCurveVector& SplineInfo = SplineComp.GetSplinePointsPosition();

	const float GrabHandleSize = 10.0f;

	const bool bShouldVisualizeScale = Settings.ScaleVisualizationWidth > 0;
	const float DefaultScale = Settings.ScaleVisualizationWidth;

	FVector OldKeyPos(0);
	FVector OldKeyRightVector(0);
	FVector OldKeyScale(0);

	const int32 NumPoints = SplineInfo.Points.Num();
	const int32 NumSegments = SplineInfo.bIsLooped ? NumPoints : NumPoints - 1;
	for (int32 KeyIdx = 0; KeyIdx < NumSegments + 1; KeyIdx++)
	{
		const FVector NewKeyPos = SplineComp.GetLocationAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);
		const FVector NewKeyRightVector = SplineComp.GetRightVectorAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);
		const FVector NewKeyUpVector = SplineComp.GetUpVectorAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);
		const FVector NewKeyScale = SplineComp.GetScaleAtSplinePoint(KeyIdx) * DefaultScale;

		const FColor KeyColor = (Settings.SelectedKeys && Settings.SelectedKeys->Contains(KeyIdx)) ? Settings.SelectedColor
			: Settings.RegularColor;

		// Draw the keypoint and up/right vectors
		if (KeyIdx < NumPoints)
		{
			if (bShouldVisualizeScale)
			{
				PDI->DrawLine(NewKeyPos, NewKeyPos - NewKeyRightVector * NewKeyScale.Y, KeyColor, SDPG_Foreground);
				PDI->DrawLine(NewKeyPos, NewKeyPos + NewKeyRightVector * NewKeyScale.Y, KeyColor, SDPG_Foreground);
				PDI->DrawLine(NewKeyPos, NewKeyPos + NewKeyUpVector * NewKeyScale.Z, KeyColor, SDPG_Foreground);

				const int32 ArcPoints = 20;
				FVector OldArcPos = NewKeyPos + NewKeyRightVector * NewKeyScale.Y;
				for (int32 ArcIndex = 1; ArcIndex <= ArcPoints; ArcIndex++)
				{
					float Sin;
					float Cos;
					FMath::SinCos(&Sin, &Cos, ArcIndex * PI / ArcPoints);
					const FVector NewArcPos = NewKeyPos + Cos * NewKeyRightVector * NewKeyScale.Y + Sin * NewKeyUpVector * NewKeyScale.Z;
					PDI->DrawLine(OldArcPos, NewArcPos, KeyColor, SDPG_Foreground);
					OldArcPos = NewArcPos;
				}
			}

			PDI->DrawPoint(NewKeyPos, KeyColor, GrabHandleSize, SDPG_Foreground);
		}

		// If not the first keypoint, draw a line to the previous keypoint.
		if (KeyIdx > 0)
		{
			const FColor LineColor = Settings.RegularColor;

			// For constant interpolation - don't draw ticks - just draw dotted line.
			if (SplineInfo.Points[KeyIdx - 1].InterpMode == CIM_Constant)
			{
				const double DashSize = GetDashSize(OldKeyPos, NewKeyPos, 0.03f);
				if (DashSize > 0.0f)
				{
					DrawDashedLine(PDI, OldKeyPos, NewKeyPos, LineColor, DashSize, SDPG_World);
				}
			}
			else
			{
				// Determine the colors to use
				const bool bKeyIdxLooped = (SplineInfo.bIsLooped && KeyIdx == NumPoints);
				const int32 BeginIdx = bKeyIdxLooped ? 0 : KeyIdx;
				const int32 EndIdx = KeyIdx - 1;
				const bool bBeginSelected = Settings.SelectedKeys && Settings.SelectedKeys->Contains(BeginIdx);
				const bool bEndSelected = Settings.SelectedKeys && Settings.SelectedKeys->Contains(BeginIdx);
				const FColor BeginColor = (bBeginSelected) ? Settings.SelectedColor : Settings.RegularColor;
				const FColor EndColor = (bEndSelected) ? Settings.SelectedColor : Settings.RegularColor;

				// Find position on first keyframe.
				FVector OldPos = OldKeyPos;
				FVector OldRightVector = OldKeyRightVector;
				FVector OldScale = OldKeyScale;

				// Then draw a line for each substep.
				constexpr int32 NumSteps = 20;
				constexpr float PartialGradientProportion = 0.75f;
				constexpr int32 PartialNumSteps = (int32)(NumSteps * PartialGradientProportion);
				const float SegmentLineThickness = 0;

				for (int32 StepIdx = 1; StepIdx <= NumSteps; StepIdx++)
				{
					const float StepRatio = StepIdx / static_cast<float>(NumSteps);
					const float Key = EndIdx + StepRatio;
					const FVector NewPos = SplineComp.GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
					const FVector NewRightVector = SplineComp.GetRightVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
					const FVector NewScale = SplineComp.GetScaleAtSplineInputKey(Key) * DefaultScale;

					// creates a gradient that starts partway through the selection
					FColor StepColor;
					if (bBeginSelected == bEndSelected)
					{
						StepColor = BeginColor;
					}
					else if (bBeginSelected && StepIdx > (NumSteps - PartialNumSteps))
					{
						const float LerpRatio = (1.0f - StepRatio) / PartialGradientProportion;
						StepColor = FMath::Lerp(BeginColor.ReinterpretAsLinear(), EndColor.ReinterpretAsLinear(), LerpRatio).ToFColor(false);
					}
					else if (bEndSelected && StepIdx <= PartialNumSteps)
					{
						const float LerpRatio = 1.0f - (StepRatio / PartialGradientProportion);
						StepColor = FMath::Lerp(BeginColor.ReinterpretAsLinear(), EndColor.ReinterpretAsLinear(), LerpRatio).ToFColor(false);
					}
					else
					{
						StepColor = Settings.RegularColor; // unselected
					}

					PDI->DrawLine(OldPos, NewPos, StepColor, SDPG_Foreground, SegmentLineThickness);
					if (bShouldVisualizeScale)
					{
						PDI->DrawLine(OldPos - OldRightVector * OldScale.Y, NewPos - NewRightVector * NewScale.Y, LineColor, SDPG_Foreground);
						PDI->DrawLine(OldPos + OldRightVector * OldScale.Y, NewPos + NewRightVector * NewScale.Y, LineColor, SDPG_Foreground);

						constexpr bool bVisualizeSplineInterpolatedVectors = false;
						if (bVisualizeSplineInterpolatedVectors)
						{
							const FVector NewUpVector = SplineComp.GetUpVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
							PDI->DrawLine(NewPos, NewPos + NewUpVector * Settings.ScaleVisualizationWidth * 0.5f, LineColor, SDPG_Foreground);
							PDI->DrawLine(NewPos, NewPos + NewRightVector * Settings.ScaleVisualizationWidth * 0.5f, LineColor, SDPG_Foreground);
						}
					}

					OldPos = NewPos;
					OldRightVector = NewRightVector;
					OldScale = NewScale;
				}
			}
		}

		OldKeyPos = NewKeyPos;
		OldKeyRightVector = NewKeyRightVector;
		OldKeyScale = NewKeyScale;
	}
}
