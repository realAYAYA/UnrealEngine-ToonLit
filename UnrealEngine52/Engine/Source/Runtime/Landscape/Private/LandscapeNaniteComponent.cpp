// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeNaniteComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "NaniteSceneProxy.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "NaniteDefinitions.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeNaniteComponent)

#if WITH_EDITOR
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "StaticMeshOperations.h"
#include "MeshUtilitiesCommon.h"
#include "OverlappingCorners.h"
#include "MeshBuild.h"
#include "StaticMeshBuilder.h"
#include "NaniteBuilder.h"
#include "Rendering/NaniteResources.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "LandscapePrivate.h"
#endif

ULandscapeNaniteComponent::ULandscapeNaniteComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnabled(true)
{
}

void ULandscapeNaniteComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (UStaticMesh* NaniteStaticMesh = GetStaticMesh())
	{
		UPackage* CurrentPackage = GetPackage();
		check(CurrentPackage);
		// At one point, the Nanite mesh was outered to the component, which leads the mesh to be duplicated when entering PIE. If we outer the mesh to the package instead, 
		//  PIE duplication will simply reference that mesh, preventing the expensive copy to occur when entering PIE: 
		if (!(CurrentPackage->GetPackageFlags() & PKG_PlayInEditor)  // No need to do it on PIE, since the outer should already have been changed in the original object 
			&& (NaniteStaticMesh->GetOuter() != CurrentPackage))
		{
			// Change the outer : 
			NaniteStaticMesh->Rename(nullptr, CurrentPackage, REN_ForceNoResetLoaders);
		}
	}
#endif // WITH_EDITOR

	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (ensure(LandscapeProxy))
	{
		// Ensure that the component lighting and shadow settings matches the actor
		UpdatedSharedPropertiesFromActor();
	}
}

void ULandscapeNaniteComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FComponentPSOPrecacheParamsList& OutParams)
{
	Super::CollectPSOPrecacheData(BasePrecachePSOParams, OutParams);
	
	// Mark high priority
	for (FComponentPSOPrecacheParams& Params : OutParams)
	{
		Params.Priority = EPSOPrecachePriority::High;
	}
}

ALandscapeProxy* ULandscapeNaniteComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

ALandscape* ULandscapeNaniteComponent::GetLandscapeActor() const
{
	ALandscapeProxy* Landscape = GetLandscapeProxy();
	if (Landscape)
	{
		return Landscape->GetLandscapeActor();
	}
	return nullptr;
}

void ULandscapeNaniteComponent::UpdatedSharedPropertiesFromActor()
{
	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();

	CastShadow = LandscapeProxy->CastShadow;
	bCastDynamicShadow = LandscapeProxy->bCastDynamicShadow;
	bCastStaticShadow = LandscapeProxy->bCastStaticShadow;
	bCastContactShadow = LandscapeProxy->bCastContactShadow;
	bCastFarShadow = LandscapeProxy->bCastFarShadow;
	bCastHiddenShadow = LandscapeProxy->bCastHiddenShadow;
	bCastShadowAsTwoSided = LandscapeProxy->bCastShadowAsTwoSided;
	bAffectDistanceFieldLighting = LandscapeProxy->bAffectDistanceFieldLighting;
	bRenderCustomDepth = LandscapeProxy->bRenderCustomDepth;
	CustomDepthStencilWriteMask = LandscapeProxy->CustomDepthStencilWriteMask;
	CustomDepthStencilValue = LandscapeProxy->CustomDepthStencilValue;
	SetCullDistance(LandscapeProxy->LDMaxDrawDistance);
	LightingChannels = LandscapeProxy->LightingChannels;

	// We don't want Nanite representation in ray tracing
	bVisibleInRayTracing = false;

	// We don't want WPO evaluation enabled on landscape meshes
	bEvaluateWorldPositionOffset = false;
}

void ULandscapeNaniteComponent::SetEnabled(bool bValue)
{
	if (bValue != bEnabled)
	{
		bEnabled = bValue;
		MarkRenderStateDirty();
	}
}

bool ULandscapeNaniteComponent::IsHLODRelevant() const
{
	// This component doesn't need to be included in HLOD, as we're already including the non-nanite LS components
	return false;
}

#if WITH_EDITOR

