// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomManager.h"
#include "HairStrandsMeshProjection.h"

#include "GeometryCacheComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "CommonRenderResources.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalRenderPublic.h"
#include "GroomTextureBuilder.h"
#include "GroomResources.h"
#include "GroomInstance.h"
#include "GroomGeometryCache.h"
#include "HAL/IConsoleManager.h"
#include "SceneView.h"
#include "HairCardsVertexFactory.h"
#include "HairStrandsVertexFactory.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "CachedGeometry.h"

static int32 GHairStrandsMinLOD = 0;
static FAutoConsoleVariableRef CVarGHairStrandsMinLOD(TEXT("r.HairStrands.MinLOD"), GHairStrandsMinLOD, TEXT("Clamp the min hair LOD to this value, preventing to reach lower/high-quality LOD."), ECVF_Scalability);

static int32 GHairStrands_UseCards = 0;
static FAutoConsoleVariableRef CVarHairStrands_UseCards(TEXT("r.HairStrands.UseCardsInsteadOfStrands"), GHairStrands_UseCards, TEXT("Force cards geometry on all groom elements. If no cards data is available, nothing will be displayed"), ECVF_Scalability);

static int32 GHairStrands_SwapBufferType = 3;
static FAutoConsoleVariableRef CVarGHairStrands_SwapBufferType(TEXT("r.HairStrands.SwapType"), GHairStrands_SwapBufferType, TEXT("Swap rendering buffer at the end of frame. This is an experimental toggle. Default:1"));

static int32 GHairStrands_ManualSkinCache = 0;
static FAutoConsoleVariableRef CVarGHairStrands_ManualSkinCache(TEXT("r.HairStrands.ManualSkinCache"), GHairStrands_ManualSkinCache, TEXT("If skin cache is not enabled, and grooms use skinning method, this enable a simple skin cache mechanisme for groom. Default:disable"));

static int32 GHairStrands_InterpolationFrustumCullingEnable = 1;
static FAutoConsoleVariableRef CVarHairStrands_InterpolationFrustumCullingEnable(TEXT("r.HairStrands.Interoplation.FrustumCulling"), GHairStrands_InterpolationFrustumCullingEnable, TEXT("Swap rendering buffer at the end of frame. This is an experimental toggle. Default:1"));

EHairBufferSwapType GetHairSwapBufferType()
{
	switch (GHairStrands_SwapBufferType)
	{
	case 0: return EHairBufferSwapType::BeginOfFrame;
	case 1: return EHairBufferSwapType::EndOfFrame;
	case 2: return EHairBufferSwapType::Tick;
	case 3: return EHairBufferSwapType::RenderFrame;
	}
	return EHairBufferSwapType::EndOfFrame;
}

DEFINE_LOG_CATEGORY_STATIC(LogGroomManager, Log, All);

///////////////////////////////////////////////////////////////////////////////////////////////////

void FHairGroupInstance::FCards::FLOD::InitVertexFactory()
{
	VertexFactory->InitResources();
}

void FHairGroupInstance::FMeshes::FLOD::InitVertexFactory()
{
	VertexFactory->InitResources();
}

static bool IsInstanceFrustumCullingEnable()
{
	return GHairStrands_InterpolationFrustumCullingEnable > 0;
}

bool NeedsUpdateCardsMeshTriangles();

static bool IsSkeletalMeshEvaluationEnabled()
{
	// When deferred skel. mesh update is enabled, hair strands skeletal mesh deformation is not allowed, as skin-cached update happen after 
	// the PreInitView calls, which causes hair LOD selection and hair simulation to have invalid value & resources
	static const auto CVarSkelMeshGDME = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DeferSkeletalDynamicDataUpdateUntilGDME"));
	return CVarSkelMeshGDME ? CVarSkelMeshGDME->GetValueOnAnyThread() == 0 : true;
}

// Returns the cached geometry of the underlying geometry on which a hair instance is attached to
FCachedGeometry GetCacheGeometryForHair(
	FRDGBuilder& GraphBuilder, 
	FHairGroupInstance* Instance, 
	FGlobalShaderMap* ShaderMap,
	const bool bOutputTriangleData)
{
	FCachedGeometry Out;
	if (Instance->Debug.GroomBindingType == EGroomBindingMeshType::SkeletalMesh)
	{
		if (IsSkeletalMeshEvaluationEnabled())
		{
			//todo: It's unsafe to be accessing the component directly here. This can be solved when we have persistent ids available on the render thread.
			if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Instance->Debug.MeshComponent))
			{
				if (FSkeletalMeshSceneProxy* SceneProxy = static_cast<FSkeletalMeshSceneProxy*>(SkeletalMeshComponent->SceneProxy))
				{
					SceneProxy->GetCachedGeometry(Out);
				}

				if (GHairStrands_ManualSkinCache > 0 && Out.Sections.Num() == 0)
				{
					//#hair_todo: Need to have a (frame) cache to insure that we don't recompute the same projection several time
					// Actual populate the cache with only the needed part based on the groom projection data. At the moment it recompute everything ...
					BuildCacheGeometry(GraphBuilder, ShaderMap, SkeletalMeshComponent, bOutputTriangleData, Out);
				}
			}
		}
	}
	else if (Instance->Debug.GroomBindingType == EGroomBindingMeshType::GeometryCache)
	{
		if (const UGeometryCacheComponent* GeometryCacheComponent = Cast<const UGeometryCacheComponent>(Instance->Debug.MeshComponent))
		{
			BuildCacheGeometry(GraphBuilder, ShaderMap, GeometryCacheComponent, bOutputTriangleData, Out);
		}
	}
	return Out;
}

// Return the LOD index of the cached geometry on which the hair are bound to. 
// Return -1 if the hair are not bound of if the underlying geometry is invalid
static int32 GetCacheGeometryLODIndex(const FCachedGeometry& In)
{
	int32 MeshLODIndex = In.LODIndex;
	for (const FCachedGeometry::Section& Section : In.Sections)
	{
		// Ensure all mesh's sections have the same LOD index
		if (MeshLODIndex < 0) MeshLODIndex = Section.LODIndex;
		check(MeshLODIndex == Section.LODIndex);
	}

	return MeshLODIndex;
}

