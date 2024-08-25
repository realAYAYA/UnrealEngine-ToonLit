// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGWorldData.h"

#include "PCGComponent.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSurfaceData.h"
#include "Data/PCGVolumeData.h"
#include "Elements/PCGSurfaceSampler.h"
#include "Elements/PCGVolumeSampler.h"
#include "Grid/PCGLandscapeCache.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "LandscapeProxy.h"
#include "Components/BrushComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGWorldData)

void FPCGWorldCommonQueryParams::Initialize()
{
	if (ActorTagFilter == EPCGWorldQueryFilterByTag::NoTagFilter)
	{
		ParsedActorTagsList.Reset();
	}
	else
	{
		TArray<FString> ParsedList = PCGHelpers::GetStringArrayFromCommaSeparatedString(ActorTagsList);
		ParsedActorTagsList.Reset();
		for (const FString& Tag : ParsedList)
		{
			ParsedActorTagsList.Add(FName(Tag));
		}
	}
}

void FPCGWorldVolumetricQueryParams::Initialize()
{
	FPCGWorldCommonQueryParams::Initialize();
}

void FPCGWorldRayHitQueryParams::Initialize()
{
	FPCGWorldCommonQueryParams::Initialize();
}

void UPCGWorldVolumetricData::Initialize(UWorld* InWorld, const FBox& InBounds)
{
	Super::Initialize(InBounds);
	World = InWorld;
	check(World.IsValid());
}

