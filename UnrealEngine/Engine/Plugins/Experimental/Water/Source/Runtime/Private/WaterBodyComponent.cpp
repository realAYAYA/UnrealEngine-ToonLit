// Copyright Epic Games, Inc. All Rights Reserved.


#include "WaterBodyComponent.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavCollision.h"
#include "AI/NavigationSystemBase.h"
#include "AI/NavigationSystemHelpers.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/MaxElement.h"
#include "Misc/SecureHash.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/SplineMeshComponent.h"
#include "Components/BoxComponent.h"
#include "BuoyancyComponent.h"
#include "WaterModule.h"
#include "WaterSubsystem.h"
#include "WaterZoneActor.h"
#include "WaterBodyExclusionVolume.h"
#include "WaterBodyIslandActor.h"
#include "WaterSplineMetadata.h"
#include "WaterSplineComponent.h"
#include "WaterRuntimeSettings.h"
#include "WaterUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GerstnerWaterWaves.h"
#include "WaterMeshComponent.h"
#include "WaterVersion.h"
#include "Misc/MapErrors.h"
#include "Misc/UObjectToken.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "WaterBodySceneProxy.h"
#include "Engine/StaticMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Operations/MeshPlaneCut.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterBodyComponent)

#if WITH_EDITOR
#include "WaterIconHelper.h"
#include "Components/BillboardComponent.h"
#include "Modules/ModuleManager.h"
#include "WaterModule.h"
#include "StaticMeshAttributes.h"
#include "WaterBodyHLODBuilder.h"
#endif

#define LOCTEXT_NAMESPACE "Water"

// ----------------------------------------------------------------------------------

DECLARE_CYCLE_STAT(TEXT("WaterBody_ComputeWaterInfo"), STAT_WaterBody_ComputeWaterInfo, STATGROUP_Water);
DECLARE_CYCLE_STAT(TEXT("WaterBody_ComputeWaterDepth"), STAT_WaterBody_ComputeWaterDepth, STATGROUP_Water);
DECLARE_CYCLE_STAT(TEXT("WaterBody_ComputeWaterDepth"), STAT_WaterBody_ComputeLocation, STATGROUP_Water);
DECLARE_CYCLE_STAT(TEXT("WaterBody_ComputeWaterDepth"), STAT_WaterBody_ComputeNormal, STATGROUP_Water);
DECLARE_CYCLE_STAT(TEXT("WaterBody_ComputeLandscapeDepth"), STAT_WaterBody_ComputeLandscapeDepth, STATGROUP_Water);
DECLARE_CYCLE_STAT(TEXT("WaterBody_ComputeWaveHeight"), STAT_WaterBody_ComputeWaveHeight, STATGROUP_Water);

// ----------------------------------------------------------------------------------

TAutoConsoleVariable<float> CVarWaterOceanFallbackDepth(
	TEXT("r.Water.OceanFallbackDepth"),
	3000.0f,
	TEXT("Depth to report for the ocean when no terrain is found under the query location. Not used when <= 0."),
	ECVF_Default);

const FName UWaterBodyComponent::WaterBodyIndexParamName(TEXT("WaterBodyIndex"));
const FName UWaterBodyComponent::WaterBodyZOffsetParamName(TEXT("WaterBodyZOffset"));
const FName UWaterBodyComponent::WaterVelocityAndHeightName(TEXT("WaterVelocityAndHeight"));
const FName UWaterBodyComponent::GlobalOceanHeightName(TEXT("GlobalOceanHeight"));
const FName UWaterBodyComponent::FixedZHeightName(TEXT("FixedZHeight"));
const FName UWaterBodyComponent::FixedVelocityName(TEXT("FixedVelocity"));
const FName UWaterBodyComponent::FixedWaterDepthName(TEXT("FixedWaterDepth"));
const FName UWaterBodyComponent::WaterAreaParamName(TEXT("WaterArea"));
const FName UWaterBodyComponent::MaxFlowVelocityParamName(TEXT("MaxFlowVelocity"));
const FName UWaterBodyComponent::WaterZMinParamName(TEXT("WaterZMin"));
const FName UWaterBodyComponent::WaterZMaxParamName(TEXT("WaterZMax"));
const FName UWaterBodyComponent::GroundZMinParamName(TEXT("GroundZMin"));

UWaterBodyComponent::UWaterBodyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAffectsLandscape = true;

	SetCollisionProfileName(GetDefault<UWaterRuntimeSettings>()->GetDefaultWaterCollisionProfileName());

	WaterMID = nullptr;
	WaterInfoMID = nullptr;

	TargetWaveMaskDepth = 2048.f;

	bFillCollisionUnderneathForNavmesh = false;
	bCanEverAffectNavigation = false;

	WaterInfoMaterial = GetDefault<UWaterRuntimeSettings>()->GetDefaultWaterInfoMaterial();

#if WITH_EDITORONLY_DATA
	// Maintain the old default values for deprecated members so delta serialization is still correct when we deprecate them in PostLoad
	bCanAffectNavigation_DEPRECATED = false;
	bFillCollisionUnderWaterBodiesForNavmesh_DEPRECATED = false;
	CollisionProfileName_DEPRECATED = GetDefault<UWaterRuntimeSettings>()->GetDefaultWaterCollisionProfileName();
#endif // WITH_EDITORONLY_DATA
}

bool UWaterBodyComponent::IsHLODRelevant() const
{
#if WITH_EDITOR
	return bEnableAutoLODGeneration;
#else
	return false;
#endif
}

void UWaterBodyComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();

	UpdateComponentVisibility(/* bAllowWaterMeshRebuild = */true);
}

void UWaterBodyComponent::OnHiddenInGameChanged()
{
	Super::OnHiddenInGameChanged();

	UpdateComponentVisibility(/* bAllowWaterMeshRebuild = */true);
}

FPrimitiveSceneProxy* UWaterBodyComponent::CreateSceneProxy()
{
	if (AffectsWaterMesh())
	{
		return new FWaterBodySceneProxy(this);
	}
	return nullptr;
}

void UWaterBodyComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterialInterfaces, bool bGetDebugMaterials) const
{
	if (WaterInfoMID)
	{
		OutMaterialInterfaces.Add(WaterInfoMID);
	}
	if (WaterLODMID)
	{
		OutMaterialInterfaces.Add(WaterLODMID);
	}
}


void UWaterBodyComponent::OnTessellatedWaterMeshBoundsChanged()
{
	// When the water info bounds changes, we need to push new bounds to the MIDs so they can address the texture correctly.
	UpdateMaterialInstances();
}

void UWaterBodyComponent::PushTessellatedWaterMeshBoundsToProxy(const FBox2D& TessellatedWaterMeshBounds)
{
	OnTessellatedWaterMeshBoundsChanged();

	if (SceneProxy)
	{
		static_cast<FWaterBodySceneProxy*>(SceneProxy)->OnTessellatedWaterMeshBoundsChanged_GameThread(TessellatedWaterMeshBounds);
	}
}

bool UWaterBodyComponent::IsFlatSurface() const
{
	// Lakes and oceans have surfaces aligned with the XY plane
	return (GetWaterBodyType() == EWaterBodyType::Lake || GetWaterBodyType() == EWaterBodyType::Ocean);
}

bool UWaterBodyComponent::IsWaveSupported() const
{
	return (GetWaterBodyType() == EWaterBodyType::Lake || GetWaterBodyType() == EWaterBodyType::Ocean || GetWaterBodyType() == EWaterBodyType::Transition);
}

bool UWaterBodyComponent::HasWaves() const
{ 
	if (!IsWaveSupported())
	{
		return false;
	}
	return GetWaterWaves() ? (GetWaterWaves()->GetWaterWaves() != nullptr) : false;
}

FBox UWaterBodyComponent::GetCollisionComponentBounds() const
{
	FBox Box(ForceInit);
	for (UPrimitiveComponent* CollisionComponent : GetCollisionComponents())
	{
		if (CollisionComponent && CollisionComponent->IsRegistered())
		{
			Box += CollisionComponent->Bounds.GetBox();
		}
	}
	return Box;
}

FBoxSphereBounds UWaterBodyComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return Super::CalcBounds(LocalToWorld);
}

AWaterBody* UWaterBodyComponent::GetWaterBodyActor() const
{
	// If we have an Owner, it must be an AWaterBody
	return GetOwner() ? CastChecked<AWaterBody>(GetOwner()) : nullptr;
}

UWaterSplineComponent* UWaterBodyComponent::GetWaterSpline() const
{
	if (const AWaterBody* OwningWaterBody = GetWaterBodyActor())
	{
		return OwningWaterBody->GetWaterSpline();
	}
	return nullptr;
}

bool UWaterBodyComponent::IsWaterSplineClosedLoop() const
{
	return (GetWaterBodyType() == EWaterBodyType::Lake) || (GetWaterBodyType() == EWaterBodyType::Ocean);
}

bool UWaterBodyComponent::IsHeightOffsetSupported() const
{
	return GetWaterBodyType() == EWaterBodyType::Ocean;
}

bool UWaterBodyComponent::AffectsLandscape() const
{
	return bAffectsLandscape && (GetWaterBodyType() != EWaterBodyType::Transition);
}

bool UWaterBodyComponent::AffectsWaterMesh() const
{ 
	return ShouldGenerateWaterMeshTile();
}

bool UWaterBodyComponent::AffectsWaterInfo() const
{
	// Currently only water bodies which are rendered by the water mesh can render into the water info texture
	return ShouldGenerateWaterMeshTile();
}

#if WITH_EDITOR
ETextureRenderTargetFormat UWaterBodyComponent::GetBrushRenderTargetFormat() const
{
	return (GetWaterBodyType() == EWaterBodyType::River) ? ETextureRenderTargetFormat::RTF_RGBA32f : ETextureRenderTargetFormat::RTF_RGBA16f;
}

void UWaterBodyComponent::GetBrushRenderDependencies(TSet<UObject*>& OutDependencies) const
{
	for (const TPair<FName, FWaterBodyWeightmapSettings>& Pair : LayerWeightmapSettings)
	{
		if (Pair.Value.ModulationTexture)
		{
			OutDependencies.Add(Pair.Value.ModulationTexture);
		}
	}

	if (WaterHeightmapSettings.Effects.Displacement.Texture)
	{
		OutDependencies.Add(WaterHeightmapSettings.Effects.Displacement.Texture);
	}
}
#endif //WITH_EDITOR

void UWaterBodyComponent::SetWaterMaterial(UMaterialInterface* InMaterial)
{
	WaterMaterial = InMaterial;
	UpdateMaterialInstances();
}

UMaterialInstanceDynamic* UWaterBodyComponent::GetWaterMaterialInstance()
{
	CreateOrUpdateWaterMID(); 
	return WaterMID;
}

UMaterialInstanceDynamic* UWaterBodyComponent::GetUnderwaterPostProcessMaterialInstance()
{
	CreateOrUpdateUnderwaterPostProcessMID(); 
	return UnderwaterPostProcessMID;
}

UMaterialInstanceDynamic* UWaterBodyComponent::GetWaterInfoMaterialInstance()
{
	CreateOrUpdateWaterInfoMID();
	return WaterInfoMID;
}

