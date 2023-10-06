// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIDrawTests.h"
#include "RHIBufferTests.h" // for VerifyBufferContents
#include "CommonRenderResources.h"
#include "RenderCaptureInterface.h"
#include "RHIStaticStates.h"

class FTestDrawInstancedVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTestDrawInstancedVS);
	FTestDrawInstancedVS() = default;
	FTestDrawInstancedVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
};
IMPLEMENT_GLOBAL_SHADER(FTestDrawInstancedVS, "/Plugin/RHITests/Private/TestDrawInstanced.usf", "TestDrawInstancedMainVS", SF_Vertex);

class FTestDrawInstancedPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTestDrawInstancedPS);
	FTestDrawInstancedPS() = default;
	FTestDrawInstancedPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		OutDrawnInstances.Bind(Initializer.ParameterMap, TEXT("OutDrawnInstances"), SPF_Mandatory);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
	LAYOUT_FIELD(FShaderParameter, OutDrawnInstances);
};
IMPLEMENT_GLOBAL_SHADER(FTestDrawInstancedPS, "/Plugin/RHITests/Private/TestDrawInstanced.usf", "TestDrawInstancedMainPS", SF_Pixel);

template <typename T>
static FBufferRHIRef CreateBufferWithData(EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, const TCHAR* Name, TConstArrayView<T> Data)
{
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
	uint32 BufferSize = sizeof(T) * Data.Num();
	FRHIResourceCreateInfo CreateInfo(Name);
	FBufferRHIRef Buffer = RHICmdList.CreateBuffer(BufferSize, UsageFlags, sizeof(T), ResourceState, CreateInfo);
	void* MappedData = RHICmdList.LockBuffer(Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly);
	FMemory::Memcpy(MappedData, Data.GetData(), BufferSize);
	RHICmdList.UnlockBuffer(Buffer);
	return Buffer;
}

bool FRHIDrawTests::Test_MultiDrawIndirect(FRHICommandListImmediate& RHICmdList)
{
	if (!GRHIGlobals.SupportsMultiDrawIndirect)
	{
		return true;
	}

	// Probably could/should automatically enable in the outer scope when running RHI Unit Tests
	// RenderCaptureInterface::FScopedCapture RenderCapture(true /*bEnable*/, &RHICmdList, TEXT("Test_MultiDrawIndirect"));

	static constexpr uint32 MaxInstances = 8;

	// D3D12 does not have a way to get the base instance ID (SV_InstanceID always starts from 0), so we must emulate it...
	const uint32 InstanceIDs[MaxInstances] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	FBufferRHIRef InstanceIDBuffer = CreateBufferWithData(EBufferUsageFlags::VertexBuffer, ERHIAccess::VertexOrIndexBuffer, TEXT("Test_MultiDrawIndirect_InstanceID"), MakeArrayView(InstanceIDs));

	FVertexDeclarationElementList VertexDeclarationElements;
	VertexDeclarationElements.Add(FVertexElement(0, 0, VET_UInt, 0, 4, true /*per instance frequency*/));
	FVertexDeclarationRHIRef VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(VertexDeclarationElements);

	const uint16 Indices[3] = { 0, 1, 2 };
	FBufferRHIRef IndexBuffer = CreateBufferWithData(EBufferUsageFlags::IndexBuffer, ERHIAccess::VertexOrIndexBuffer, TEXT("Test_MultiDrawIndirect_IndexBuffer"), MakeArrayView(Indices));

	static constexpr uint32 OutputBufferStride = sizeof(uint32);
	static constexpr uint32 OutputBufferSize = OutputBufferStride * MaxInstances;
	FRHIResourceCreateInfo OutputBufferCreateInfo(TEXT("Test_MultiDrawIndirect_OutputBuffer"));
	FBufferRHIRef OutputBuffer = RHICmdList.CreateBuffer(OutputBufferSize, EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::SourceCopy, OutputBufferStride, ERHIAccess::UAVCompute, OutputBufferCreateInfo);

	const uint32 CountValues[4] = { 1, 1, 16, 0 };
	FBufferRHIRef CountBuffer = CreateBufferWithData(EBufferUsageFlags::DrawIndirect | EBufferUsageFlags::UnorderedAccess, ERHIAccess::IndirectArgs, TEXT("Test_MultiDrawIndirect_Count"), MakeArrayView(CountValues));

	const FRHIDrawIndexedIndirectParameters DrawArgs[] =
	{
		// IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation
		{3, 1, 0, 0, 0}, // fill slot 0
		// gap in slot 1
		{3, 2, 0, 0, 2}, // fill slots 2, 3 using 1 sub-draw
		// gap in slot 4
		{3, 1, 0, 0, 5}, // fill slots 5, 6 using 2 sub-draws
		{3, 1, 0, 0, 6},
		{3, 1, 0, 0, 7}, // this draw is expected to never execute
	};

	const uint32 ExpectedDrawnInstances[MaxInstances] = { 1, 0, 1, 1, 0, 1, 1, 0 };

	FBufferRHIRef DrawArgBuffer = CreateBufferWithData(EBufferUsageFlags::DrawIndirect | EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::VertexBuffer, ERHIAccess::IndirectArgs,
		TEXT("Test_MultiDrawIndirect_DrawArgs"), MakeArrayView(DrawArgs));

	FUnorderedAccessViewRHIRef OutputBufferUAV = RHICmdList.CreateUnorderedAccessView(OutputBuffer, 
		FRHIViewDesc::CreateBufferUAV()
		.SetType(FRHIViewDesc::EBufferType::Typed)
		.SetFormat(PF_R32_UINT));

	RHICmdList.ClearUAVUint(OutputBufferUAV, FUintVector4(0));

	const FIntPoint RenderTargetSize(4, 4);
	FRHITextureDesc RenderTargetTextureDesc(ETextureDimension::Texture2D, ETextureCreateFlags::RenderTargetable, PF_B8G8R8A8, FClearValueBinding(), RenderTargetSize, 1, 1, 1, 1, 0);
	FRHITextureCreateDesc RenderTargetCreateDesc(RenderTargetTextureDesc, ERHIAccess::RTV, TEXT("Test_MultiDrawIndirect_RenderTarget"));
	FTextureRHIRef RenderTarget = RHICreateTexture(RenderTargetCreateDesc);

	TShaderMapRef<FTestDrawInstancedVS> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<FTestDrawInstancedPS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.PrimitiveType = EPrimitiveType::PT_TriangleList;

	FRHITexture* ColorRTs[1] = { RenderTarget.GetReference() };
	FRHIRenderPassInfo RenderPassInfo(1, ColorRTs, ERenderTargetActions::DontLoad_DontStore);

	RHICmdList.Transition(FRHITransitionInfo(OutputBufferUAV, ERHIAccess::UAVCompute, ERHIAccess::UAVGraphics, EResourceTransitionFlags::None));
	RHICmdList.BeginUAVOverlap(); // Output UAV can be written without syncs between draws (each draw is expected to write into different slots)

	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("Test_MultiDrawIndirect"));
	RHICmdList.SetViewport(0, 0, 0, float(RenderTargetSize.X), float(RenderTargetSize.Y), 1);

	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	check(InstanceIDBuffer->GetStride() == 4);
	RHICmdList.SetStreamSource(0, InstanceIDBuffer, 0);

	FRHIBatchedShaderParameters ShaderParameters;
	ShaderParameters.SetUAVParameter(PixelShader->OutDrawnInstances.GetBaseIndex(), OutputBufferUAV);
	RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), ShaderParameters);

	const uint32 DrawArgsStride = sizeof(DrawArgs[0]);
	const uint32 CountStride = sizeof(CountValues[0]);

	RHICmdList.MultiDrawIndexedPrimitiveIndirect(IndexBuffer, 
		DrawArgBuffer, DrawArgsStride*0, // 1 sub-draw with instance index 0
		CountBuffer, CountStride*0, // count buffer contains 1 in this slot
		5 // expect to draw only 1 instance due to GPU-side upper bound
	);

	RHICmdList.MultiDrawIndexedPrimitiveIndirect(IndexBuffer,
		DrawArgBuffer, DrawArgsStride*1, // 1 sub-draw with 2 instances at base index 2
		CountBuffer, CountStride*1, // count buffer contains 1 in this slot
		4 // expect to draw only 1 instance due to GPU-side upper bound
	);

	RHICmdList.MultiDrawIndexedPrimitiveIndirect(IndexBuffer,
		DrawArgBuffer, DrawArgsStride*2, // 2 sub-draws with 1 instance each starting at base index 5
		CountBuffer, CountStride*2, // count buffer contains 16 in this slot
		2 // expect to draw only 2 instances due to CPU-side upper bound
	);

	RHICmdList.MultiDrawIndexedPrimitiveIndirect(IndexBuffer,
		DrawArgBuffer, DrawArgsStride*4, // 1 sub-draw with 1 instance each starting at base index 7
		CountBuffer, CountStride*3, // count buffer contains 0 in this slot
		1 // expect to skip the draw due to GPU-side count of 0
	);

	RHICmdList.MultiDrawIndexedPrimitiveIndirect(IndexBuffer,
		DrawArgBuffer, DrawArgsStride*4, // 1 sub-draw with 1 instance each starting at base index 7
		CountBuffer, CountStride*0, // count buffer contains 1 in this slot
		0 // expect to skip the draw due to CPU-side count of 0
	);

	RHICmdList.EndRenderPass();

	RHICmdList.EndUAVOverlap();

	RHICmdList.Transition(FRHITransitionInfo(OutputBufferUAV, ERHIAccess::UAVGraphics, ERHIAccess::CopySrc, EResourceTransitionFlags::None));

	TConstArrayView<uint8> ExpectedOutputView = MakeArrayView(reinterpret_cast<const uint8*>(ExpectedDrawnInstances), sizeof(ExpectedDrawnInstances));
	bool bSucceeded = FRHIBufferTests::VerifyBufferContents(TEXT("Test_MultiDrawIndirect"), RHICmdList, OutputBuffer, ExpectedOutputView);

	return bSucceeded;
}