bool ULandscapeNaniteComponent::InitializeForLandscape(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId)
{
	// Use the package as the outer, to avoid duplicating the mesh when entering PIE and duplicating all objects : 
	UStaticMesh* NaniteStaticMesh = NewObject<UStaticMesh>(/*Outer = */GetPackage(), TEXT("LandscapeNaniteMesh"), RF_Transactional);

	FMeshDescription* MeshDescription = nullptr;

	// Mesh
	{
		FStaticMeshSourceModel& SrcModel = NaniteStaticMesh->AddSourceModel();
		
		// Don't allow the engine to recalculate normals
		SrcModel.BuildSettings.bRecomputeNormals = false;
		SrcModel.BuildSettings.bRecomputeTangents = false;
		SrcModel.BuildSettings.bRemoveDegenerates = false;
		SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel.BuildSettings.bUseFullPrecisionUVs = false;

		FMeshNaniteSettings& NaniteSettings = NaniteStaticMesh->NaniteSettings;
		NaniteSettings.bEnabled = true;
		NaniteSettings.FallbackPercentTriangles = 0.01f; // Keep effectively no fallback mesh triangles
		NaniteSettings.FallbackRelativeError = 1.0f;

		const int32 LOD = Landscape->GetLandscapeActor()->NaniteLODIndex;

		MeshDescription = NaniteStaticMesh->CreateMeshDescription(LOD);
		{
			TArray<UMaterialInterface*, TInlineAllocator<4>> InputMaterials;
			TArray<FName, TInlineAllocator<4>> InputMaterialSlotNames;
			TInlineComponentArray<ULandscapeComponent*> InputComponents;

			for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
			{
				UMaterialInterface* Material = nullptr;
				if (Component)
				{
					Material = Component->GetMaterialInstance(LOD);
					InputMaterialSlotNames.Add(FName(*FString::Format(TEXT("LandscapeMat_{0}"), { InputComponents.Num() })));
					InputMaterials.Add(Material ? Material : UMaterial::GetDefaultMaterial(MD_Surface));
					InputComponents.Add(Component);
				}
			}

			if (InputComponents.Num() == 0)
			{
				UE_LOG(LogLandscape, Verbose, TEXT("%s : no Nanite mesh to export"), *GetOwner()->GetActorNameOrLabel());
				return false;
			}

			if (InputMaterials.Num() > NANITE_MAX_CLUSTER_MATERIALS)
			{
				UE_LOG(LogLandscape, Warning, TEXT("%s : Nanite landscape mesh would have more than %i materials, which is currently not supported. Please reduce the number of components in this landscape actor to enable Nanite."), *GetOwner()->GetActorNameOrLabel(), NANITE_MAX_CLUSTER_MATERIALS)
				return false;
			}

			ALandscapeProxy::FRawMeshExportParams ExportParams;
			ExportParams.ComponentsToExport = MakeArrayView(InputComponents.GetData(), InputComponents.Num());
			ExportParams.ComponentsMaterialSlotName = MakeArrayView(InputMaterialSlotNames.GetData(), InputMaterialSlotNames.Num());
			ExportParams.ExportLOD = LOD;
			ExportParams.ExportCoordinatesType = ALandscapeProxy::FRawMeshExportParams::EExportCoordinatesType::RelativeToProxy;
			ExportParams.UVConfiguration.ExportUVMappingTypes.SetNumZeroed(4);
			ExportParams.UVConfiguration.ExportUVMappingTypes[0] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::TerrainCoordMapping_XY; // In LandscapeVertexFactory, Texcoords0 = ETerrainCoordMappingType::TCMT_XY (or ELandscapeCustomizedCoordType::LCCT_CustomUV0)
			ExportParams.UVConfiguration.ExportUVMappingTypes[1] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::TerrainCoordMapping_XZ; // In LandscapeVertexFactory, Texcoords1 = ETerrainCoordMappingType::TCMT_XZ (or ELandscapeCustomizedCoordType::LCCT_CustomUV1)
			ExportParams.UVConfiguration.ExportUVMappingTypes[2] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::TerrainCoordMapping_YZ; // In LandscapeVertexFactory, Texcoords2 = ETerrainCoordMappingType::TCMT_YZ (or ELandscapeCustomizedCoordType::LCCT_CustomUV2)
			ExportParams.UVConfiguration.ExportUVMappingTypes[3] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::WeightmapUV; // In LandscapeVertexFactory, Texcoords3 = ELandscapeCustomizedCoordType::LCCT_WeightMapUV
			// COMMENT [jonathan.bard] ATM Nanite meshes only support up to 4 UV sets so we cannot support those 2 : 
			//ExportParams.UVConfiguration.ExportUVMappingTypes[4] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::LightmapUV; // In LandscapeVertexFactory, Texcoords4 = lightmap UV
			//ExportParams.UVConfiguration.ExportUVMappingTypes[5] = ALandscapeProxy::FRawMeshExportParams::EUVMappingType::HeightmapUV; // // In LandscapeVertexFactory, Texcoords5 = heightmap UV

			bool bSuccess = Landscape->ExportToRawMesh(ExportParams, *MeshDescription);

			const FPolygonGroupArray& PolygonGroups = MeshDescription->PolygonGroups();
			checkf(bSuccess && (PolygonGroups.Num() == InputComponents.Num()), TEXT("Invalid landscape static mesh raw mesh export for actor %s (%i components)"), *GetOwner()->GetName(), InputComponents.Num());

			check(InputMaterials.Num() == InputComponents.Num());
			FStaticMeshAttributes MeshAttributes(*MeshDescription);
			TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames = MeshAttributes.GetPolygonGroupMaterialSlotNames();
			int32 ComponentIndex = 0;
			for (UMaterialInterface* Material : InputMaterials)
			{
				check(Material != nullptr);
				const FName MaterialSlotName = InputMaterialSlotNames[ComponentIndex];
				check(PolygonGroupMaterialSlotNames.GetRawArray().Contains(MaterialSlotName));
				NaniteStaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material, MaterialSlotName));
				++ComponentIndex;
			}
			UE_LOG(LogLandscape, Verbose, TEXT("Successful export of raw static mesh for Nanite landscape (%i components) for actor %s"), InputComponents.Num(), *GetOwner()->GetName());
		}

		NaniteStaticMesh->CommitMeshDescription(0);
		NaniteStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	}

	SetStaticMesh(NaniteStaticMesh);
	UStaticMesh::BatchBuild({ NaniteStaticMesh });

	// Disable collisions (needs to be done after UStaticMesh::BatchBuild) since it's what will create the UBodySetup :
	if (UBodySetup* BodySetup = NaniteStaticMesh->GetBodySetup())
	{
		BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		// We won't ever enable collisions (since collisions are handled by ULandscapeHeighfieldCollisionComponent), ensure we don't even cook or load any collision data on this mesh: 
		BodySetup->bNeverNeedsCookedCollisionData = true;
	}

	// Disable navigation
	NaniteStaticMesh->bHasNavigationData = false;

	ProxyContentId = NewProxyContentId;

	return true;
}