UMaterialInstanceDynamic* UWaterBodyComponent::GetWaterLODMaterialInstance()
{
	CreateOrUpdateWaterLODMID();
	return WaterLODMID;
}

void UWaterBodyComponent::SetUnderwaterPostProcessMaterial(UMaterialInterface* InMaterial)
{
	UnderwaterPostProcessMaterial = InMaterial;
	UpdateMaterialInstances();
}

void UWaterBodyComponent::SetWaterAndUnderWaterPostProcessMaterial(UMaterialInterface* InWaterMaterial, UMaterialInterface* InUnderWaterPostProcessMaterial)
{
	bool bUpdateInstances = WaterMaterial != InWaterMaterial || UnderwaterPostProcessMaterial != InUnderWaterPostProcessMaterial;

	WaterMaterial = InWaterMaterial;
	UnderwaterPostProcessMaterial = InUnderWaterPostProcessMaterial;

	if (bUpdateInstances)
	{
		UpdateMaterialInstances();
	}
}

bool UWaterBodyComponent::ShouldGenerateWaterMeshTile() const
{
	return bAlwaysGenerateWaterMeshTiles
		|| ((GetWaterBodyType() != EWaterBodyType::Transition)
		&& (GetWaterMeshOverride() == nullptr)
		&& (GetWaterMaterial() != nullptr));
}

void UWaterBodyComponent::AddIsland(AWaterBodyIsland* Island)
{
	WaterBodyIslands.AddUnique(Island);
}

void UWaterBodyComponent::RemoveIsland(AWaterBodyIsland* Island)
{
	WaterBodyIslands.RemoveSwap(Island);
}

void UWaterBodyComponent::UpdateIslands()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWaterBodyComponent::UpdateIslands);

	// For now, islands are not detected dynamically
#if WITH_EDITOR
	if (GetWorld())
	{
		for (AWaterBodyIsland* Island : TActorRange<AWaterBodyIsland>(GetWorld()))
		{
			Island->UpdateOverlappingWaterBodyComponents();
		}
	}
#endif // WITH_EDITOR
}

void UWaterBodyComponent::AddExclusionVolume(AWaterBodyExclusionVolume* InExclusionVolume)
{
	WaterBodyExclusionVolumes.AddUnique(InExclusionVolume);
}

void UWaterBodyComponent::RemoveExclusionVolume(AWaterBodyExclusionVolume* InExclusionVolume)
{
	WaterBodyExclusionVolumes.RemoveSwap(InExclusionVolume);
}

void UWaterBodyComponent::UpdateExclusionVolumes()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWaterBodyComponent::UpdateExclusionVolumes);
	if (GetWorld())
	{
		for (AWaterBodyExclusionVolume* ExclusionVolume : TActorRange<AWaterBodyExclusionVolume>(GetWorld()))
		{
			ExclusionVolume->UpdateOverlappingWaterBodies();
		}
	}
}

void UWaterBodyComponent::UpdateWaterZones()
{
	if (UWorld* World = GetWorld())
	{
		AWaterZone* OldWaterZone = GetWaterZone();
		AWaterZone* FoundZone = FindWaterZone();

		if (OldWaterZone != FoundZone)
		{
			if (OldWaterZone)
			{
				OldWaterZone->RemoveWaterBodyComponent(this);
				OldWaterZone = nullptr;
			}

			if (FoundZone)
			{
				FoundZone->AddWaterBodyComponent(this);
			}

			OwningWaterZone = FoundZone;
		}
	}
}

AWaterZone* UWaterBodyComponent::GetWaterZone() const
{
	return OwningWaterZone.Get();
}

FPostProcessVolumeProperties UWaterBodyComponent::GetPostProcessProperties() const
{
	FPostProcessVolumeProperties Ret;
	Ret.bIsEnabled = UnderwaterPostProcessSettings.bEnabled;
	Ret.bIsUnbound = false;
	Ret.BlendRadius = UnderwaterPostProcessSettings.BlendRadius;
	Ret.BlendWeight = UnderwaterPostProcessSettings.BlendWeight;
	Ret.Priority = UnderwaterPostProcessSettings.Priority;
	Ret.Settings = &CurrentPostProcessSettings;
	return Ret;
}

void UWaterBodyComponent::PostDuplicate(bool bDuplicateForPie)
{
	Super::PostDuplicate(bDuplicateForPie);

#if WITH_EDITOR
	if (!bDuplicateForPie && GIsEditor)
	{
		// After duplication due to copy-pasting, UWaterSplineMetadata might have been edited without the spline component being made aware of that (for some reason, USplineComponent::PostDuplicate isn't called)::
		GetWaterSpline()->SynchronizeWaterProperties();

		FOnWaterBodyChangedParams Params;
		Params.bShapeOrPositionChanged = true;
		Params.bWeightmapSettingsChanged = true;
		OnWaterBodyChanged(Params);
	}

	RegisterOnUpdateWavesData(GetWaterWaves(), /* bRegister = */true);
#endif // WITH_EDITOR
}

float UWaterBodyComponent::FindInputKeyClosestToWorldLocation(const FVector& WorldLocation) const
{
	return GetWaterSpline()->FindInputKeyClosestToWorldLocation(WorldLocation);
}

float UWaterBodyComponent::GetConstantSurfaceZ() const
{
	const UWaterSplineComponent* const WaterSpline = GetWaterSpline();

	// A single Z doesn't really make sense for non-flat water bodies, but it can be useful for when using FixedZ post process for example. Take the first spline key in that case :
	float WaterSurfaceZ = (IsFlatSurface() || WaterSpline == nullptr) ? GetComponentLocation().Z : WaterSpline->GetLocationAtSplineInputKey(0.0f, ESplineCoordinateSpace::World).Z;
	
	// Apply body height offset if applicable (ocean)
	if (IsHeightOffsetSupported())
	{
		WaterSurfaceZ += GetHeightOffset();
	}

	return WaterSurfaceZ;
}

float UWaterBodyComponent::GetConstantDepth() const
{
	// Only makes sense when you consider the water depth to be constant for the whole water body, in which case we just use the first spline key's : 
	const UWaterSplineComponent* const WaterSpline = GetWaterSpline();
	float SplinePointDepth = WaterSpline ? WaterSpline->GetFloatPropertyAtSplineInputKey(0.0f, GET_MEMBER_NAME_CHECKED(UWaterSplineMetadata, Depth)) : 0.0f;

	// Ensure that the existing spline depth is used if it is non-zero, otherwise use the new FixedWaterDepth parameter.
	return SplinePointDepth != 0.f ? SplinePointDepth : FixedWaterDepth;
}

FVector UWaterBodyComponent::GetConstantVelocity() const
{
	// Only makes sense when you consider the water velocity to be constant for the whole water body, in which case we just use the first spline key's : 
	return GetWaterVelocityVectorAtSplineInputKey(0.0f);
}

void UWaterBodyComponent::GetSurfaceMinMaxZ(float& OutMinZ, float& OutMaxZ) const
{
	const float SurfaceZ = GetConstantSurfaceZ();
	const float MaxWaveHeight = GetMaxWaveHeight();
	OutMaxZ = SurfaceZ + MaxWaveHeight;
	OutMinZ = SurfaceZ - MaxWaveHeight;
}

EWaterBodyQueryFlags UWaterBodyComponent::CheckAndAjustQueryFlags(EWaterBodyQueryFlags InQueryFlags) const
{
	EWaterBodyQueryFlags Result = InQueryFlags;

	// Waves only make sense for the following queries : 
	check(!EnumHasAnyFlags(Result, EWaterBodyQueryFlags::IncludeWaves)
		|| EnumHasAnyFlags(Result, EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::ComputeNormal | EWaterBodyQueryFlags::ComputeDepth | EWaterBodyQueryFlags::ComputeImmersionDepth));

	// Simple waves only make sense when computing waves : 
	check(!EnumHasAnyFlags(Result, EWaterBodyQueryFlags::SimpleWaves)
		|| EnumHasAnyFlags(Result, EWaterBodyQueryFlags::IncludeWaves));

	if (EnumHasAnyFlags(InQueryFlags, EWaterBodyQueryFlags::ComputeDepth | EWaterBodyQueryFlags::ComputeImmersionDepth))
	{
		// We need location when querying depth : 
		Result |= EWaterBodyQueryFlags::ComputeLocation;
	}

	if (EnumHasAnyFlags(InQueryFlags, EWaterBodyQueryFlags::IncludeWaves) && HasWaves())
	{
		// We need location and water depth when computing waves :
		Result |= EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::ComputeDepth;
	}

	return Result;
}

bool UWaterBodyComponent::IsWorldLocationInExclusionVolume(const FVector& InWorldLocation) const
{
	for (const TSoftObjectPtr<AWaterBodyExclusionVolume>& ExclusionVolume : WaterBodyExclusionVolumes)
	{
		if (ExclusionVolume.IsValid() && ExclusionVolume->EncompassesPoint(InWorldLocation))
		{
			return true;
		}
	}

	return false;
}

