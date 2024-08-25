// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserGenericDenoiser.h"
#include "Algo/Transform.h"
#include "NNEDenoiserHistory.h"
#include "NNEDenoiserIOProcess.h"
#include "NNEDenoiserLog.h"
#include "NNEDenoiserModelInstance.h"
#include "NNEDenoiserResourceManager.h"
#include "NNEDenoiserTransferFunction.h"
#include "NNEDenoiserUtils.h"
#include <type_traits>

namespace UE::NNEDenoiser::Private
{

class FResourceAccess : public IResourceAccess
{
public:
	FResourceAccess(const FResourceManager& ResourceManager) : ResourceManager(ResourceManager) { }

	virtual FRDGTextureRef GetTexture(EResourceName TensorName, int32 FrameIdx) const override
	{
		return ResourceManager.GetTexture(TensorName, FrameIdx);
	}

	virtual FRDGTextureRef GetIntermediateTexture(EResourceName TensorName, int32 FrameIdx) const override
	{
		return ResourceManager.GetIntermediateTexture(TensorName, FrameIdx);
	}

private:
	const FResourceManager& ResourceManager;
};

FRDGBufferRef CreateBufferRDG(FRDGBuilder& GraphBuilder, const NNE::FTensorDesc& TensorDesc, const NNE::FTensorShape& TensorShape)
{
	const uint32 BytesPerElement = TensorDesc.GetElementByteSize();
	const uint32 NumElements = TensorShape.Volume();

	return GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NumElements), *TensorDesc.GetName());
}

TArray<FRDGBufferRef> CreateBuffersRDG(FRDGBuilder& GraphBuilder, TConstArrayView<NNE::FTensorDesc> TensorDescs, TConstArrayView<NNE::FTensorShape> TensorShapes)
{
	check(TensorDescs.Num() == TensorShapes.Num());

	TArray<FRDGBufferRef> Result;
	Result.SetNumUninitialized(TensorDescs.Num());

	for (int32 I = 0; I < TensorDescs.Num(); I++)
	{
		Result[I] = CreateBufferRDG(GraphBuilder, TensorDescs[I], TensorShapes[I]);
	}
	return Result;
}

TArray<NNE::FTensorBindingRDG> GetBindingRDG(TConstArrayView<FRDGBufferRef> Buffers)
{
	TArray<NNE::FTensorBindingRDG> Result;
	Result.Reserve(Buffers.Num());
	
	Algo::Transform(Buffers, Result, [] (FRDGBuffer* Buffer) { return NNE::FTensorBindingRDG{Buffer}; });

	return Result;
}

void RegisterTextureIfNeeded(FResourceManager& ResourceManager, FRDGTextureRef Texture, EResourceName ResourceName, const IInputProcess& InputProcess)
{
	const int32 MinNumFrames = ResourceName == EResourceName::Output ? 1 : 0;
	const int32 NumFrames = FMath::Max(InputProcess.NumFrames(ResourceName), MinNumFrames);
	if (NumFrames <= 0)
	{
		return;
	}
	
	ResourceManager.AddTexture(ResourceName, Texture, NumFrames);
}

bool PrepareAndValidate(IModelInstance& ModelInstance, const IInputProcess& InputProcess, const IOutputProcess& OutputProcess, FIntPoint Extent)
{
	bool Result = InputProcess.PrepareAndValidate(ModelInstance, Extent);
	Result &= OutputProcess.Validate(ModelInstance, Extent);

	return Result;
}

void AddTilePasses(FRDGBuilder& GraphBuilder, IModelInstance& ModelInstance, const IInputProcess& InputProcess, const IOutputProcess& OutputProcess,
		FResourceManager& ResourceManager, TConstArrayView<FRDGBufferRef> InputBuffers, TConstArrayView<FRDGBufferRef> OutputBuffers)
{
	FResourceAccess ResourceAccess(ResourceManager);

	// 1. Read input and write to input buffers
	InputProcess.AddPasses(GraphBuilder, ModelInstance.GetInputTensorDescs(), ModelInstance.GetInputTensorShapes(), ResourceAccess, InputBuffers);

	// 2. Create buffer binding and infer the model
	NNE::IModelInstanceRDG::EEnqueueRDGStatus Status = ModelInstance.EnqueueRDG(GraphBuilder, GetBindingRDG(InputBuffers), GetBindingRDG(OutputBuffers));
	checkf(Status == NNE::IModelInstanceRDG::EEnqueueRDGStatus::Ok, TEXT("EnqueueRDG failed: %d"), static_cast<int>(Status));

	// 3. Write output based on output buffer
	FRDGTextureRef OutputTexture = ResourceManager.GetTexture(EResourceName::Output, 0);
	OutputProcess.AddPasses(GraphBuilder, ModelInstance.GetOutputTensorDescs(), ModelInstance.GetOutputTensorShapes(), ResourceAccess, OutputBuffers, OutputTexture);
}

