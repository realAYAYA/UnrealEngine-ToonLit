// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "LandscapeComponent.h"
#include "MeshMaterialShader.h"
#include "LandscapeAsyncTextureReadback.h"

class ALandscapeProxy;
class FLandscapeComponentSceneProxy;
class FTextureRenderTarget2DResource;
class UTextureRenderTarget2D;

// data also accessible by render thread
class FLandscapeGrassWeightExporter_RenderThread
{
	FLandscapeGrassWeightExporter_RenderThread(const TArray<int32>& InHeightMips)
		: HeightMips(InHeightMips)
	{
		// even when doing a synchronous readback, we use the async readback structure
		AsyncReadbackPtr = new FLandscapeAsyncTextureReadback();
	}

	friend class FLandscapeGrassWeightExporter;

public:
	virtual ~FLandscapeGrassWeightExporter_RenderThread()
	{
		if (AsyncReadbackPtr != nullptr)
		{
			AsyncReadbackPtr->QueueDeletionFromGameThread();
			AsyncReadbackPtr = nullptr;
		}
	}

	struct FComponentInfo
	{
		TObjectPtr<ULandscapeComponent> Component = nullptr;
		TArray<TObjectPtr<ULandscapeGrassType>> RequestedGrassTypes;
		FVector2D ViewOffset = FVector2D::ZeroVector;
		int32 PixelOffsetX = 0;
		FLandscapeComponentSceneProxy* SceneProxy = nullptr;
		int32 NumPasses = 0;
		int32 FirstHeightMipsPassIndex = MAX_int32;

		FComponentInfo(ULandscapeComponent* InComponent, bool bInNeedsGrassmap, bool bInNeedsHeightmap, const TArray<int32>& InHeightMips)
			: Component(InComponent)
			, SceneProxy((FLandscapeComponentSceneProxy*)InComponent->SceneProxy)
		{
			if (bInNeedsGrassmap)
			{
				RequestedGrassTypes = InComponent->GetGrassTypes();
			}
			int32 NumGrassMaps = RequestedGrassTypes.Num();
			if (bInNeedsHeightmap || NumGrassMaps > 0)
			{
				NumPasses += FMath::DivideAndRoundUp(2 /* heightmap */ + NumGrassMaps, 4);
			}
			if (InHeightMips.Num() > 0)
			{
				FirstHeightMipsPassIndex = NumPasses;
				NumPasses += InHeightMips.Num();
			}
		}
	};

	FSceneInterface* SceneInterface = nullptr;
	TArray<FComponentInfo, TInlineAllocator<1>> ComponentInfos;
	FIntPoint TargetSize;
	TArray<int32> HeightMips;
	float PassOffsetX;
	FVector ViewOrigin;
	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;

	FLandscapeAsyncTextureReadback* AsyncReadbackPtr = nullptr;

	void RenderLandscapeComponentToTexture_RenderThread(FRHICommandListImmediate& RHICmdList);
};

class FLandscapeGrassWeightExporter : public FLandscapeGrassWeightExporter_RenderThread
{
	TObjectPtr<ALandscapeProxy> LandscapeProxy;
	int32 ComponentSizeVerts;
	int32 SubsectionSizeQuads;
	int32 NumSubsections;
	TArray<TObjectPtr<ULandscapeGrassType>> GrassTypes;

public:
	FLandscapeGrassWeightExporter(ALandscapeProxy* InLandscapeProxy, TArrayView<ULandscapeComponent* const> InLandscapeComponents, bool bInNeedsGrassmap = true, bool bInNeedsHeightmap = true, const TArray<int32>& InHeightMips = {});

	// If using the async readback path, check its status and update if needed. Return true when the AsyncReadbackResults are available.
	// You must call this periodically, or the async readback may not complete.
	// bInForceFinish will force the RenderThread to wait until GPU completes the readback, ensuring the readback is completed after the render thread executes the command.
	// NOTE: you may still see false returned, this just means the render thread hasn't executed the command yet.
	bool CheckAndUpdateAsyncReadback(bool& bOutRenderCommandsQueued, const bool bInForceFinish = false)
	{
		check(AsyncReadbackPtr != nullptr);
		return AsyncReadbackPtr->CheckAndUpdate(bOutRenderCommandsQueued, bInForceFinish);
	}

	// return true if the async readback is complete.  (Does not update the readback state)
	bool IsAsyncReadbackComplete()
	{
		check(AsyncReadbackPtr != nullptr);
		return AsyncReadbackPtr->IsComplete();
	}

	// Fetches the results from the GPU texture and translates them into FLandscapeComponentGrassDatas.
	// If using async readback, requires AsyncReadback to be complete before calling this.
	// bFreeAsyncReadback if true will call FreeAsyncReadback() to free the readback resource (otherwise you must do it manually)
	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> FetchResults(bool bFreeAsyncReadback);

	void FreeAsyncReadback();

	// Applies the results using pre-fetched data.
	static void ApplyResults(TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>>& Results);

	// Fetches the results and applies them to the landscape components
	// If using async readback, requires AsyncReadback to be complete before calling this.
	void ApplyResults();

	void CancelAndSelfDestruct();
};

namespace UE::Landscape::Grass
{
	void AddGrassWeightShaderTypes(FMaterialShaderTypes& InOutShaderTypes);
}