FWaterBodyQueryResult UWaterBodyComponent::QueryWaterInfoClosestToWorldLocation(const FVector& InWorldLocation, EWaterBodyQueryFlags InQueryFlags, const TOptional<float>& InSplineInputKey) const
{
	SCOPE_CYCLE_COUNTER(STAT_WaterBody_ComputeWaterInfo);

	// Use the (optional) input spline input key if it has already been computed: 
	FWaterBodyQueryResult Result(InSplineInputKey);
	Result.SetQueryFlags(CheckAndAjustQueryFlags(InQueryFlags));

	if (!EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::IgnoreExclusionVolumes))
	{
		// No early-out, so that the requested information is still set. It is expected for the caller to check for IsInExclusionVolume() because technically, the returned information will be invalid :
		Result.SetIsInExclusionVolume(IsWorldLocationInExclusionVolume(InWorldLocation));
	}

	// Lakes and oceans have surfaces aligned with the XY plane
	const bool bFlatSurface = IsFlatSurface();

	// Compute water plane location :
	if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeLocation))
	{
		SCOPE_CYCLE_COUNTER(STAT_WaterBody_ComputeLocation);
		FVector WaterPlaneLocation = InWorldLocation;
		// If in exclusion volume, force the water plane location at the query location. It is technically invalid, but it's up to the caller to check whether we're in an exclusion volume. 
		//  If the user fails to do so, at least it allows immersion depth to be 0.0f, which means the query location is NOT in water :
		if (!Result.IsInExclusionVolume())
		{
			WaterPlaneLocation.Z = bFlatSurface ? GetComponentLocation().Z : GetWaterSpline()->GetLocationAtSplineInputKey(Result.LazilyComputeSplineKey(*this, InWorldLocation), ESplineCoordinateSpace::World).Z;

			// Apply body height offset if applicable (ocean)
			if (IsHeightOffsetSupported())
			{
				WaterPlaneLocation.Z += GetHeightOffset();
			}
		}

		Result.SetWaterPlaneLocation(WaterPlaneLocation);
		// When not including waves, water surface == water plane : 
		Result.SetWaterSurfaceLocation(WaterPlaneLocation);
	}

	// Compute water plane normal :
	FVector WaterPlaneNormal = FVector::UpVector;
	if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeNormal))
	{
		SCOPE_CYCLE_COUNTER(STAT_WaterBody_ComputeNormal);
		// Default to Z up for the normal
		if (!bFlatSurface)
		{
			// For rivers default to using spline up vector to account for sloping rivers
			WaterPlaneNormal = GetWaterSpline()->GetUpVectorAtSplineInputKey(Result.LazilyComputeSplineKey(*this, InWorldLocation), ESplineCoordinateSpace::World);
		}

		Result.SetWaterPlaneNormal(WaterPlaneNormal);
		// When not including waves, water surface == water plane : 
		Result.SetWaterSurfaceNormal(WaterPlaneNormal);
	}

	// Compute water plane depth : 
	float WaveAttenuationFactor = 1.0f;
	if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeDepth))
	{
		SCOPE_CYCLE_COUNTER(STAT_WaterBody_ComputeWaterDepth);

		check(EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeLocation));
		float WaterPlaneDepth = 0.0f;

		// The better option for computing water depth for ocean and lake is landscape : 
		const bool bTryUseLandscape = (GetWaterBodyType() == EWaterBodyType::Ocean || GetWaterBodyType() == EWaterBodyType::Lake);
		if (bTryUseLandscape)
		{
			TOptional<float> LandscapeHeightOptional;
			if (ALandscapeProxy* LandscapePtr = FindLandscape())
			{
				SCOPE_CYCLE_COUNTER(STAT_WaterBody_ComputeLandscapeDepth);
				LandscapeHeightOptional = LandscapePtr->GetHeightAtLocation(InWorldLocation);
			}

			bool bValidLandscapeData = LandscapeHeightOptional.IsSet();
			if (bValidLandscapeData)
			{
				WaterPlaneDepth = Result.GetWaterPlaneLocation().Z - LandscapeHeightOptional.GetValue();
				// Special case : cancel out waves for under-landscape ocean
				if ((WaterPlaneDepth < 0.0f) && (GetWaterBodyType() == EWaterBodyType::Ocean))
				{
					WaveAttenuationFactor = 0.0f;
				}
			}

			// If the height is invalid, we either have invalid landscape data or we're under the 
			if (!bValidLandscapeData || (WaterPlaneDepth < 0.0f))
			{
				if (GetWaterBodyType() == EWaterBodyType::Ocean)
				{
					// Fallback value when landscape is not found under the ocean water.
					WaterPlaneDepth = CVarWaterOceanFallbackDepth.GetValueOnAnyThread();
				}
				else
				{
					check(GetWaterBodyType() == EWaterBodyType::Lake);
					// For an underwater lake, consider an uniform depth across the projection segment on the lake spline :
					WaterPlaneDepth = WaterSplineMetadata->Depth.Eval(Result.LazilyComputeSplineKey(*this, InWorldLocation), 0.f);
				}
			}
		}
		else
		{
			// For rivers and transitions, depth always come from the spline :
			WaterPlaneDepth = WaterSplineMetadata->Depth.Eval(Result.LazilyComputeSplineKey(*this, InWorldLocation), 0.f);
		}

		WaterPlaneDepth = FMath::Max(WaterPlaneDepth, 0.0f);
		Result.SetWaterPlaneDepth(WaterPlaneDepth);

		// When not including waves, water surface == water plane : 
		Result.SetWaterSurfaceDepth(WaterPlaneDepth);
	}

	// Optionally compute water surface location/normal/depth for waves : 
	if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::IncludeWaves) && HasWaves())
	{
		SCOPE_CYCLE_COUNTER(STAT_WaterBody_ComputeWaveHeight);
		FWaveInfo WaveInfo;

		if (!Result.IsInExclusionVolume())
		{
			WaveInfo.AttenuationFactor = WaveAttenuationFactor;
			WaveInfo.Normal = WaterPlaneNormal;
			const bool bSimpleWaves = EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::SimpleWaves);
			GetWaveInfoAtPosition(Result.GetWaterPlaneLocation(), Result.GetWaterSurfaceDepth(), bSimpleWaves, WaveInfo);
		}

		Result.SetWaveInfo(WaveInfo);

		if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeLocation))
		{
			FVector WaterSurfaceLocation = Result.GetWaterSurfaceLocation();
			WaterSurfaceLocation.Z += WaveInfo.Height;
			Result.SetWaterSurfaceLocation(WaterSurfaceLocation);
		}

		if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeNormal))
		{
			Result.SetWaterSurfaceNormal(WaveInfo.Normal);
		}

		if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeDepth))
		{
			Result.SetWaterSurfaceDepth(Result.GetWaterSurfaceDepth() + WaveInfo.Height);
		}
	}

	if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeImmersionDepth))
	{
		check(EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeLocation));

		// Immersion depth indicates how much under the water surface is the world location. 
		//  therefore, it takes into account the waves if IncludeWaves is passed :
		Result.SetImmersionDepth(Result.GetWaterSurfaceLocation().Z - InWorldLocation.Z);
		// When in an exclusion volume, the queried location is considered out of water (immersion depth == 0.0f)
		check(!Result.IsInExclusionVolume() || (Result.GetImmersionDepth() == 0.0f));
	}

	// Compute velocity : 
	if (EnumHasAnyFlags(Result.GetQueryFlags(), EWaterBodyQueryFlags::ComputeVelocity))
	{
		FVector Velocity = FVector::ZeroVector;
		if (!Result.IsInExclusionVolume())
		{
			Velocity = GetWaterVelocityVectorAtSplineInputKey(Result.LazilyComputeSplineKey(*this, InWorldLocation));
		}

		Result.SetVelocity(Velocity);
	}

	return Result;

}

void UWaterBodyComponent::GetWaterSurfaceInfoAtLocation(const FVector& InLocation, FVector& OutWaterSurfaceLocation, FVector& OutWaterSurfaceNormal, FVector& OutWaterVelocity, float& OutWaterDepth, bool bIncludeDepth /* = false */) const
{
	EWaterBodyQueryFlags QueryFlags =
		EWaterBodyQueryFlags::ComputeLocation
		| EWaterBodyQueryFlags::ComputeNormal
		| EWaterBodyQueryFlags::ComputeVelocity;

	if (bIncludeDepth)
	{
		QueryFlags |= EWaterBodyQueryFlags::ComputeDepth;
	}

	FWaterBodyQueryResult QueryResult = QueryWaterInfoClosestToWorldLocation(InLocation, QueryFlags);
	OutWaterSurfaceLocation = QueryResult.GetWaterSurfaceLocation();
	OutWaterSurfaceNormal = QueryResult.GetWaterSurfaceNormal();
	OutWaterVelocity = QueryResult.GetVelocity();

	if (bIncludeDepth)
	{
		OutWaterDepth = QueryResult.GetWaterSurfaceDepth();
	}
}

float UWaterBodyComponent::GetWaterVelocityAtSplineInputKey(float InKey) const
{
	return WaterSplineMetadata ? WaterSplineMetadata->WaterVelocityScalar.Eval(InKey, 0.f) : 0.0f;
}

FVector UWaterBodyComponent::GetWaterVelocityVectorAtSplineInputKey(float InKey) const
{
	UWaterSplineComponent* WaterSpline = GetWaterSpline();
	const float WaterVelocityScalar = GetWaterVelocityAtSplineInputKey(InKey);
	const FVector SplineDirection = WaterSpline ? WaterSpline->GetDirectionAtSplineInputKey(InKey, ESplineCoordinateSpace::World) : FVector::ZeroVector;
	return SplineDirection * WaterVelocityScalar;
}

float UWaterBodyComponent::GetAudioIntensityAtSplineInputKey(float InKey) const
{
	return WaterSplineMetadata ? WaterSplineMetadata->AudioIntensity.Eval(InKey, 0.f) : 0.0f;
}

void UWaterBodyComponent::OnRegister()
{
#if WITH_EDITORONLY_DATA
	// Prevents USceneComponent from creating the SpriteComponent in OnRegister because we want to provide a different texture
	bVisualizeComponent = false;
#endif // WITH_EDITORONLY_DATA

	Super::OnRegister();

	AWaterBody* OwningWaterBodyActor = GetWaterBodyActor();
	WaterSplineMetadata = OwningWaterBodyActor->GetWaterSplineMetadata();

	check(WaterSplineMetadata);

#if WITH_EDITOR
	RegisterOnChangeWaterSplineData(/*bRegister = */true); 

	CreateWaterSpriteComponent();
#endif // WITH_EDITOR

	if (AWaterZone* WaterZone = GetWaterZone())
	{
		WaterZone->AddWaterBodyComponent(this);
	}
}

void UWaterBodyComponent::OnUnregister()
{
#if WITH_EDITOR
	RegisterOnChangeWaterSplineData(/*bRegister = */false);
#endif // WITH_EDITOR

	if (AWaterZone* WaterZone = GetWaterZone())
	{
		WaterZone->RemoveWaterBodyComponent(this);
	}

	Super::OnUnregister();
}

TArray<AWaterBodyIsland*> UWaterBodyComponent::GetIslands() const
{
	TArray<AWaterBodyIsland*> IslandActors;
	IslandActors.Reserve(WaterBodyIslands.Num());

	for (const TSoftObjectPtr<AWaterBodyIsland>& IslandPtr : WaterBodyIslands)
	{
		if (AWaterBodyIsland* Island = IslandPtr.Get())
		{
			IslandActors.Add(Island);
		}
	}

	return IslandActors;
}

TArray<AWaterBodyExclusionVolume*> UWaterBodyComponent::GetExclusionVolumes() const
{
	TArray<AWaterBodyExclusionVolume*> Result;
	Result.Reserve(WaterBodyExclusionVolumes.Num());

	for (const TSoftObjectPtr<AWaterBodyExclusionVolume>& VolumePtr : WaterBodyExclusionVolumes)
	{
		if (AWaterBodyExclusionVolume* Volume = VolumePtr.Get())
		{
			Result.Add(Volume);
		}
	}

	return Result;
}

// Our transient MIDs are per-object and shall not survive duplicating nor be exported to text when copy-pasting : 
EObjectFlags UWaterBodyComponent::GetTransientMIDFlags() const
{
	return RF_Transient | RF_NonPIEDuplicateTransient | RF_TextExportTransient;
}

void UWaterBodyComponent::UpdateMaterialInstances()
{
	CreateOrUpdateWaterMID();
	CreateOrUpdateWaterInfoMID();
	CreateOrUpdateWaterLODMID();
	CreateOrUpdateUnderwaterPostProcessMID();
}

