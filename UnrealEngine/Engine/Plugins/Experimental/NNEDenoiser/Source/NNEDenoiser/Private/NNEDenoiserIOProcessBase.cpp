// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserIOProcessBase.h"
#include "NNEDenoiserLog.h"
#include "NNEDenoiserModelInstance.h"
#include "NNEDenoiserParameters.h"
#include "NNEDenoiserShadersDefaultCS.h"
#include "NNEDenoiserUtils.h"
#include "RHIStaticStates.h"

DECLARE_GPU_STAT_NAMED(FNNEDenoiserReadInput, TEXT("NNEDenoiser.ReadInput"));
DECLARE_GPU_STAT_NAMED(FNNEDenoiserWriteOutput, TEXT("NNEDenoiser.WriteOutput"));

namespace UE::NNEDenoiser::Private
{

namespace IOProcessBaseHelper
{

void AddReadInputPass(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputTexture,
	FRDGBufferUAVRef InputBufferUAV,
	FIntPoint InputBufferSize,
	UE::NNEDenoiserShaders::Internal::ENNEDenoiserDataType DataType,
	const TArray<FIntPoint>& ChannelMapping)
{
	using namespace UE::NNEDenoiserShaders::Internal;

	if (ChannelMapping.IsEmpty())
	{
		UE_LOG(LogNNEDenoiser, Warning, TEXT("AddReadInputPass: ChannelMapping is empty. Nothing to do!"));
		return;
	}
	
	if (ChannelMapping.Num() >= FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS)
	{
		UE_LOG(LogNNEDenoiser, Warning, TEXT("AddReadInputPass: ChannelMapping has too many entries!"));
		return;
	}

	const FIntVector InputTextureSize = InputTexture->Desc.GetSize();

	FNNEDenoiserReadInputCS::FParameters *ReadInputParameters = GraphBuilder.AllocParameters<FNNEDenoiserReadInputCS::FParameters>();
	ReadInputParameters->InputTextureWidth = InputTextureSize.X;
	ReadInputParameters->InputTextureHeight = InputTextureSize.Y;
	ReadInputParameters->InputTexture = InputTexture;
	ReadInputParameters->InputBufferWidth = InputBufferSize.X;
	ReadInputParameters->InputBufferHeight = InputBufferSize.Y;
	ReadInputParameters->InputBuffer = InputBufferUAV;
	for (int32 Idx = 0; Idx < ChannelMapping.Num(); Idx++)
	{
		ReadInputParameters->BufferChannel_TextureChannel_Unused_Unused[Idx] = { ChannelMapping[Idx].X, ChannelMapping[Idx].Y, 0, 0 };
	}

	FNNEDenoiserReadInputCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FNNEDenoiserReadInputCS::FNNEDenoiserDataType>(DataType);
	PermutationVector.Set<FNNEDenoiserReadInputCS::FNNEDenoiserNumMappedChannels>(ChannelMapping.Num());

	FIntVector ReadInputThreadGroupCount = FIntVector(
		FMath::DivideAndRoundUp(InputBufferSize.X, FNNEDenoiserConstants::THREAD_GROUP_SIZE),
		FMath::DivideAndRoundUp(InputBufferSize.Y, FNNEDenoiserConstants::THREAD_GROUP_SIZE),
		1);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FNNEDenoiserReadInputCS> ReadInputShader(GlobalShaderMap, PermutationVector);

	RDG_EVENT_SCOPE(GraphBuilder, "NNEDenoiser.ReadInput");
	RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEDenoiserReadInput);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("NNEDenoiser.ReadInput"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ReadInputShader,
		ReadInputParameters,
		ReadInputThreadGroupCount);
}

void AddWriteOutputPass(
	FRDGBuilder& GraphBuilder,
	FRDGBufferUAVRef OutputBufferUAV,
	FIntPoint OutputBufferSize,
	FRDGTextureUAVRef OutputTextureUAV,
	UE::NNEDenoiserShaders::Internal::ENNEDenoiserDataType DataType,
	const TArray<FIntPoint>& ChannelMapping)
{
	using namespace UE::NNEDenoiserShaders::Internal;

	const FIntVector OutputTextureSize = OutputTextureUAV->Desc.Texture->Desc.GetSize();

	FNNEDenoiserWriteOutputCS::FParameters *WriteOutputParameters = GraphBuilder.AllocParameters<FNNEDenoiserWriteOutputCS::FParameters>();
	WriteOutputParameters->OutputBufferWidth = OutputBufferSize.X;
	WriteOutputParameters->OutputBufferHeight = OutputBufferSize.Y;
	WriteOutputParameters->OutputBuffer = OutputBufferUAV;
	WriteOutputParameters->OutputTextureWidth = OutputTextureSize.X;
	WriteOutputParameters->OutputTextureHeight = OutputTextureSize.Y;
	WriteOutputParameters->OutputTexture = OutputTextureUAV;
	for (int32 Idx = 0; Idx < ChannelMapping.Num(); Idx++)
	{
		WriteOutputParameters->BufferChannel_TextureChannel_Unused_Unused[Idx] = { ChannelMapping[Idx].X, ChannelMapping[Idx].Y, 0, 0 };
	}

	FNNEDenoiserWriteOutputCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FNNEDenoiserWriteOutputCS::FNNEDenoiserDataType>(DataType);
	PermutationVector.Set<FNNEDenoiserWriteOutputCS::FNNEDenoiserNumMappedChannels>(ChannelMapping.Num());

	FIntVector WriteOutputThreadGroupCount = FIntVector(
		FMath::DivideAndRoundUp(OutputTextureSize.X, FNNEDenoiserConstants::THREAD_GROUP_SIZE),
		FMath::DivideAndRoundUp(OutputTextureSize.Y, FNNEDenoiserConstants::THREAD_GROUP_SIZE),
		1);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FNNEDenoiserWriteOutputCS> WriteOutputShader(GlobalShaderMap, PermutationVector);

	RDG_EVENT_SCOPE(GraphBuilder, "NNEDenoiser.WriteOutput");
	RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEDenoiserWriteOutput);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("NNEDenoiser.WriteOutput"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		WriteOutputShader,
		WriteOutputParameters,
		WriteOutputThreadGroupCount);
}

