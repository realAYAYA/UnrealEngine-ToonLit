// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrimitiveUniformShaderParameters.h"
#include "PrimitiveSceneInfo.h"
#include "NaniteSceneProxy.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "SceneInterface.h"
#include "UnrealEngine.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "PrimitiveSceneShaderData.h"

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

void FSinglePrimitiveStructured::InitRHI(FRHICommandListBase& RHICmdList)
{
	SCOPED_LOADTIMER(FSinglePrimitiveStructuredBuffer_InitRHI);

	FRHIResourceCreateInfo CreateInfo(TEXT("PrimitiveSceneDataBuffer"));

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	{	
		PrimitiveSceneDataBufferRHI = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), FPrimitiveSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
		PrimitiveSceneDataBufferSRV = RHICmdList.CreateShaderResourceView(PrimitiveSceneDataBufferRHI);
	}

	{
		const static FLazyName ClassName(TEXT("FSinglePrimitiveStructured"));
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("PrimitiveSceneDataTexture"), FPrimitiveSceneShaderData::DataStrideInFloat4s, 1, PF_A32B32G32R32F)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV)
			.SetClassName(ClassName);

		PrimitiveSceneDataTextureRHI = RHICreateTexture(Desc);
		PrimitiveSceneDataTextureSRV = RHICmdList.CreateShaderResourceView(PrimitiveSceneDataTextureRHI, 0);
	}

	CreateInfo.DebugName = TEXT("LightmapSceneDataBuffer");
	LightmapSceneDataBufferRHI = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), FLightmapSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
	LightmapSceneDataBufferSRV = RHICmdList.CreateShaderResourceView(LightmapSceneDataBufferRHI);

	CreateInfo.DebugName = TEXT("InstanceSceneDataBuffer");
	InstanceSceneDataBufferRHI = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), FInstanceSceneShaderData::GetDataStrideInFloat4s() * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
	InstanceSceneDataBufferSRV = RHICmdList.CreateShaderResourceView(InstanceSceneDataBufferRHI);

	CreateInfo.DebugName = TEXT("InstancePayloadDataBuffer");
	InstancePayloadDataBufferRHI = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), 1 /* unused dummy */ * sizeof(FVector4f), BUF_Static | BUF_ShaderResource, CreateInfo);
	InstancePayloadDataBufferSRV = RHICmdList.CreateShaderResourceView(InstancePayloadDataBufferRHI);

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	CreateInfo.DebugName = TEXT("SkyIrradianceEnvironmentMap");
	SkyIrradianceEnvironmentMapRHI = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), sizeof(FVector4f) * 8, BUF_Static | BUF_ShaderResource, CreateInfo);
	SkyIrradianceEnvironmentMapSRV = RHICmdList.CreateShaderResourceView(SkyIrradianceEnvironmentMapRHI);

	UploadToGPU(RHICmdList);
}