bool UPCGWorldVolumetricData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	// This is a pure implementation.

	check(World.IsValid());

	FPCGMetadataAttribute<FSoftObjectPath>* ActorOverlappedAttribute = ((OutMetadata && QueryParams.bGetReferenceToActorHit) ? OutMetadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGPointDataConstants::ActorReferenceAttribute) : nullptr);

	FCollisionObjectQueryParams ObjectQueryParams(QueryParams.CollisionChannel);
	FCollisionShape CollisionShape = FCollisionShape::MakeBox(InBounds.GetExtent() * InTransform.GetScale3D());
	FCollisionQueryParams Params; // TODO: apply properties from the settings when/if they exist
	Params.bTraceComplex = QueryParams.bTraceComplex;

	TArray<FOverlapResult> Overlaps;
	/*bool bOverlaps =*/ World->OverlapMultiByObjectType(Overlaps, InTransform.TransformPosition(InBounds.GetCenter()), InTransform.GetRotation(), ObjectQueryParams, CollisionShape, Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		// Skip invisible walls / triggers / volumes
		const UPrimitiveComponent* OverlappedComponent = Overlap.GetComponent();
		if (OverlappedComponent->IsA<UBrushComponent>())
		{
			continue;
		}

		// Skip "no collision" type actors
		if (!OverlappedComponent->IsQueryCollisionEnabled() || OverlappedComponent->GetCollisionResponseToChannel(QueryParams.CollisionChannel) != ECR_Block)
		{
			continue;
		}

		// Skip to-be-cleaned-up PCG-created objects
		if (OverlappedComponent->ComponentHasTag(PCGHelpers::MarkedForCleanupPCGTag) || (OverlappedComponent->GetOwner() && OverlappedComponent->GetOwner()->ActorHasTag(PCGHelpers::MarkedForCleanupPCGTag)))
		{
			continue;
		}

		// Optionally skip all PCG created objects
		if (QueryParams.bIgnorePCGHits && (OverlappedComponent->ComponentHasTag(PCGHelpers::DefaultPCGTag) || (OverlappedComponent->GetOwner() && OverlappedComponent->GetOwner()->ActorHasTag(PCGHelpers::DefaultPCGActorTag))))
		{
			continue;
		}

		// Skip self-generated PCG objects optionally
		if (QueryParams.bIgnoreSelfHits && OriginatingComponent.IsValid() && OverlappedComponent->ComponentTags.Contains(OriginatingComponent->GetFName()))
		{
			continue;
		}

		// Additional filter as provided in the QueryParams base class
		if (QueryParams.ActorTagFilter != EPCGWorldQueryFilterByTag::NoTagFilter)
		{
			if (AActor* Actor = OverlappedComponent->GetOwner())
			{
				bool bFoundMatch = false;
				for (const FName& Tag : Actor->Tags)
				{
					if (QueryParams.ParsedActorTagsList.Contains(Tag))
					{
						bFoundMatch = true;
						break;
					}
				}

				if (bFoundMatch != (QueryParams.ActorTagFilter == EPCGWorldQueryFilterByTag::IncludeTagged))
				{
					continue;
				}
			}
			else if (QueryParams.ActorTagFilter == EPCGWorldQueryFilterByTag::IncludeTagged)
			{
				continue;
			}
		}

		if (QueryParams.bIgnoreLandscapeHits && OverlappedComponent->GetOwner() && OverlappedComponent->GetOwner()->IsA<ALandscapeProxy>())
		{
			continue;
		}

		if (QueryParams.bSearchForOverlap)
		{
			OutPoint = FPCGPoint(InTransform, 1.0f, 0);
			UPCGBlueprintHelpers::SetSeedFromPosition(OutPoint);
			OutPoint.SetLocalBounds(InBounds);

			if (ActorOverlappedAttribute && OverlappedComponent->GetOwner())
			{
				OutMetadata->InitializeOnSet(OutPoint.MetadataEntry);
				ActorOverlappedAttribute->SetValue(OutPoint.MetadataEntry, FSoftObjectPath(OverlappedComponent->GetOwner()));
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	// No valid hits found
	if (!QueryParams.bSearchForOverlap)
	{
		OutPoint = FPCGPoint(InTransform, 1.0f, 0);
		UPCGBlueprintHelpers::SetSeedFromPosition(OutPoint);
		OutPoint.SetLocalBounds(InBounds);
		return true;
	}
	else
	{
		return false;
	}
}

const UPCGPointData* UPCGWorldVolumetricData::CreatePointData(FPCGContext* Context, const FBox& InBounds) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGWorldVolumetricData::CreatePointData);

	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);
	TArray<FPCGPoint>& Points = Data->GetMutablePoints();

	FBox EffectiveBounds = Bounds;
	if (InBounds.IsValid)
	{
		if (Bounds.IsValid)
		{
			EffectiveBounds = Bounds.Overlap(InBounds);
		}
		else
		{
			EffectiveBounds = InBounds;
		}
	}
	
	// Early out
	if (!EffectiveBounds.IsValid)
	{
		if (!Bounds.IsValid && !InBounds.IsValid)
		{
			UE_LOG(LogPCG, Error, TEXT("PCG World Volumetric Data cannot generate without sampling bounds. Consider using a Volume Sampler with the Unbounded option disabled."));
		}
		
		return Data;
	}

	PCGVolumeSampler::FVolumeSamplerParams SamplerParams;
	SamplerParams.VoxelSize = VoxelSize;
	SamplerParams.Bounds = EffectiveBounds;

	Data = PCGVolumeSampler::SampleVolume(Context, SamplerParams, this);
	UE_LOG(LogPCG, Verbose, TEXT("Volumetric world extracted %d points"), Data->GetPoints().Num());

	return Data;
}

UPCGSpatialData* UPCGWorldVolumetricData::CopyInternal() const
{
	UPCGWorldVolumetricData* NewVolumetricData = NewObject<UPCGWorldVolumetricData>();

	CopyBaseVolumeData(NewVolumetricData);

	NewVolumetricData->World = World;
	NewVolumetricData->OriginatingComponent = OriginatingComponent;
	NewVolumetricData->QueryParams = QueryParams;
	NewVolumetricData->QueryParams.Initialize();

	return NewVolumetricData;
}

/** World Ray Hit data implementation */
void UPCGWorldRayHitData::Initialize(UWorld* InWorld, const FBox& InBounds)
{
	World = InWorld;
	Bounds = InBounds;
}

void UPCGWorldRayHitData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	// This data does not have a bespoke CRC implementation so just use a global unique data CRC.
	AddUIDToCrc(Ar);
}