static void RunInternalHairStrandsInterpolation(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	const uint32 ViewUniqueID,
	const FHairStrandsInstances& Instances,
	const FShaderPrintData* ShaderPrintData,
	FGlobalShaderMap* ShaderMap, 
	EHairStrandsInterpolationType Type,
	FHairStrandClusterData* ClusterData)
{
	check(IsInRenderingThread());

	#if RHI_RAYTRACING
	const uint32 ViewRayTracingMask = View->Family->EngineShowFlags.PathTracing ? EHairViewRayTracingMask::PathTracing : EHairViewRayTracingMask::RayTracing;
	#else
	const uint32 ViewRayTracingMask = 0u;
	#endif

	// Update dynamic mesh triangles
	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		int32 MeshLODIndex = -1;
		if (Instance->GeometryType == EHairGeometryType::NoneGeometry)
			continue;
	
		check(Instance->HairGroupPublicData);

		FHairStrandsProjectionMeshData::LOD MeshDataLOD;
		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Instance, ShaderMap, true);
		for (int32 SectionIndex = 0; SectionIndex < CachedGeometry.Sections.Num(); ++SectionIndex)
		{
			// Ensure all mesh's sections have the same LOD index
			const int32 SectionLodIndex = CachedGeometry.Sections[SectionIndex].LODIndex;
			if (MeshLODIndex < 0) MeshLODIndex = SectionLodIndex;
			check(MeshLODIndex == SectionLodIndex);

			MeshDataLOD.Sections.Add(ConvertMeshSection(CachedGeometry, SectionIndex));
		}

		Instance->Debug.MeshLODIndex = MeshLODIndex;
		if (0 <= MeshLODIndex)
		{
			const EHairGeometryType InstanceGeometryType = Instance->GeometryType;
			const EHairBindingType InstanceBindingType = Instance->BindingType;

			const uint32 HairLODIndex = Instance->HairGroupPublicData->LODIndex;
			const EHairBindingType BindingType = Instance->HairGroupPublicData->GetBindingType(HairLODIndex);
			const bool bSimulationEnable = Instance->HairGroupPublicData->IsSimulationEnable(HairLODIndex);
			const bool bDeformationEnable = Instance->HairGroupPublicData->bIsDeformationEnable;
			const bool bGlobalDeformationEnable = Instance->HairGroupPublicData->IsGlobalInterpolationEnable(HairLODIndex);
			check(InstanceBindingType == BindingType);

			if (EHairStrandsInterpolationType::RenderStrands == Type)
			{
				if (InstanceGeometryType == EHairGeometryType::Strands)
				{
					if (BindingType == EHairBindingType::Skinning || bGlobalDeformationEnable)
					{
						check(Instance->Strands.HasValidRootData());
						check(Instance->Strands.DeformedRootResource->IsValid(MeshLODIndex));

						AddHairStrandUpdateMeshTrianglesPass(
							GraphBuilder, 
							ShaderMap, 
							MeshLODIndex, 
							HairStrandsTriangleType::DeformedPose, 
							MeshDataLOD, 
							Instance->Strands.RestRootResource, 
							Instance->Strands.DeformedRootResource);

						AddHairStrandUpdatePositionOffsetPass(
							GraphBuilder,
							ShaderMap,
							MeshLODIndex,
							Instance->Strands.DeformedRootResource,
							Instance->Strands.DeformedResource);
					}
					else if (bSimulationEnable || bDeformationEnable)
					{
						check(Instance->Strands.HasValidData());

						AddHairStrandUpdatePositionOffsetPass(
							GraphBuilder,
							ShaderMap,
							MeshLODIndex,
							nullptr,
							Instance->Strands.DeformedResource);
					}
				}
				else if (InstanceGeometryType == EHairGeometryType::Cards)
				{
					if (Instance->Cards.IsValid(HairLODIndex))
					{
						FHairGroupInstance::FCards::FLOD& CardsInstance = Instance->Cards.LODs[HairLODIndex];
						if (BindingType == EHairBindingType::Skinning || bGlobalDeformationEnable)
						{
							check(CardsInstance.Guides.IsValid());
							check(CardsInstance.Guides.HasValidRootData());
							check(CardsInstance.Guides.DeformedRootResource->IsValid(MeshLODIndex));

							AddHairStrandUpdateMeshTrianglesPass(
								GraphBuilder,
								ShaderMap,
								MeshLODIndex,
								HairStrandsTriangleType::DeformedPose,
								MeshDataLOD,
								CardsInstance.Guides.RestRootResource,
								CardsInstance.Guides.DeformedRootResource);

							AddHairStrandUpdatePositionOffsetPass(
								GraphBuilder,
								ShaderMap,
								MeshLODIndex,
								CardsInstance.Guides.DeformedRootResource,
								CardsInstance.Guides.DeformedResource);
						}
						else if (bSimulationEnable || bDeformationEnable)
						{
							check(CardsInstance.Guides.IsValid());
							AddHairStrandUpdatePositionOffsetPass(
								GraphBuilder,
								ShaderMap,
								MeshLODIndex,
								nullptr,
								CardsInstance.Guides.DeformedResource);
						}
					}
				}
				else if (InstanceGeometryType == EHairGeometryType::Meshes)
				{
					// Nothing to do
				}
			}
			else if (EHairStrandsInterpolationType::SimulationStrands == Type)
			{
				// Guide update need to run only if simulation is enabled, or if RBF is enabled (since RFB are transfer through guides)
				if (bGlobalDeformationEnable || bSimulationEnable || bDeformationEnable)
				{
					check(Instance->Guides.IsValid());

					if (BindingType == EHairBindingType::Skinning)
					{
						check(Instance->Guides.IsValid());
						check(Instance->Guides.HasValidRootData());
						check(Instance->Guides.DeformedRootResource->IsValid(MeshLODIndex));

						AddHairStrandUpdateMeshTrianglesPass(
							GraphBuilder,
							ShaderMap,
							Instance->Debug.MeshLODIndex,
							HairStrandsTriangleType::DeformedPose,
							MeshDataLOD,
							Instance->Guides.RestRootResource,
							Instance->Guides.DeformedRootResource);

						if (bGlobalDeformationEnable)
						{
							AddHairStrandInitMeshSamplesPass(
								GraphBuilder,
								ShaderMap,
								Instance->Debug.MeshLODIndex,
								HairStrandsTriangleType::DeformedPose,
								MeshDataLOD,
								Instance->Guides.RestRootResource,
								Instance->Guides.DeformedRootResource);

							AddHairStrandUpdateMeshSamplesPass(
								GraphBuilder,
								ShaderMap,
								Instance->Debug.MeshLODIndex,
								MeshDataLOD,
								Instance->Guides.RestRootResource,
								Instance->Guides.DeformedRootResource);
						}

						AddHairStrandUpdatePositionOffsetPass(
							GraphBuilder,
							ShaderMap,
							Instance->Debug.MeshLODIndex,
							Instance->Guides.DeformedRootResource,
							Instance->Guides.DeformedResource);

						// Add manual transition for the GPU solver as Niagara does not track properly the RDG buffer, and so doesn't issue the correct transitions
						if (MeshLODIndex >= 0)
						{
							GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, Instance->Guides.DeformedRootResource->LODs[MeshLODIndex].GetDeformedUniqueTrianglePosition0Buffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
							GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, Instance->Guides.DeformedRootResource->LODs[MeshLODIndex].GetDeformedUniqueTrianglePosition1Buffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
							GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, Instance->Guides.DeformedRootResource->LODs[MeshLODIndex].GetDeformedUniqueTrianglePosition2Buffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);

							if (bGlobalDeformationEnable)
							{
								GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, Instance->Guides.DeformedRootResource->LODs[MeshLODIndex].GetDeformedSamplePositionsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
								GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, Instance->Guides.DeformedRootResource->LODs[MeshLODIndex].GetMeshSampleWeightsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
							}
						}
						GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
					}
					else if (bSimulationEnable || bDeformationEnable)
					{
						check(Instance->Guides.IsValid());

						AddHairStrandUpdatePositionOffsetPass(
							GraphBuilder,
							ShaderMap,
							Instance->Debug.MeshLODIndex,
							nullptr,
							Instance->Guides.DeformedResource);

						// Add manual transition for the GPU solver as Niagara does not track properly the RDG buffer, and so doesn't issue the correct transitions
						GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
					}
				}
			}
		}
	}

	// Reset deformation
	if (EHairStrandsInterpolationType::SimulationStrands == Type)
	{
		for (FHairStrandsInstance* AbstractInstance : Instances)
		{
			FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

			if (Instance->GeometryType == EHairGeometryType::NoneGeometry)
				continue;

			ResetHairStrandsInterpolation(GraphBuilder, ShaderMap, Instance, Instance->Debug.MeshLODIndex);
		}
	}

	// Hair interpolation
	if (EHairStrandsInterpolationType::RenderStrands == Type)
	{
		check(View);
		const FVector& TranslatedWorldOffset = View->ViewMatrices.GetPreViewTranslation();
		for (FHairStrandsInstance* AbstractInstance : Instances)
		{
			FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

			if (Instance->GeometryType == EHairGeometryType::NoneGeometry)
				continue;

 			ComputeHairStrandsInterpolation(
				GraphBuilder, 
				ShaderMap,
				ViewUniqueID,
				ViewRayTracingMask,
				TranslatedWorldOffset,
				ShaderPrintData,
				Instance,
				Instance->Debug.MeshLODIndex,
				ClusterData);
		}
	}
}

