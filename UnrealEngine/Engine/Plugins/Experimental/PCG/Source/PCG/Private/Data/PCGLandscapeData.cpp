// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGLandscapeData.h"

#include "PCGHelpers.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPolyLineData.h"
#include "Data/PCGSpatialData.h"
#include "Grid/PCGLandscapeCache.h"
#include "Helpers/PCGAsync.h"

#include "Landscape.h"
#include "LandscapeEdit.h"

void UPCGLandscapeData::Initialize(const TArray<TWeakObjectPtr<ALandscapeProxy>>& InLandscapes, const FBox& InBounds, bool bInHeightOnly, bool bInUseMetadata)
{
	for (TWeakObjectPtr<ALandscapeProxy> InLandscape : InLandscapes)
	{
		if (InLandscape.IsValid())
		{
			Landscapes.Emplace(InLandscape.Get());

			// Build landscape info list
			LandscapeInfos.Emplace(PCGHelpers::GetLandscapeBounds(InLandscape.Get()), InLandscape->GetLandscapeInfo());
		}
	}

	check(!Landscapes.IsEmpty());

	ALandscapeProxy* FirstLandscape = Landscapes[0].Get();

	TargetActor = FirstLandscape;
	Bounds = InBounds;
	bHeightOnly = bInHeightOnly;
	bUseMetadata = bInUseMetadata;

	Transform = FirstLandscape->GetActorTransform();

	// Store cache pointer for easier access
	LandscapeCache = (FirstLandscape->GetWorld() && FirstLandscape->GetWorld()->GetSubsystem<UPCGSubsystem>()) ? FirstLandscape->GetWorld()->GetSubsystem<UPCGSubsystem>()->GetLandscapeCache() : nullptr;

	// TODO: find a better way to do this - maybe there should be a prototype metadata in the landscape cache
	if (LandscapeCache)
	{
		if (!bHeightOnly && bUseMetadata)
		{
			for (TSoftObjectPtr<ALandscapeProxy> Landscape : Landscapes)
			{
				const TArray<FName> Layers = LandscapeCache->GetLayerNames(Landscape.Get());

				for (const FName& Layer : Layers)
				{
					Metadata->CreateFloatAttribute(Layer, 0.0f, /*bAllowInterpolation=*/true);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogPCG, Error, TEXT("Landscape is unable to access the landscape cache"));
	}
}

void UPCGLandscapeData::PostLoad()
{
	ALandscapeProxy* FirstLandscape = nullptr;

	for (TSoftObjectPtr<ALandscapeProxy> Landscape : Landscapes)
	{
		Landscape.LoadSynchronous();
		if (Landscape.Get())
		{
			LandscapeInfos.Emplace(PCGHelpers::GetLandscapeBounds(Landscape.Get()), Landscape->GetLandscapeInfo());

			if (!FirstLandscape)
			{
				FirstLandscape = Landscape.Get();
			}
		}
		else
		{
			UE_LOG(LogPCG, Warning, TEXT("Was unable to load landscape in landscape data"));
		}
	}

	TargetActor = FirstLandscape;
	LandscapeCache = (FirstLandscape && FirstLandscape->GetWorld() && FirstLandscape->GetWorld()->GetSubsystem<UPCGSubsystem>()) ? FirstLandscape->GetWorld()->GetSubsystem<UPCGSubsystem>()->GetLandscapeCache() : nullptr;
}

FBox UPCGLandscapeData::GetBounds() const
{
	return Bounds;
}

FBox UPCGLandscapeData::GetStrictBounds() const
{
	// TODO: if the landscape contains holes, then the strict bounds
	// should be empty
	return Bounds;
}

bool UPCGLandscapeData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UPCGLandscapeData::SamplePoint);
	if (!LandscapeCache)
	{
		return false;
	}

	const ULandscapeInfo* LandscapeInfo = GetLandscapeInfo(InTransform.GetLocation());
	if (!LandscapeInfo)
	{
		return false;
	}

	const FTransform& LandscapeTransform = LandscapeInfo->GetLandscapeProxy()->GetTransform();

	// TODO: compute full transform when we want to support bounds
	const FVector LocalPoint = LandscapeTransform.InverseTransformPosition(InTransform.GetLocation());
	const FIntPoint ComponentMapKey(FMath::FloorToInt(LocalPoint.X / LandscapeInfo->ComponentSizeQuads), FMath::FloorToInt(LocalPoint.Y / LandscapeInfo->ComponentSizeQuads));

	if (ULandscapeComponent* LandscapeComponent = LandscapeInfo->XYtoComponentMap.FindRef(ComponentMapKey))
	{
		const FPCGLandscapeCacheEntry* LandscapeCacheEntry = LandscapeCache->GetCacheEntry(LandscapeComponent, ComponentMapKey);

		if (!LandscapeCacheEntry)
		{
			return false;
		}

		const FVector2D ComponentLocalPoint(LocalPoint.X - ComponentMapKey.X * LandscapeInfo->ComponentSizeQuads, LocalPoint.Y - ComponentMapKey.Y * LandscapeInfo->ComponentSizeQuads);

		if (bHeightOnly)
		{
			LandscapeCacheEntry->GetInterpolatedPointHeightOnly(ComponentLocalPoint, OutPoint);
		}
		else
		{
			LandscapeCacheEntry->GetInterpolatedPoint(ComponentLocalPoint, OutPoint, bUseMetadata ? OutMetadata : nullptr);
		}

		return true;
	}
	else
	{
		return false;
	}
}

