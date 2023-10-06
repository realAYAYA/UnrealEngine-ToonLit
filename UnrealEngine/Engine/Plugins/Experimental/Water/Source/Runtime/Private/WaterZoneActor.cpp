// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterZoneActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "WaterModule.h"
#include "WaterSubsystem.h"
#include "WaterMeshComponent.h"
#include "WaterBodyActor.h"
#include "EngineUtils.h"
#include "LandscapeComponent.h"
#include "LandscapeProxy.h"
#include "WaterUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WaterViewExtension.h"
#include "Algo/AnyOf.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterZoneActor)

#if WITH_EDITOR
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "WaterIconHelper.h"
#include "Components/BoxComponent.h"
#include "WaterZoneActorDesc.h"
#endif // WITH_EDITOR

static int32 ForceUpdateWaterInfoNextFrames = 0;
static FAutoConsoleVariableRef CVarForceUpdateWaterInfoNextFrames(
	TEXT("r.Water.WaterInfo.ForceUpdateWaterInfoNextFrames"),
	ForceUpdateWaterInfoNextFrames,
	TEXT("Force the water info texture to regenerate on the next N frames. A negative value will force update every frame."));

TAutoConsoleVariable<float> CVarWaterFallbackDepth(
	TEXT("r.Water.FallbackDepth"),
	3000.0f,
	TEXT("Depth to use for all water when there are no ground actors defined."),
	ECVF_Default);



AWaterZone::AWaterZone(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, RenderTargetResolution(512, 512)
{
	WaterMesh = CreateDefaultSubobject<UWaterMeshComponent>(TEXT("WaterMesh"));
	SetRootComponent(WaterMesh);
	ZoneExtent = FVector2D(51200., 51200.);
	LocalTessellationExtent = FVector(35000., 35000., 10000.);
	
#if	WITH_EDITOR
	// Setup bounds component
	{
		BoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsComponent"));
		BoundsComponent->SetCollisionObjectType(ECC_WorldStatic);
		BoundsComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		BoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BoundsComponent->SetGenerateOverlapEvents(false);
		BoundsComponent->SetupAttachment(WaterMesh);
		// Bounds component extent is half-extent, ZoneExtent is full extent.
		BoundsComponent->SetBoxExtent(FVector(ZoneExtent / 2., 8192.));
	}

	if (GIsEditor && !IsTemplate())
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.OnActorSelectionChanged().AddUObject(this, &AWaterZone::OnActorSelectionChanged);
	}

	ActorIcon = FWaterIconHelper::EnsureSpriteComponentCreated(this, TEXT("/Water/Icons/WaterZoneActorSprite"));
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	TessellatedWaterMeshExtent_DEPRECATED = FVector(35000., 35000., 10000.);

	bIsSpatiallyLoaded = false;
#endif
}

void AWaterZone::SetZoneExtent(FVector2D NewExtent)
{
	ZoneExtent = NewExtent;
	OnExtentChanged();
}

FBox2D AWaterZone::GetZoneBounds2D() const
{
	// GetZoneExtents returns the full extent of the zone but BoxSphereBounds expects a half-extent.
	const FVector2D WaterZoneLocation = FVector2D(GetActorLocation());
	const FVector2D WaterZoneHalfExtent = GetZoneExtent() / 2.0;
	const FBox2D WaterZoneBounds(WaterZoneLocation - WaterZoneHalfExtent, WaterZoneLocation + WaterZoneHalfExtent);

	return WaterZoneBounds;
}

FBox AWaterZone::GetZoneBounds() const
{
	const FBox2D ZoneBounds2D = GetZoneBounds2D();
	// #todo_water [roey]: Water zone doesn't have an explicit z bounds yet. For now just use the x or y.
	const double StreamingBoundsZ = FMath::Max(ZoneExtent.X, ZoneExtent.Y);

	return FBox(FVector3d(ZoneBounds2D.Min, -StreamingBoundsZ / 2.), FVector3d(ZoneBounds2D.Max, StreamingBoundsZ / 2.0));
}

void AWaterZone::SetRenderTargetResolution(FIntPoint NewResolution)
{
	RenderTargetResolution = NewResolution;
	MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
}

void AWaterZone::BeginPlay()
{
	Super::BeginPlay();

	MarkForRebuild(EWaterZoneRebuildFlags::All);

	FWorldDelegates::LevelAddedToWorld.AddUObject(this, &AWaterZone::OnLevelAddedToWorld);
	FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &AWaterZone::OnLevelRemovedFromWorld);
}

void AWaterZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
}

void AWaterZone::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	// Water mesh component was made new root component. Make sure it doesn't have a parent
	WaterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	Super::PostLoadSubobjects(OuterInstanceGraph);
}

void AWaterZone::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WaterBodyStaticMeshComponents)
	{
		LocalTessellationExtent = TessellatedWaterMeshExtent_DEPRECATED;
		bEnableLocalOnlyTessellation = bEnableNonTesselatedLODMesh_DEPRECATED;
	}

	// WaterMesh ExtentInTiles returns the half extent.
	const FVector2D ExtentInTiles = 2.0 * FVector2D(WaterMesh->GetExtentInTiles());
	ZoneExtent = FVector2D(ExtentInTiles * WaterMesh->GetTileSize());
	OnExtentChanged();
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
void AWaterZone::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UBoxComponent::StaticClass()));
}
#endif

void AWaterZone::MarkForRebuild(EWaterZoneRebuildFlags Flags)
{
	if (EnumHasAnyFlags(Flags, EWaterZoneRebuildFlags::UpdateWaterMesh))
	{
		UE_LOG(LogWater, Verbose, TEXT("AWaterZone::MarkForRebuild (UpdateWaterMesh)"));
		WaterMesh->MarkWaterMeshGridDirty();
	}
	if (EnumHasAnyFlags(Flags, EWaterZoneRebuildFlags::UpdateWaterInfoTexture))
	{
		UE_LOG(LogWater, Verbose, TEXT("AWaterZone::MarkForRebuild (UpdateWaterInfoTexture)"));
		bNeedsWaterInfoRebuild = true;
	}
}

void AWaterZone::ForEachWaterBodyComponent(TFunctionRef<bool(UWaterBodyComponent*)> Predicate) const
{
	for (const TWeakObjectPtr<UWaterBodyComponent>& WaterBodyComponent : OwnedWaterBodies)
	{
		if (WaterBodyComponent.IsValid() && !Predicate(WaterBodyComponent.Get()))
		{
			break;
		}
	}
}

void AWaterZone::AddWaterBodyComponent(UWaterBodyComponent* WaterBodyComponent)
{
	if (OwnedWaterBodies.Find(WaterBodyComponent) == INDEX_NONE)
	{
		OwnedWaterBodies.Add(WaterBodyComponent);

		EWaterZoneRebuildFlags RebuildFlags = EWaterZoneRebuildFlags::None;
		if (WaterBodyComponent->AffectsWaterInfo())
		{
			RebuildFlags |= EWaterZoneRebuildFlags::UpdateWaterInfoTexture;
		}
		if (WaterBodyComponent->AffectsWaterMesh())
		{
			RebuildFlags |= EWaterZoneRebuildFlags::UpdateWaterMesh;
		}

		MarkForRebuild(RebuildFlags);
	}
}

void AWaterZone::RemoveWaterBodyComponent(UWaterBodyComponent* WaterBodyComponent)
{
	int32 Index;
	if (OwnedWaterBodies.Find(WaterBodyComponent, Index))
	{
		OwnedWaterBodies.RemoveAtSwap(Index);

		EWaterZoneRebuildFlags RebuildFlags = EWaterZoneRebuildFlags::None;
		if (WaterBodyComponent->AffectsWaterInfo())
		{
			RebuildFlags |= EWaterZoneRebuildFlags::UpdateWaterInfoTexture;
		}
		if (WaterBodyComponent->AffectsWaterMesh())
		{
			RebuildFlags |= EWaterZoneRebuildFlags::UpdateWaterMesh;
		}
		
		MarkForRebuild(RebuildFlags);
	}
}

void AWaterZone::Update()
{
	if (bNeedsWaterInfoRebuild || (ForceUpdateWaterInfoNextFrames != 0))
	{
		ForceUpdateWaterInfoNextFrames = (ForceUpdateWaterInfoNextFrames < 0) ? ForceUpdateWaterInfoNextFrames : FMath::Max(0, ForceUpdateWaterInfoNextFrames - 1);
		if (UpdateWaterInfoTexture())
		{
			bNeedsWaterInfoRebuild = false;
		}
	}
	
	if (WaterMesh)
	{
		WaterMesh->Update();
	}
}

#if	WITH_EDITOR
void AWaterZone::ForceUpdateWaterInfoTexture()
{
	UpdateWaterInfoTexture();
}

