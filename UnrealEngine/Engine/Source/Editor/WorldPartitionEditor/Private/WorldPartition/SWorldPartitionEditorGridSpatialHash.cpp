// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/SWorldPartitionEditorGridSpatialHash.h"
#include "WorldPartition/WorldPartitionEditorPerProjectUserSettings.h"
#include "WorldPartition/WorldPartitionEditorSpatialHash.h"
#include "WorldPartition/WorldPartition.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"

#define LOCTEXT_NAMESPACE "WorldPartitionEditor"

static inline FBox2D ToBox2D(const FBox& Box)
{
	return FBox2D(FVector2D(Box.Min), FVector2D(Box.Max));
}

void SWorldPartitionEditorGridSpatialHash::Construct(const FArguments& InArgs)
{
	SWorldPartitionEditorGrid2D::Construct(SWorldPartitionEditorGrid::FArguments().InWorld(InArgs._InWorld));
}

int32 SWorldPartitionEditorGridSpatialHash::GetSelectionSnap() const
{
	UWorldPartitionEditorSpatialHash* EditorSpatialHash = (UWorldPartitionEditorSpatialHash*)WorldPartition->EditorHash;
	
	int32 EffectiveCellSize = EditorSpatialHash->CellSize;
	for (const UWorldPartitionEditorSpatialHash::FCellNodeHashLevel& HashLevel : EditorSpatialHash->HashLevels)
	{
		const FVector2D CellScreenSize = WorldToScreen.TransformVector(FVector2D(EffectiveCellSize, EffectiveCellSize));
		if (CellScreenSize.X > 64)
		{
			break;
		}

		EffectiveCellSize *= 2;
	}

	return EffectiveCellSize;
}

