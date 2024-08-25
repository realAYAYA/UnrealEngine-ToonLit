// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenHeightfields.h"
#include "RendererPrivate.h"
#include "ComponentRecreateRenderStateContext.h"

TAutoConsoleVariable<int32> CVarLumenSceneHeightfieldTracing(
	TEXT("r.LumenScene.Heightfield.Tracing"),
	1,
	TEXT("Enables heightfield (Landscape) software ray tracing (default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSceneHeightfieldMaxTracingSteps(
	TEXT("r.LumenScene.Heightfield.MaxTracingSteps"),
	32,
	TEXT("Sets the maximum steps for heightfield (Landscape) software ray tracing (default = 32)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenSceneHeightfieldReceiverBias(
	TEXT("r.LumenScene.Heightfield.ReceiverBias"),
	0.01f,
	TEXT("Extra bias for Landscape surface points. Helps to fix mismatching LOD artifacts between fixed LOD in Surface Cache and Landscape CLOD."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

bool Lumen::UseHeightfieldTracingForVoxelLighting(const FLumenSceneData& LumenSceneData)
{
	bool bHeightfieldEnabled = CVarLumenSceneHeightfieldTracing.GetValueOnRenderThread() != 0;
	bool bHasHeightfields = LumenSceneData.Heightfields.Num() > 0;
	return bHeightfieldEnabled && bHasHeightfields;
}

bool Lumen::UseHeightfieldTracing(const FSceneViewFamily& ViewFamily, const FLumenSceneData& LumenSceneData)
{
	return UseHeightfieldTracingForVoxelLighting(LumenSceneData)
		&& Lumen::UseMeshSDFTracing(ViewFamily)
		&& ViewFamily.EngineShowFlags.LumenDetailTraces;
}

int32 Lumen::GetHeightfieldMaxTracingSteps()
{
	return FMath::Clamp(CVarLumenSceneHeightfieldMaxTracingSteps.GetValueOnRenderThread(), 1, 256);
}

float Lumen::GetHeightfieldReceiverBias()
{
	return FMath::Clamp(CVarLumenSceneHeightfieldReceiverBias.GetValueOnRenderThread(), 0.001, 100.0);
}

void FLumenHeightfieldGPUData::FillData(const FLumenHeightfield& RESTRICT Heightfield, const TSparseSpanArray<FLumenMeshCards>& MeshCards, FVector4f* RESTRICT OutData)
{
	// Note: layout must match GetLumenHeightfieldData in usf

	FDFVector3 BoundsCenter{};
	FVector3f BoundsExtent = FVector3f::ZeroVector;
	uint32 MeshCardsIndex = UINT32_MAX;

	if (Heightfield.MeshCardsIndex >= 0)
	{
		MeshCardsIndex = Heightfield.MeshCardsIndex;
		const FBox WorldSpaceBounds = MeshCards[Heightfield.MeshCardsIndex].GetWorldSpaceBounds();
		BoundsCenter = FDFVector3(WorldSpaceBounds.GetCenter());
		BoundsExtent = (FVector3f)WorldSpaceBounds.GetExtent();
	}

	OutData[0] = BoundsCenter.High;
	OutData[1] = BoundsCenter.Low;
	OutData[2] = BoundsExtent;
	OutData[0].W = *((float*)&MeshCardsIndex);

	static_assert(DataStrideInFloat4s == 3, "Data stride doesn't match");
}