void AddReadInputPassForKind(
	FRDGBuilder& GraphBuilder,
	const IResourceAccess& ResourceAccess,
	FIntPoint BufferSize,
	NNEDenoiserShaders::Internal::ENNEDenoiserDataType DataType,
	EResourceName TensorName,
	const FResourceMapping& ResourceMapping,
	FRDGBufferUAVRef BufferUAV)
{
	for (const TPair<int32, TArray<FIntPoint>>& ChannelMappingKeyValue : ResourceMapping.GetChannelMappingPerFrame(TensorName))
	{
		const int32 FrameIdx = -ChannelMappingKeyValue.Key;
		FRDGTextureRef InputTexture = ResourceAccess.GetTexture(TensorName, FrameIdx);

		AddReadInputPass(GraphBuilder, InputTexture, BufferUAV, BufferSize, DataType, ChannelMappingKeyValue.Value);
	}
};

} // namespace IOProcessBaseHelper

bool FInputProcessBase::PrepareAndValidate(IModelInstance& ModelInstance, FIntPoint Extent) const
{
	SCOPED_NAMED_EVENT_TEXT("NNEDenoiser.Prepare", FColor::Magenta);

	const int32 NumBatches = 1;

	UE_LOG(LogNNEDenoiser, Log, TEXT("Configure model for extent %dx%d..."), Extent.X, Extent.Y);

	TConstArrayView<UE::NNE::FTensorDesc> InputTensorDescs = ModelInstance.GetInputTensorDescs();
	if (InputTensorDescs.Num() != InputLayout.Num())
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Wrong number of inputs (expected %d, got %d)!"), InputLayout.Num(), InputTensorDescs.Num())
		return false;
	}

	UE_LOG(LogNNEDenoiser, Log, TEXT("Input shapes (set):"));

	TArray<UE::NNE::FTensorShape> InputShapes;
	for (int32 Idx = 0; Idx < InputTensorDescs.Num(); Idx++)
	{
		TConstArrayView<int32> InputSymbolicTensorShapeData = InputTensorDescs[Idx].GetShape().GetData();
		const TArray<int32, TInlineAllocator<4>> RequiredInputShapeData = { NumBatches, InputLayout.NumChannels(Idx), -1, -1 };

		if (!IsTensorShapeValid(InputSymbolicTensorShapeData, RequiredInputShapeData, TEXT("Input")))
		{
			return false;
		}

		const int32 ModelInputWidth = InputSymbolicTensorShapeData[3] >= 0 ? InputSymbolicTensorShapeData[3] : Extent.X;
		const int32 ModelInputHeight = InputSymbolicTensorShapeData[2] >= 0 ? InputSymbolicTensorShapeData[2] : Extent.Y;

		if (Extent.X < ModelInputWidth || Extent.Y < ModelInputHeight)
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("Input image too small (model expects at least size %dx%d, got %dx%d!"), ModelInputWidth, ModelInputHeight, Extent.X, Extent.Y);
			return false;
		}

		const FIntVector4 ModelInputShape = { 1, InputSymbolicTensorShapeData[1], ModelInputHeight, ModelInputWidth };

		InputShapes.Add(UE::NNE::FTensorShape::Make({ (uint32)ModelInputShape.X, (uint32)ModelInputShape.Y, (uint32)ModelInputShape.Z, (uint32)ModelInputShape.W }));

		UE_LOG(LogNNEDenoiser, Log, TEXT("%d: (%d, %d, %d, %d)"), Idx, ModelInputShape.X, ModelInputShape.Y, ModelInputShape.Z, ModelInputShape.W)
	}

	const IModelInstance::ESetInputTensorShapesStatus Status = ModelInstance.SetInputTensorShapes(InputShapes);
	if (Status != IModelInstance::ESetInputTensorShapesStatus::Ok)
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Could not configure model instance (ModelInstance.SetInputTensorShapes() failed)!"))
		return false;
	}

	return true;
}

int32 FInputProcessBase::NumFrames(EResourceName Name) const
{
	return InputLayout.NumFrames(Name);
}

void FInputProcessBase::AddPasses(
		FRDGBuilder& GraphBuilder,
		TConstArrayView<UE::NNE::FTensorDesc> TensorDescs,
		TConstArrayView<UE::NNE::FTensorShape> TensorShapes,
		const IResourceAccess& ResourceAccess,
		TConstArrayView<FRDGBufferRef> OutputBuffers) const
{
	const UEnum* ResourceNameEnum = StaticEnum<EResourceName>();
	for (int32 I = 0; I < ResourceNameEnum->NumEnums(); I++)
	{
		const EResourceName TensorName = (EResourceName)ResourceNameEnum->GetValueByIndex(I);
		const int32 NumFrames = InputLayout.NumFrames(TensorName);

		for (int32 J = 0; J < NumFrames; J++)
		{
			if (!HasPreprocessInput(TensorName, J))
			{
				continue;
			}

			FRDGTextureRef InputTexture = ResourceAccess.GetTexture(TensorName, J);
			FRDGTextureRef PreprocessedInputTexture = ResourceAccess.GetIntermediateTexture(TensorName, J);

			PreprocessInput(GraphBuilder, InputTexture, TensorName, J, PreprocessedInputTexture);
		}
	}

	for (int32 I = 0; I < TensorDescs.Num(); I++)
	{
		WriteInputBuffer(GraphBuilder, TensorDescs[I], TensorShapes[I], ResourceAccess, InputLayout.GetChecked(I), OutputBuffers[I]);
	}
}

void FInputProcessBase::WriteInputBuffer(
	FRDGBuilder& GraphBuilder,
	const UE::NNE::FTensorDesc& TensorDesc,
	const UE::NNE::FTensorShape& TensorShape,
	const IResourceAccess& ResourceAccess,
	const FResourceMapping& ResourceMapping,
	FRDGBufferRef Buffer) const
{
	const ENNETensorDataType TensorDataType = TensorDesc.GetDataType();
	const NNEDenoiserShaders::Internal::ENNEDenoiserDataType DataType = GetDenoiserShaderDataType(TensorDataType);
	const FIntPoint BufferSize(TensorShape.GetData()[3], TensorShape.GetData()[2]);

	FRDGBufferUAVRef BufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Buffer, GetBufferFormat(TensorDataType)));

	IOProcessBaseHelper::AddReadInputPassForKind(GraphBuilder, ResourceAccess, BufferSize, DataType, EResourceName::Color, ResourceMapping, BufferUAV);
	IOProcessBaseHelper::AddReadInputPassForKind(GraphBuilder, ResourceAccess, BufferSize, DataType, EResourceName::Albedo, ResourceMapping, BufferUAV);
	IOProcessBaseHelper::AddReadInputPassForKind(GraphBuilder, ResourceAccess, BufferSize, DataType, EResourceName::Normal, ResourceMapping, BufferUAV);
	IOProcessBaseHelper::AddReadInputPassForKind(GraphBuilder, ResourceAccess, BufferSize, DataType, EResourceName::Flow, ResourceMapping, BufferUAV);
	IOProcessBaseHelper::AddReadInputPassForKind(GraphBuilder, ResourceAccess, BufferSize, DataType, EResourceName::Output, ResourceMapping, BufferUAV);
}