bool UPCGWorldRayHitData::SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	// TODO: This seems to be a projection - along a direction. I suspect that UPCGWorldVolumetricData is the SamplePoint(), and this is the ProjectPoint() (in a direction)?
	check(World.IsValid());

	FPCGMetadataAttribute<FSoftObjectPath>* ActorHitAttribute = ((OutMetadata && QueryParams.bGetReferenceToActorHit) ? OutMetadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGPointDataConstants::ActorReferenceAttribute) : nullptr);
	FPCGMetadataAttribute<FSoftObjectPath>* PhysicalMaterialAttribute = ((OutMetadata && QueryParams.bGetReferenceToPhysicalMaterial) ? OutMetadata->GetMutableTypedAttribute<FSoftObjectPath>(PCGWorldRayHitConstants::PhysicalMaterialReferenceAttribute) : nullptr);

	// Todo: consider prebuilding this
	FCollisionObjectQueryParams ObjectQueryParams(QueryParams.CollisionChannel);
	FCollisionQueryParams Params; // TODO: apply properties from the settings when/if they exist
	Params.bTraceComplex = QueryParams.bTraceComplex;
	Params.bReturnPhysicalMaterial = (PhysicalMaterialAttribute != nullptr);

	// Project the InTransform location on the ray origin plane
	const FVector PointLocation = InTransform.GetLocation();
	FVector RayStart = PointLocation - ((PointLocation - QueryParams.RayOrigin) | QueryParams.RayDirection) * QueryParams.RayDirection;
	FVector RayEnd = RayStart + QueryParams.RayDirection * QueryParams.RayLength;

	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, RayStart, RayEnd, ObjectQueryParams, Params);

	for (const FHitResult& Hit : Hits)
	{
		// Skip invisible walls / triggers / volumes
		const UPrimitiveComponent* HitComponent = Hit.GetComponent();
		if (HitComponent->IsA<UBrushComponent>())
		{
			continue;
		}

		// Skip "No collision" type actors
		if (!HitComponent->IsQueryCollisionEnabled() || HitComponent->GetCollisionResponseToChannel(QueryParams.CollisionChannel) != ECR_Block)
		{
			continue;
		}
		
		// Skip to-be-cleaned-up PCG-created objects
		if (HitComponent->ComponentHasTag(PCGHelpers::MarkedForCleanupPCGTag) || (HitComponent->GetOwner() && HitComponent->GetOwner()->ActorHasTag(PCGHelpers::MarkedForCleanupPCGTag)))
		{
			continue;
		}

		// Optionally skip all PCG created objects
		if (QueryParams.bIgnorePCGHits && (HitComponent->ComponentHasTag(PCGHelpers::DefaultPCGTag) || (HitComponent->GetOwner() && HitComponent->GetOwner()->ActorHasTag(PCGHelpers::DefaultPCGActorTag))))
		{
			continue;
		}

		// Skip self-generated PCG objects optionally
		if (QueryParams.bIgnoreSelfHits && OriginatingComponent.IsValid() && HitComponent->ComponentTags.Contains(OriginatingComponent->GetFName()))
		{
			continue;
		}

		// Additional filter as provided in the QueryParams base class
		if (QueryParams.ActorTagFilter != EPCGWorldQueryFilterByTag::NoTagFilter)
		{
			AActor* Actor = HitComponent->GetOwner();
			
			if (Actor)
			{
				bool bFoundMatch = false;
				for (const FName& Tag : Actor->Tags)
				{
					if (QueryParams.ParsedActorTagsList.Contains(Tag))
					{
						bFoundMatch = true;
						break;
					}
				}

				if (bFoundMatch != (QueryParams.ActorTagFilter == EPCGWorldQueryFilterByTag::IncludeTagged))
				{
					continue;
				}
			}
			else if (QueryParams.ActorTagFilter == EPCGWorldQueryFilterByTag::IncludeTagged)
			{
				continue;
			}
		}

		bool bHitOnLandscape = false;
		if (QueryParams.bIgnoreLandscapeHits || QueryParams.bApplyMetadataFromLandscape)
		{
			bHitOnLandscape = HitComponent->GetOwner() && HitComponent->GetOwner()->IsA<ALandscapeProxy>();
		}

		if (QueryParams.bIgnoreLandscapeHits && bHitOnLandscape)
		{
			continue;
		}

		// Finally, fill in OutPoint - we're done
		// Implementation note: this uses the same orthonormalization process as the landscape cache
		ensure(Hit.ImpactNormal.IsNormalized());
		const FVector ArbitraryVector = (FMath::Abs(Hit.ImpactNormal.Y) < (1.f - UE_KINDA_SMALL_NUMBER) ? FVector::YAxisVector : FVector::ZAxisVector);
		const FVector XAxis = (ArbitraryVector ^ Hit.ImpactNormal).GetSafeNormal();
		const FVector YAxis = (Hit.ImpactNormal ^ XAxis);

		OutPoint = FPCGPoint(FTransform(XAxis, YAxis, Hit.ImpactNormal, Hit.ImpactPoint), 1.0f, 0);
		UPCGBlueprintHelpers::SetSeedFromPosition(OutPoint);

		// TODO: generalize for other sources of metadata?
		if (QueryParams.bApplyMetadataFromLandscape && bHitOnLandscape && OutMetadata && World->GetSubsystem<UPCGSubsystem>())
		{
			UPCGLandscapeCache* LandscapeCache = World->GetSubsystem<UPCGSubsystem>()->GetLandscapeCache();
			if (LandscapeCache)
			{
				LandscapeCache->SampleMetadataOnPoint(Cast<ALandscapeProxy>(HitComponent->GetOwner()), OutPoint, OutMetadata);
			}
		}

		if (ActorHitAttribute && HitComponent->GetOwner())
		{
			OutMetadata->InitializeOnSet(OutPoint.MetadataEntry);
			ActorHitAttribute->SetValue(OutPoint.MetadataEntry, FSoftObjectPath(HitComponent->GetOwner()));
		}

		if (PhysicalMaterialAttribute && Hit.PhysMaterial.Get())
		{
			OutMetadata->InitializeOnSet(OutPoint.MetadataEntry);
			PhysicalMaterialAttribute->SetValue(OutPoint.MetadataEntry, FSoftObjectPath(Hit.PhysMaterial.Get()));
		}

		return true;
	}

	return false;
}