bool UWaterBodyComponent::UpdateWaterHeight()
{
	bool bWaterBodyChanged = false;
	const AActor* Owner = GetOwner();
	USplineComponent* WaterSpline = GetWaterSpline();
	if (IsFlatSurface() && WaterSpline && Owner)
	{
		const int32 NumSplinePoints = WaterSpline->GetNumberOfSplinePoints();

		const float ActorZ = Owner->GetActorLocation().Z;

		for (int32 PointIndex = 0; PointIndex < NumSplinePoints; ++PointIndex)
		{
			FVector WorldLoc = WaterSpline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);

			if (WorldLoc.Z != ActorZ)
			{
				bWaterBodyChanged = true;
				WorldLoc.Z = ActorZ;
				WaterSpline->SetLocationAtSplinePoint(PointIndex, WorldLoc, ESplineCoordinateSpace::World);
			}
		}
	}

	return bWaterBodyChanged;
}

void UWaterBodyComponent::CreateOrUpdateWaterMID()
{
	// If GetWorld fails we may be in a blueprint
	if (GetWorld())
	{
		WaterMID = FWaterUtils::GetOrCreateTransientMID(WaterMID, TEXT("WaterMID"), WaterMaterial, GetTransientMIDFlags());

		SetDynamicParametersOnMID(WaterMID);
	}
}

void UWaterBodyComponent::CreateOrUpdateUnderwaterPostProcessMID()
{
	// If GetWorld fails we may be in a blueprint
	if (GetWorld())
	{
		UnderwaterPostProcessMID = FWaterUtils::GetOrCreateTransientMID(UnderwaterPostProcessMID, TEXT("UnderwaterPostProcessMID"), UnderwaterPostProcessMaterial, GetTransientMIDFlags());

		SetDynamicParametersOnUnderwaterPostProcessMID(UnderwaterPostProcessMID);

		// update the transient post process settings accordingly : 
		PrepareCurrentPostProcessSettings();
	}
}

void UWaterBodyComponent::CreateOrUpdateWaterInfoMID()
{
	if (GetWorld())
	{
		WaterInfoMID = FWaterUtils::GetOrCreateTransientMID(WaterInfoMID, TEXT("WaterInfoMID"), WaterInfoMaterial, GetTransientMIDFlags());

		SetDynamicParametersOnWaterInfoMID(WaterInfoMID);
	}
}

void UWaterBodyComponent::CreateOrUpdateWaterLODMID()
{
	if (GetWorld())
	{
		WaterLODMID = FWaterUtils::GetOrCreateTransientMID(WaterLODMID, TEXT("WaterLODMID"), WaterLODMaterial, GetTransientMIDFlags());

		SetDynamicParametersOnMID(WaterLODMID);
	}
}

void UWaterBodyComponent::PrepareCurrentPostProcessSettings()
{
	// Prepare the transient settings that are actually used by the post-process system : 
	// - Copy all the non-transient settings :
	CurrentPostProcessSettings = UnderwaterPostProcessSettings.PostProcessSettings;

	// - Control the WeightedBlendables with the transient underwater post process MID : 
	if (UnderwaterPostProcessMID != nullptr)
	{
		if (CurrentPostProcessSettings.WeightedBlendables.Array.Num() == 0)
		{
			CurrentPostProcessSettings.WeightedBlendables.Array.Emplace();
		}
		FWeightedBlendable& Blendable = CurrentPostProcessSettings.WeightedBlendables.Array[0];
		Blendable.Object = UnderwaterPostProcessMID;
		Blendable.Weight = 1.0f;
	}
	else
	{
		CurrentPostProcessSettings.WeightedBlendables.Array.Empty();
	}
}

ALandscapeProxy* UWaterBodyComponent::FindLandscape() const
{
	if (bAffectsLandscape && !Landscape.IsValid())
	{
		const FVector Location = GetComponentLocation();
		for (TObjectIterator<ALandscapeProxy> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld())
			{
				FBox Box = It->GetComponentsBoundingBox();
				if (Box.IsInsideOrOnXY(Location))
				{
					Landscape = *It;
					return Landscape.Get();
				}
			}
		}
	}
	return Landscape.Get();
}

void UWaterBodyComponent::UpdateComponentVisibility(bool bAllowWaterMeshRebuild)
{
	if (UWorld* World = GetWorld())
	{
	 	const bool bIsWaterRenderingEnabled = FWaterUtils::IsWaterEnabled(/*bIsRenderThread = */false);
	 
		bool bIsRenderedByWaterMesh = ShouldGenerateWaterMeshTile();
		bool bLocalVisible = bIsWaterRenderingEnabled && !bIsRenderedByWaterMesh && GetVisibleFlag();
		bool bLocalHiddenInGame = !bIsWaterRenderingEnabled || bIsRenderedByWaterMesh || bHiddenInGame;

	 	for (UPrimitiveComponent* Component : GetStandardRenderableComponents())
	 	{
	 		Component->SetVisibility(bLocalVisible);
	 		Component->SetHiddenInGame(bLocalHiddenInGame);
	 	}

		if (AWaterZone* WaterZone = GetWaterZone())
		{
			// If the component is being or can be rendered by the water mesh or renders into the water info texture, rebuild it in case its visibility has changed : 
			if (bAllowWaterMeshRebuild && AffectsWaterMesh())
			{
				WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterMesh);
			}

			if (AffectsWaterInfo())
			{
				WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
			}
		}
	}
}

#if WITH_EDITOR
void UWaterBodyComponent::PreEditUndo()
{
	Super::PreEditUndo();

	// On undo, when PreEditChange is called, PropertyAboutToChange is nullptr so we need to unregister from the previous object here :
	RegisterOnUpdateWavesData(GetWaterWaves(), /*bRegister = */false);
}

void UWaterBodyComponent::PostEditUndo()
{
	Super::PostEditUndo();

	// Since this component may become unregistered/deleted if we are undoing the creation of a water body.
	// Ensure we only trigger updates if this component is registered.
	if (IsRegistered())
	{
		FOnWaterBodyChangedParams Params;
		Params.bShapeOrPositionChanged = true;
		Params.bWeightmapSettingsChanged = true;
		OnWaterBodyChanged(Params);

		// On undo, when PostEditChangeProperty is called, PropertyChangedEvent is fake so we need to register to the new object here :
		RegisterOnUpdateWavesData(GetWaterWaves(), /*bRegister = */true);

		RequestGPUWaveDataUpdate();
	}
}

void UWaterBodyComponent::PostEditImport()
{
	Super::PostEditImport();

	FOnWaterBodyChangedParams Params;
	Params.bShapeOrPositionChanged = true;
	Params.bWeightmapSettingsChanged = true;
	OnWaterBodyChanged(Params);

	RequestGPUWaveDataUpdate();
}

void UWaterBodyComponent::OnPostEditChangeProperty(FOnWaterBodyChangedParams& InOutOnWaterBodyChangedParams)
{
	const FPropertyChangedEvent& PropertyChangedEvent = InOutOnWaterBodyChangedParams.PropertyChangedEvent;
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UWaterBodyComponent, LayerWeightmapSettings))
	{
		InOutOnWaterBodyChangedParams.bWeightmapSettingsChanged = true;
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(UWaterBodyComponent, WaterMaterial)) ||
			(PropertyName == GET_MEMBER_NAME_CHECKED(UWaterBodyComponent, UnderwaterPostProcessMaterial)) ||
			(PropertyName == GET_MEMBER_NAME_CHECKED(UWaterBodyComponent, WaterInfoMaterial)))
	{
		UpdateMaterialInstances();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaterBodyComponent, TargetWaveMaskDepth))
	{
		RequestGPUWaveDataUpdate();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaterBodyComponent, MaxWaveHeightOffset))
	{
		// Waves data affect the navigation :
		InOutOnWaterBodyChangedParams.bShapeOrPositionChanged = true;
	}
	else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == FName(TEXT("RelativeScale3D")))
	{
		// All water bodies which can ever be rendered by the water mesh shouldn't have a z-scale.
		if (CanEverAffectWaterMesh())
		{
			FVector Scale = GetRelativeScale3D();
			Scale.Z = 1.f;
			SetRelativeScale3D(Scale);
		}
		InOutOnWaterBodyChangedParams.bShapeOrPositionChanged = true;
	}
}

TArray<TSharedRef<FTokenizedMessage>> UWaterBodyComponent::CheckWaterBodyStatus() const
{
	TArray<TSharedRef<FTokenizedMessage>> Result;

	const UWorld* World = GetWorld();
	if (!IsTemplate() && World && World->WorldType != EWorldType::EditorPreview)
	{
		if (AffectsWaterMesh() && (GetWaterZone() == nullptr))
		{
			Result.Add(FTokenizedMessage::Create(EMessageSeverity::Error)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(
					LOCTEXT("MapCheck_Message_MissingWaterZone", "Water body {0} requires a WaterZone actor to be rendered. Please add one to the map. "),
					FText::FromString(GetWaterBodyActor()->GetActorLabel())))));
		}

		if (AffectsLandscape() && (FindLandscape() == nullptr))
		{
			Result.Add(FTokenizedMessage::Create(EMessageSeverity::Error)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(FText::Format(
					LOCTEXT("MapCheck_Message_MissingLandscape", "Water body {0} requires a Landscape to be rendered. Please add one to the map. "),
					FText::FromString(GetWaterBodyActor()->GetActorLabel())))));
		}
	}
	return Result;
}

void UWaterBodyComponent::CheckForErrors()
{
	Super::CheckForErrors();

	TArray<TSharedRef<FTokenizedMessage>> StatusMessages = CheckWaterBodyStatus();
	for (const TSharedRef<FTokenizedMessage>& StatusMessage : StatusMessages)
	{
		FMessageLog("MapCheck").AddMessage(StatusMessage);
	}
}

void UWaterBodyComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FOnWaterBodyChangedParams Params(PropertyChangedEvent);
	OnPostEditChangeProperty(Params);
	
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsTemplate())
	{
		OnWaterBodyChanged(Params);
	}
}

void UWaterBodyComponent::OnWaterSplineDataChanged(const FOnWaterSplineDataChangedParams& InParams)
{
	// Transfer the FOnWaterSplineDataChangedParams parameters to FOnWaterBodyChangedParams :
	FOnWaterBodyChangedParams Params(InParams.PropertyChangedEvent);
	Params.bShapeOrPositionChanged = true;
	OnWaterBodyChanged(Params);
}

void UWaterBodyComponent::RegisterOnUpdateWavesData(UWaterWavesBase* InWaterWaves, bool bRegister)
{
	if (InWaterWaves != nullptr)
	{
		if (bRegister)
		{
			InWaterWaves->OnUpdateWavesData.AddUObject(this, &UWaterBodyComponent::OnWavesDataUpdated);
		}
		else
		{
			InWaterWaves->OnUpdateWavesData.RemoveAll(this);
		}
	}
}

void UWaterBodyComponent::OnWavesDataUpdated(UWaterWavesBase* InWaterWaves, EPropertyChangeType::Type InChangeType)
{
	RequestGPUWaveDataUpdate();

	FOnWaterBodyChangedParams Params;
	// Waves data affect the navigation :
	Params.PropertyChangedEvent.ChangeType = InChangeType;
	Params.bShapeOrPositionChanged = true;
	OnWaterBodyChanged(Params);
}