void FSinglePrimitiveStructured::UploadToGPU(FRHICommandListBase& RHICmdList)
{
	void* LockedData = nullptr;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	LockedData = RHICmdList.LockBuffer(PrimitiveSceneDataBufferRHI, 0, FPrimitiveSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memcpy(LockedData, PrimitiveSceneData.Data.GetData(), FPrimitiveSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f));
	RHICmdList.UnlockBuffer(PrimitiveSceneDataBufferRHI);

	LockedData = RHICmdList.LockBuffer(LightmapSceneDataBufferRHI, 0, FLightmapSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memcpy(LockedData, LightmapSceneData.Data.GetData(), FLightmapSceneShaderData::DataStrideInFloat4s * sizeof(FVector4f));
	RHICmdList.UnlockBuffer(LightmapSceneDataBufferRHI);

	LockedData = RHICmdList.LockBuffer(InstanceSceneDataBufferRHI, 0, FInstanceSceneShaderData::GetDataStrideInFloat4s() * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memcpy(LockedData, InstanceSceneData.Data.GetData(), FInstanceSceneShaderData::GetDataStrideInFloat4s() * sizeof(FVector4f));
	RHICmdList.UnlockBuffer(InstanceSceneDataBufferRHI);

	LockedData = RHICmdList.LockBuffer(InstancePayloadDataBufferRHI, 0, 1 /* unused dummy */ * sizeof(FVector4f), RLM_WriteOnly);
	FPlatformMemory::Memset(LockedData, 0x00, sizeof(FVector4f));
	RHICmdList.UnlockBuffer(InstancePayloadDataBufferRHI);

//#if WITH_EDITOR
	if (IsFeatureLevelSupported(GMaxRHIShaderPlatform, ERHIFeatureLevel::SM5))
	{
		// Create level instance SRV
		FRHIResourceCreateInfo LevelInstanceBufferCreateInfo(TEXT("EditorVisualizeLevelInstanceDataBuffer"));
		EditorVisualizeLevelInstanceDataBufferRHI = RHICmdList.CreateVertexBuffer(sizeof(uint32), BUF_Static | BUF_ShaderResource, LevelInstanceBufferCreateInfo);

		LockedData = RHICmdList.LockBuffer(EditorVisualizeLevelInstanceDataBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);

		*reinterpret_cast<uint32*>(LockedData) = 0;

		RHICmdList.UnlockBuffer(EditorVisualizeLevelInstanceDataBufferRHI);

		EditorVisualizeLevelInstanceDataBufferSRV = RHICmdList.CreateShaderResourceView(EditorVisualizeLevelInstanceDataBufferRHI, sizeof(uint32), PF_R32_UINT);

		// Create selection outline SRV
		FRHIResourceCreateInfo SelectionBufferCreateInfo(TEXT("EditorSelectedDataBuffer"));
		EditorSelectedDataBufferRHI = RHICmdList.CreateVertexBuffer(sizeof(uint32), BUF_Static | BUF_ShaderResource, SelectionBufferCreateInfo);

		LockedData = RHICmdList.LockBuffer(EditorSelectedDataBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);

		*reinterpret_cast<uint32*>(LockedData) = 0;

		RHICmdList.UnlockBuffer(EditorSelectedDataBufferRHI);

		EditorSelectedDataBufferSRV = RHICmdList.CreateShaderResourceView(EditorSelectedDataBufferRHI, sizeof(uint32), PF_R32_UINT);
	}
//#endif

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

TGlobalResource<FSinglePrimitiveStructured> GIdentityPrimitiveBuffer;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
TGlobalResource<FSinglePrimitiveStructured> GTilePrimitiveBuffer;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FPrimitiveSceneShaderData::FPrimitiveSceneShaderData(const FPrimitiveSceneProxy* RESTRICT Proxy)
	: Data(InPlace, NoInit)
{
	FPrimitiveUniformShaderParametersBuilder Builder = FPrimitiveUniformShaderParametersBuilder{};
	Proxy->BuildUniformShaderParameters(Builder);
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
	Data[0].W	= FMath::AsFloat((uint32)PrimitiveUniformShaderParameters.SingleCaptureIndex |
								 ((PrimitiveUniformShaderParameters.VisibilityFlags & 0xFFFFu) << 16u));

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
	Data[27].W = FMath::AsFloat((PrimitiveUniformShaderParameters.InstancePayloadDataStride & 0x00FFFFFFu) |
								(PrimitiveUniformShaderParameters.InstancePayloadExtensionSize << 24u));
	
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

	Data[31].X = PrimitiveUniformShaderParameters.MaxWPOExtent;
	Data[31].Y = PrimitiveUniformShaderParameters.MinMaterialDisplacement;
	Data[31].Z = PrimitiveUniformShaderParameters.MaxMaterialDisplacement;
	Data[31].W = FMath::AsFloat(PrimitiveUniformShaderParameters.CustomStencilValueAndMask);

	// Set all the custom primitive data float4. This matches the loop in SceneData.ush
	const int32 CustomPrimitiveDataStartIndex = 32;
	for (int32 DataIndex = 0; DataIndex < FCustomPrimitiveData::NumCustomPrimitiveDataFloat4s; ++DataIndex)
	{
		Data[CustomPrimitiveDataStartIndex + DataIndex] = PrimitiveUniformShaderParameters.CustomPrimitiveData[DataIndex];
	}
}

TUniformBufferRef<FPrimitiveUniformShaderParameters> CreatePrimitiveUniformBufferImmediate(
	const FMatrix& LocalToWorld,
	const FBoxSphereBounds& WorldBounds,
	const FBoxSphereBounds& LocalBounds,
	const FBoxSphereBounds& PreSkinnedLocalBounds,
	bool bReceivesDecals,
	bool bOutputVelocity
)
{
	check(IsInRenderingThread());
	return TUniformBufferRef<FPrimitiveUniformShaderParameters>::CreateUniformBufferImmediate(
		FPrimitiveUniformShaderParametersBuilder{}
		.Defaults()
			.LocalToWorld(LocalToWorld)
			.ActorWorldPosition(WorldBounds.Origin)
			.WorldBounds(WorldBounds)
			.LocalBounds(LocalBounds)
			.PreSkinnedLocalBounds(PreSkinnedLocalBounds)
			.ReceivesDecals(bReceivesDecals)
			.OutputVelocity(bOutputVelocity)
		.Build(),
		UniformBuffer_MultiFrame
	);
}

FPrimitiveUniformShaderParameters GetIdentityPrimitiveParameters()
{
	// Don't use FMatrix44f::Identity here as GetIdentityPrimitiveParameters is used by TGlobalResource<FIdentityPrimitiveUniformBuffer> and because
	// static initialization order is undefined, FMatrix44f::Identity might be all 0's or random data the first time this is called.
	return FPrimitiveUniformShaderParametersBuilder{}
		.Defaults()
			.LocalToWorld(FMatrix(FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 1)))
			.ActorWorldPosition(FVector(0.0, 0.0, 0.0))
			.WorldBounds(FBoxSphereBounds(EForceInit::ForceInit))
			.LocalBounds(FBoxSphereBounds(EForceInit::ForceInit))
		.Build();
}