void AWaterZone::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// Ensure that the water mesh is rebuilt if it moves
	EWaterZoneRebuildFlags RebuildFlags = EWaterZoneRebuildFlags::UpdateWaterInfoTexture | EWaterZoneRebuildFlags::UpdateWaterMesh;

	UpdateOverlappingWaterBodies();

	MarkForRebuild(RebuildFlags);
}

void AWaterZone::PostEditUndo()
{
	Super::PostEditUndo();

	UpdateOverlappingWaterBodies();
	MarkForRebuild(EWaterZoneRebuildFlags::All);
}

void AWaterZone::PostEditImport()
{
	Super::PostEditImport();

	UpdateOverlappingWaterBodies();
	MarkForRebuild(EWaterZoneRebuildFlags::All);
}

void AWaterZone::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AWaterZone, ZoneExtent))
	{
		OnExtentChanged();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWaterZone, BoundsComponent))
	{
		OnBoundsComponentModified();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWaterZone, RenderTargetResolution))
	{
		MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWaterZone, bHalfPrecisionTexture))
	{
		MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWaterZone, VelocityBlurRadius))
	{
		MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWaterZone, LocalTessellationExtent))
	{
		MarkForRebuild(EWaterZoneRebuildFlags::All);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWaterZone, bEnableLocalOnlyTessellation))
	{
		MarkForRebuild(EWaterZoneRebuildFlags::All);
	}
}

void AWaterZone::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	TArray<AWaterBody*> NewWaterBodiesSelection;
	Algo::TransformIf(NewSelection, NewWaterBodiesSelection, [](UObject* Obj) { return Obj->IsA<AWaterBody>(); }, [](UObject* Obj) { return static_cast<AWaterBody*>(Obj); });
	NewWaterBodiesSelection.Sort();
	TArray<TWeakObjectPtr<AWaterBody>> NewWeakWaterBodiesSelection;
	NewWeakWaterBodiesSelection.Reserve(NewWaterBodiesSelection.Num());
	Algo::Transform(NewWaterBodiesSelection, NewWeakWaterBodiesSelection, [](AWaterBody* Body) { return TWeakObjectPtr<AWaterBody>(Body); });

	// Ensure that the water mesh is rebuilt if water body selection changed
	if (SelectedWaterBodies != NewWeakWaterBodiesSelection)
	{
		SelectedWaterBodies = NewWeakWaterBodiesSelection;
		MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterMesh | EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
	}
}

TUniquePtr<class FWorldPartitionActorDesc> AWaterZone::CreateClassActorDesc() const
{
	return TUniquePtr<FWorldPartitionActorDesc>(new FWaterZoneActorDesc());
}

FBox AWaterZone::GetStreamingBounds() const
{
	return GetZoneBounds();
}

#endif // WITH_EDITOR

void AWaterZone::OnExtentChanged()
{
	// Compute the new tile extent based on the new bounds
	const float MeshTileSize = WaterMesh->GetTileSize();
	const FVector2D ZoneHalfExtent = ZoneExtent / 2.0;

	int32 NewExtentInTilesX = FMath::FloorToInt(ZoneHalfExtent.X / MeshTileSize);
	int32 NewExtentInTilesY = FMath::FloorToInt(ZoneHalfExtent.Y / MeshTileSize);
	
	// We must ensure that the zone is always at least 1x1
	NewExtentInTilesX = FMath::Max(1, NewExtentInTilesX);
	NewExtentInTilesY = FMath::Max(1, NewExtentInTilesY);

	WaterMesh->SetExtentInTiles(FIntPoint(NewExtentInTilesX, NewExtentInTilesY));

#if WITH_EDITOR
	BoundsComponent->SetBoxExtent(FVector(ZoneHalfExtent, 8192.f));
#endif // WITH_EDITOR

	UpdateOverlappingWaterBodies();

	MarkForRebuild(EWaterZoneRebuildFlags::All);
}

bool AWaterZone::UpdateOverlappingWaterBodies()
{
	TArray<TWeakObjectPtr<UWaterBodyComponent>> OldOwnedWaterBodies = OwnedWaterBodies;
	FWaterBodyManager::ForEachWaterBodyComponent(GetWorld(), [](UWaterBodyComponent* WaterBodyComponent)
		{
			WaterBodyComponent->UpdateWaterZones();
			return true;
		});
	return (OwnedWaterBodies != OldOwnedWaterBodies);
}