static void RunHairStrandsInterpolation_Guide(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	const uint32 ViewUniqueID,
	const FHairStrandsInstances& Instances,
	const FShaderPrintData* ShaderPrintData,
	FGlobalShaderMap* ShaderMap,
	FHairStrandClusterData* ClusterData)
{
	check(IsInRenderingThread());

	DECLARE_GPU_STAT(HairGuideInterpolation);
	RDG_EVENT_SCOPE(GraphBuilder, "HairGuideInterpolation");
	RDG_GPU_STAT_SCOPE(GraphBuilder, HairGuideInterpolation);

	RunInternalHairStrandsInterpolation(
		GraphBuilder,
		View,
		ViewUniqueID,
		Instances,
		ShaderPrintData,
		ShaderMap,
		EHairStrandsInterpolationType::SimulationStrands,
		ClusterData);
}

static void RunHairStrandsInterpolation_Strands(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	const uint32 ViewUniqueID,
	const FHairStrandsInstances& Instances,
	const FShaderPrintData* ShaderPrintData,
	FGlobalShaderMap* ShaderMap,
	FHairStrandClusterData* ClusterData)
{
	check(IsInRenderingThread());

	DECLARE_GPU_STAT(HairStrandsInterpolation);
	RDG_EVENT_SCOPE(GraphBuilder, "HairStrandsInterpolation");
	RDG_GPU_STAT_SCOPE(GraphBuilder, HairStrandsInterpolation);

	RunInternalHairStrandsInterpolation(
		GraphBuilder,
		View,
		ViewUniqueID,
		Instances,
		ShaderPrintData,
		ShaderMap,
		EHairStrandsInterpolationType::RenderStrands,
		ClusterData);
}

static void RunHairStrandsGatherCluster(
	const FHairStrandsInstances& Instances,
	FHairStrandClusterData* ClusterData)
{
	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		if (Instance->GeometryType != EHairGeometryType::Strands)
			continue;

		if (Instance->Strands.IsValid())
		{
			if (Instance->Strands.bIsCullingEnabled)
			{
				RegisterClusterData(Instance, ClusterData);
			}
			else
			{
				Instance->HairGroupPublicData->bCullingResultAvailable = false;
			}
		}
	}
}