void UWaterBodyComponent::OnWaterSplineMetadataChanged(UWaterSplineMetadata* InWaterSplineMetadata, FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bShapeOrPositionChanged = false;

	FName ChangedProperty = PropertyChangedEvent.GetPropertyName();
	if ((ChangedProperty == NAME_None)
		|| (ChangedProperty == GET_MEMBER_NAME_CHECKED(UWaterSplineMetadata, Depth))
		|| (ChangedProperty == GET_MEMBER_NAME_CHECKED(UWaterSplineMetadata, RiverWidth))
		|| (ChangedProperty == GET_MEMBER_NAME_CHECKED(UWaterSplineMetadata, WaterVelocityScalar)))
	{
		// Those changes require an update of the water brush (except in interactive mode, where we only apply the change once the value is actually set): 
		bShapeOrPositionChanged = true;
	}

	if ((ChangedProperty == NAME_None)
		|| (ChangedProperty == GET_MEMBER_NAME_CHECKED(UWaterSplineMetadata, RiverWidth)))
	{ 
		// River Width is driving the spline shape, make sure the spline component is aware of the change : 
		GetWaterSpline()->SynchronizeWaterProperties();
	}

	// Waves data affect the navigation : 
	OnWaterBodyChanged(bShapeOrPositionChanged);
}

void UWaterBodyComponent::RegisterOnChangeWaterSplineData(bool bRegister)
{
	if (UWaterSplineComponent* WaterSpline = GetWaterSpline())
	{
		if (bRegister)
		{
			WaterSpline->OnWaterSplineDataChanged().AddUObject(this, &UWaterBodyComponent::OnWaterSplineDataChanged);
		}
		else
		{
			WaterSpline->OnWaterSplineDataChanged().RemoveAll(this);
		}
	}

	if (WaterSplineMetadata != nullptr)
	{
		if (bRegister)
		{
			WaterSplineMetadata->OnChangeData.AddUObject(this, &UWaterBodyComponent::OnWaterSplineMetadataChanged);
		}
		else
		{
			WaterSplineMetadata->OnChangeData.RemoveAll(this);
		}
	}
}

#endif // WITH_EDITOR

void UWaterBodyComponent::GetNavigationData(struct FNavigationRelevantData& Data) const
{
	if (IsNavigationRelevant())
	{
		TArray<UPrimitiveComponent*> LocalCollisionComponents = GetCollisionComponents(/*bInOnlyEnabledComponents = */true);
		for (int32 CompIdx = 0; CompIdx < LocalCollisionComponents.Num(); CompIdx++)
		{
			UPrimitiveComponent* PrimComp = LocalCollisionComponents[CompIdx];
			if (PrimComp == nullptr)
			{
				UE_LOG(LogNavigation, Warning, TEXT("%s: skipping null collision component at index %d in %s"), ANSI_TO_TCHAR(__FUNCTION__), CompIdx, *GetFullNameSafe(this));
				continue;
			}


			FCompositeNavModifier CompositeNavModifier;
			CompositeNavModifier.CreateAreaModifiers(PrimComp, WaterNavAreaClass);
			for (FAreaNavModifier& AreaNavModifier : CompositeNavModifier.GetMutableAreas())
			{
				AreaNavModifier.SetExpandTopByCellHeight(true);
			}

			Data.Modifiers.Add(CompositeNavModifier);
			// skip recursion on this component
			if (PrimComp != this)
			{
				PrimComp->GetNavigationData(Data);
			}
		}
	}
}

FBox UWaterBodyComponent::GetNavigationBounds() const
{
	return GetCollisionComponentBounds();
}

bool UWaterBodyComponent::IsNavigationRelevant() const
{
	return Super::IsNavigationRelevant() && (GetCollisionComponents().Num() > 0);
}

void UWaterBodyComponent::ApplyCollisionSettings()
{
	// Transfer the collision settings of the water body component to all of its child collision components 
	TArray<UPrimitiveComponent*> CollisionComponents = GetCollisionComponents(/*bInOnlyEnabledComponents = */false);
	for (UPrimitiveComponent* CollisionComponent : CollisionComponents)
	{
		CopySharedCollisionSettingsToComponent(CollisionComponent);
	}
}

void UWaterBodyComponent::ApplyNavigationSettings()
{
	// Transfer the navigation settings of the water body component to all of its child collision components 
	const TArray<UPrimitiveComponent*> CollisionComponents = GetCollisionComponents(/*bInOnlyEnabledComponents = */false);
	for (UPrimitiveComponent* CollisionComponent : CollisionComponents)
	{
		CopySharedNavigationSettingsToComponent(CollisionComponent);
	}
}

void UWaterBodyComponent::RequestGPUWaveDataUpdate()
{
	if (FWaterBodyManager* Manager = UWaterSubsystem::GetWaterBodyManager(GetWorld()))
	{
		Manager->RequestWaveDataRebuild();
	}
}

void UWaterBodyComponent::BeginUpdateWaterBody()
{
	UpdateSplineComponent();
}

void UWaterBodyComponent::UpdateWaterBody(bool bWithExclusionVolumes)
{
	// The first update is without exclusion volumes : perform it.
	// The second update is with exclusion volumes but there's no need to perform it again if we don't have exclusion volumes anyway, because the result will be the same.
	if (!bWithExclusionVolumes || GetExclusionVolumes().Num() > 0)
	{
		OnUpdateBody(bWithExclusionVolumes);
	}
}

void UWaterBodyComponent::UpdateAll(const FOnWaterBodyChangedParams& InParams)
{
	BeginUpdateWaterBody();

	AWaterBody* WaterBodyOwner = GetWaterBodyActor();
	check(WaterBodyOwner);

	bool bShapeOrPositionChanged = InParams.bShapeOrPositionChanged;
	
	if (GIsEditor || IsBodyDynamic())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UWaterBodyComponent::UpdateAll);

		bShapeOrPositionChanged |= UpdateWaterHeight();

		if (bShapeOrPositionChanged)
		{
			// We might be affected to a different landscape now that our shape has changed : 
			Landscape.Reset();
		}

		// First, update the water body without taking into account exclusion volumes, as those rely on the collision to detect overlapping water bodies
		UpdateWaterBody(/* bWithExclusionVolumes*/false);

		// Then, update the list of exclusion volumes after this adjustment
		if (bShapeOrPositionChanged)
		{
			UpdateIslands();

			UpdateExclusionVolumes();

			UpdateWaterZones();
		}

		// Finally, generate the body once again, this time with the updated list of exclusion volumes
		UpdateWaterBody(/*bWithExclusionVolumes*/true);

		ApplyCollisionSettings();

		ApplyNavigationSettings();

		if (bShapeOrPositionChanged)
		{
			FNavigationSystem::UpdateActorAndComponentData(*WaterBodyOwner);

			UpdateWaterBodyRenderData();
		}

		UpdateComponentVisibility(/* bAllowWaterMeshRebuild = */true);

#if WITH_EDITOR
		UpdateWaterSpriteComponent();
#endif
	}

	// #todo_water: this should be reconsidered along with the entire concept of non-dynamic water bodies now that evening is runtime-generated.
	if (bShapeOrPositionChanged)
	{
		UpdateWaterBodyRenderData();
	}
}

void UWaterBodyComponent::UpdateSplineComponent()
{
	if (USplineComponent* WaterSpline = GetWaterSpline())
	{
		WaterSpline->SetClosedLoop(IsWaterSplineClosedLoop());
	}
}

void UWaterBodyComponent::OnWaterBodyChanged(bool bShapeOrPositionChanged, bool bWeightmapSettingsChanged)
{
	FOnWaterBodyChangedParams Params;
	Params.bShapeOrPositionChanged = bShapeOrPositionChanged;
	Params.bWeightmapSettingsChanged = bWeightmapSettingsChanged;
	OnWaterBodyChanged(Params);
}

void UWaterBodyComponent::OnWaterBodyChanged(const FOnWaterBodyChangedParams& InParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWaterBodyComponent::OnWaterBodyChanged)
	// It's possible to get called without a water spline after the Redo of a water body deletion (i.e. the water body actor gets deleted again, hence its SplineComp is restored to nullptr)
	//  This is a very-edgy case that needs to be checked everywhere that UpdateAll might hook into so it's simpler to just skip it all. The actor is in limbo by then anyway (it only survives because
	//  of the editor transaction) :
	if (GetWaterSpline())
	{
		UpdateAll(InParams);

		// Some of the spline parameters need to be transferred to the underwater post process MID, if any : 
		if (InParams.bShapeOrPositionChanged)
		{
			SetDynamicParametersOnUnderwaterPostProcessMID(UnderwaterPostProcessMID);
		}
	}

#if WITH_EDITOR
	AWaterBody* const WaterBodyActor = GetWaterBodyActor();
	// Transfer the FOnWaterBodyChangedParams parameters to FWaterBrushActorChangedEventParams :
	IWaterBrushActorInterface::FWaterBrushActorChangedEventParams Params(WaterBodyActor, InParams.PropertyChangedEvent);
	Params.bShapeOrPositionChanged = InParams.bShapeOrPositionChanged;
	Params.bWeightmapSettingsChanged = InParams.bWeightmapSettingsChanged;
	WaterBodyActor->BroadcastWaterBrushActorChangedEvent(Params);
#endif
}

void UWaterBodyComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FWaterCustomVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
}

void UWaterBodyComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// WaterMeshOverride is now enough to override the water mesh (bOverrideWaterMesh_DEPRECATED was superfluous), so make sure to discard WaterMeshOverride (except on custom water bodies) when the boolean wasn't set :
	if (!bOverrideWaterMesh_DEPRECATED && (WaterMeshOverride != nullptr) && (GetWaterBodyType() != EWaterBodyType::Transition))
	{
		WaterMeshOverride = nullptr;
	}

	// If available, use far mesh material as the HLOD material for water bodies created before HLOD support was added.
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WaterHLODSupportAdded)
	{
		if (const AWaterZone* WaterZone = GetWaterZone())
		{
			const UWaterMeshComponent* WaterMeshComponent = WaterZone->GetWaterMeshComponent();
			check(WaterMeshComponent);

			WaterHLODMaterial = WaterMeshComponent->FarDistanceMaterial;
		}
	}
#endif // WITH_EDITORONLY_DATA

	DeprecateData();

	if (IsComponentPSOPrecachingEnabled())
	{
		FPSOPrecacheParams PrecachePSOParams;
		SetupPrecachePSOParams(PrecachePSOParams);
		if (WaterMaterial)
		{
			WaterMaterial->PrecachePSOs(&FLocalVertexFactory::StaticType, PrecachePSOParams);
		}
		if (UnderwaterPostProcessMaterial)
		{
			UnderwaterPostProcessMaterial->PrecachePSOs(&FLocalVertexFactory::StaticType, PrecachePSOParams);
		}
		if (WaterInfoMaterial)
		{
			WaterInfoMaterial->PrecachePSOs(&FLocalVertexFactory::StaticType, PrecachePSOParams);
		}
	}

#if WITH_EDITOR
	RegisterOnUpdateWavesData(GetWaterWaves(), /* bRegister = */true);
#endif // WITH_EDITOR
}

