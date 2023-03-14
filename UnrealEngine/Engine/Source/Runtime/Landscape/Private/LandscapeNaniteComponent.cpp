// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeNaniteComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"
#include "NaniteSceneProxy.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"

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

	ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
	if (ensure(LandscapeProxy))
	{
		// Ensure that the component lighting and shadow settings matches the actor
		UpdatedSharedPropertiesFromActor();
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

#if WITH_EDITOR

void ULandscapeNaniteComponent::InitializeForLandscape(ALandscapeProxy* Landscape, const FGuid& NewProxyContentId)
{
	UStaticMesh* NaniteStaticMesh = NewObject<UStaticMesh>(this /* Outer */, TEXT("LandscapeNaniteMesh"), RF_Transactional);

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

		const int32 LOD = 0; // Always uses high quality LOD

		MeshDescription = NaniteStaticMesh->CreateMeshDescription(LOD);
		{
			TArray<UMaterialInterface*, TInlineAllocator<4>> InputMaterials;
			TInlineComponentArray<ULandscapeComponent*> InputComponents;

			for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
			{
				if (Component)
				{
					InputMaterials.Add(Component->GetLandscapeMaterial(LOD));
					InputComponents.Add(Component);
				}
			}

			if (InputComponents.Num() == 0)
			{
				// TODO: Error
				return;
			}

			FBoxSphereBounds UnusedBounds;
			if (!Landscape->ExportToRawMesh(
				MakeArrayView(InputComponents.GetData(), InputComponents.Num()),
				LOD,
				*MeshDescription,
				UnusedBounds,
				true /* Ignore Bounds */
			))
			{
				// TODO: Error
				return;
			}

			for (UMaterialInterface* Material : InputMaterials)
			{
				if (Material == nullptr)
				{
					Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				NaniteStaticMesh->GetStaticMaterials().Add(FStaticMaterial(Material));
			}
		}

		NaniteStaticMesh->CommitMeshDescription(0);
		NaniteStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	}

	// Disable collisions
	if (UBodySetup* BodySetup = NaniteStaticMesh->GetBodySetup())
	{
		BodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
	}

	SetStaticMesh(NaniteStaticMesh);
	UStaticMesh::BatchBuild({ NaniteStaticMesh });

	ProxyContentId = NewProxyContentId;
}

void ULandscapeNaniteComponent::InitializePlatformForLandscape(ALandscapeProxy* Landscape, const ITargetPlatform* TargetPlatform)
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
				break;
			}
		}
	}
}

#endif