// Return the LOD which should be used for a given screen size and LOD bias value
// This function is mirrored in HairStrandsClusterCommon.ush
static float GetHairInstanceLODIndex(const TArray<float>& InLODScreenSizes, float InScreenSize, float InLODBias)
{
	const uint32 LODCount = InLODScreenSizes.Num();
	check(LODCount > 0);

	float OutLOD = 0;
	if (LODCount > 1 && InScreenSize < InLODScreenSizes[0])
	{
		for (uint32 LODIt = 1; LODIt < LODCount; ++LODIt)
		{
			if (InScreenSize >= InLODScreenSizes[LODIt])
			{
				uint32 PrevLODIt = LODIt - 1;

				const float S_Delta = abs(InLODScreenSizes[PrevLODIt] - InLODScreenSizes[LODIt]);
				const float S = S_Delta > 0 ? FMath::Clamp(FMath::Abs(InScreenSize - InLODScreenSizes[LODIt]) / S_Delta, 0.f, 1.f) : 0;
				OutLOD = PrevLODIt + (1 - S);
				break;
			}
			else if (LODIt == LODCount - 1)
			{
				OutLOD = LODIt;
			}
		}
	}

	if (InLODBias != 0)
	{
		OutLOD = FMath::Clamp(OutLOD + InLODBias, 0.f, float(LODCount - 1));
	}
	return OutLOD;
}


static EHairGeometryType ConvertLODGeometryType(EHairGeometryType Type, bool InbUseCards, EShaderPlatform Platform)
{
	// Force cards only if it is enabled or fallback on cards if strands are disabled
	InbUseCards = (InbUseCards || !IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform)) && IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform);

	switch (Type)
	{
	case EHairGeometryType::Strands: return IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform) ? (InbUseCards ? EHairGeometryType::Cards : EHairGeometryType::Strands) : (InbUseCards ? EHairGeometryType::Cards : EHairGeometryType::NoneGeometry);
	case EHairGeometryType::Cards:   return IsHairStrandsEnabled(EHairStrandsShaderType::Cards, Platform) ? EHairGeometryType::Cards : EHairGeometryType::NoneGeometry;
	case EHairGeometryType::Meshes:  return IsHairStrandsEnabled(EHairStrandsShaderType::Meshes, Platform) ? EHairGeometryType::Meshes : EHairGeometryType::NoneGeometry;
	}
	return EHairGeometryType::NoneGeometry;
}

static void RunHairBufferSwap(const FHairStrandsInstances& Instances, const TArray<const FSceneView*> Views)
{
	EShaderPlatform ShaderPlatform = EShaderPlatform::SP_NumPlatforms;
	if (Views.Num() > 0)
	{
		ShaderPlatform = Views[0]->GetShaderPlatform();
	}

	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		int32 MeshLODIndex = -1;
		check(Instance);
		if (!Instance)
			continue;

		check(Instance->HairGroupPublicData);

		// Swap current/previous buffer for all LODs
		const bool bIsPaused = Views[0]->Family->bWorldIsPaused;
		if (!bIsPaused)
		{
			if (Instance->Guides.DeformedResource) { Instance->Guides.DeformedResource->SwapBuffer(); }
			if (Instance->Strands.DeformedResource) { Instance->Strands.DeformedResource->SwapBuffer(); }

			// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
			if (IsHairStrandContinuousDecimationReorderingEnabled())
			{
				if (Instance->Guides.DeformedRootResource) { Instance->Guides.DeformedRootResource->SwapBuffer(); }
				if (Instance->Strands.DeformedRootResource) { Instance->Strands.DeformedRootResource->SwapBuffer(); }
			}

			Instance->Debug.LastFrameIndex = Views[0]->Family->FrameNumber;
		}
	}
}

static void AddCopyHairStrandsPositionPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FHairStrandsRestResource& RestResources,
	FHairStrandsDeformedResource& DeformedResources)
{
	FRDGBufferRef RestBuffer = Register(GraphBuilder, RestResources.PositionBuffer, ERDGImportedBufferFlags::None).Buffer;
	FRDGBufferRef Deformed0Buffer = Register(GraphBuilder, DeformedResources.DeformedPositionBuffer[0], ERDGImportedBufferFlags::None).Buffer;
	FRDGBufferRef Deformed1Buffer = Register(GraphBuilder, DeformedResources.DeformedPositionBuffer[1], ERDGImportedBufferFlags::None).Buffer;
	AddCopyBufferPass(GraphBuilder, Deformed0Buffer, RestBuffer);
	AddCopyBufferPass(GraphBuilder, Deformed1Buffer, RestBuffer);
}

#if RHI_RAYTRACING
static void AllocateRaytracingResources(FHairGroupInstance* Instance)
{
	if (IsHairRayTracingEnabled() && !Instance->Strands.RenRaytracingResource)
	{
		check(Instance->Strands.Data);

		// Allocate dynamic raytracing resources (owned by the groom component/instance)
		FHairResourceName ResourceName(FName(Instance->Debug.GroomAssetName), Instance->Debug.GroupIndex);
		Instance->Strands.RenRaytracingResource		 = new FHairStrandsRaytracingResource(*Instance->Strands.Data, ResourceName);
		Instance->Strands.RenRaytracingResourceOwned = true;
	}
	Instance->Strands.ViewRayTracingMask |= EHairViewRayTracingMask::PathTracing;
}
#endif

void AddHairStreamingRequest(FHairGroupInstance* Instance, int32 InLODIndex)
{
	if (Instance && InLODIndex >= 0)
	{
		check(Instance->HairGroupPublicData);

		// Hypothesis: the mesh LOD will be the same as the hair LOD
		const int32 MeshLODIndex = InLODIndex;
		
		// Insure that MinLOD is necessary taken into account if a force LOD is request (i.e., LODIndex>=0). If a Force LOD 
		// is not resquested (i.e., LODIndex<0), the MinLOD is applied after ViewLODIndex has been determined in the codeblock below
		const float MinLOD = FMath::Max(0, GHairStrandsMinLOD);
		float LODIndex = FMath::Max(InLODIndex, MinLOD);

		const TArray<EHairGeometryType>& LODGeometryTypes = Instance->HairGroupPublicData->GetLODGeometryTypes();
		const TArray<bool>& LODVisibilities = Instance->HairGroupPublicData->GetLODVisibilities();
		const int32 LODCount = LODVisibilities.Num();

		LODIndex = FMath::Clamp(LODIndex, 0.f, float(LODCount - 1));

		const int32 IntLODIndex = FMath::Clamp(FMath::FloorToInt(LODIndex), 0, LODCount - 1);
		const bool bIsVisible = LODVisibilities[IntLODIndex];
		const bool bForceCards = GHairStrands_UseCards > 0 || Instance->bForceCards; // todo
		EHairGeometryType GeometryType = ConvertLODGeometryType(LODGeometryTypes[IntLODIndex], bForceCards, GMaxRHIShaderPlatform);

		if (GeometryType == EHairGeometryType::Meshes)
		{
			if (!Instance->Meshes.IsValid(IntLODIndex))
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}
		}
		else if (GeometryType == EHairGeometryType::Cards)
		{
			if (!Instance->Cards.IsValid(IntLODIndex))
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}
		}
		else if (GeometryType == EHairGeometryType::Strands)
		{
			if (!Instance->Strands.IsValid())
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}
		}

		if (!bIsVisible)
		{
			GeometryType = EHairGeometryType::NoneGeometry;
		}

		const bool bSimulationEnable			= Instance->HairGroupPublicData->IsSimulationEnable(LODIndex);
		const bool bDeformationEnable			= Instance->HairGroupPublicData->bIsDeformationEnable;
		const bool bGlobalInterpolationEnable	= Instance->HairGroupPublicData->IsGlobalInterpolationEnable(LODIndex);
		const bool bLODNeedsGuides				= bSimulationEnable || bDeformationEnable || bGlobalInterpolationEnable;

		const EHairResourceLoadingType LoadingType = GetHairResourceLoadingType(GeometryType, int32(LODIndex));
		if (LoadingType != EHairResourceLoadingType::Async || GeometryType == EHairGeometryType::NoneGeometry)
		{
			return;
		}

		// Lazy allocation of resources
		// Note: Allocation will only be done if the resources is not initialized yet. Guides deformed position are also initialized from the Rest position at creation time.
		if (Instance->Guides.Data && bLODNeedsGuides)
		{
			if (Instance->Guides.RestRootResource)			{ Instance->Guides.RestRootResource->StreamInData(); Instance->Guides.RestRootResource->StreamInLODData(MeshLODIndex); }
			if (Instance->Guides.RestResource)				{ Instance->Guides.RestResource->StreamInData(); }
		}

		if (GeometryType == EHairGeometryType::Meshes)
		{
			FHairGroupInstance::FMeshes::FLOD& InstanceLOD = Instance->Meshes.LODs[IntLODIndex];
		}
		else if (GeometryType == EHairGeometryType::Cards)
		{
			FHairGroupInstance::FCards::FLOD& InstanceLOD = Instance->Cards.LODs[IntLODIndex];

			if (InstanceLOD.InterpolationResource)			{ InstanceLOD.InterpolationResource->StreamInData(); }
			if (InstanceLOD.DeformedResource)				{ InstanceLOD.DeformedResource->StreamInData(); }
			if (InstanceLOD.Guides.RestRootResource)		{ InstanceLOD.Guides.RestRootResource->StreamInData(); InstanceLOD.Guides.RestRootResource->StreamInLODData(MeshLODIndex); }
			if (InstanceLOD.Guides.RestResource)			{ InstanceLOD.Guides.RestResource->StreamInData(); }
			if (InstanceLOD.Guides.InterpolationResource)	{ InstanceLOD.Guides.InterpolationResource->StreamInData(); }
		}
		else if (GeometryType == EHairGeometryType::Strands)
		{
			if (Instance->Strands.RestRootResource)			{ Instance->Strands.RestRootResource->StreamInData(); Instance->Strands.RestRootResource->StreamInLODData(MeshLODIndex); }
			if (Instance->Strands.RestResource)				{ Instance->Strands.RestResource->StreamInData(); }
			if (Instance->Strands.ClusterCullingResource)	{ Instance->Strands.ClusterCullingResource->StreamInData(); }
			if (Instance->Strands.InterpolationResource)	{ Instance->Strands.InterpolationResource->StreamInData(); }
		}
	}
}

static void RunHairLODSelection(
	FRDGBuilder& GraphBuilder, 
	const FHairStrandsInstances& Instances, 
	const TArray<const FSceneView*>& Views, 
	FGlobalShaderMap* ShaderMap)
{
	EShaderPlatform ShaderPlatform = EShaderPlatform::SP_NumPlatforms;
	if (Views.Num() > 0)
	{
		ShaderPlatform = Views[0]->GetShaderPlatform();
	}

	// Detect if one of the current view has path tracing enabled for allocating raytracing resources
#if RHI_RAYTRACING
	bool bHasPathTracingView = false;
	for (const FSceneView* View : Views)
	{
		if (View->Family->EngineShowFlags.PathTracing)
		{
			bHasPathTracingView = true;
			break;
		}
	}
#endif

	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		check(Instance);
		check(Instance->HairGroupPublicData);
		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Instance, ShaderMap, false);
		const int32 MeshLODIndex = GetCacheGeometryLODIndex(CachedGeometry);

		// Perform LOD selection based on all the views	
		// CPU LOD selection. 
		// * When enable the CPU LOD selection allow to change the geometry representation. 
		// * GPU LOD selecion allow fine grain LODing, but does not support representation changes (strands, cards, meshes)
		// 
		// Use the forced LOD index if set, otherwise compute the LOD based on the maximal screensize accross all views
		// Compute the view where the screen size is maximale
		const float PrevLODIndex = Instance->HairGroupPublicData->GetLODIndex();
		const float PrevMeshLODIndex = Instance->HairGroupPublicData->GetMeshLODIndex();

		// 0. Initial LOD index picking
		// Insure that MinLOD is necessary taken into account if a force LOD is request (i.e., LODIndex>=0). If a Force LOD 
		// is not resquested (i.e., LODIndex<0), the MinLOD is applied after ViewLODIndex has been determined in the codeblock below
		const int32 LODCount = Instance->HairGroupPublicData->GetLODVisibilities().Num();
		const float MinLOD = FMath::Max(0, GHairStrandsMinLOD);
		
		// If continuous LOD is enabled, we bypass all other type of geometric representation, and only use LOD0
		float LODIndex = IsHairVisibilityComputeRasterContinuousLODEnabled() ? 0.0 : (Instance->Debug.LODForcedIndex >= 0 ? FMath::Max(Instance->Debug.LODForcedIndex, MinLOD) : -1.0f);
		float LODViewIndex = -1;
		{
			float MaxScreenSize = 0.f;
			const FSphere SphereBound = Instance->GetBounds().GetSphere();
			for (const FSceneView* View : Views)
			{
				const float ScreenSize = ComputeBoundsScreenSize(FVector4(SphereBound.Center, 1), SphereBound.W, *View);
				const float LODBias = Instance->Strands.Modifier.LODBias;
				const float CurrLODViewIndex = FMath::Max(MinLOD, GetHairInstanceLODIndex(Instance->HairGroupPublicData->GetLODScreenSizes(), ScreenSize, LODBias));
				MaxScreenSize = FMath::Max(MaxScreenSize, ScreenSize);

				// Select highest LOD accross all views
				LODViewIndex = LODViewIndex < 0 ? CurrLODViewIndex : FMath::Min(LODViewIndex, CurrLODViewIndex);
			}

			if (LODIndex < 0)
			{
				LODIndex = LODViewIndex;
			}

			LODIndex = FMath::Clamp(LODIndex, 0.f, float(LODCount - 1));

			// Feedback game thread with LOD selection 
			Instance->Debug.LODPredictedIndex = LODViewIndex;
			Instance->HairGroupPublicData->DebugScreenSize = MaxScreenSize;

			if (IsHairStrandContinuousDecimationReorderingEnabled())
			{
				if (IsHairVisibilityComputeRasterContinuousLODEnabled())
				{
					Instance->HairGroupPublicData->ContinuousLODBounds = SphereBound;
					Instance->HairGroupPublicData->MaxScreenSize = MaxScreenSize;
				}

				Instance->HairGroupPublicData->UpdateTemporalIndex();
			}
			else
			{
				Instance->HairGroupPublicData->MaxScreenSize = 1.0;
			}
		}

		// Function for selecting, loading, & initializing LOD resources
		auto SelectValidLOD = [Instance, ShaderPlatform, ShaderMap, PrevLODIndex, LODCount, MeshLODIndex, PrevMeshLODIndex
		#if RHI_RAYTRACING
		, bHasPathTracingView
		#endif // RHI_RAYTRACING
		] (FRDGBuilder& GraphBuilder, float LODIndex) -> bool
		{
			const int32 IntLODIndex = FMath::Clamp(FMath::FloorToInt(LODIndex), 0, LODCount - 1);
			const TArray<EHairGeometryType>& LODGeometryTypes = Instance->HairGroupPublicData->GetLODGeometryTypes();
			const TArray<bool>& LODVisibilities = Instance->HairGroupPublicData->GetLODVisibilities();
			const bool bIsVisible = LODVisibilities[IntLODIndex];
			const bool bForceCards = GHairStrands_UseCards > 0 || Instance->bForceCards; // todo
			EHairGeometryType GeometryType = ConvertLODGeometryType(LODGeometryTypes[IntLODIndex], bForceCards, ShaderPlatform);

			// Skip cluster culling if strands have a single LOD (i.e.: LOD0), and the instance is static (i.e., no skinning, no simulation, ...)
			bool bCullingEnable = false;
			if (GeometryType == EHairGeometryType::Meshes)
			{
				if (!Instance->Meshes.IsValid(IntLODIndex))
				{
					GeometryType = EHairGeometryType::NoneGeometry;
				}
			}
			else if (GeometryType == EHairGeometryType::Cards)
			{
				if (!Instance->Cards.IsValid(IntLODIndex))
				{
					GeometryType = EHairGeometryType::NoneGeometry;
				}
			}
			else if (GeometryType == EHairGeometryType::Strands)
			{
				if (!Instance->Strands.IsValid())
				{
					GeometryType = EHairGeometryType::NoneGeometry;
				}
				else
				{
					const bool bNeedLODing		= LODIndex > 0;
					const bool bNeedDeformation = Instance->Strands.DeformedResource != nullptr;
					bCullingEnable = bNeedLODing || bNeedDeformation;
				}
			}

			if (!bIsVisible)
			{
				GeometryType = EHairGeometryType::NoneGeometry;
			}

			const bool bSimulationEnable = Instance->HairGroupPublicData->IsSimulationEnable(IntLODIndex);
			const bool bDeformationEnable			= Instance->HairGroupPublicData->bIsDeformationEnable;
			const bool bGlobalInterpolationEnable =  Instance->HairGroupPublicData->IsGlobalInterpolationEnable(IntLODIndex);
			const bool bLODNeedsGuides = bSimulationEnable || bDeformationEnable || bGlobalInterpolationEnable;

			const EHairResourceLoadingType LoadingType = GetHairResourceLoadingType(GeometryType, IntLODIndex);
			EHairResourceStatus ResourceStatus = EHairResourceStatus::None;

			// Lazy allocation of resources
			// Note: Allocation will only be done if the resources is not initialized yet. Guides deformed position are also initialized from the Rest position at creation time.
			if (Instance->Guides.Data && bLODNeedsGuides)
			{
				if (Instance->Guides.RestRootResource)			{ Instance->Guides.RestRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); Instance->Guides.RestRootResource->AllocateLOD(GraphBuilder, MeshLODIndex, LoadingType, ResourceStatus); }
				if (Instance->Guides.RestResource)				{ Instance->Guides.RestResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (Instance->Guides.DeformedRootResource)		{ Instance->Guides.DeformedRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); Instance->Guides.DeformedRootResource->AllocateLOD(GraphBuilder, MeshLODIndex, LoadingType, ResourceStatus); }
				if (Instance->Guides.DeformedResource)			
				{ 
					// Ensure the rest resources are correctly loaded prior to initialized and copy the rest position into the deformed positions
					if (Instance->Guides.RestResource->bIsInitialized)
					{
						const bool bNeedCopy = !Instance->Guides.DeformedResource->bIsInitialized; 
						Instance->Guides.DeformedResource->Allocate(GraphBuilder, EHairResourceLoadingType::Sync); 
						if (bNeedCopy) 
						{ 
							AddCopyHairStrandsPositionPass(GraphBuilder, ShaderMap, *Instance->Guides.RestResource, *Instance->Guides.DeformedResource); 
						}
					}
				}
			}

			if (GeometryType == EHairGeometryType::Meshes)
			{
				FHairGroupInstance::FMeshes::FLOD& InstanceLOD = Instance->Meshes.LODs[IntLODIndex];

				if (InstanceLOD.DeformedResource)				{ InstanceLOD.DeformedResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				#if RHI_RAYTRACING
				if (InstanceLOD.RaytracingResource)				{ InstanceLOD.RaytracingResource->Allocate(GraphBuilder, LoadingType, ResourceStatus);}
				#endif

				InstanceLOD.InitVertexFactory();
			}
			else if (GeometryType == EHairGeometryType::Cards)
			{
				FHairGroupInstance::FCards::FLOD& InstanceLOD = Instance->Cards.LODs[IntLODIndex];

				if (InstanceLOD.InterpolationResource)			{ InstanceLOD.InterpolationResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.DeformedResource)				{ InstanceLOD.DeformedResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.RestRootResource)		{ InstanceLOD.Guides.RestRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); InstanceLOD.Guides.RestRootResource->AllocateLOD(GraphBuilder, MeshLODIndex, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.RestResource)			{ InstanceLOD.Guides.RestResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.DeformedRootResource)	{ InstanceLOD.Guides.DeformedRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); InstanceLOD.Guides.DeformedRootResource->AllocateLOD(GraphBuilder, MeshLODIndex, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.DeformedResource)		{ InstanceLOD.Guides.DeformedResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (InstanceLOD.Guides.InterpolationResource)	{ InstanceLOD.Guides.InterpolationResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				#if RHI_RAYTRACING
				if (InstanceLOD.RaytracingResource)				{ InstanceLOD.RaytracingResource->Allocate(GraphBuilder, LoadingType, ResourceStatus);}
				#endif
				InstanceLOD.InitVertexFactory();
			}
			else if (GeometryType == EHairGeometryType::Strands)
			{
				check(Instance->HairGroupPublicData);
				Instance->HairGroupPublicData->Allocate(GraphBuilder);

				if (Instance->Strands.RestRootResource)			{ Instance->Strands.RestRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); Instance->Strands.RestRootResource->AllocateLOD(GraphBuilder, MeshLODIndex, LoadingType, ResourceStatus); }
				if (Instance->Strands.RestResource)				{ Instance->Strands.RestResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (Instance->Strands.ClusterCullingResource)	{ Instance->Strands.ClusterCullingResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				if (Instance->Strands.InterpolationResource)	{ Instance->Strands.InterpolationResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }

				if (Instance->Strands.DeformedRootResource)		{ Instance->Strands.DeformedRootResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); Instance->Strands.DeformedRootResource->AllocateLOD(GraphBuilder, MeshLODIndex, LoadingType, ResourceStatus); }
				if (Instance->Strands.DeformedResource)			{ Instance->Strands.DeformedResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				#if RHI_RAYTRACING
				if (bHasPathTracingView)						{ AllocateRaytracingResources(Instance); }
				if (Instance->Strands.RenRaytracingResource)	{ Instance->Strands.RenRaytracingResource->Allocate(GraphBuilder, LoadingType, ResourceStatus); }
				#endif
				Instance->Strands.VertexFactory->InitResources();

				// Early initialization, so that when filtering MeshBatch block in SceneVisiblity, we can use this value to know 
				// if the hair instance is visible or not (i.e., HairLengthScale > 0)
				Instance->HairGroupPublicData->VFInput.Strands.HairLengthScale = Instance->Strands.Modifier.HairLengthScale;
			}

			// Only switch LOD if the data are ready to be used
			const bool bIsLODDataReady = !!(ResourceStatus & EHairResourceStatus::Loading);
			if (bIsLODDataReady)
			{
				EHairBindingType BindingType = Instance->HairGroupPublicData->GetBindingType(IntLODIndex);
				Instance->HairGroupPublicData->SetLODVisibility(bIsVisible);
				Instance->HairGroupPublicData->SetLODIndex(LODIndex);
				Instance->HairGroupPublicData->SetLODBias(0);
				Instance->HairGroupPublicData->SetMeshLODIndex(MeshLODIndex);
				Instance->HairGroupPublicData->VFInput.GeometryType = GeometryType;
				Instance->HairGroupPublicData->VFInput.BindingType = BindingType;
				Instance->HairGroupPublicData->VFInput.bHasLODSwitch = (FMath::FloorToInt(PrevLODIndex) != FMath::FloorToInt(LODIndex));
				Instance->GeometryType = GeometryType;
				Instance->BindingType = BindingType;
				Instance->Guides.bIsSimulationEnable = Instance->HairGroupPublicData->IsSimulationEnable(IntLODIndex);
				Instance->Guides.bHasGlobalInterpolation = Instance->HairGroupPublicData->IsGlobalInterpolationEnable(IntLODIndex);
				Instance->Guides.bIsDeformationEnable = Instance->HairGroupPublicData->bIsDeformationEnable;
				Instance->Strands.bIsCullingEnabled = bCullingEnable;
			}

			// Ensure that if the binding type is set to Skinning (or has RBF enabled), the skel. mesh is valid and support skin cache.
			if ((Instance->Guides.bHasGlobalInterpolation || Instance->BindingType == EHairBindingType::Skinning) && MeshLODIndex < 0)
			{
				return false;
			}

			// If requested LOD's resources are not ready yet, and the previous LOD was invalid or if the MeshLODIndex has changed 
			// (which would required extra data loading) change the geometry to be invalid, so that it don't get processed this frame.
			const bool bIsLODValid = PrevLODIndex >= 0;
			const bool bHasMeshLODChanged = PrevMeshLODIndex != MeshLODIndex && (Instance->Guides.bHasGlobalInterpolation || Instance->BindingType == EHairBindingType::Skinning);
			if (!bIsLODDataReady && (!bIsLODValid || bHasMeshLODChanged))
			{
				return false;
			}
			return true;
		};

		// 1. Try to find an available LOD
		bool bFoundValidLOD = SelectValidLOD(GraphBuilder, LODIndex);

		// 2. If no valid LOD are found, try to find a coarser LOD which is:
		// * loaded 
		// * does not require skinning binding 
		// * does not require global interpolation (as global interpolation requires skel. mesh data)
		// * does not require simulation (as simulation requires LOD to be selected on the game thread i.e., Predicted/Forced, and so we can override this LOD here)
		if (!bFoundValidLOD)
		{
			for (int32 FallbackLODIndex = FMath::Clamp(FMath::FloorToInt(LODIndex + 1), 0, LODCount - 1); FallbackLODIndex < LODCount; ++FallbackLODIndex)
			{
				if (Instance->HairGroupPublicData->GetBindingType(FallbackLODIndex) == EHairBindingType::Rigid &&
					!Instance->HairGroupPublicData->IsSimulationEnable(FallbackLODIndex) &&
					!Instance->HairGroupPublicData->IsGlobalInterpolationEnable(FallbackLODIndex))
				{
					bFoundValidLOD = SelectValidLOD(GraphBuilder, FallbackLODIndex);
					break;
				}
			}
		}

		// 3. If no LOD are valid, then mark the instance as invalid for this frame
		if (!bFoundValidLOD)
		{
			Instance->GeometryType = EHairGeometryType::NoneGeometry;
			Instance->BindingType = EHairBindingType::NoneBinding;
		}

		// Update the local-to-world transform based on the binding type 
		if (GetHairSwapBufferType() != EHairBufferSwapType::Tick)
		{
			Instance->Debug.SkinningPreviousLocalToWorld = Instance->Debug.SkinningCurrentLocalToWorld;
			Instance->Debug.SkinningCurrentLocalToWorld = CachedGeometry.LocalToWorld;
		}
		Instance->LocalToWorld = Instance->GetCurrentLocalToWorld();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
bool HasHairStrandsFolliculeMaskQueries();
void RunHairStrandsFolliculeMaskQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap);

#if WITH_EDITOR
bool HasHairStrandsTexturesQueries();
void RunHairStrandsTexturesQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* ShaderPrintData);

bool HasHairStrandsPositionQueries();
void RunHairStrandsPositionQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* DebugShaderData);

bool HasHairCardsAtlasQueries();
void RunHairCardsAtlasQueries(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* ShaderPrintData);
#endif

static void RunHairStrandsProcess(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, const struct FShaderPrintData* ShaderPrintData)
{
#if WITH_EDITOR
	if (HasHairStrandsTexturesQueries())
	{
		RunHairStrandsTexturesQueries(GraphBuilder, ShaderMap, ShaderPrintData);
	}

	if (HasHairStrandsPositionQueries())
	{
		RunHairStrandsPositionQueries(GraphBuilder, ShaderMap, ShaderPrintData);
	}
#endif

	if (HasHairStrandsFolliculeMaskQueries())
	{
		RunHairStrandsFolliculeMaskQueries(GraphBuilder, ShaderMap);
	}

#if WITH_EDITOR
	if (HasHairCardsAtlasQueries())
	{
		RunHairCardsAtlasQueries(GraphBuilder, ShaderMap, ShaderPrintData);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RunHairStrandsDebug(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FSceneView& View,
	const FHairStrandsInstances& Instances,
	const FUintVector4& InstanceCountPerType,
	const FShaderPrintData* ShaderPrintData,
	FRDGTextureRef SceneColor,
	FRDGTextureRef SceneDepth,
	FIntRect Viewport,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HairStrands Bookmark API

void ProcessHairStrandsBookmark(
	FRDGBuilder* GraphBuilder,
	EHairStrandsBookmark Bookmark,
	FHairStrandsBookmarkParameters& Parameters)
{
	check(Parameters.Instances != nullptr);

	const bool bCulling =
		IsInstanceFrustumCullingEnable() && (
			Bookmark == EHairStrandsBookmark::ProcessGatherCluster ||
			Bookmark == EHairStrandsBookmark::ProcessStrandsInterpolation ||
			Bookmark == EHairStrandsBookmark::ProcessDebug);
	FHairStrandsInstances& Instances = bCulling ? Parameters.VisibleInstances : *Parameters.Instances;

	if (Bookmark == EHairStrandsBookmark::ProcessTasks)
	{
		const bool bHasHairStardsnProcess =
		#if WITH_EDITOR
			HasHairCardsAtlasQueries() ||
			HasHairStrandsTexturesQueries() ||
			HasHairStrandsPositionQueries() ||
		#endif
			HasHairStrandsFolliculeMaskQueries();

		if (bHasHairStardsnProcess)
		{
			check(GraphBuilder);
			RunHairStrandsProcess(*GraphBuilder, Parameters.ShaderMap, Parameters.ShaderPrintData);
		}
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessLODSelection)
	{
		if (GetHairSwapBufferType() == EHairBufferSwapType::BeginOfFrame)
		{
			RunHairBufferSwap(
				*Parameters.Instances,
				Parameters.AllViews);
		}
		check(GraphBuilder);
		RunHairLODSelection(
			*GraphBuilder,
			*Parameters.Instances,
			Parameters.AllViews,
			Parameters.ShaderMap);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessEndOfFrame)
	{
		if (GetHairSwapBufferType() == EHairBufferSwapType::EndOfFrame)
		{
			RunHairBufferSwap(
				*Parameters.Instances,
				Parameters.AllViews);
		}
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessGuideInterpolation)
	{
		check(GraphBuilder);
		RunHairStrandsInterpolation_Guide(
			*GraphBuilder,
			Parameters.View,
			Parameters.ViewUniqueID,
			Instances,
			Parameters.ShaderPrintData,
			Parameters.ShaderMap,
			&Parameters.HairClusterData);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessGatherCluster)
	{
		RunHairStrandsGatherCluster(
			Instances,
			&Parameters.HairClusterData);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessStrandsInterpolation)
	{
		check(GraphBuilder);
		RunHairStrandsInterpolation_Strands(
			*GraphBuilder,
			Parameters.View,
			Parameters.ViewUniqueID,
			Instances,
			Parameters.ShaderPrintData,
			Parameters.ShaderMap,
			&Parameters.HairClusterData);
	}
	else if (Bookmark == EHairStrandsBookmark::ProcessDebug)
	{
		check(GraphBuilder);
		RunHairStrandsDebug(
			*GraphBuilder,
			Parameters.ShaderMap,
			*Parameters.View,
			Instances,
			Parameters.InstanceCountPerType,
			Parameters.ShaderPrintData,
			Parameters.SceneColorTexture,
			Parameters.SceneDepthTexture,
			Parameters.View->UnscaledViewRect,
			Parameters.View->ViewUniformBuffer);
	}
}