const UPCGPointData* UPCGWorldRayHitData::CreatePointData(FPCGContext* Context, const FBox& InBounds) const
{
	UPCGPointData* Data = NewObject<UPCGPointData>();
	Data->InitializeFromData(this);

	FBox EffectiveBounds = Bounds;
	if (InBounds.IsValid)
	{
		if (Bounds.IsValid)
		{
			EffectiveBounds = Bounds.Overlap(InBounds);
		}
		else
		{
			EffectiveBounds = InBounds;
		}
	}

	// Early out
	if (!EffectiveBounds.IsValid)
	{
		if (!Bounds.IsValid && !InBounds.IsValid)
		{
			UE_LOG(LogPCG, Error, TEXT("PCG World Ray Hit Data cannot generate without sampling bounds. Consider using a Surface Sampler with the Unbounded option disabled."));
		}

		return Data;
	}

	PCGSurfaceSampler::FSurfaceSamplerParams SamplerParams;
	if (SamplerParams.Initialize(nullptr, Context, EffectiveBounds))
	{
		Data = PCGSurfaceSampler::SampleSurface(Context, this, nullptr, SamplerParams);
	}

	return Data;
}

UPCGSpatialData* UPCGWorldRayHitData::CopyInternal() const
{
	UPCGWorldRayHitData* NewData = NewObject<UPCGWorldRayHitData>();

	CopyBaseSurfaceData(NewData);

	NewData->World = World;
	NewData->OriginatingComponent = OriginatingComponent;
	NewData->Bounds = Bounds;
	NewData->QueryParams = QueryParams;
	NewData->QueryParams.Initialize();

	return NewData;
}