void UWaterBodyComponent::DeprecateData()
{
#if WITH_EDITORONLY_DATA
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WaterBodyComponentCollisionSettingsRefactor)
	{
		// Deprecate the old collision / navigation data and update it on all sub-components :
		SetCollisionProfileName(CollisionProfileName_DEPRECATED);
		SetGenerateOverlapEvents(bGenerateCollisions_DEPRECATED);
		// Transfer info to sub-components :
		ApplyCollisionSettings();

		bool bCanAffectNav = bGenerateCollisions_DEPRECATED && bCanAffectNavigation_DEPRECATED;
		SetCustomNavigableGeometry(bCanAffectNav ? EHasCustomNavigableGeometry::EvenIfNotCollidable : EHasCustomNavigableGeometry::No);
		SetCanEverAffectNavigation(bCanAffectNav);
		bFillCollisionUnderneathForNavmesh = bFillCollisionUnderWaterBodiesForNavmesh_DEPRECATED;
		// Transfer info to sub-components :
		ApplyNavigationSettings();
	}
#endif // WITH_EDITORONLY_DATA
}

void UWaterBodyComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITOR
	RegisterOnChangeWaterSplineData(/*bRegister = */false);
	RegisterOnUpdateWavesData(GetWaterWaves(), /*bRegister = */false);
#endif // WITH_EDITOR

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

bool UWaterBodyComponent::MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* Hit, EMoveComponentFlags MoveFlags, ETeleportType Teleport)
{
	FQuat CorrectedRotation = NewRotation;

	// All water bodies which can ever be rendered by the water mesh shouldn't have a z-scale or non-z rotation
	if (CanEverAffectWaterMesh())
	{
		FVector Scale = GetRelativeScale3D();
		Scale.Z = 1.f;
		SetRelativeScale3D(Scale);

		// Restrict rotation to the Z-axis only
		CorrectedRotation.X = 0.f;
		CorrectedRotation.Y = 0.f;
	}
	return Super::MoveComponentImpl(Delta, CorrectedRotation, bSweep, Hit, MoveFlags, Teleport);
}

void UWaterBodyComponent::OnComponentCollisionSettingsChanged(bool bUpdateOverlaps)
{
	if (IsRegistered() && !IsTemplate())			// not for CDOs
	{
		Super::OnComponentCollisionSettingsChanged(bUpdateOverlaps);

		// Transfer all settings leading to OnComponentCollisionSettingsChanged to be called to the sub-components handling collisions:
		ApplyCollisionSettings();
	}
}

void UWaterBodyComponent::OnGenerateOverlapEventsChanged()
{
	if (IsRegistered() && !IsTemplate())			// not for CDOs
	{
		Super::OnGenerateOverlapEventsChanged();

		ApplyCollisionSettings();
	}
}

void UWaterBodyComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	uint32 SectionsSize = 0;
	for (const FWaterBodyMeshSection& Section : WaterBodyMeshSections)
	{
		SectionsSize += Section.GetAllocatedSize();
	}

	// Account for all non-editor data properties :
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(
		WaterBodyMeshSections.GetAllocatedSize() 
		+ SectionsSize
		+ WaterBodyMeshIndices.GetAllocatedSize()
		+ WaterBodyMeshVertices.GetAllocatedSize()
		+ DilatedWaterBodyMeshIndices.GetAllocatedSize()
		+ DilatedWaterBodyMeshVertices.GetAllocatedSize());
}

bool UWaterBodyComponent::SetDynamicParametersOnMID(UMaterialInstanceDynamic* InMID)
{
	UWaterSubsystem* WaterSubsystem = UWaterSubsystem::GetWaterSubsystem(GetWorld());
	if ((InMID == nullptr) || (WaterSubsystem == nullptr))
	{
		return false;
	}

	const float GlobalOceanHeight = WaterSubsystem->GetOceanTotalHeight();
	InMID->SetScalarParameterValue(WaterBodyIndexParamName, WaterBodyIndex);
	InMID->SetScalarParameterValue(GlobalOceanHeightName, GlobalOceanHeight);
	InMID->SetScalarParameterValue(FixedZHeightName, GetConstantSurfaceZ());
	InMID->SetScalarParameterValue(FixedWaterDepthName, GetConstantDepth());

	InMID->SetVectorParameterValue(FixedVelocityName, GetConstantVelocity());

	if (const AWaterZone* WaterZone = GetWaterZone())
	{
		InMID->SetTextureParameterValue(WaterVelocityAndHeightName, WaterZone->WaterInfoTexture);

		const UWaterMeshComponent* WaterMeshComponent = WaterZone->GetWaterMeshComponent();
		check(WaterMeshComponent);

		// Location should be the bottom left of the zone
		const FVector2D WaterInfoExtent = FVector2D(WaterZone->GetTessellatedWaterMeshExtent());
		const FVector2D WaterInfoLocation = FVector2D(WaterZone->GetTessellatedWaterMeshCenter()) - (WaterInfoExtent / 2.f);

		FVector4 WaterArea;
		WaterArea.X = WaterInfoLocation.X;
		WaterArea.Y = WaterInfoLocation.Y;
		WaterArea.Z = WaterInfoExtent.X;
		WaterArea.W = WaterInfoExtent.Y;
		InMID->SetDoubleVectorParameterValue(WaterAreaParamName, WaterArea);

		const FVector2f WaterHeightExtents = WaterZone->GetWaterHeightExtents();
		const float GroundZMin = WaterZone->GetGroundZMin();
		InMID->SetScalarParameterValue(WaterZMinParamName, WaterHeightExtents.X);
		InMID->SetScalarParameterValue(WaterZMaxParamName, WaterHeightExtents.Y);
		InMID->SetScalarParameterValue(GroundZMinParamName, GroundZMin);
	}

	return true;
}

bool UWaterBodyComponent::SetDynamicParametersOnUnderwaterPostProcessMID(UMaterialInstanceDynamic* InMID)
{
	UWaterSubsystem* WaterSubsystem = UWaterSubsystem::GetWaterSubsystem(GetWorld());
	if ((InMID == nullptr) || (WaterSubsystem == nullptr))
	{
		return false;
	}

	// The post process MID needs the same base parameters as the water materials : 
	SetDynamicParametersOnMID(InMID);

	// Add here the list of parameters that the underwater material needs (for not nothing more than the standard material) :

	return true;
}

bool UWaterBodyComponent::SetDynamicParametersOnWaterInfoMID(UMaterialInstanceDynamic* InMID)
{
	UWaterSubsystem* WaterSubsystem = UWaterSubsystem::GetWaterSubsystem(GetWorld());
	if ((InMID == nullptr) || (WaterSubsystem == nullptr))
	{
		return false;
	}

	InMID->SetScalarParameterValue(WaterBodyZOffsetParamName, WaterHeightmapSettings.FalloffSettings.ZOffset);
	InMID->SetScalarParameterValue(MaxFlowVelocityParamName, FWaterUtils::GetWaterMaxFlowVelocity(false));

	return true;
}

float UWaterBodyComponent::GetWaveReferenceTime() const
{
	if (HasWaves())
	{
		if (UWaterSubsystem* WaterSubsystem = UWaterSubsystem::GetWaterSubsystem(GetWorld()))
		{
			return WaterSubsystem->GetWaterTimeSeconds();
		}
	}
	return 0.0f;
}

/** Returns wave-related information at the given world position and for this water depth.
 Pass bSimpleWaves = true for the simple version (faster computation, lesser accuracy, doesn't perturb the normal) */
bool UWaterBodyComponent::GetWaveInfoAtPosition(const FVector& InPosition, float InWaterDepth, bool bInSimpleWaves, FWaveInfo& InOutWaveInfo) const
{
	if (!HasWaves())
	{
		return false; //Collision needs to be fixed for rivers
	}

	float MaxWaveHeight = GetMaxWaveHeight();

	InOutWaveInfo.ReferenceTime = GetWaveReferenceTime();
	InOutWaveInfo.AttenuationFactor *= GetWaveAttenuationFactor(InPosition, InWaterDepth);

	// No need to perform computation if we're going to cancel it out afterwards :
	if (InOutWaveInfo.AttenuationFactor > 0.0f)
	{
		// Maximum amplitude that the wave can reach at this location : 
		InOutWaveInfo.MaxHeight = MaxWaveHeight * InOutWaveInfo.AttenuationFactor;

		float WaveHeight;
		if (bInSimpleWaves)
		{
			WaveHeight = GetSimpleWaveHeightAtPosition(InPosition, InWaterDepth, InOutWaveInfo.ReferenceTime);
		}
		else
		{
			FVector ComputedNormal;
			WaveHeight = GetWaveHeightAtPosition(InPosition, InWaterDepth, InOutWaveInfo.ReferenceTime, ComputedNormal);
			// Attenuate the normal :
			ComputedNormal = FMath::Lerp(InOutWaveInfo.Normal, ComputedNormal, InOutWaveInfo.AttenuationFactor);
			if (!ComputedNormal.IsZero())
			{
				InOutWaveInfo.Normal = ComputedNormal;
			}
		}

		// Attenuate the wave amplitude :
		InOutWaveInfo.Height = WaveHeight * InOutWaveInfo.AttenuationFactor;
	}

	return true;
}

float UWaterBodyComponent::GetMaxWaveHeight() const
{
	return (HasWaves() ? GetWaterWaves()->GetMaxWaveHeight() : 0.0f) + MaxWaveHeightOffset;
}

void UWaterBodyComponent::CopySharedCollisionSettingsToComponent(UPrimitiveComponent* InComponent)
{
	InComponent->SetCollisionEnabled(GetCollisionEnabled());
	InComponent->SetNotifyRigidBodyCollision(BodyInstance.bNotifyRigidBodyCollision);
	InComponent->SetCollisionResponseToChannels(BodyInstance.GetResponseToChannels());
	InComponent->SetCollisionProfileName(GetCollisionProfileName(), /*bUpdateOverlaps=*/ true);
	InComponent->SetGenerateOverlapEvents(GetGenerateOverlapEvents());
}

void UWaterBodyComponent::CopySharedNavigationSettingsToComponent(UPrimitiveComponent* InComponent)
{
	InComponent->SetCanEverAffectNavigation(CanEverAffectNavigation());
	InComponent->SetCustomNavigableGeometry(HasCustomNavigableGeometry());
	InComponent->bFillCollisionUnderneathForNavmesh = GetCollisionEnabled() != ECollisionEnabled::NoCollision && bFillCollisionUnderneathForNavmesh;
}

float UWaterBodyComponent::GetWaveHeightAtPosition(const FVector& InPosition, float InWaterDepth, float InTime, FVector& OutNormal) const
{
	check(HasWaves());

	return GetWaterWaves()->GetWaveHeightAtPosition(InPosition, InWaterDepth, InTime, OutNormal);
}

float UWaterBodyComponent::GetSimpleWaveHeightAtPosition(const FVector& InPosition, float InWaterDepth, float InTime) const
{
	check(HasWaves());

	return GetWaterWaves()->GetSimpleWaveHeightAtPosition(InPosition, InWaterDepth, InTime);
}