const UPCGPointData* UPCGLandscapeData::CreatePointData(FPCGContext* Context, const FBox& InBounds) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGLandscapeData::CreatePointData);

	if (LandscapeInfos.IsEmpty() || !LandscapeCache)
	{
		UE_LOG(LogPCG, Error, TEXT("PCG Landscape cache or Landscape info are not initialized"));
		return nullptr;
	}

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);
	TArray<FPCGPoint>& Points = Data->GetMutablePoints();

	FBox EffectiveBounds = Bounds;
	if (InBounds.IsValid)
	{
		EffectiveBounds = Bounds.Overlap(InBounds);
	}

	// Early out
	if (!EffectiveBounds.IsValid)
	{
		return Data;
	}

	for (const TPair<FBox, ULandscapeInfo*>& LandscapeInfoPair : LandscapeInfos)
	{
		ULandscapeInfo* LandscapeInfo = LandscapeInfoPair.Value;

		const FTransform& LandscapeTransform = LandscapeInfo->GetLandscapeProxy()->GetTransform();
		const int32 ComponentSizeQuads = LandscapeInfo->ComponentSizeQuads;

		// TODO: add offset to nearest edge, will have an impact if the grid size doesn't match the landscape size
		const FVector MinPt = LandscapeTransform.InverseTransformPosition(EffectiveBounds.Min);
		const FVector MaxPt = LandscapeTransform.InverseTransformPosition(EffectiveBounds.Max);

		// Note: the MaxX/Y here are inclusive, hence the floor & the +1 in the sizes
		const int32 MinX = FMath::CeilToInt(MinPt.X);
		const int32 MaxX = FMath::FloorToInt(MaxPt.X);
		const int32 MinY = FMath::CeilToInt(MinPt.Y);
		const int32 MaxY = FMath::FloorToInt(MaxPt.Y);

		//Early out if the bounds do not overlap any landscape vertices
		if (MaxX < MinX || MaxY < MinY)
		{
			continue;
		}

		const int64 PointCountUpperBound = (1 + MaxX - MinX) * (1 + MaxY - MinY);
		const int32 PointsBeforeNum = Points.Num();
		if (PointCountUpperBound > 0)
		{
			Points.Reserve(PointsBeforeNum + PointCountUpperBound);
		}

		const int32 MinComponentX = MinX / ComponentSizeQuads;
		const int32 MaxComponentX = MaxX / ComponentSizeQuads;
		const int32 MinComponentY = MinY / ComponentSizeQuads;
		const int32 MaxComponentY = MaxY / ComponentSizeQuads;

		for (int32 ComponentX = MinComponentX; ComponentX <= MaxComponentX; ++ComponentX)
		{
			for (int32 ComponentY = MinComponentY; ComponentY <= MaxComponentY; ++ComponentY)
			{
				FIntPoint ComponentMapKey(ComponentX, ComponentY);
				if (ULandscapeComponent* LandscapeComponent = LandscapeInfo->XYtoComponentMap.FindRef(ComponentMapKey))
				{
					const FPCGLandscapeCacheEntry* LandscapeCacheEntry = LandscapeCache->GetCacheEntry(LandscapeComponent, ComponentMapKey);

					if (!LandscapeCacheEntry)
					{
						continue;
					}

					// Rebase our bounds in the component referential
					const int32 LocalMinX = FMath::Clamp(MinX - ComponentMapKey.X * ComponentSizeQuads, 0, ComponentSizeQuads - 1);
					const int32 LocalMaxX = FMath::Clamp(MaxX - ComponentMapKey.X * ComponentSizeQuads, 0, ComponentSizeQuads - 1);

					const int32 LocalMinY = FMath::Clamp(MinY - ComponentMapKey.Y * ComponentSizeQuads, 0, ComponentSizeQuads - 1);
					const int32 LocalMaxY = FMath::Clamp(MaxY - ComponentMapKey.Y * ComponentSizeQuads, 0, ComponentSizeQuads - 1);

					// We can't really copy data from the component points wholesale because the component points have an additional boundary point.
					// TODO: consider optimizing this, though it will impact the Sample then
					for (int32 LocalX = LocalMinX; LocalX <= LocalMaxX; ++LocalX)
					{
						for (int32 LocalY = LocalMinY; LocalY <= LocalMaxY; ++LocalY)
						{
							const int32 PointIndex = LocalX + LocalY * (ComponentSizeQuads + 1);

							FPCGPoint& Point = Points.Emplace_GetRef();
							if (bHeightOnly)
							{
								LandscapeCacheEntry->GetPointHeightOnly(PointIndex, Point);
							}
							else
							{
								LandscapeCacheEntry->GetPoint(PointIndex, Point, bUseMetadata ? Data->Metadata : nullptr);
							}
						}
					}
				}
			}
		}

		check(Points.Num() - PointsBeforeNum <= PointCountUpperBound);
		UE_LOG(LogPCG, Verbose, TEXT("Landscape %s extracted %d of %d potential points"), *LandscapeInfo->GetLandscapeProxy()->GetFName().ToString(), Points.Num() - PointsBeforeNum, PointCountUpperBound);
	}

	return Data;
}

const ULandscapeInfo* UPCGLandscapeData::GetLandscapeInfo(const FVector& InPosition) const
{
	check(!LandscapeInfos.IsEmpty());

	// Early out
	if (LandscapeInfos.Num() == 1)
	{
		return LandscapeInfos[0].Value;
	}

	// As discussed in the header, this loop here is the reason why we do not really support overlapping landscapes.
	// TODO: we could maybe improve on this if we find the "nearest" landscape on a Z perspective, but this might still lead to issues
	for (const TPair<FBox, ULandscapeInfo*>& LandscapeInfoPair : LandscapeInfos)
	{
		if (LandscapeInfoPair.Key.IsInside(InPosition))
		{
			return LandscapeInfoPair.Value;
		}
	}

	return nullptr;
}