FGenericDenoiser::FGenericDenoiser(TUniquePtr<IModelInstance> ModelInstance, TUniquePtr<IInputProcess> InputProcess, TUniquePtr<IOutputProcess> OutputProcess, FParameters DenoiserParameters) :
	ModelInstance(MoveTemp(ModelInstance)), InputProcess(MoveTemp(InputProcess)), OutputProcess(MoveTemp(OutputProcess)), DenoiserParameters(DenoiserParameters)
{

}

FGenericDenoiser::~FGenericDenoiser()
{

}

TUniquePtr<FHistory> FGenericDenoiser::AddPasses(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef ColorTex,
	FRDGTextureRef AlbedoTex,
	FRDGTextureRef NormalTex,
	FRDGTextureRef OutputTex,
	FRDGTextureRef FlowTex,
	const FRHIGPUMask& GPUMask,
	FHistory* History)
{
	using namespace UE::NNEDenoiserShaders::Internal;

	SCOPED_NAMED_EVENT_TEXT("NNEDenoiser.AddPasses", FColor::Magenta);

	// note: FlowTex is currently optional and can be null
	check(ColorTex != FRDGTextureRef{});
	check(AlbedoTex != FRDGTextureRef{});
	check(NormalTex != FRDGTextureRef{});
	check(OutputTex != FRDGTextureRef{});

	const FIntPoint Extent = ColorTex->Desc.Extent;

	TUniquePtr<FHistory> Result;

	if (Extent == LastExtent || PrepareAndValidate(*ModelInstance, *InputProcess, *OutputProcess, Extent))
	{
		LastExtent = Extent;

		const TConstArrayView<uint32> InputShapeData = ModelInstance->GetInputTensorShapes()[0].GetData();
		const FIntPoint ModelInputSize{(int32)InputShapeData[3], (int32)InputShapeData[2]};

		const FTiling Tiling = CreateTiling(ModelInputSize, DenoiserParameters.TileMinimumOverlap, Extent);

		TMap<EResourceName, TArray<TRefCountPtr<IPooledRenderTarget>>> ResourceMap;
		if (History)
		{
			ResourceMap = History->GetResourceMap();
		}

		FResourceManager ResourceManager(GraphBuilder, Tiling, ResourceMap);
		RegisterTextureIfNeeded(ResourceManager, ColorTex, EResourceName::Color, *InputProcess);
		RegisterTextureIfNeeded(ResourceManager, AlbedoTex, EResourceName::Albedo, *InputProcess);
		RegisterTextureIfNeeded(ResourceManager, NormalTex, EResourceName::Normal, *InputProcess);
		if (FlowTex != FRDGTextureRef{})
		{
			RegisterTextureIfNeeded(ResourceManager, FlowTex, EResourceName::Flow, *InputProcess);
		}
		RegisterTextureIfNeeded(ResourceManager, OutputTex, EResourceName::Output, *InputProcess);

		TArray<FRDGBufferRef> InputBuffers = CreateBuffersRDG(GraphBuilder, ModelInstance->GetInputTensorDescs(), ModelInstance->GetInputTensorShapes());
		TArray<FRDGBufferRef> OutputBuffers = CreateBuffersRDG(GraphBuilder, ModelInstance->GetOutputTensorDescs(), ModelInstance->GetOutputTensorShapes());

		UE_LOG(LogNNEDenoiser, Log, TEXT("Divided work into %dx%d tiles of size %dx%d each..."), Tiling.Count.X, Tiling.Count.Y, Tiling.TileSize.X, Tiling.TileSize.Y);

		for (int32 I = 0; I < Tiling.Tiles.Num(); I++)
		{
			ResourceManager.BeginTile(I);

			// Do inference on tile
			AddTilePasses(GraphBuilder, *ModelInstance, *InputProcess, *OutputProcess, ResourceManager, InputBuffers, OutputBuffers);

			ResourceManager.EndTile();
		}

		ResourceMap = ResourceManager.MakeHistoryResourceMap();
		if (!ResourceMap.IsEmpty())
		{
			Result = MakeUnique<FHistory>(*DebugName, MoveTemp(ResourceMap));
		}
	}
	else
	{
		// If something went wrong, just copy color to output...
		AddCopyTexturePass(GraphBuilder, ColorTex, OutputTex, FRHICopyTextureInfo{});
	}

	return Result;
}

} // namespace UE::NNEDenoiser::Private