float UWaterBodyComponent::GetWaveAttenuationFactor(const FVector& InPosition, float InWaterDepth) const
{
	check(HasWaves());

	return GetWaterWaves()->GetWaveAttenuationFactor(InPosition, InWaterDepth, TargetWaveMaskDepth);
}

UWaterWavesBase* UWaterBodyComponent::GetWaterWaves() const
{
	if (AWaterBody* OwningWaterBody = GetWaterBodyActor())
	{
		return OwningWaterBody->GetWaterWaves();
	}
	return nullptr;
}

AWaterZone* UWaterBodyComponent::FindWaterZone() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWaterBodyComponent::FindWaterZone);

	if (!WaterZoneOverride.IsNull())
	{
		return WaterZoneOverride.Get();
	}

	// Score each overlapping water zone and then pick the best.
	TArray<TPair<int32, AWaterZone*>, TInlineAllocator<4>> ViableZones;

	if (const UWorld* World = GetWorld())
	{
		const ULevel* PreferredLevel = GetTypedOuter<ULevel>();
		if (PreferredLevel)
		{
			for (AWaterZone* WaterZone : TActorRange<AWaterZone>(World, AWaterZone::StaticClass(), EActorIteratorFlags::SkipPendingKill))
			{
				// WaterZone->GetZoneExtents returns the full extent of the zone but BoxSphereBounds expects a half-extent.
				const FVector2D WaterZoneLocation = FVector2D(WaterZone->GetActorLocation());
				const FVector2D WaterZoneHalfExtent = WaterZone->GetZoneExtent() / 2.0;
				const FBox2D WaterZoneBounds(WaterZoneLocation - WaterZoneHalfExtent, WaterZoneLocation + WaterZoneHalfExtent);
				const FBox ComponentBounds3d = CalcBounds(GetComponentTransform()).GetBox();
				const FBox2D ComponentBounds = FBox2D(FVector2D(ComponentBounds3d.Min), FVector2D(ComponentBounds3d.Max));

				// Only consider WaterZones which this component overlaps 
				// but prefer choosing water zones which are part of the same outered level.
				if (ComponentBounds.Intersect(WaterZoneBounds))
				{
					if (WaterZone->GetTypedOuter<ULevel>() == PreferredLevel)
					{
						ViableZones.Add({ WaterZone->GetOverlapPriority(), WaterZone });
						continue;
					}

					// Fallback to zones not in the preferred level only as a final resort.
					ViableZones.Add({ TNumericLimits<int32>::Min(), WaterZone});
				}
			}
		}
	}
	
	if (ViableZones.Num() == 0)
	{
		return nullptr;
	}

	return Algo::MaxElementBy(ViableZones, [](const TPair<int32, AWaterZone*>& A) { return A.Key; })->Value;
}

static inline FColor PackFlowData(float VelocityMagnitude, float DirectionAngle, float MaxVelocity)
{
	check((DirectionAngle >= 0.f) && (DirectionAngle <= TWO_PI));

	const float NormalizedMagnitude = FMath::Clamp(VelocityMagnitude, 0.f, MaxVelocity) / MaxVelocity;
	const float NormalizedAngle = DirectionAngle / TWO_PI;
	const float MappedMagnitude = NormalizedMagnitude * TNumericLimits<uint16>::Max();
	const float MappedAngle = NormalizedAngle * TNumericLimits<uint16>::Max();

	const uint16 ResultMag = (uint16)MappedMagnitude;
	const uint16 ResultAngle = (uint16)MappedAngle;
	
	FColor Result;
	Result.R = (ResultMag >> 8) & 0xFF;
	Result.G = (ResultMag >> 0) & 0xFF;
	Result.B = (ResultAngle >> 8) & 0xFF;
	Result.A = (ResultAngle >> 0) & 0xFF;

	return Result;
}


void UWaterBodyComponent::RebuildWaterBodyInfoMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWaterBodyComponent::RebuildWaterBodyInfoMesh);

	using namespace UE::Geometry;

	WaterBodyMeshVertices.Empty();
	WaterBodyMeshIndices.Empty();
	DilatedWaterBodyMeshVertices.Empty();
	DilatedWaterBodyMeshIndices.Empty();

	FDynamicMesh3 WaterBodyMesh(EMeshComponents::VertexColors);
	FDynamicMesh3 WaterBodyDilatedMesh(EMeshComponents::None);

	GenerateWaterBodyMesh(WaterBodyMesh, &WaterBodyDilatedMesh);
	
	const float MaxFlowVelocity = FWaterUtils::GetWaterMaxFlowVelocity(false);
	WaterBodyMeshVertices.Reserve(WaterBodyMesh.VertexCount());
	WaterBodyMeshIndices.Reserve(3 * WaterBodyMesh.TriangleCount());
	for (int VertexIndex : WaterBodyMesh.VertexIndicesItr())
	{
		FVertexInfo VertexInfo;
		const bool bWantColors = true;
		const bool bWantNormals = false;
		const bool bWantUVs = false;
		WaterBodyMesh.GetVertex(VertexIndex, VertexInfo, bWantNormals, bWantColors, bWantUVs);

		const FVector3f LocalSpacePosition = FVector3f(VertexInfo.Position);
		FDynamicMeshVertex MeshVertex(LocalSpacePosition);

		const float FlowSpeed = VertexInfo.Color.X;
		const float FlowDirection = VertexInfo.Color.Y;
		MeshVertex.Color = PackFlowData(FlowSpeed, FlowDirection, MaxFlowVelocity);

		WaterBodyMeshVertices.Add(MeshVertex);
	}
	for (FIndex3i Triangle : WaterBodyMesh.TrianglesItr())
	{
		WaterBodyMeshIndices.Append({ (uint32)Triangle.A, (uint32)Triangle.B, (uint32)Triangle.C });
	}

	DilatedWaterBodyMeshIndices.Reserve(3 * WaterBodyDilatedMesh.TriangleCount());
	DilatedWaterBodyMeshVertices.Reserve(WaterBodyDilatedMesh.VertexCount());
	for (FVector3d Vertex : WaterBodyDilatedMesh.VerticesItr())
	{
		DilatedWaterBodyMeshVertices.Emplace(FVector3f(Vertex));
	}
	for (FIndex3i Triangle : WaterBodyDilatedMesh.TrianglesItr())
	{
		DilatedWaterBodyMeshIndices.Append( {(uint32)Triangle.A, (uint32)Triangle.B, (uint32)Triangle.C });
	}
}

