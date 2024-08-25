// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterMeshComponent.h"
#include "EngineUtils.h"
#include "MaterialDomain.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "WaterBodyComponent.h"
#include "WaterMeshSceneProxy.h"
#include "WaterModule.h"
#include "WaterSplineComponent.h"
#include "WaterSubsystem.h"
#include "WaterUtils.h"
#include "WaterBodyInfoMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterMeshComponent)

/** Scalability CVars*/
static TAutoConsoleVariable<int32> CVarWaterMeshLODCountBias(
	TEXT("r.Water.WaterMesh.LODCountBias"), 0,
	TEXT("This value is added to the LOD Count of each Water Mesh Component. Negative values will lower the quality(fewer and larger water tiles at the bottom level of the water quadtree), higher values will increase quality (more and smaller water tiles at the bottom level of the water quadtree)"),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarWaterMeshTessFactorBias(
	TEXT("r.Water.WaterMesh.TessFactorBias"), 0,
	TEXT("This value is added to the tessellation factor of each Mesh Component. Negative values will lower the overall density/resolution or the vertex grid, higher values will increase the density/resolution "),
	ECVF_Scalability);

static TAutoConsoleVariable<float> CVarWaterMeshLODScaleBias(
	TEXT("r.Water.WaterMesh.LODScaleBias"), 0.0f,
	TEXT("This value is added to the LOD Scale of each Mesh Component. Negative values will lower the overall density/resolution or the vertex grid and make the LODs smaller, higher values will increase the density/resolution and make the LODs larger. Smallest value is -0.5. That will make the inner LOD as tight and optimized as possible"),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarWaterMeshGPUQuadTree(
	TEXT("r.Water.WaterMesh.GPUQuadTree"),
	0,
	TEXT("Builds the water quadtree on the GPU and does indirect draws of water tiles, driven by the GPU."),
	ECVF_RenderThreadSafe
);

/** Debug CVars */ 
static TAutoConsoleVariable<int32> CVarWaterMeshShowTileGenerationGeometry(
	TEXT("r.Water.WaterMesh.ShowTileGenerationGeometry"),
	0,
	TEXT("This debug option will display the geometry used for intersecting the water grid and generating tiles"),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarWaterMeshForceRebuildMeshPerFrame(
	TEXT("r.Water.WaterMesh.ForceRebuildMeshPerFrame"),
	0,
	TEXT("Force rebuilding the entire mesh each frame"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarWaterMeshEnabled(
	TEXT("r.Water.WaterMesh.Enabled"),
	1,
	TEXT("If the water mesh is enabled or disabled. This affects both rendering and the water tile generation"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarWaterMeshMIDDeduplication(
	TEXT("r.Water.WaterMesh.MIDDeduplication"),
	0,
	TEXT("Deduplicate per-water body MIDs"),
	ECVF_RenderThreadSafe
);

extern TAutoConsoleVariable<float> CVarWaterSplineResampleMaxDistance;


// ----------------------------------------------------------------------------------

UWaterMeshComponent::UWaterMeshComponent()
{
	bAutoActivate = true;
	bHasPerInstanceHitProxies = true;

	SetMobility(EComponentMobility::Static);
}

void UWaterMeshComponent::PostLoad()
{
	Super::PostLoad();
}

void UWaterMeshComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	const FVertexFactoryType* WaterVertexFactoryNonIndirectType = GetWaterVertexFactoryType(/*bWithWaterSelectionSupport = */ false, EWaterVertexFactoryDrawMode::NonIndirect);
	const FVertexFactoryType* WaterVertexFactoryIndirectType = GetWaterVertexFactoryType(/*bWithWaterSelectionSupport = */ false, EWaterVertexFactoryDrawMode::Indirect);
	const FVertexFactoryType* WaterVertexFactoryIndirectISRType = GetWaterVertexFactoryType(/*bWithWaterSelectionSupport = */ false, EWaterVertexFactoryDrawMode::IndirectInstancedStereo);
	if (FarDistanceMaterial)
	{
		FMaterialInterfacePSOPrecacheParams& ComponentParams = OutParams[OutParams.AddDefaulted()];
		ComponentParams.Priority = EPSOPrecachePriority::High;
		ComponentParams.MaterialInterface = FarDistanceMaterial;
		ComponentParams.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(WaterVertexFactoryNonIndirectType));
		ComponentParams.PSOPrecacheParams = BasePrecachePSOParams;
	}
	for (UMaterialInterface* MaterialInterface : UsedMaterials)
	{
		if (MaterialInterface)
		{
			FMaterialInterfacePSOPrecacheParams& ComponentParams = OutParams[OutParams.AddDefaulted()];
			ComponentParams.Priority = EPSOPrecachePriority::High;
			ComponentParams.MaterialInterface = MaterialInterface;
			ComponentParams.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(WaterVertexFactoryNonIndirectType));
			ComponentParams.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(WaterVertexFactoryIndirectType));
			ComponentParams.VertexFactoryDataList.Add(FPSOPrecacheVertexFactoryData(WaterVertexFactoryIndirectISRType));
			ComponentParams.PSOPrecacheParams = BasePrecachePSOParams;
		}
	}
}

void UWaterMeshComponent::PostInitProperties()
{
	Super::PostInitProperties();

	UpdateBounds();
	MarkRenderTransformDirty();
}

FPrimitiveSceneProxy* UWaterMeshComponent::CreateSceneProxy()
{
	// Early out
	if (!bIsEnabled)
	{
		return nullptr;
	}

	return new FWaterMeshSceneProxy(this);
}

void UWaterMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	for (UMaterialInterface* Mat : UsedMaterials)
	{
		if (Mat)
		{
			OutMaterials.Add(Mat);
		}
	}
}

void UWaterMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	UE_LOG(LogWater, Warning, TEXT("SetMaterial is not compatible with UWaterMeshComponent since all materials on this component are auto-populated from the Water Bodies contained within it."));
}

#if WITH_EDITOR

bool UWaterMeshComponent::ShouldRenderSelected() const
{
	if (bSelectable)
	{
		bool bShouldRender = Super::ShouldRenderSelected();
		if (!bShouldRender)
		{
			if (AWaterZone* Owner = GetOwner<AWaterZone>())
			{
				Owner->ForEachWaterBodyComponent([&bShouldRender](UWaterBodyComponent* WaterBodyComponent)
				{
					check(WaterBodyComponent);
					bShouldRender |= WaterBodyComponent->ShouldRenderSelected();

					// Stop iterating over water body components by returning false as soon as one component says it should be "render selected" :
					return !bShouldRender;
				});
			}
		}

		return bShouldRender;
	}
	return false;
}

#endif // WITH_EDITOR

FMaterialRelevance UWaterMeshComponent::GetWaterMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Combine the material relevance for all materials.
	FMaterialRelevance Result;
	for (UMaterialInterface* Mat : UsedMaterials)
	{
		Result |= Mat->GetRelevance_Concurrent(InFeatureLevel);
	}

	return Result;
}

void UWaterMeshComponent::SetDynamicWaterMeshCenter(const FVector2D& NewCenter)
{
	if (!DynamicWaterMeshCenter.Equals(NewCenter))
	{
		DynamicWaterMeshCenter = NewCenter;
		MarkWaterMeshGridDirty();
	}
}

void UWaterMeshComponent::SetTileSize(float NewTileSize)
{
	TileSize = NewTileSize;
	MarkWaterMeshGridDirty();
	MarkRenderStateDirty();
}

FIntPoint UWaterMeshComponent::GetExtentInTiles() const
{
	if (const AWaterZone* WaterZone = GetOwner<AWaterZone>(); ensureMsgf(WaterZone != nullptr, TEXT("WaterMeshComponent is owned by an actor that is not a WaterZone. This is not supported!")))
	{
		const float MeshTileSize = TileSize;
		const FVector2D ZoneHalfExtent = FVector2D(WaterZone->GetDynamicWaterInfoExtent()) / 2.0;
		const int32 HalfExtentInTilesX = FMath::RoundUpToPowerOfTwo(ZoneHalfExtent.X / MeshTileSize);
		const int32 HalfExtentInTilesY = FMath::RoundUpToPowerOfTwo(ZoneHalfExtent.Y / MeshTileSize);
		const FIntPoint HalfExtentInTiles = FIntPoint(HalfExtentInTilesX, HalfExtentInTilesY);

		// QuadTreeResolution caches the resolution so it is clearly visible to the user in the details panel. It represents the full extent rather than the half extent
		QuadTreeResolution = HalfExtentInTiles * 2;

		return HalfExtentInTiles;
	}

	return FIntPoint(1, 1);
}

FBoxSphereBounds UWaterMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Always return valid bounds (tree is initialized with invalid bounds and if nothing is inserted, the tree bounds will stay invalid)
	FBox NewBounds = WaterQuadTree.GetBounds();

	if (NewBounds.Min.Z >= NewBounds.Max.Z)
	{
		NewBounds.Min.Z = 0.0f;
		NewBounds.Max.Z = 100.0f;
	}
	// Add the far distance to the bounds if it's valid
	if (FarDistanceMaterial)
	{
		NewBounds = NewBounds.ExpandBy(FVector(FarDistanceMeshExtent, FarDistanceMeshExtent, 0.0f));
	}
	return NewBounds;
}

static bool IsMaterialUsedWithWater(const UMaterialInterface* InMaterial)
{
	return (InMaterial && InMaterial->CheckMaterialUsage_Concurrent(EMaterialUsage::MATUSAGE_Water));
}

void UWaterMeshComponent::RebuildWaterMesh(float InTileSize, const FIntPoint& InExtentInTiles)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RebuildWaterMesh);

	AWaterZone* WaterZone = CastChecked<AWaterZone>(GetOwner());

	// Position snapped to the grid
	const FVector2D GridPosition = WaterZone->IsLocalOnlyTessellationEnabled() ? GetDynamicWaterMeshCenter() : FVector2D(FMath::GridSnap<FVector::FReal>(GetComponentLocation().X, InTileSize), FMath::GridSnap<FVector::FReal>(GetComponentLocation().Y, InTileSize));
	const FVector2D WorldExtent = FVector2D(InTileSize * InExtentInTiles.X, InTileSize * InExtentInTiles.Y);

	FBox2D WaterWorldBox = FBox2D(-WorldExtent + GridPosition, WorldExtent + GridPosition);
	
	// If the dynamic bounds is outside the full bounds of the water mesh, we shouldn't regenerate the quadtree
	if (!(WaterWorldBox.GetArea() > 0.f))
	{
		return;
	}

	const bool bIsGPUQuadTree = CVarWaterMeshGPUQuadTree.GetValueOnGameThread() != 0;

	// This resets the tree to an initial state, ready for node insertion
	WaterQuadTree.InitTree(WaterWorldBox, InTileSize, InExtentInTiles, bIsGPUQuadTree);

	UsedMaterials.Empty();

	// Will be updated with the ocean min bound, to be used to place the far mesh just under the ocean to avoid seams
	float FarMeshHeight = GetComponentLocation().Z;
	// Only use a far mesh when there is an ocean in the zone.
	bool bHasOcean = false;

	const UWaterSubsystem* WaterSubsystem = UWaterSubsystem::GetWaterSubsystem(GetWorld());

	const float GlobalOceanHeight = WaterSubsystem ? WaterSubsystem->GetOceanTotalHeight() : TNumericLimits<float>::Lowest();
	const float OceanFlood = WaterSubsystem ? WaterSubsystem->GetOceanFloodHeight() : 0.0f;
	const bool bIsFlooded = OceanFlood > 0.0f;

	// Go through all water body actors to figure out bounds and water tiles
	AWaterZone* OwningZone = GetOwner<AWaterZone>();
	check(OwningZone);

	// Min and max user defined priority range. (Input also clamped on OverlapMaterialPriority in AWaterBody)
	constexpr int32 MinWaterBodyPriority = -8192;
	constexpr int32 MaxWaterBodyPriority = 8191;
	constexpr int32 GPUQuadTreeMaxNumPriorities = 8;

	// The GPU quadtree only supports 8 different priority values, so we need to remap priorities into that space.
	// Fortunately, rivers and non-river water bodies are rendered into their own "priority space", so we don't need to worry about
	// moving river priorities into their own range.
	TArray<int16> SortedPriorities;
	if (bIsGPUQuadTree)
	{
		OwningZone->ForEachWaterBodyComponent([this, WaterWorldBox, &SortedPriorities](UWaterBodyComponent* WaterBodyComponent)
		{
			check(WaterBodyComponent);
			AActor* Actor = WaterBodyComponent->GetOwner();
			check(Actor);

			// Skip invisible water bodies
			if (!WaterBodyComponent->ShouldRender() || !WaterBodyComponent->ShouldGenerateWaterMeshTile())
			{
				return true;
			}

			// Don't process water bodies which have their spline outside of this water mesh
			const FBox WaterBodyBounds = WaterBodyComponent->Bounds.GetBox();
			if (!WaterBodyBounds.IntersectXY(FBox(FVector(WaterWorldBox.Min, 0.0f), FVector(WaterWorldBox.Max, 0.0f))))
			{
				return true;
			}

			const int16 Priority = static_cast<int16>(FMath::Clamp(WaterBodyComponent->GetOverlapMaterialPriority(), MinWaterBodyPriority, MaxWaterBodyPriority));
			SortedPriorities.AddUnique(Priority);

			return true;
		});

		SortedPriorities.Sort();

		if (SortedPriorities.Num() > GPUQuadTreeMaxNumPriorities)
		{
			UE_LOG(LogWater, Warning, TEXT("WaterZone has more unique water body priorities (%i) than can be supported with GPU driven water quadtree rendering (%i)!"), SortedPriorities.Num(), GPUQuadTreeMaxNumPriorities);
		}
	}

	// Lambda for setting parameters on shared/deduplicated MIDs
	auto SetDynamicParametersOnSharedMID = [&](UMaterialInstanceDynamic* InMID)
	{
		if ((InMID == nullptr) || (WaterSubsystem == nullptr))
		{
			return false;
		}

		InMID->SetScalarParameterValue(UWaterBodyComponent::WaterBodyIndexParamName, -1);
		InMID->SetScalarParameterValue(UWaterBodyComponent::GlobalOceanHeightName, WaterSubsystem->GetOceanTotalHeight());
		InMID->SetScalarParameterValue(UWaterBodyComponent::WaterZoneIndexParamName, OwningZone->GetWaterZoneIndex());
		InMID->SetTextureParameterValue(UWaterBodyComponent::WaterVelocityAndHeightName, OwningZone->WaterInfoTexture);
		return true;
	};
	

	bool bAnyWaterMeshesNotReady = false;

	OwningZone->ForEachWaterBodyComponent([this, bIsGPUQuadTree, WaterWorldBox, bIsFlooded, GlobalOceanHeight, OceanFlood, &FarMeshHeight, &bHasOcean, &SortedPriorities, &bAnyWaterMeshesNotReady, &SetDynamicParametersOnSharedMID](UWaterBodyComponent* WaterBodyComponent)
	{
		check(WaterBodyComponent);
		AActor* Actor = WaterBodyComponent->GetOwner();
		check(Actor);

		// Skip invisible water bodies
		if (!WaterBodyComponent->ShouldRender())
		{
			return true;
		}

		// No need to generate anything in the case of a custom water
		if (!WaterBodyComponent->ShouldGenerateWaterMeshTile())
		{
			return true;
		}

		// Don't process water bodies that has their spline outside of this water mesh
		const FBox WaterBodyBounds = WaterBodyComponent->Bounds.GetBox();
		if (!WaterBodyBounds.IntersectXY(FBox(FVector(WaterWorldBox.Min, 0.0f), FVector(WaterWorldBox.Max, 0.0f))))
		{
			return true;
		}

		FWaterBodyRenderData RenderData;

		const EWaterBodyType WaterBodyType = WaterBodyComponent->GetWaterBodyType();

		if (WaterBodyType != EWaterBodyType::Ocean)
		{
			if (bIsFlooded)
			{
				// If water body is below ocean height and not set to snap to the ocean height, skip it
				const float CompareHeight = (WaterBodyType == EWaterBodyType::River) ? Actor->GetComponentsBoundingBox().Max.Z : WaterBodyComponent->GetComponentLocation().Z;
				if (CompareHeight <= GlobalOceanHeight)
				{
					return true;
				}
			}
		}

		const bool bDeduplicateMIDs = CVarWaterMeshMIDDeduplication.GetValueOnGameThread() != 0;

		if (bDeduplicateMIDs)
		{
			// Assign material instance(s)
			UMaterialInterface* WaterMaterial = WaterBodyComponent->GetWaterMaterial();
			if (WaterMaterial)
			{
				const bool bIsRiver = WaterBodyType == EWaterBodyType::River;

				UMaterialInterface* Materials[3] = {};
				Materials[0] = WaterBodyComponent->GetWaterMaterial();
				Materials[1] = bIsRiver ? WaterBodyComponent->GetRiverToLakeTransitionMaterial() : nullptr;
				Materials[2] = bIsRiver ? WaterBodyComponent->GetRiverToOceanTransitionMaterial() : nullptr;

				FName Names[] = { TEXT("WaterMID"), TEXT("LakeTransitionMID"), TEXT("OceanTransitionMID") };

				UMaterialInterface* OutMaterials[3] = {};
				for (int i = 0; i < 3; ++i)
				{
					UMaterialInterface* Material = Materials[i];
					if (Material)
					{
						if (IsMaterialUsedWithWater(Material))
						{
							const bool bHasExistingMID = MaterialToMID.Contains(Material);
							UMaterialInstanceDynamic* MID = nullptr;
							if (!bHasExistingMID)
							{
								MID = FWaterUtils::GetOrCreateTransientMID(nullptr, Names[i], Material, RF_Transient | RF_NonPIEDuplicateTransient | RF_TextExportTransient);
								SetDynamicParametersOnSharedMID(MID);
								MaterialToMID.Add(Material, MID);
							}
							else
							{
								MID = MaterialToMID[Material];
								// Update the parameters on this MID if this is the first time this material is seen in this rebuild.
								// This is necessary to handle cases where GlobalOceanHeight or the water info texture pointer change
								// TODO: GlobalOceanHeight can probably be removed, leaving only the texture parameter.
								// Can we then skip this and set the parameters only on creation?
								if (!UsedMaterials.Contains(MID))
								{
									SetDynamicParametersOnSharedMID(MID);
								}
							}
							OutMaterials[i] = MID;
						}
						else
						{
							OutMaterials[i] = UMaterial::GetDefaultMaterial(MD_Surface);
						}
						UsedMaterials.Add(OutMaterials[i]);
					}
				}

				RenderData.Material = OutMaterials[0];
				RenderData.RiverToLakeMaterial = OutMaterials[1];
				RenderData.RiverToOceanMaterial = OutMaterials[2];
			}

			RenderData.Priority = static_cast<int16>(FMath::Clamp(WaterBodyComponent->GetOverlapMaterialPriority(), MinWaterBodyPriority, MaxWaterBodyPriority));
			RenderData.WaterBodyIndex = static_cast<int16>(WaterBodyComponent->GetWaterBodyIndex());
			RenderData.SurfaceBaseHeight = WaterBodyComponent->GetComponentLocation().Z;
			RenderData.MaxWaveHeight = WaterBodyComponent->GetMaxWaveHeight();
			RenderData.BoundsMinZ = WaterBodyComponent->Bounds.GetBox().Min.Z;
			RenderData.BoundsMaxZ = WaterBodyComponent->Bounds.GetBox().Max.Z;
			RenderData.WaterBodyType = static_cast<int8>(WaterBodyType);
#if WITH_WATER_SELECTION_SUPPORT
			RenderData.HitProxy = new HActor(/*InActor = */Actor, /*InPrimComponent = */nullptr);
			RenderData.bWaterBodySelected = Actor->IsSelected();
#endif // WITH_WATER_SELECTION_SUPPORT

			if (WaterBodyType == EWaterBodyType::Ocean && bIsFlooded)
			{
				RenderData.SurfaceBaseHeight += OceanFlood;
				RenderData.Priority -= 1;
			}
		}
		else
		{
			// Assign material instance
			UMaterialInstanceDynamic* WaterMaterial = WaterBodyComponent->GetWaterMaterialInstance();
			RenderData.Material = WaterMaterial;

			if (RenderData.Material)
			{
				if (!IsMaterialUsedWithWater(RenderData.Material))
				{
					RenderData.Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}
				else
				{
					// Add ocean height as a scalar parameter
					WaterBodyComponent->SetDynamicParametersOnMID(WaterMaterial);
				}

				// Add material so that the component keeps track of all potential materials used
				UsedMaterials.Add(RenderData.Material);
			}

			RenderData.Priority = static_cast<int16>(FMath::Clamp(WaterBodyComponent->GetOverlapMaterialPriority(), MinWaterBodyPriority, MaxWaterBodyPriority));
			RenderData.WaterBodyIndex = static_cast<int16>(WaterBodyComponent->GetWaterBodyIndex());
			RenderData.SurfaceBaseHeight = WaterBodyComponent->GetComponentLocation().Z;
			RenderData.MaxWaveHeight = WaterBodyComponent->GetMaxWaveHeight();
			RenderData.BoundsMinZ = WaterBodyComponent->Bounds.GetBox().Min.Z;
			RenderData.BoundsMaxZ = WaterBodyComponent->Bounds.GetBox().Max.Z;
			RenderData.WaterBodyType = static_cast<int8>(WaterBodyType);
#if WITH_WATER_SELECTION_SUPPORT
			RenderData.HitProxy = new HActor(/*InActor = */Actor, /*InPrimComponent = */nullptr);
			RenderData.bWaterBodySelected = Actor->IsSelected();
#endif // WITH_WATER_SELECTION_SUPPORT

			if (WaterBodyType == EWaterBodyType::Ocean && bIsFlooded)
			{
				RenderData.SurfaceBaseHeight += OceanFlood;
				RenderData.Priority -= 1;
			}

			// For rivers, set up transition materials if they exist
			if (WaterBodyType == EWaterBodyType::River)
			{
				UMaterialInstanceDynamic* RiverToLakeMaterial = WaterBodyComponent->GetRiverToLakeTransitionMaterialInstance();
				if (IsMaterialUsedWithWater(RiverToLakeMaterial))
				{
					RenderData.RiverToLakeMaterial = RiverToLakeMaterial;
					UsedMaterials.Add(RenderData.RiverToLakeMaterial);
					// Add ocean height as a scalar parameter
					WaterBodyComponent->SetDynamicParametersOnMID(RiverToLakeMaterial);
				}

				UMaterialInstanceDynamic* RiverToOceanMaterial = WaterBodyComponent->GetRiverToOceanTransitionMaterialInstance();
				if (IsMaterialUsedWithWater(RiverToOceanMaterial))
				{
					RenderData.RiverToOceanMaterial = RiverToOceanMaterial;
					UsedMaterials.Add(RenderData.RiverToOceanMaterial);
					// Add ocean height as a scalar parameter
					WaterBodyComponent->SetDynamicParametersOnMID(RiverToOceanMaterial);
				}
			}
		}

		if (RenderData.RiverToLakeMaterial || RenderData.RiverToOceanMaterial)
		{
			// Move rivers up to it's own priority space, so that they always have precedence if they have transitions and that they only compare agains other rivers with transitions
			RenderData.Priority += (MaxWaterBodyPriority-MinWaterBodyPriority)+1;
		}

		uint32 WaterBodyRenderDataIndex = WaterQuadTree.AddWaterBodyRenderData(RenderData);

		if (bIsGPUQuadTree)
		{
			// On the GPU path, we only submit FWaterBodyQuadTreeRasterInfo for the water quadtree to be rasterized on the GPU. In particular, we need the static mesh, a transform and a priority/WaterBodyRenderData index.

			// TODO: Add support for flooded ocean water heights. This is a legacy feature and currently not used in any project (?), so it's probably not urgent.
			// In order to implement this, we would need to render a fullscreen quad/triangle for the ocean instead of using the spline derived mesh.

			UWaterBodyInfoMeshComponent* WaterBodyInfoMeshComponent = WaterBodyComponent->GetWaterInfoMeshComponent();
			if (ensure(WaterBodyInfoMeshComponent))
			{
				UStaticMesh* StaticMesh = WaterBodyInfoMeshComponent->GetStaticMesh();
				if (ensure(StaticMesh))
				{
					bAnyWaterMeshesNotReady |= StaticMesh->IsCompiling();
					FStaticMeshRenderData* StaticMeshRenderData = StaticMesh->GetRenderData();
					if (StaticMeshRenderData)
					{
						const int16 ClampedPriority = static_cast<int16>(FMath::Clamp(WaterBodyComponent->GetOverlapMaterialPriority(), MinWaterBodyPriority, MaxWaterBodyPriority));

						FWaterBodyQuadTreeRasterInfo RasterInfo;
						RasterInfo.LocalToWorld = WaterBodyComponent->GetComponentTransform();
						RasterInfo.RenderData = StaticMeshRenderData;
						RasterInfo.WaterBodyRenderDataIndex = WaterBodyRenderDataIndex;
						RasterInfo.Priority = FMath::Clamp(SortedPriorities.IndexOfByKey(ClampedPriority), 0, GPUQuadTreeMaxNumPriorities - 1);
						RasterInfo.bIsRiver = WaterBodyType == EWaterBodyType::River;

						WaterQuadTree.AddWaterBodyRasterInfo(RasterInfo);
					}
				}
			}

			if (WaterBodyType == EWaterBodyType::Ocean)
			{
				// Place far mesh height just below the ocean level
				FarMeshHeight = RenderData.SurfaceBaseHeight - RenderData.MaxWaveHeight;
				bHasOcean = true;
			}
		}
		else
		{
			switch (WaterBodyType)
			{
			case EWaterBodyType::River:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(River);

				TArray<FBox, TInlineAllocator<16>> Boxes;
				TArray<UPrimitiveComponent*> CollisionComponents = WaterBodyComponent->GetCollisionComponents();
				for (UPrimitiveComponent* Comp : CollisionComponents)
				{
					if (UBodySetup* BodySetup = (Comp != nullptr) ? Comp->GetBodySetup() : nullptr)
					{
						// Go through all sub shapes on the bodysetup to get a tight fit along water body
						for (const FKConvexElem& ConvElem : BodySetup->AggGeom.ConvexElems)
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(Add);

							FBox SubBox = ConvElem.ElemBox.TransformBy(Comp->GetComponentTransform().ToMatrixWithScale());
							SubBox.Max.Z += WaterBodyComponent->GetMaxWaveHeight();

							Boxes.Add(SubBox);
						}
					}
					else
					{
						// fallback on global AABB: 
						FVector Center;
						FVector Extent;
						Actor->GetActorBounds(false, Center, Extent);
						FBox Box(FBox::BuildAABB(Center, Extent));
						Box.Max.Z += WaterBodyComponent->GetMaxWaveHeight();
						Boxes.Add(Box);
					}
				}

				for (const FBox& Box : Boxes)
				{
					WaterQuadTree.AddWaterTilesInsideBounds(Box, WaterBodyRenderDataIndex);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (!!CVarWaterMeshShowTileGenerationGeometry.GetValueOnGameThread())
					{
						DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Red);
					}
#endif
				}
				break;
			}
			case EWaterBodyType::Lake:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Lake);

				const UWaterSplineComponent* SplineComp = WaterBodyComponent->GetWaterSpline();
				const int32 NumOriginalSplinePoints = SplineComp->GetNumberOfSplinePoints();
				float ConstantZ = SplineComp->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World).Z;

				FBox LakeBounds = Actor->GetComponentsBoundingBox(/* bNonColliding = */true);
				LakeBounds.Max.Z += WaterBodyComponent->GetMaxWaveHeight();

				// Skip lakes with less than 3 spline points
				if (NumOriginalSplinePoints < 3)
				{
					break;
				}

				TArray<TArray<FVector2D>> PolygonBatches;
				// Reuse the convex hulls generated for the physics shape because the work has already been done, but we can fallback to a simple spline evaluation method in case there's no physics:
				bool bUseFallbackMethod = true;

				TArray<UPrimitiveComponent*> CollisionComponents = WaterBodyComponent->GetCollisionComponents();
				if (CollisionComponents.Num() > 0)
				{
					UPrimitiveComponent* Comp = CollisionComponents[0];
					if (UBodySetup* BodySetup = (Comp != nullptr) ? Comp->GetBodySetup() : nullptr)
					{
						FTransform CompTransform = Comp->GetComponentTransform();
						// Go through all sub shapes on the bodysetup to get a tight fit along water body
						for (const FKConvexElem& ConvElem : BodySetup->AggGeom.ConvexElems)
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(Add);

							// Vertex data contains the bottom vertices first, then the top ones :
							int32 TotalNumVertices = ConvElem.VertexData.Num();
							check(TotalNumVertices % 2 == 0);
							int32 NumVertices = TotalNumVertices / 2;
							if (NumVertices > 0)
							{
								bUseFallbackMethod = false;

								// Because the physics shape is made of multiple convex hulls, we cannot simply add their vertices to one big list but have to have 1 batch of polygons per convex hull
								TArray<FVector2D>& Polygon = PolygonBatches.Emplace_GetRef();
								Polygon.SetNum(NumVertices);
								for (int32 i = 0; i < NumVertices; ++i)
								{
									// Gather the top vertices :
									Polygon[i] = FVector2D(CompTransform.TransformPosition(ConvElem.VertexData[NumVertices + i]));
								}
							}
						}
					}
				}

				if (bUseFallbackMethod)
				{
					TArray<FVector> PolyLineVertices;
					SplineComp->ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(CVarWaterSplineResampleMaxDistance.GetValueOnGameThread()), PolyLineVertices);

					TArray<FVector2D>& Polygon = PolygonBatches.Emplace_GetRef();
					Polygon.Reserve(PolyLineVertices.Num());
					Algo::Transform(PolyLineVertices, Polygon, [](const FVector& Vertex) { return FVector2D(Vertex); });
				}

				for (const TArray<FVector2D>& Polygon : PolygonBatches)
				{
					WaterQuadTree.AddLake(Polygon, LakeBounds, WaterBodyRenderDataIndex);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (!!CVarWaterMeshShowTileGenerationGeometry.GetValueOnGameThread())
					{
						float Z = SplineComp->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World).Z;
						int32 NumVertices = Polygon.Num();
						for (int32 i = 0; i < NumVertices; i++)
						{
							const FVector2D& Point0 = Polygon[i];
							const FVector2D& Point1 = Polygon[(i + 1) % NumVertices];
							DrawDebugLine(GetWorld(), FVector(Point0.X, Point0.Y, Z), FVector(Point1.X, Point1.Y, Z), FColor::Green);
						}
					}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				}

				break;
			}
			case EWaterBodyType::Ocean:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Ocean);

				// Add ocean based on the ocean spline when there is no flood. Otherwise add ocean everywhere
				if (bIsFlooded)
				{
					FBox OceanBounds = Actor->GetComponentsBoundingBox();
					OceanBounds.Max.Z += WaterBodyComponent->GetMaxWaveHeight() + OceanFlood;
					WaterQuadTree.AddWaterTilesInsideBounds(OceanBounds, WaterBodyRenderDataIndex);
				}
				else
				{
					const UWaterSplineComponent* SplineComp = WaterBodyComponent->GetWaterSpline();
					const int32 NumOriginalSplinePoints = SplineComp->GetNumberOfSplinePoints();

					// Skip oceans with less than 3 spline points
					if (NumOriginalSplinePoints < 3)
					{
						break;
					}

					TArray<FVector2D> Polygon;

					TArray<FVector> PolyLineVertices;
					SplineComp->ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(CVarWaterSplineResampleMaxDistance.GetValueOnGameThread()), PolyLineVertices);

					Polygon.Reserve(PolyLineVertices.Num());
					Algo::Transform(PolyLineVertices, Polygon, [](const FVector& Vertex) { return FVector2D(Vertex); });

					FBox OceanBounds = WaterBodyComponent->Bounds.GetBox();
					OceanBounds.Max.Z += WaterBodyComponent->GetMaxWaveHeight();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (!!CVarWaterMeshShowTileGenerationGeometry.GetValueOnGameThread())
					{
						float Z = SplineComp->GetLocationAtDistanceAlongSpline(0.0f, ESplineCoordinateSpace::World).Z;
						int32 NumVertices = Polygon.Num();
						for (int32 i = 0; i < NumVertices; i++)
						{
							const FVector2D& Point0 = Polygon[i];
							const FVector2D& Point1 = Polygon[(i + 1) % NumVertices];
							DrawDebugLine(GetWorld(), FVector(Point0.X, Point0.Y, Z), FVector(Point1.X, Point1.Y, Z), FColor::Blue);
						}

						DrawDebugBox(GetWorld(), OceanBounds.GetCenter(), OceanBounds.GetExtent(), FColor::Blue);
					}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

					WaterQuadTree.AddOcean(Polygon, OceanBounds, WaterBodyRenderDataIndex);
				}

				// Place far mesh height just below the ocean level
				FarMeshHeight = RenderData.SurfaceBaseHeight - WaterBodyComponent->GetMaxWaveHeight();
				bHasOcean = true;

				break;
			}
			case EWaterBodyType::Transition:
				// Transitions dont require rendering
				break;
			default:
				ensureMsgf(false, TEXT("This water body type is not implemented and will not produce any water tiles. "));
			}
		}

		return true;
	});

	// Build the far distance mesh instances if needed
	if ((bHasOcean || bUseFarMeshWithoutOcean) && (IsMaterialUsedWithWater(FarDistanceMaterial) && FarDistanceMeshExtent > 0.0f))
	{
		UsedMaterials.Add(FarDistanceMaterial);

		// Far Mesh should stitch to the edge of the water zone
		const FBox2D FarMeshBounds = WaterZone->GetZoneBounds2D();

		WaterQuadTree.AddFarMesh(FarDistanceMaterial, FarMeshBounds, FarDistanceMeshExtent, FarMeshHeight);
	}

	// Remove all materials from the MaterialToMID map that aren't currently used
	MaterialToMID = MaterialToMID.FilterByPredicate([this](const auto& Pair) { return UsedMaterials.Contains(Pair.Value); });

	WaterQuadTree.Unlock(true);

	MarkRenderStateDirty();

	// Force another rebuild next frame if water body meshes are still compiling
	if (bIsGPUQuadTree && bAnyWaterMeshesNotReady)
	{
		bNeedsRebuild = true;
	}
}

