// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"
#include "RHIGPUReadback.h"

class FScene;
class FViewInfo;
struct FLumenSceneFrameTemporaries;

class FLumenSceneReadback : public FRenderResource
{
public:
	FLumenSceneReadback();
	~FLumenSceneReadback();

	struct FAddOp
	{
		uint32 PrimitiveGroupIndex;
		float DistanceSq;
	};

	struct FRemoveOp
	{
		uint32 PrimitiveGroupIndex;
	};

	struct FBuffersRHI
	{
		FRHIGPUBufferReadback* AddOps = nullptr;
		FRHIGPUBufferReadback* RemoveOps = nullptr;
	};

	struct FBuffersRDG
	{
		FRDGBufferRef AddOps = nullptr;
		FRDGBufferRef RemoveOps = nullptr;
	};

	FBuffersRDG GetWriteBuffers(FRDGBuilder& GraphBuilder);
	void SubmitWriteBuffers(FRDGBuilder& GraphBuilder, FBuffersRDG BuffersRDG);

	FBuffersRHI GetLatestReadbackBuffers();

	int32 GetMaxAddOps() const { return MaxAddOps;  }
	int32 GetMaxRemoveOps() const { return MaxRemoveOps; }

	int32 GetAddOpsBufferSizeInBytes() const { return MaxAddOps * sizeof(FAddOp); }
	int32 GetRemoveOpsBufferSizeInBytes() const { return MaxRemoveOps * sizeof(FRemoveOp);  }

private:
	const int32 MaxAddOps = 1024;
	const int32 MaxRemoveOps = 1024;

	const int32 MaxReadbackBuffers = 4;
	int32 ReadbackBuffersWriteIndex = 0;
	int32 ReadbackBuffersNumPending = 0;
	TArray<FBuffersRHI> ReadbackBuffers;
};

namespace LumenScene
{
	float GetCardMaxDistance(const FViewInfo& View);
	float GetCardTexelDensity();
	float GetFarFieldCardTexelDensity();
	float GetFarFieldCardMaxDistance();
	int32 GetCardMinResolution(bool bOrthographicCamera);

	void GPUDrivenUpdate(FRDGBuilder& GraphBuilder, const FScene* Scene, TArray<FViewInfo>& Views, const FLumenSceneFrameTemporaries& FrameTemporaries);
};