int32 SWorldPartitionEditorGridSpatialHash::PaintGrid(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	UWorldPartitionEditorSpatialHash* EditorSpatialHash = (UWorldPartitionEditorSpatialHash*)WorldPartition->EditorHash;
	
	// Found the best cell size depending on the current zoom
	const int64 EffectiveCellSize = GetSelectionSnap();
	
	// Compute visible rect
	const FBox2D ViewRect(FVector2D(ForceInitToZero), AllottedGeometry.GetLocalSize());
	const FBox2D ViewRectWorld(ScreenToWorld.TransformPoint(ViewRect.Min), ScreenToWorld.TransformPoint(ViewRect.Max));

	FBox VisibleGridRectWorld(
		FVector(
			FMath::Max(FMath::FloorToFloat(EditorSpatialHash->EditorBounds.Min.X / EffectiveCellSize) * EffectiveCellSize, ViewRectWorld.Min.X),
			FMath::Max(FMath::FloorToFloat(EditorSpatialHash->EditorBounds.Min.Y / EffectiveCellSize) * EffectiveCellSize, ViewRectWorld.Min.Y),
			FMath::FloorToFloat(EditorSpatialHash->EditorBounds.Min.Z / EffectiveCellSize) * EffectiveCellSize
		),
		FVector(
			FMath::Min(FMath::CeilToFloat(EditorSpatialHash->EditorBounds.Max.X / EffectiveCellSize) * EffectiveCellSize, ViewRectWorld.Max.X),
			FMath::Min(FMath::CeilToFloat(EditorSpatialHash->EditorBounds.Max.Y / EffectiveCellSize) * EffectiveCellSize, ViewRectWorld.Max.Y),
			FMath::CeilToFloat(EditorSpatialHash->EditorBounds.Max.Z / EffectiveCellSize) * EffectiveCellSize
		)
	);

	// Shadow whole grid area
	{
		FSlateColorBrush ShadowBrush(FLinearColor::Black);
		FLinearColor ShadowColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));

		FPaintGeometry GridGeometry = AllottedGeometry.ToPaintGeometry(
			WorldToScreen.TransformPoint(FVector2D(VisibleGridRectWorld.Min)),
			WorldToScreen.TransformPoint(FVector2D(VisibleGridRectWorld.Max)) - WorldToScreen.TransformPoint(FVector2D(VisibleGridRectWorld.Min))
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			GridGeometry,
			&ShadowBrush,
			ESlateDrawEffect::None,
			ShadowColor
		);
	}

	// Paint minimap
	LayerId = PaintMinimap(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);

	// Paint grid lines
	if (ToBox2D(VisibleGridRectWorld).GetArea() > 0.0f)
	{
		const FLinearColor Color = FLinearColor(0.1f, 0.1f, 0.1f, 1.f);

		UE::Math::TIntVector2<int64> TopLeftW(
			FMath::FloorToFloat(VisibleGridRectWorld.Min.X / EffectiveCellSize) * EffectiveCellSize,
			FMath::FloorToFloat(VisibleGridRectWorld.Min.Y / EffectiveCellSize) * EffectiveCellSize
		);

		UE::Math::TIntVector2<int64> BottomRightW(
			FMath::CeilToFloat(VisibleGridRectWorld.Max.X / EffectiveCellSize) * EffectiveCellSize,
			FMath::CeilToFloat(VisibleGridRectWorld.Max.Y / EffectiveCellSize) * EffectiveCellSize
		);

		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(2);

		// Horizontal
		for (int64 i=TopLeftW.Y; i<=BottomRightW.Y; i+=EffectiveCellSize)
		{
			FVector2D LineStartH(TopLeftW.X, i);
			FVector2D LineEndH(BottomRightW.X, i);

			LinePoints[0] = WorldToScreen.TransformPoint(LineStartH);
			LinePoints[1] = WorldToScreen.TransformPoint(LineEndH);

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, ESlateDrawEffect::None, Color, false, 1.0f);
		}

		// Vertical
		for (int64 i=TopLeftW.X; i<=BottomRightW.X; i+=EffectiveCellSize)
		{
			FVector2D LineStartH(i, TopLeftW.Y);
			FVector2D LineEndH(i, BottomRightW.Y);

			LinePoints[0] = WorldToScreen.TransformPoint(LineStartH);
			LinePoints[1] = WorldToScreen.TransformPoint(LineEndH);

			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), LinePoints, ESlateDrawEffect::None, Color, false, 1.0f);
		}

		// Draw coordinates
		if ((EffectiveCellSize == EditorSpatialHash->CellSize) && GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>()->GetShowCellCoords())
		{
			const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FVector2D CellScreenSize = WorldToScreen.TransformVector(FVector2D(EffectiveCellSize, EffectiveCellSize));

			FSlateFontInfo CoordsFont;
			FVector2D DefaultCoordTextSize;
			bool bNeedsGradient = true;
			// Use top-left coordinate as default coord text
			const FString DefaultCoordText(FString::Printf(TEXT("(%lld,%lld)"), TopLeftW.X / EffectiveCellSize, TopLeftW.Y / EffectiveCellSize));
			for(int32 DesiredFontSize = 24; DesiredFontSize >= 8; DesiredFontSize -= 2)
			{
				CoordsFont = FCoreStyle::GetDefaultFontStyle("Bold", DesiredFontSize);
				DefaultCoordTextSize = FontMeasure->Measure(DefaultCoordText, CoordsFont);

				if (CellScreenSize.X > DefaultCoordTextSize.X)
				{
					bNeedsGradient = (DesiredFontSize == 8);
					break;
				}
			}

			if (CellScreenSize.X > DefaultCoordTextSize.X)
			{
				static float GradientDistance = 64.0f;
				float ColorGradient = bNeedsGradient ? FMath::Min((CellScreenSize.X - DefaultCoordTextSize.X) / GradientDistance, 1.0f) : 1.0f;
				const FLinearColor CoordTextColor(1.0f, 1.0f, 1.0f, ColorGradient);

				for (int64 y = TopLeftW.Y; y < BottomRightW.Y; y += EffectiveCellSize)
				{
					for (int64 x = TopLeftW.X; x < BottomRightW.X; x += EffectiveCellSize)
					{
						const FString CoordText(FString::Printf(TEXT("(%lld,%lld)"), x / EffectiveCellSize, y / EffectiveCellSize));
						const FVector2D CoordTextSize = FontMeasure->Measure(CoordText, CoordsFont);

						FSlateDrawElement::MakeText(
							OutDrawElements,
							++LayerId,
							AllottedGeometry.ToPaintGeometry(WorldToScreen.TransformPoint(FVector2D(x + EffectiveCellSize / 2, y + EffectiveCellSize / 2)) - CoordTextSize / 2, FVector2D(1,1)),
							FString::Printf(TEXT("(%lld,%lld)"), x / EffectiveCellSize, y / EffectiveCellSize),
							CoordsFont,
							ESlateDrawEffect::None,
							CoordTextColor
						);
					}
				}
			}
		}

		++LayerId;
	}

	return SWorldPartitionEditorGrid2D::PaintGrid(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);
}

#undef LOCTEXT_NAMESPACE