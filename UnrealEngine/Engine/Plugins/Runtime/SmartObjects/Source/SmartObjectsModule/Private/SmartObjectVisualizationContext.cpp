// Copyright Epic Games, Inc. All Rights Reserved.
#include "SmartObjectVisualizationContext.h"
#include "CanvasTypes.h"
#include "SceneView.h"
#include "SceneManagement.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectTypes.h"

#if WITH_EDITOR

FSmartObjectVisualizationContext::FSmartObjectVisualizationContext(const USmartObjectDefinition& InDefinition, const UWorld& InWorld)
	: Definition(InDefinition)
	, World(InWorld)
{
	PreviewValidationFilterClass = InDefinition.GetPreviewValidationFilterClass();
	if (PreviewValidationFilterClass.Get() == nullptr)
	{
		PreviewValidationFilterClass = USmartObjectSlotValidationFilter::StaticClass(); 
	}
}

bool FSmartObjectVisualizationContext::IsValidForDraw() const
{
	return View != nullptr
		&& PDI != nullptr
		&& Font != nullptr;
}

bool FSmartObjectVisualizationContext::IsValidForDrawHUD() const
{
	return View != nullptr
		&& Canvas != nullptr
		&& Font != nullptr;
}

FVector2D FSmartObjectVisualizationContext::Project(const FVector& Location) const
{
	if (View != nullptr && Canvas != nullptr)
	{
		Location.DiagnosticCheckNaN();
		const FPlane V = View->Project(Location);

		const FIntRect CanvasRect = Canvas->GetViewRect();
		const FVector::FReal HalfWidth = CanvasRect.Width() * 0.5;
		const FVector::FReal HalfHeight = CanvasRect.Height() * 0.5;

		return FVector2D(FMath::FloorToFloat((1 + V.X) * HalfWidth), FMath::FloorToFloat((1 - V.Y) * HalfHeight));
	}
	
	return FVector2D::ZeroVector;
}

bool FSmartObjectVisualizationContext::IsLocationVisible(const FVector& Location) const
{
	return View != nullptr && View->ViewFrustum.IntersectPoint(Location);
}

FVector::FReal FSmartObjectVisualizationContext::GetDistanceToCamera(const FVector& Location) const
{
	if (View)
	{
		return FVector::Distance(Location, View->ViewLocation);
	}
	return UE_DOUBLE_BIG_NUMBER;
}

void FSmartObjectVisualizationContext::DrawString(const float StartX, const float StartY, const TCHAR* Text, const FLinearColor& Color, const FLinearColor& ShadowColor) const
{
	if (Canvas && Font)
	{
		Canvas->DrawShadowedString(StartX, StartY, Text, Font, Color, ShadowColor);
	}
}

void FSmartObjectVisualizationContext::DrawString(const FVector& Location, const TCHAR* Text, const FLinearColor& Color, const FLinearColor& ShadowColor) const
{
	if (Canvas
		&& Font
		&& IsLocationVisible(Location))
	{
		const FVector2D ScreenPos = Project(Location);

		int32 SizeX = 0;
		int32 SizeY = 0;
		StringSize(Font, SizeX, SizeY, Text);

		Canvas->DrawShadowedString(ScreenPos.X - SizeX/2, ScreenPos.Y - SizeY/2, Text, Font, Color, ShadowColor);
	}
}

void FSmartObjectVisualizationContext::DrawArrow(const FVector& Start, const FVector& End, const FLinearColor& Color, const float ArrowHeadLength, const float EndLocationInset,
													const uint8 DepthPriorityGroup, const float Thickness, const float DepthBias, bool bScreenSpace) const
{
	if (PDI == nullptr)
	{
		return;
	}
	
	const FVector Diff = End - Start;
	const FVector::FReal Length = Diff.Size();
	const FVector Dir = Length >  UE_KINDA_SMALL_NUMBER ? (Diff / Length) : FVector::ForwardVector;  
	const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector);

	const FVector::FReal StartLen = FMath::Min(EndLocationInset, Length / 2.0);
	const FVector::FReal EndLen = FMath::Max(Length - EndLocationInset, Length / 2.0);
	
	const FVector StartLoc = Start + Dir * StartLen;
	const FVector EndLoc = Start + Dir * EndLen;

	PDI->DrawTranslucentLine(StartLoc, EndLoc, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
	PDI->DrawTranslucentLine(EndLoc, EndLoc - Dir * ArrowHeadLength + Side * ArrowHeadLength * 0.5f, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
	PDI->DrawTranslucentLine(EndLoc, EndLoc - Dir * ArrowHeadLength - Side * ArrowHeadLength * 0.5f, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
}

#endif // WITH_EDITOR