void UWaterBodyComponent::RebuildWaterBodyLODSections()
{
	using namespace UE::Geometry;

	WaterBodyMeshSections.Empty();

	TRACE_CPUPROFILER_EVENT_SCOPE(UWaterBodyComponent::RebuildWaterBodyLODSections);

	const AWaterZone* WaterZone = GetWaterZone();
	check(WaterZone);

	const UWaterMeshComponent* WaterMesh = WaterZone->GetWaterMeshComponent();
	check(WaterMesh);

	const float WaterLODSectionSize = WaterZone->GetNonTessellatedLODSectionSize();
	const FVector2i WaterZoneExtentInLODSections = FVector2i(FMath::DivideAndRoundUp(WaterZone->GetZoneExtent(), FVector2d(WaterLODSectionSize)));

	const float GridTileSize = WaterMesh->GetTileSize();
	const FVector2d GridPosition = FVector2d(WaterMesh->GetComponentLocation().GridSnap(GridTileSize));
	const FVector2d MeshOrigin = GridPosition - (GridTileSize * FVector2d(WaterMesh->GetExtentInTiles()));

	FDynamicMesh3 EditMesh(EMeshComponents::VertexColors);

	GenerateWaterBodyMesh(EditMesh);

	// Transform everything to world space. This is necessary to maintain an axis-aligned grid. The water mesh is guaranteed to always be axis-aligned in worldspace
	// so we transform into that coordinate system to guarantee the calculations occur with axis-aligned section tiles.
	for (int32 VID : EditMesh.VertexIndicesItr())
	{
		FVertexInfo VertexInfo = EditMesh.GetVertexInfo(VID);
		VertexInfo.Position = GetComponentTransform().TransformPosition(VertexInfo.Position);
		EditMesh.SetVertex(VID, VertexInfo.Position);
	}


	// Slice the mesh along the grid defined by the Water Mesh
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeWaterBodyMeshSections::PartitionWaterBodyMesh);

		// We start at an index of -1 and go to +1 here to ensure the bordering sections don't contain triangles that could extend outside the bounds of the cell.
		for (int32 SectionIndexX = -1; SectionIndexX < WaterZoneExtentInLODSections.X + 1; ++SectionIndexX)
		{
			const FVector3d SlicePos = FVector3d(MeshOrigin + SectionIndexX * FVector2d(WaterLODSectionSize, 0.0), 0.0);
			const FVector3d SliceNormal(-1.0, 0.0, 0.0);

			FMeshPlaneCut Cut(&EditMesh, SlicePos, SliceNormal);
			if (!ensure(Cut.CutWithoutDelete(false)))
			{
				continue;
			}
		}

		for (int32 SectionIndexY = -1; SectionIndexY < WaterZoneExtentInLODSections.Y + 1; ++SectionIndexY)
		{
			const FVector3d SlicePos = FVector3d(MeshOrigin + SectionIndexY * FVector2d(0.0, WaterLODSectionSize), 0.0);
			const FVector3d SliceNormal(0.0, -1.0, 0.0);

			FMeshPlaneCut Cut(&EditMesh, SlicePos, SliceNormal);
			if (!ensure(Cut.CutWithoutDelete(false)))
			{
				continue;
			}
		}
	}

	FDynamicMeshAABBTree3 AABBTree(&EditMesh, true);

	const uint32 NumberOfSections = WaterZoneExtentInLODSections.X * WaterZoneExtentInLODSections.Y;
	TArray<FWaterBodyMeshSection> Sections;
	Sections.SetNum(NumberOfSections);

	// Need to access this before the ParallelFor since Foreground Worker threads don't count as gamethread.
	const float MaxFlowVelocity = FWaterUtils::GetWaterMaxFlowVelocity(false);
	ParallelFor(NumberOfSections, [&](int32 SectionIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeWaterBodyMeshSections::BuildMeshSection);
		const int32 SectionIndexX = SectionIndex % WaterZoneExtentInLODSections.X;
		const int32 SectionIndexY = SectionIndex / WaterZoneExtentInLODSections.X;

		const FVector2d SectionPos = MeshOrigin + FVector2d(SectionIndexX, SectionIndexY) * WaterLODSectionSize;

		const FAxisAlignedBox2d GridRectXY(SectionPos, SectionPos + WaterLODSectionSize);

		FDynamicMeshAABBTree3::FTreeTraversal Traversal;
		
		int SelectAllDepth = TNumericLimits<int>::Max();
		int CurrentDepth = -1;
		
		TArray<int> Triangles;

		Traversal.NextBoxF =
			[&GridRectXY, &SelectAllDepth, &CurrentDepth](const FAxisAlignedBox3d& Box, int Depth)
		{
			CurrentDepth = Depth;
			if (Depth > SelectAllDepth)
			{
				// We are deeper than the depth whose AABB was first detected to be contained in the RectangleXY,
				// descend and collect all leaf triangles
				return true;
			}
			
			SelectAllDepth = TNumericLimits<int>::Max();
			
			const FAxisAlignedBox2d BoxXY(FVector2d(Box.Min.X, Box.Min.Y), FVector2d(Box.Max.X, Box.Max.Y));
			if (GridRectXY.Intersects(BoxXY))
			{
				if (GridRectXY.Contains(BoxXY))
				{
					SelectAllDepth = Depth;
				}
				
				return true;		
			}
			return false;
		};
		

		Traversal.NextTriangleF =
			[&GridRectXY, &SelectAllDepth, &CurrentDepth, &Triangles, &EditMesh]
			(int TriangleID)
		{
			if (CurrentDepth >= SelectAllDepth)
			{
				// This TriangleID is entirely contained in the selection rectangle so we can skip intersection testing
				Triangles.Add(TriangleID);
				return;
			}

			FAxisAlignedBox2d ExpandedGridRect(GridRectXY);
			// Expand the rect by 1 unit to add an floating point error margin. 
			ExpandedGridRect.Expand(1);
		
			// this triangle is either on the edge or intersecting with the edge but since we did the plane cuts we know it can't be crossing over into the next cell
			FIndex3i Triangle = EditMesh.GetTriangle(TriangleID);
			FVector3d VertexA;
			FVector3d VertexB;
			FVector3d VertexC;
			EditMesh.GetTriVertices(TriangleID, VertexA, VertexB, VertexC);
			if ((ExpandedGridRect.Contains(FVector2d(VertexA))) &&
				(ExpandedGridRect.Contains(FVector2d(VertexB))) &&
				(ExpandedGridRect.Contains(FVector2d(VertexC))))
			{
				Triangles.Add(TriangleID);
			}
		};
			

		AABBTree.DoTraversal(Traversal);

		if (Triangles.Num() == 0)
		{
			return;
		}

		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;

		bool bHasBoundaryTriangle = false;
		if (GetWaterBodyType() != EWaterBodyType::River)
		{
			for (int32 TriangleID : Triangles)
			{
				if (EditMesh.IsBoundaryTriangle(TriangleID))
				{
					bHasBoundaryTriangle = true;
					break;
				}
			}
		}

		// Simplification can occur for lakes/rivers since they have flat geometry and no vertex color data.
		if (!bHasBoundaryTriangle && (GetWaterBodyType() != EWaterBodyType::River))
		{
			const float MeshZ = GetConstantSurfaceZ();
			uint32 TriIndices[4];

			auto AddQuadVertex = [&](FVector2D Position, int QuadIndex)
			{
				// Transform back into local space when rebuilding the mesh.
				const FVector3f LocalSpacePosition = FVector3f(GetComponentTransform().InverseTransformPosition(FVector(Position, MeshZ)));
				FDynamicMeshVertex MeshVertex(LocalSpacePosition);
				MeshVertex.Color = FColor(0);
				TriIndices[QuadIndex] = Vertices.Add(MeshVertex);
			};

			AddQuadVertex(GridRectXY.Min, 0);
			AddQuadVertex(GridRectXY.Max, 1);
			AddQuadVertex(FVector2D(GridRectXY.Min.X, GridRectXY.Max.Y), 2);
			AddQuadVertex(FVector2D(GridRectXY.Max.X, GridRectXY.Min.Y), 3);

			Indices.Append({ TriIndices[0], TriIndices[2], TriIndices[1], TriIndices[0], TriIndices[1], TriIndices[3] });
		}
		else
		{
			TMap<uint32, uint32> VertexMap;
			for (int32 TriangleID : Triangles)
			{
				FIndex3i Triangle = EditMesh.GetTriangle(TriangleID);
				
				TArray<uint32, TInlineAllocator<3>> TriIndices;
				for (int32 TriIndex = 0; TriIndex < 3; ++TriIndex)
				{
					if (uint32* VertexIndexPtr = VertexMap.Find(Triangle[TriIndex]))
					{
						TriIndices.Add(*VertexIndexPtr);
					}
					else
					{
						FVertexInfo VertexInfo;
						const bool bWantColors = true;
						const bool bWantNormals = false;
						const bool bWantUVs = false;
						EditMesh.GetVertex(Triangle[TriIndex], VertexInfo, bWantNormals, bWantColors, bWantUVs);

						// Transform back into local space when rebuilding the mesh.
						const FVector3f LocalSpacePosition = FVector3f(GetComponentTransform().InverseTransformPosition(VertexInfo.Position));
						FDynamicMeshVertex MeshVertex(LocalSpacePosition);
						MeshVertex.Color = PackFlowData(VertexInfo.Color.X, VertexInfo.Color.Y, MaxFlowVelocity);

						uint32 NewIndex = Vertices.Add(MeshVertex);
						VertexMap.Add(Triangle[TriIndex], NewIndex);
						TriIndices.Add(NewIndex);
					}
				}

				Indices.Append(TriIndices);
			}
		}

		if (Vertices.Num() != 0)
		{
			Sections[SectionIndex].Vertices = MoveTemp(Vertices);
			Sections[SectionIndex].Indices = MoveTemp(Indices);
			Sections[SectionIndex].SectionBounds = FBox2D(FVector2D(SectionPos), FVector2D(SectionPos + WaterLODSectionSize));
		}
	});

	for (FWaterBodyMeshSection& Section : Sections)
	{
		if (Section.IsValid())
		{
			WaterBodyMeshSections.Emplace(MoveTemp(Section));
		}
	}
}


void UWaterBodyComponent::UpdateWaterBodyRenderData()
{
	const bool bAffectsWaterInfo = AffectsWaterInfo();
	const bool bAffectsWaterMesh = AffectsWaterMesh();

	if (bAffectsWaterInfo)
	{
		RebuildWaterBodyInfoMesh();

		if (AWaterZone* WaterZone = GetWaterZone())
		{
			WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
		}
	}

	if (bAffectsWaterMesh)
	{
		checkf(bAffectsWaterInfo, TEXT("Invalid water body! WaterBody affects water mesh but doesn't affect water info"));

		UpdateNonTessellatedMeshSections();
	}

	if (bAffectsWaterInfo || bAffectsWaterMesh)
	{
		MarkRenderStateDirty();
	}
}

void UWaterBodyComponent::UpdateNonTessellatedMeshSections()
{
	if (AWaterZone* WaterZone = GetWaterZone(); WaterZone && WaterZone->IsNonTessellatedLODMeshEnabled())
	{
		RebuildWaterBodyLODSections();
	}
	else
	{
		// Clear the mesh sections if LOD is disabled
		WaterBodyMeshSections.Empty();
	}

	MarkRenderStateDirty();
}

#if WITH_EDITOR

void UWaterBodyComponent::CreateWaterSpriteComponent()
{
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, GetWaterSpriteTextureName());

	IWaterModuleInterface& WaterModule = FModuleManager::GetModuleChecked<IWaterModuleInterface>(TEXT("Water"));
	if (IWaterEditorServices* WaterEditorServices = WaterModule.GetWaterEditorServices())
	{
		WaterEditorServices->RegisterWaterActorSprite(GetClass(), Texture);
	}

	bVisualizeComponent = true;
	CreateSpriteComponent(Texture);

	// CreateSpriteComponent will not create a component if we are in a game world such as PIE.
	if (SpriteComponent != nullptr)
	{
		SpriteComponent->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));
		SpriteComponent->SetRelativeLocation(FVector(0.f, 0.f,  GetDefault<UWaterRuntimeSettings>()->WaterBodyIconWorldZOffset));
	}
}

void UWaterBodyComponent::UpdateWaterSpriteComponent()
{
	if (SpriteComponent)
	{
		SpriteComponent->SetVisibility(IsIconVisible());

		UTexture2D* IconTexture = SpriteComponent->Sprite;
		IWaterModuleInterface& WaterModule = FModuleManager::GetModuleChecked<IWaterModuleInterface>("Water");
		if (const IWaterEditorServices* WaterEditorServices = WaterModule.GetWaterEditorServices())
		{
			bool bHasError = false;
			TArray<TSharedRef<FTokenizedMessage>> StatusMessages = CheckWaterBodyStatus();
			for (const TSharedRef<FTokenizedMessage>& StatusMessage : StatusMessages)
			{
				// Message severities are ordered from most severe to least severe.
				if (StatusMessage->GetSeverity() <= EMessageSeverity::Error)
				{
					bHasError = true;
					break;
				}
			}

			if (bHasError)
			{
				IconTexture = WaterEditorServices->GetErrorSprite();
			}
			else
			{
				IconTexture = WaterEditorServices->GetWaterActorSprite(GetClass());
			}
		}


		const FVector ZOffset(0.0f, 0.0f, GetDefault<UWaterRuntimeSettings>()->WaterBodyIconWorldZOffset);
		SpriteComponent->SetWorldLocation(GetWaterSpriteLocation() + ZOffset);
		SpriteComponent->Sprite = IconTexture;

		SpriteComponent->MarkRenderStateDirty();
	}
}

TSubclassOf<UHLODBuilder> UWaterBodyComponent::GetCustomHLODBuilderClass() const
{
	return UWaterBodyHLODBuilder::StaticClass();
}

FMeshDescription UWaterBodyComponent::GetHLODMeshDescription() const
{
	if (WaterMeshOverride)
	{
		return *WaterMeshOverride->GetMeshDescription(WaterMeshOverride->GetNumLODs() - 1);
	}
	
	FMeshDescription MeshDescription;

	FStaticMeshAttributes StaticMeshAttributes(MeshDescription);
	StaticMeshAttributes.Register();

	TVertexAttributesRef<FVector3f> VertexPositions = StaticMeshAttributes.GetVertexPositions();

	const int32 NumVertices = WaterBodyMeshVertices.Num();
	const int32 NumTriangles = WaterBodyMeshIndices.Num() / 3;

	MeshDescription.ReserveNewVertices(NumVertices);
	MeshDescription.ReserveNewVertexInstances(NumVertices);
	MeshDescription.ReserveNewTriangles(NumTriangles);

	FPolygonGroupID PolygonGroupID = MeshDescription.CreatePolygonGroup();

	// Positions
	for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		FVertexID VertexID = MeshDescription.CreateVertex();
		VertexPositions[VertexID] = WaterBodyMeshVertices[VertexIndex].Position;
	}

	// Triangles
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		TStaticArray<FVertexInstanceID, 3> VertexInstanceIDs;

		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			uint32 VertexIndex = WaterBodyMeshIndices[TriangleIndex * 3 + Corner];

			FVertexID VertexID(VertexIndex);
			FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexID);
			
			VertexInstanceIDs[Corner] = VertexInstanceID;
		}

		// Create a triangle
		MeshDescription.CreateTriangle(PolygonGroupID, VertexInstanceIDs);
	}
	
	return MeshDescription;
}

UMaterialInterface* UWaterBodyComponent::GetHLODMaterial() const
{
	return WaterHLODMaterial;
}

void UWaterBodyComponent::SetHLODMaterial(UMaterialInterface* InMaterial)
{
	WaterHLODMaterial = InMaterial;
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE


