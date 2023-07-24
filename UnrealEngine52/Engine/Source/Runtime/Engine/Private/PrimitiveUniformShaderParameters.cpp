// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrimitiveUniformShaderParameters.h"
#include "PrimitiveSceneInfo.h"
#include "NaniteSceneProxy.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "SceneInterface.h"
#include "UnrealEngine.h"
#include "DataDrivenShaderPlatformInfo.h"

FPrimitiveUniformShaderParametersBuilder& FPrimitiveUniformShaderParametersBuilder::InstanceDrawDistance(FVector2f DistanceMinMax)
{
	// Only scale the far distance by scalability parameters
	DistanceMinMax.Y *= GetCachedScalabilityCVars().ViewDistanceScale;
	Parameters.InstanceDrawDistanceMinMaxSquared = FMath::Square(DistanceMinMax);
	bHasInstanceDrawDistanceCull = true;
	return *this;
}

FPrimitiveUniformShaderParametersBuilder& FPrimitiveUniformShaderParametersBuilder::InstanceWorldPositionOffsetDisableDistance(float WPODisableDistance)
{
	WPODisableDistance *= GetCachedScalabilityCVars().ViewDistanceScale;
	bHasWPODisableDistance = true;
	Parameters.InstanceWPODisableDistanceSquared = WPODisableDistance * WPODisableDistance;

	return *this;
}

void FSinglePrimitiveStructured::InitRHI() 
{
	SCOPED_LOADTIMER(FSinglePrimitiveStructuredBuffer_InitRHI);

	FRHIResourceCreateInfo CreateInfo(TEXT("PrimitiveSceneDataBuffer"));

	{	
		PrimitiveSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4f), FPrimitiveSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
		PrimitiveSceneDataBufferSRV = RHICreateShaderResourceView(PrimitiveSceneDataBufferRHI);
	}

	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("PrimitiveSceneDataTexture"), FPrimitiveSceneShaderData::DataStrideInFloat4s, 1, PF_A32B32G32R32F)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);

		PrimitiveSceneDataTextureRHI = RHICreateTexture(Desc);
		PrimitiveSceneDataTextureSRV = RHICreateShaderResourceView(PrimitiveSceneDataTextureRHI, 0);
	}

	CreateInfo.DebugName = TEXT("LightmapSceneDataBuffer");
	LightmapSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4f), FLightmapSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
	LightmapSceneDataBufferSRV = RHICreateShaderResourceView(LightmapSceneDataBufferRHI);

	CreateInfo.DebugName = TEXT("InstanceSceneDataBuffer");
	InstanceSceneDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4f), FInstanceSceneShaderData::GetDataStrideInFloat4s() * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
	InstanceSceneDataBufferSRV = RHICreateShaderResourceView(InstanceSceneDataBufferRHI);

	CreateInfo.DebugName = TEXT("InstancePayloadDataBuffer");
	InstancePayloadDataBufferRHI = RHICreateStructuredBuffer(sizeof(FVector4f), 1 /* unused dummy */ * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
	InstancePayloadDataBufferSRV = RHICreateShaderResourceView(InstancePayloadDataBufferRHI);

	CreateInfo.DebugName = TEXT("SkyIrradianceEnvironmentMap");
	SkyIrradianceEnvironmentMapRHI = RHICreateStructuredBuffer(sizeof(FVector4f), sizeof(FVector4f) * 8, BUF_Static | BUF_ShaderResource, CreateInfo);
	SkyIrradianceEnvironmentMapSRV = RHICreateShaderResourceView(SkyIrradianceEnvironmentMapRHI);

	UploadToGPU();
}

void FSinglePrimitiveStructured::UploadToGPU()
{
	void* LockedData = nullptr;

	LockedData = RHILockBuffer(PrimitiveSceneDataBufferRHI, 0, FPrimitiveSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memcpy(LockedData, PrimitiveSceneData.Data.GetData(), FPrimitiveSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f));
	RHIUnlockBuffer(PrimitiveSceneDataBufferRHI);

	LockedData = RHILockBuffer(LightmapSceneDataBufferRHI, 0, FLightmapSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memcpy(LockedData, LightmapSceneData.Data.GetData(), FLightmapSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f));
	RHIUnlockBuffer(LightmapSceneDataBufferRHI);

	LockedData = RHILockBuffer(InstanceSceneDataBufferRHI, 0, FInstanceSceneShaderData::GetDataStrideInFloat4s() * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memcpy(LockedData, InstanceSceneData.Data.GetData(), FInstanceSceneShaderData::GetDataStrideInFloat4s() * sizeof(FVector4f));
	RHIUnlockBuffer(InstanceSceneDataBufferRHI);

	LockedData = RHILockBuffer(InstancePayloadDataBufferRHI, 0, 1 /* unused dummy */ * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memset(LockedData, 0x00, sizeof(FVector4f));
	RHIUnlockBuffer(InstancePayloadDataBufferRHI);

//#if WITH_EDITOR
	if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM5))
	{
		// Create level instance SRV
		FRHIResourceCreateInfo LevelInstanceBufferCreateInfo(TEXT("EditorVisualizeLevelInstanceDataBuffer"));
		EditorVisualizeLevelInstanceDataBufferRHI = RHICreateVertexBuffer(sizeof(uint32), BUF_Static | BUF_ShaderResource, LevelInstanceBufferCreateInfo);

		LockedData = RHILockBuffer(EditorVisualizeLevelInstanceDataBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);

		*reinterpret_cast<uint32*>(LockedData) = 0;

		RHIUnlockBuffer(EditorVisualizeLevelInstanceDataBufferRHI);

		EditorVisualizeLevelInstanceDataBufferSRV = RHICreateShaderResourceView(EditorVisualizeLevelInstanceDataBufferRHI, sizeof(uint32), PF_R32_UINT);

		// Create selection outline SRV
		FRHIResourceCreateInfo SelectionBufferCreateInfo(TEXT("EditorSelectedDataBuffer"));
		EditorSelectedDataBufferRHI = RHICreateVertexBuffer(sizeof(uint32), BUF_Static | BUF_ShaderResource, SelectionBufferCreateInfo);

		LockedData = RHILockBuffer(EditorSelectedDataBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);

		*reinterpret_cast<uint32*>(LockedData) = 0;

		RHIUnlockBuffer(EditorSelectedDataBufferRHI);

		EditorSelectedDataBufferSRV = RHICreateShaderResourceView(EditorSelectedDataBufferRHI, sizeof(uint32), PF_R32_UINT);
	}
//#endif
}

TGlobalResource<FSinglePrimitiveStructured> GIdentityPrimitiveBuffer;
TGlobalResource<FSinglePrimitiveStructured> GTilePrimitiveBuffer;

FPrimitiveSceneShaderData::FPrimitiveSceneShaderData(const FPrimitiveSceneProxy* RESTRICT Proxy)
	: Data(InPlace, NoInit)
{
	bool bHasPrecomputedVolumetricLightmap;
	FMatrix PreviousLocalToWorld;
	int32 SingleCaptureIndex;
	bool bOutputVelocity;

	Proxy->GetScene().GetPrimitiveUniformShaderParameters_RenderThread(
		Proxy->GetPrimitiveSceneInfo(),
		bHasPrecomputedVolumetricLightmap,
		PreviousLocalToWorld,
		SingleCaptureIndex,
		bOutputVelocity
	);

	bOutputVelocity |= Proxy->AlwaysHasVelocity();

	FBoxSphereBounds PreSkinnedLocalBounds;
	Proxy->GetPreSkinnedLocalBounds(PreSkinnedLocalBounds);

	FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy->GetPrimitiveSceneInfo();

	uint32 NaniteResourceID = INDEX_NONE;
	uint32 NaniteHierarchyOffset = INDEX_NONE;
	uint32 NaniteImposterIndex = INDEX_NONE;
	uint32 NaniteFilterFlags = 0u;
	uint32 NaniteRayTracingDataOffset = INDEX_NONE;
	bool bReverseCulling = false;

	if (Proxy->IsNaniteMesh())
	{
		auto NaniteProxy = static_cast<const Nanite::FSceneProxyBase*>(Proxy);
		NaniteProxy->GetNaniteResourceInfo(NaniteResourceID, NaniteHierarchyOffset, NaniteImposterIndex);
		NaniteFilterFlags = uint32(NaniteProxy->GetFilterFlags());
		NaniteRayTracingDataOffset = NaniteProxy->GetRayTracingDataOffset();
		bReverseCulling = NaniteProxy->IsCullingReversedByComponent(); // needed because Nanite doesn't use raster state
	}

	FPrimitiveUniformShaderParametersBuilder Builder = FPrimitiveUniformShaderParametersBuilder{}
		.Defaults()
		.LocalToWorld(Proxy->GetLocalToWorld())
		.PreviousLocalToWorld(PreviousLocalToWorld)
		.ActorWorldPosition(Proxy->GetActorPosition())
		.WorldBounds(Proxy->GetBounds())
		.LocalBounds(Proxy->GetLocalBounds())
		.PreSkinnedLocalBounds(PreSkinnedLocalBounds)
		.CustomPrimitiveData(Proxy->GetCustomPrimitiveData())
		.LightingChannelMask(Proxy->GetLightingChannelMask())
		.LightmapDataIndex(Proxy->GetPrimitiveSceneInfo()->GetLightmapDataOffset())
		.LightmapUVIndex(Proxy->GetLightMapCoordinateIndex())
		.SingleCaptureIndex(SingleCaptureIndex)
		.PersistentPrimitiveIndex(Proxy->GetPrimitiveSceneInfo()->GetPersistentIndex().Index)
		.InstanceSceneDataOffset(Proxy->GetPrimitiveSceneInfo()->GetInstanceSceneDataOffset())
		.NumInstanceSceneDataEntries(Proxy->GetPrimitiveSceneInfo()->GetNumInstanceSceneDataEntries())
		.InstancePayloadDataOffset(Proxy->GetPrimitiveSceneInfo()->GetInstancePayloadDataOffset())
		.InstancePayloadDataStride(Proxy->GetPrimitiveSceneInfo()->GetInstancePayloadDataStride())
		.HasDistanceFieldRepresentation(Proxy->HasDistanceFieldRepresentation())
		.HasCapsuleRepresentation(Proxy->HasDynamicIndirectShadowCasterRepresentation())
		.UseSingleSampleShadowFromStationaryLights(Proxy->UseSingleSampleShadowFromStationaryLights())
		.ReceivesDecals(Proxy->ReceivesDecals())
		.CacheShadowAsStatic(PrimitiveSceneInfo->ShouldCacheShadowAsStatic())
		.OutputVelocity(bOutputVelocity)
		.EvaluateWorldPositionOffset(Proxy->EvaluateWorldPositionOffset() && Proxy->AnyMaterialHasWorldPositionOffset())
		.MaxWorldPositionOffsetDisplacement(Proxy->GetMaxWorldPositionOffsetDisplacement())
		.CastContactShadow(Proxy->CastsContactShadow())
		.CastShadow(Proxy->CastsDynamicShadow())
		.CastHiddenShadow(Proxy->CastsHiddenShadow())
		.VisibleInGame(Proxy->IsDrawnInGame())
		.VisibleInEditor(Proxy->IsDrawnInEditor())
		.VisibleInReflectionCaptures(Proxy->IsVisibleInReflectionCaptures())
		.VisibleInRealTimeSkyCaptures(Proxy->IsVisibleInRealTimeSkyCaptures())
		.VisibleInRayTracing(Proxy->IsVisibleInRayTracing())
		.VisibleInLumenScene(Proxy->IsVisibleInLumenScene())
		.VisibleInSceneCaptureOnly(Proxy->IsVisibleInSceneCaptureOnly())
		.HiddenInSceneCapture(Proxy->IsHiddenInSceneCapture())
		.ForceHidden(Proxy->IsForceHidden())
		.Holdout(Proxy->Holdout())
		.UseVolumetricLightmap(bHasPrecomputedVolumetricLightmap)
		.NaniteResourceID(NaniteResourceID)
		.NaniteHierarchyOffset(NaniteHierarchyOffset)
		.NaniteImposterIndex(NaniteImposterIndex)
		.NaniteFilterFlags(NaniteFilterFlags)
		.NaniteRayTracingDataOffset(NaniteRayTracingDataOffset)
		.PrimitiveComponentId(Proxy->GetPrimitiveComponentId().PrimIDValue)
		.EditorColors(Proxy->GetWireframeColor(), Proxy->GetLevelColor())
		.ReverseCulling(bReverseCulling);

	FVector2f InstanceDrawDistanceMinMax;
	if (Proxy->GetInstanceDrawDistanceMinMax(InstanceDrawDistanceMinMax))
	{
		Builder.InstanceDrawDistance(InstanceDrawDistanceMinMax);
	}

	float WPODisableDistance;
	if (Proxy->GetInstanceWorldPositionOffsetDisableDistance(WPODisableDistance))
	{
		Builder.InstanceWorldPositionOffsetDisableDistance(WPODisableDistance);
	}

	const TConstArrayView<FRenderBounds> InstanceBounds = Proxy->GetInstanceLocalBounds();
	if (InstanceBounds.Num() > 0)
	{
		Builder.InstanceLocalBounds(InstanceBounds[0]);
	}

	if (Proxy->ShouldRenderCustomDepth())
	{
		Builder.CustomDepthStencil(Proxy->GetCustomDepthStencilValue(), Proxy->GetStencilWriteMask());
	}

	Setup(Builder.Build());
}

void FPrimitiveSceneShaderData::Setup(const FPrimitiveUniformShaderParameters& PrimitiveUniformShaderParameters)
{
	static_assert(NUM_LIGHTING_CHANNELS == 3, "The FPrimitiveSceneShaderData packing currently assumes a maximum of 3 lighting channels.");

	// Note: layout must match GetPrimitiveData in usf

	// Set W directly in order to bypass NaN check, when passing int through FVector to shader.

	Data[0].X	= FMath::AsFloat(PrimitiveUniformShaderParameters.Flags);
	Data[0].Y	= FMath::AsFloat(PrimitiveUniformShaderParameters.InstanceSceneDataOffset);
	Data[0].Z	= FMath::AsFloat(PrimitiveUniformShaderParameters.NumInstanceSceneDataEntries);
	Data[0].W	= FMath::AsFloat((uint32)PrimitiveUniformShaderParameters.SingleCaptureIndex);

	Data[1].X	= PrimitiveUniformShaderParameters.TilePosition.X;
	Data[1].Y	= PrimitiveUniformShaderParameters.TilePosition.Y;
	Data[1].Z	= PrimitiveUniformShaderParameters.TilePosition.Z;
	Data[1].W	= FMath::AsFloat(PrimitiveUniformShaderParameters.PrimitiveComponentId);

	// Pack these matrices into the buffer as float3x4 transposed

	FMatrix44f LocalToRelativeWorldTranspose = PrimitiveUniformShaderParameters.LocalToRelativeWorld.GetTransposed();
	Data[2]		= *(const FVector4f*)&LocalToRelativeWorldTranspose.M[0][0];
	Data[3]		= *(const FVector4f*)&LocalToRelativeWorldTranspose.M[1][0];
	Data[4]		= *(const FVector4f*)&LocalToRelativeWorldTranspose.M[2][0];

	FMatrix44f RelativeWorldToLocalTranspose = PrimitiveUniformShaderParameters.RelativeWorldToLocal.GetTransposed();
	Data[5]		= *(const FVector4f*)&RelativeWorldToLocalTranspose.M[0][0];
	Data[6]		= *(const FVector4f*)&RelativeWorldToLocalTranspose.M[1][0];
	Data[7]		= *(const FVector4f*)&RelativeWorldToLocalTranspose.M[2][0];

	FMatrix44f PreviousLocalToRelativeWorldTranspose = PrimitiveUniformShaderParameters.PreviousLocalToRelativeWorld.GetTransposed();
	Data[8]		= *(const FVector4f*)&PreviousLocalToRelativeWorldTranspose.M[0][0];
	Data[9]		= *(const FVector4f*)&PreviousLocalToRelativeWorldTranspose.M[1][0];
	Data[10]	= *(const FVector4f*)&PreviousLocalToRelativeWorldTranspose.M[2][0];

	FMatrix44f PreviousRelativeWorldToLocalTranspose = PrimitiveUniformShaderParameters.PreviousRelativeWorldToLocal.GetTransposed();
	Data[11]	= *(const FVector4f*)&PreviousRelativeWorldToLocalTranspose.M[0][0];
	Data[12]	= *(const FVector4f*)&PreviousRelativeWorldToLocalTranspose.M[1][0];
	Data[13]	= *(const FVector4f*)&PreviousRelativeWorldToLocalTranspose.M[2][0];

	FMatrix44f WorldToPreviousWorldTranspose = PrimitiveUniformShaderParameters.WorldToPreviousWorld.GetTransposed();
	Data[14]	= *(const FVector4f*)&WorldToPreviousWorldTranspose.M[0][0];
	Data[15]	= *(const FVector4f*)&WorldToPreviousWorldTranspose.M[1][0];
	Data[16]	= *(const FVector4f*)&WorldToPreviousWorldTranspose.M[2][0];

	Data[17]	= FVector4f(PrimitiveUniformShaderParameters.InvNonUniformScale, PrimitiveUniformShaderParameters.ObjectBoundsX);
	Data[18]	= PrimitiveUniformShaderParameters.ObjectRelativeWorldPositionAndRadius;

	Data[19]	= FVector4f(PrimitiveUniformShaderParameters.ActorRelativeWorldPosition, 0.0f);
	Data[19].W	= FMath::AsFloat(PrimitiveUniformShaderParameters.LightmapUVIndex);

	Data[20]	= FVector4f(PrimitiveUniformShaderParameters.ObjectOrientation, 0.0f);
	Data[20].W	= FMath::AsFloat(PrimitiveUniformShaderParameters.LightmapDataIndex);

	Data[21]	= PrimitiveUniformShaderParameters.NonUniformScale;

	Data[22]	= FVector4f(PrimitiveUniformShaderParameters.PreSkinnedLocalBoundsMin, 0.0f);
	Data[22].W	= FMath::AsFloat(PrimitiveUniformShaderParameters.NaniteResourceID);

	Data[23]	= FVector4f(PrimitiveUniformShaderParameters.PreSkinnedLocalBoundsMax, 0.0f);
	Data[23].W	= FMath::AsFloat(PrimitiveUniformShaderParameters.NaniteHierarchyOffset);

	Data[24]	= FVector4f(PrimitiveUniformShaderParameters.LocalObjectBoundsMin, PrimitiveUniformShaderParameters.ObjectBoundsY);
	Data[25]	= FVector4f(PrimitiveUniformShaderParameters.LocalObjectBoundsMax, PrimitiveUniformShaderParameters.ObjectBoundsZ);

	Data[26].X = PrimitiveUniformShaderParameters.InstanceLocalBoundsCenter.X;
	Data[26].Y = PrimitiveUniformShaderParameters.InstanceLocalBoundsCenter.Y;
	Data[26].Z = PrimitiveUniformShaderParameters.InstanceLocalBoundsCenter.Z;
	Data[26].W = FMath::AsFloat(PrimitiveUniformShaderParameters.InstancePayloadDataOffset);

	Data[27].X = PrimitiveUniformShaderParameters.InstanceLocalBoundsExtent.X;
	Data[27].Y = PrimitiveUniformShaderParameters.InstanceLocalBoundsExtent.Y;
	Data[27].Z = PrimitiveUniformShaderParameters.InstanceLocalBoundsExtent.Z;
	Data[27].W = FMath::AsFloat(PrimitiveUniformShaderParameters.InstancePayloadDataStride);
	
	Data[28].X = PrimitiveUniformShaderParameters.WireframeColor.X;
	Data[28].Y = PrimitiveUniformShaderParameters.WireframeColor.Y;
	Data[28].Z = PrimitiveUniformShaderParameters.WireframeColor.Z;
	Data[28].W = FMath::AsFloat(PrimitiveUniformShaderParameters.PackedNaniteFlags);

	Data[29].X = PrimitiveUniformShaderParameters.LevelColor.X;
	Data[29].Y = PrimitiveUniformShaderParameters.LevelColor.Y;
	Data[29].Z = PrimitiveUniformShaderParameters.LevelColor.Z;
	Data[29].W = FMath::AsFloat(uint32(PrimitiveUniformShaderParameters.PersistentPrimitiveIndex));

	Data[30].X = PrimitiveUniformShaderParameters.InstanceDrawDistanceMinMaxSquared.X;
	Data[30].Y = PrimitiveUniformShaderParameters.InstanceDrawDistanceMinMaxSquared.Y;
	Data[30].Z = PrimitiveUniformShaderParameters.InstanceWPODisableDistanceSquared;
	Data[30].W = FMath::AsFloat(PrimitiveUniformShaderParameters.NaniteRayTracingDataOffset);

	Data[31].X = PrimitiveUniformShaderParameters.MaxWPODisplacement;
	Data[31].Y = FMath::AsFloat(PrimitiveUniformShaderParameters.CustomStencilValueAndMask);
	// .ZW Unused

	// Set all the custom primitive data float4. This matches the loop in SceneData.ush
	const int32 CustomPrimitiveDataStartIndex = 32;
	for (int32 DataIndex = 0; DataIndex < FCustomPrimitiveData::NumCustomPrimitiveDataFloat4s; ++DataIndex)
	{
		Data[CustomPrimitiveDataStartIndex + DataIndex] = PrimitiveUniformShaderParameters.CustomPrimitiveData[DataIndex];
	}
}