bool FOutputProcessBase::Validate(const IModelInstance& ModelInstance, FIntPoint Extent) const
{
	SCOPED_NAMED_EVENT_TEXT("NNEDenoiser.Prepare", FColor::Magenta);

	const int32 NumBatches = 1;

	TConstArrayView<UE::NNE::FTensorDesc> OutputTensorDescs = ModelInstance.GetOutputTensorDescs();
	if (OutputTensorDescs.Num() != OutputLayout.Num())
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Wrong number of outputs (expected %d, got %d)!"), OutputLayout.Num(), OutputTensorDescs.Num())
		return true;
	}

	for (int32 Idx = 0; Idx < OutputTensorDescs.Num(); Idx++)
	{
		TConstArrayView<int32> OutputSymbolicShapeData = OutputTensorDescs[Idx].GetShape().GetData();
		const TArray<int32, TInlineAllocator<4>> RequiredOutputShapeData = { NumBatches, OutputLayout.NumChannels(Idx), -1, -1 };

		if (!IsTensorShapeValid(OutputSymbolicShapeData, RequiredOutputShapeData, TEXT("Output")))
		{
			return false;
		}
	}

	TConstArrayView<UE::NNE::FTensorShape> OutputShapes = ModelInstance.GetOutputTensorShapes();
	if (OutputShapes.Num() != OutputLayout.Num())
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Wrong number of output shapes or not resolved yet!"));
		return true;
	}
	
	UE_LOG(LogNNEDenoiser, Log, TEXT("Output shapes (resolved):"));

	for (int32 Idx = 0; Idx < OutputTensorDescs.Num(); Idx++)
	{
		TConstArrayView<uint32> OutputShapeData = OutputShapes[Idx].GetData();
		const TArray<int32, TInlineAllocator<4>> RequiredOutputShapeData = { NumBatches, OutputLayout.NumChannels(Idx), -1, -1 };

		if (!IsTensorShapeValid(OutputShapeData, RequiredOutputShapeData, TEXT("Output")))
		{
			return false;
		}
	
		UE_LOG(LogNNEDenoiser, Log, TEXT("%d: (%d, %d, %d, %d)"), Idx, OutputShapeData[0], OutputShapeData[1], OutputShapeData[2], OutputShapeData[3]);
	}

	return true;
}

void FOutputProcessBase::AddPasses(
	FRDGBuilder& GraphBuilder,
	TConstArrayView<UE::NNE::FTensorDesc> TensorDescs,
	TConstArrayView<UE::NNE::FTensorShape> TensorShapes,
	const IResourceAccess& ResourceAccess,
	TConstArrayView<FRDGBufferRef> Buffers,
	FRDGTextureRef OutputTexture) const
{
	for (int32 I = 0; I < TensorDescs.Num(); I++)
	{
		ReadOutputBuffer(GraphBuilder, TensorDescs[I], TensorShapes[I], ResourceAccess, Buffers[I], OutputLayout.GetChecked(I), OutputTexture);
	}

	const EResourceName TensorName = EResourceName::Output;
	const int32 FrameIdx = 0;

	if (HasPostprocessOutput(TensorName, FrameIdx))
	{
		FRDGTextureRef PostprocessInputTexture = ResourceAccess.GetTexture(EResourceName::Output, FrameIdx);
		FRDGTextureRef PostprocessOutputTexture = ResourceAccess.GetIntermediateTexture(EResourceName::Output, FrameIdx);
		
		PostprocessOutput(GraphBuilder, PostprocessInputTexture, PostprocessOutputTexture);
	}
}

void FOutputProcessBase::ReadOutputBuffer(
	FRDGBuilder& GraphBuilder,
	const UE::NNE::FTensorDesc& TensorDesc,
	const UE::NNE::FTensorShape& TensorShape,
	const IResourceAccess& ResourceAccess,
	FRDGBufferRef Buffer,
	const FResourceMapping& ResourceMapping,
	FRDGTextureRef Texture) const
{
	const ENNETensorDataType TensorDataType = TensorDesc.GetDataType();
	const NNEDenoiserShaders::Internal::ENNEDenoiserDataType DataType = GetDenoiserShaderDataType(TensorDataType);
	const FIntPoint BufferSize(TensorShape.GetData()[3], TensorShape.GetData()[2]);
		
	TMap<int32, TArray<FIntPoint>> ChannelMappingPerFrame = ResourceMapping.GetChannelMappingPerFrame(EResourceName::Output);
	checkf(ChannelMappingPerFrame.Num() == 1, TEXT("Invalid output mapping"));
	const TArray<FIntPoint>& ChannelMapping = ChannelMappingPerFrame.CreateConstIterator().Value();

	FRDGBufferUAVRef BufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Buffer, GetBufferFormat(TensorDataType)));
	FRDGTextureUAVRef TextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Texture));

	IOProcessBaseHelper::AddWriteOutputPass(GraphBuilder, BufferUAV, BufferSize, TextureUAV, DataType, ChannelMapping);
}

} // namespace UE::NNEDenoiser::Private