bool ULandscapeNaniteComponent::InitializePlatformForLandscape(ALandscapeProxy* Landscape, const ITargetPlatform* TargetPlatform)
{
	// This is a workaround. IsCachedCookedPlatformDataLoaded needs to return true to ensure that StreamablePages are loaded from DDC
	if (TargetPlatform)
	{
		UStaticMesh* NaniteStaticMesh = GetStaticMesh();

		NaniteStaticMesh->BeginCacheForCookedPlatformData(TargetPlatform);
		FStaticMeshCompilingManager::Get().FinishCompilation({ NaniteStaticMesh });

		const double StartTime = FPlatformTime::Seconds();

		while (!NaniteStaticMesh->IsCachedCookedPlatformDataLoaded(TargetPlatform))
		{
			FAssetCompilingManager::Get().ProcessAsyncTasks(true);
			FPlatformProcess::Sleep(0.01);

			constexpr double MaxWaitSeconds = 240.0;
			if (FPlatformTime::Seconds() - StartTime > MaxWaitSeconds)
			{
				UE_LOG(LogLandscape, Error, TEXT("ULandscapeNaniteComponent::InitializePlatformForLandscape waited more than %f seconds for IsCachedCookedPlatformDataLoaded to return true"), MaxWaitSeconds);
				return false;
			}
		}
	}

	return true;
}

#endif