#if WITH_EDITOR
void AWaterZone::OnBoundsComponentModified()
{
	// BoxExtent is the radius of the box (half-extent).
	const FVector2D NewBounds = 2.0 * FVector2D(BoundsComponent->GetUnscaledBoxExtent());

	SetZoneExtent(NewBounds);
}
#endif // WITH_EDITOR

bool AWaterZone::UpdateWaterInfoTexture()
{
	if (UWorld* World = GetWorld(); World && FApp::CanEverRender())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AWaterZone::UpdateWaterInfoTexture);

		float WaterZMin(TNumericLimits<float>::Max());
		float WaterZMax(TNumericLimits<float>::Lowest());
	
		// Collect a list of all materials used in the water info render to ensure they have complete shaders maps.
		// If they do not, we must submit compile jobs for them and wait until they are finished before re-rendering.
		TArray<UMaterialInterface*> UsedMaterials;

		TArray<UWaterBodyComponent*> WaterBodiesToRender;
		ForEachWaterBodyComponent([World, &WaterBodiesToRender, &WaterZMax, &WaterZMin, &UsedMaterials](UWaterBodyComponent* WaterBodyComponent)
		{
			// skip components which don't affect the water info texture
			if (!WaterBodyComponent->AffectsWaterInfo())
			{
				return true;
			}

			WaterBodiesToRender.Add(WaterBodyComponent);
			const FBox WaterBodyBounds = WaterBodyComponent->CalcBounds(WaterBodyComponent->GetComponentToWorld()).GetBox();
			WaterZMax = FMath::Max(WaterZMax, WaterBodyBounds.Max.Z);
			WaterZMin = FMath::Min(WaterZMin, WaterBodyBounds.Min.Z);

#if WITH_EDITOR
			if (UMaterialInterface* WaterInfoMaterial = WaterBodyComponent->GetWaterInfoMaterialInstance())
			{
				UsedMaterials.Add(WaterInfoMaterial);
			}
#endif // WITH_EDITOR

			return true;
		});

		// If we don't have any water bodies we don't need to do anything.
		if (WaterBodiesToRender.Num() == 0)
		{
			return true;
		}

		WaterHeightExtents = FVector2f(WaterZMin, WaterZMax);

		// Only compute the ground min since we can use the water max z as the ground max z for more precision.
		GroundZMin = TNumericLimits<float>::Max();
		float GroundZMax = TNumericLimits<float>::Lowest();

		TArray<TWeakObjectPtr<UPrimitiveComponent>> GroundPrimitiveComponents;

		const FBox WaterZoneBounds = GetZoneBounds();
		for (ALandscapeProxy* LandscapeProxy : TActorRange<ALandscapeProxy>(World))
		{
			const FBox LandscapeBox = LandscapeProxy->GetComponentsBoundingBox();
			// Only consider landscapes which this zone intersects with in XY and if the landscape volume is not zero sized
			if (WaterZoneBounds.IntersectXY(LandscapeBox) && LandscapeBox.GetVolume() > 0.0)
			{
				GroundZMin = FMath::Min(GroundZMin, LandscapeBox.Min.Z);
				GroundZMax = FMath::Max(GroundZMax, LandscapeBox.Max.Z);
				TInlineComponentArray<ULandscapeComponent*> LandscapeComponents(LandscapeProxy);
				GroundPrimitiveComponents.Append(LandscapeComponents);
			}
		}

		// If we have no ground components we need to set GroundZMin to be something sensible because it won't have been set above.
		if (GroundPrimitiveComponents.Num() == 0)
		{
			GroundZMax = WaterZMax;
			GroundZMin = WaterZMin - CVarWaterFallbackDepth.GetValueOnGameThread();
		}

#if WITH_EDITOR
		// Check all the ground components have complete shader maps before we try to render them into the water info texture
		for (TWeakObjectPtr<UPrimitiveComponent> GroundPrimCompPtr : GroundPrimitiveComponents)
		{
			if (UPrimitiveComponent* GroundPrimComp = GroundPrimCompPtr.Get())
			{
					TArray<UMaterialInterface*> TmpUsedMaterials;
					GroundPrimComp->GetUsedMaterials(TmpUsedMaterials, false);
					UsedMaterials.Append(TmpUsedMaterials);
			}
		}

		// Loop through all used materials and ensure that compile jobs are submitted for all which do not have complete shader maps before early-ing out of the info update.
		bool bHasIncompleteShaderMaps = false;
		const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
		for (UMaterialInterface* Material : UsedMaterials)
		{
			if (Material)
			{
				if (FMaterialResource* MaterialResource = Material->GetMaterialResource(FeatureLevel))
				{
					if (!MaterialResource->IsGameThreadShaderMapComplete())
					{
#if WITH_EDITOR
						MaterialResource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::High);
#endif
						bHasIncompleteShaderMaps = true;
					}
				}
			}
		}

		if (bHasIncompleteShaderMaps)
		{
			return false;
		}
#endif // WITH_EDITOR

		const ETextureRenderTargetFormat Format = bHalfPrecisionTexture ? ETextureRenderTargetFormat::RTF_RGBA16f : RTF_RGBA32f;
		WaterInfoTexture = FWaterUtils::GetOrCreateTransientRenderTarget2D(WaterInfoTexture, TEXT("WaterInfoTexture"), RenderTargetResolution, Format);

		UE::WaterInfo::FRenderingContext Context;
		Context.ZoneToRender = this;
		Context.WaterBodies = WaterBodiesToRender;
		Context.GroundPrimitiveComponents = MoveTemp(GroundPrimitiveComponents);
		Context.CaptureZ = FMath::Max(WaterZMax, GroundZMax) + CaptureZOffset;
		Context.TextureRenderTarget = WaterInfoTexture;

		if (TWeakPtr<FWaterViewExtension> WaterViewExtension = UWaterSubsystem::GetWaterViewExtension(World); WaterViewExtension.IsValid())
		{
			WaterViewExtension.Pin()->MarkWaterInfoTextureForRebuild(Context);
		}

		UE_LOG(LogWater, Verbose, TEXT("Queued Water Info texture update"));
	}

	return true;
}

FVector AWaterZone::GetDynamicWaterMeshCenter() const
{
	if (IsLocalOnlyTessellationEnabled())
	{
		return LocalTessellationCenter;
	}
	else
	{
		return GetActorLocation();
	}
}

FVector AWaterZone::GetDynamicWaterMeshExtent() const
{
	if (IsLocalOnlyTessellationEnabled())
	{
		return LocalTessellationExtent;
	}
	else
	{
		// #todo_water [roey]: better implementation for 3D extent
		return FVector(GetZoneExtent(), 0.0);
	}
}

void AWaterZone::OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld)
{
	OnLevelChanged(InLevel, InWorld);
}

void AWaterZone::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	OnLevelChanged(InLevel, InWorld);
}

void AWaterZone::OnLevelChanged(ULevel* InLevel, UWorld* InWorld)
{
	if ((InLevel == nullptr) || (InWorld != GetWorld()))
	{
		return;
	}

	const FBox WaterZoneBounds = GetZoneBounds();
	const bool bContainsActorsAffectingWaterZone = Algo::AnyOf(InLevel->Actors, [this, &WaterZoneBounds](const AActor* Actor)
	{
		return IsAffectingWaterZone(WaterZoneBounds, Actor);
	});

	if (bContainsActorsAffectingWaterZone)
	{
		MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
	}
}

bool AWaterZone::IsAffectingWaterZone(const FBox& InWaterZoneBounds, const AActor* InActor) const
{
	if (InActor == nullptr)
	{
		return false;
	}

	if (const AWaterBody* WaterBodyActor = Cast<const AWaterBody>(InActor))
	{
		if (const UWaterBodyComponent* WaterBodyComponent = WaterBodyActor->GetWaterBodyComponent())
		{
			if (WaterBodyComponent->GetWaterZone() == this)
			{
				return WaterBodyComponent->AffectsWaterInfo();
			}
		}
	}
	else if (const ALandscapeProxy* LandscapeProxy = Cast<const ALandscapeProxy>(InActor))
	{
		bool bIntersectsWaterZone = false;

		LandscapeProxy->ForEachComponent<ULandscapeComponent>(false, [&bIntersectsWaterZone, &InWaterZoneBounds](const ULandscapeComponent* LandscapeComponent)
		{
			if (!bIntersectsWaterZone && (LandscapeComponent != nullptr))
			{
				const FBox LandscapeBox = LandscapeComponent->Bounds.GetBox();

				bIntersectsWaterZone = InWaterZoneBounds.IntersectXY(LandscapeBox) && (LandscapeBox.GetVolume() > 0.0);
			}
		});

		return bIntersectsWaterZone;
	}

	return false;
}
