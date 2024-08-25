// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGWorldQuery.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Grid/PCGLandscapeCache.h"
#include "Helpers/PCGHelpers.h"

#include "GameFramework/Actor.h"
#include "LandscapeProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGWorldQuery)

#define LOCTEXT_NAMESPACE "PCGWorldQuery"

#if WITH_EDITOR
FText UPCGWorldQuerySettings::GetNodeTooltipText() const
{
	return LOCTEXT("WorldQueryTooltip", "Allows generic access (based on overlaps) to collisions in the world that behaves like a volume.");
}
#endif

TArray<FPCGPinProperties> UPCGWorldQuerySettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Volume);

	return PinProperties;
}

FPCGElementPtr UPCGWorldQuerySettings::CreateElement() const
{
	return MakeShared<FPCGWorldVolumetricQueryElement>();
}

bool FPCGWorldVolumetricQueryElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGWorldVolumetricQueryElement::Execute);

	const UPCGWorldQuerySettings* Settings = Context->GetInputSettings<UPCGWorldQuerySettings>();
	check(Settings);

	FPCGWorldVolumetricQueryParams QueryParams = Settings->QueryParams;
	
	// TODO: Add params pin + Apply param data overrides

	check(Context->SourceComponent.IsValid());
	UWorld* World = Context->SourceComponent->GetWorld();

	UPCGWorldVolumetricData* Data = NewObject<UPCGWorldVolumetricData>();
	Data->Initialize(World);
	Data->QueryParams = QueryParams;
	Data->QueryParams.Initialize();
	Data->OriginatingComponent = Context->SourceComponent;

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = Data;

	return true;
}

#if WITH_EDITOR
FText UPCGWorldRayHitSettings::GetNodeTooltipText() const
{
	return LOCTEXT("WorldRayHitTooltip", "Allows generic access (based on raycasts) to collisions in the world that behaves like a surface.");
}
#endif

TArray<FPCGPinProperties> UPCGWorldRayHitSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Surface);

	return PinProperties;
}

FPCGElementPtr UPCGWorldRayHitSettings::CreateElement() const
{
	return MakeShared<FPCGWorldRayHitQueryElement>();
}

bool FPCGWorldRayHitQueryElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGWorldRayHitQueryElement::Execute);

	const UPCGWorldRayHitSettings* Settings = Context->GetInputSettings<UPCGWorldRayHitSettings>();
	check(Settings);

	FPCGWorldRayHitQueryParams QueryParams = Settings->QueryParams;
	
	// TODO: Support params pin + Apply param data

	if (!QueryParams.bOverrideDefaultParams)
	{
		// Compute default parameters based on original owner component - raycast down local Z axis
		// TODO: Might want to revisit this for 3D partitioning. The reasoning here is that ray origin should be the same
		// if we are partitioned or non-partitioned. But with 3D partitioning we might want something different.
		// Since it is not yet widely used, we'll stick with same behavior for all cases.
		UPCGComponent* SourceComponent = Context->SourceComponent.Get();
		UPCGComponent* OriginalComponent = SourceComponent ? SourceComponent->GetOriginalComponent() : nullptr;
		AActor* Owner = OriginalComponent ? OriginalComponent->GetOwner() : nullptr;
		const FTransform& Transform = Owner ? Owner->GetTransform() : FTransform::Identity;

		const FBox LocalBounds = PCGHelpers::GetActorLocalBounds(Owner);
		const FVector RayOrigin = Transform.TransformPosition(FVector(0, 0, LocalBounds.Max.Z));
		const FVector RayEnd = Transform.TransformPosition(FVector(0, 0, LocalBounds.Min.Z));

		const FVector::FReal RayLength = (RayEnd - RayOrigin).Length();
		const FVector RayDirection = (RayLength > UE_SMALL_NUMBER ? (RayEnd - RayOrigin) / RayLength : FVector(0, 0, -1.0));

		QueryParams.RayOrigin = RayOrigin;
		QueryParams.RayDirection = RayDirection;
		QueryParams.RayLength = RayLength;
	}
	else // user provided ray parameters
	{
		const FVector::FReal RayDirectionLength = QueryParams.RayDirection.Length();
		if (RayDirectionLength > UE_SMALL_NUMBER)
		{
			QueryParams.RayDirection = QueryParams.RayDirection / RayDirectionLength;
			QueryParams.RayLength *= RayDirectionLength;
		}
		else
		{
			QueryParams.RayDirection = FVector(0, 0, -1.0);
		}
	}

	check(Context->SourceComponent.IsValid());
	UWorld* World = Context->SourceComponent->GetWorld();

	UPCGWorldRayHitData* Data = NewObject<UPCGWorldRayHitData>();
	Data->Initialize(World);
	Data->QueryParams = QueryParams;
	Data->QueryParams.Initialize();
	Data->OriginatingComponent = Context->SourceComponent;

	if (Data->QueryParams.bGetReferenceToActorHit && Data->Metadata)
	{
		Data->Metadata->FindOrCreateAttribute(PCGPointDataConstants::ActorReferenceAttribute, FSoftObjectPath(), /*bAllowInterpolation=*/false, /*bOverrideParent=*/false);
	}
	else
	{
		Data->QueryParams.bGetReferenceToActorHit = false;
	}

	if (Data->QueryParams.bGetReferenceToPhysicalMaterial && Data->Metadata)
	{
		Data->Metadata->FindOrCreateAttribute(PCGWorldRayHitConstants::PhysicalMaterialReferenceAttribute, FSoftObjectPath(), /*bAllowInterpolation=*/false, /*bOverrideParent=*/false);
	}
	else
	{
		Data->QueryParams.bGetReferenceToPhysicalMaterial = false;
	}

	bool bHasLandscapeMetadata = false;
	if (QueryParams.bApplyMetadataFromLandscape && Data->Metadata)
	{
		UPCGSubsystem* PCGSubsystem = UPCGSubsystem::GetInstance(World);
		UPCGLandscapeCache* LandscapeCache = PCGSubsystem ? PCGSubsystem->GetLandscapeCache() : nullptr;

		if (LandscapeCache)
		{
			TFunction<bool(const AActor*)> BoundsCheck = [](const AActor*) -> bool { return true; };
			TFunction<bool(const AActor*)> SelfIgnoreCheck = [](const AActor*) -> bool { return true; };

			FPCGActorSelectorSettings ActorSelector;
			ActorSelector.ActorFilter = EPCGActorFilter::AllWorldActors;
			ActorSelector.ActorSelection = EPCGActorSelection::ByClass;
			ActorSelector.ActorSelectionClass = ALandscapeProxy::StaticClass();
			ActorSelector.bSelectMultiple = true;

			if (Data->Bounds.IsValid)
			{
				BoundsCheck = [Data, Component=Context->SourceComponent.Get()](const AActor* OtherActor) -> bool
				{
					const FBox OtherActorBounds = OtherActor ? PCGHelpers::GetGridBounds(OtherActor, Component) : FBox(EForceInit::ForceInit);
					return OtherActorBounds.Intersect(Data->Bounds);
				};
			}
	
			TArray<AActor*> LandscapeActors = PCGActorSelector::FindActors(ActorSelector, Context->SourceComponent.Get(), BoundsCheck, SelfIgnoreCheck);
			for (AActor* Landscape : LandscapeActors)
			{
				if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Landscape))
				{
					const TArray<FName> Layers = LandscapeCache->GetLayerNames(LandscapeProxy);
					for (FName Layer : Layers)
					{
						if (!Data->Metadata->HasAttribute(Layer))
						{
							Data->Metadata->CreateFloatAttribute(Layer, 0.0f, /*bAllowInterpolation=*/true);
							bHasLandscapeMetadata = true;
						}
					}
				}
			}
		}
	}

	if (!bHasLandscapeMetadata)
	{
		Data->QueryParams.bApplyMetadataFromLandscape = false;
	}

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = Data;

	return true;
}

#undef LOCTEXT_NAMESPACE