void UWaterMeshComponent::Update()
{
	bIsEnabled = FWaterUtils::IsWaterMeshEnabled(/*bIsRenderThread = */false) && FApp::CanEverRender();

	// Early out
	if (!bIsEnabled)
	{
		return;
	}

	const int32 NewLODCountBias = CVarWaterMeshLODCountBias.GetValueOnGameThread();
	const int32 NewTessFactorBias = CVarWaterMeshTessFactorBias.GetValueOnGameThread();
	const float NewLODScaleBias = CVarWaterMeshLODScaleBias.GetValueOnGameThread();
	if (bNeedsRebuild 
		|| !!CVarWaterMeshShowTileGenerationGeometry.GetValueOnGameThread() 
		|| !!CVarWaterMeshForceRebuildMeshPerFrame.GetValueOnGameThread() 
		|| (NewLODCountBias != LODCountBiasScalability)
		|| (NewTessFactorBias != TessFactorBiasScalability) 
		|| (NewLODScaleBias != LODScaleBiasScalability))
	{
		LODCountBiasScalability = NewLODCountBias;
		TessFactorBiasScalability = NewTessFactorBias;
		LODScaleBiasScalability = NewLODScaleBias;
		const float LODCountBiasFactor = FMath::Pow(2.0f, (float)LODCountBiasScalability);

		FIntPoint ExtentInTiles = GetExtentInTiles();
		RebuildWaterMesh(TileSize / LODCountBiasFactor, FIntPoint(FMath::CeilToInt(ExtentInTiles.X * LODCountBiasFactor), FMath::CeilToInt(ExtentInTiles.Y * LODCountBiasFactor)));
		PrecachePSOs();
		bNeedsRebuild = false;
	}
}

#if WITH_EDITOR
void UWaterMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();

		// Properties that needs the scene proxy to be rebuilt
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, LODScale)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, TessellationFactor)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, TileSize)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, ForceCollapseDensityLevel)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, FarDistanceMaterial)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, FarDistanceMeshExtent)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UWaterMeshComponent, bUseFarMeshWithoutOcean))
		{
			MarkWaterMeshGridDirty();
			MarkRenderStateDirty();
		}
	}
}
#endif

