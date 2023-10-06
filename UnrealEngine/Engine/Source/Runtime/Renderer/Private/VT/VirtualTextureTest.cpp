// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureTest.h"

#if 0
#include "SpriteIndexBuffer.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "RenderTargetPool.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "SceneUtils.h"

// TEMP will have reference from VT typedef
#include "TexturePagePool.h"
#include "VirtualTextureSpace.h"
#include "VirtualTextureSystem.h"


TGlobalResource< FVirtualTextureTestType > GVirtualTextureTestType;



FVirtualTextureTestType::FVirtualTextureTestType()
{
	PhysicalTextureFormat = PF_B8G8R8A8;
	

	Pool = new FTexturePagePool( 64 * 64, 2 );
	PhysicalTextureSize = FIntPoint(64 * pPageSize, 64 * pPageSize);

#if CPU_TEST == 0	
	Space = new FVirtualTextureSpace( 1024, 2, PF_R16_UINT, Pool );
	BeginInitResource(Space);

	VT = new FVirtualTextureTest(1024, 1024, 1);
	Space->Allocator.Alloc(VT);
#endif
	
	
}

FVirtualTextureTestType::~FVirtualTextureTestType()
{
#if CPU_TEST == 0	
	ReleaseResourceAndFlush( Space );
	delete Space;
#endif
	delete Pool;
}

void FVirtualTextureTestType::InitRHI(FRHICommandListBase&)
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FPooledRenderTargetDesc Desc( FPooledRenderTargetDesc::Create2DDesc( PhysicalTextureSize, PhysicalTextureFormat, FClearValueBinding::None, TexCreate_None, TexCreate_SRGB | TexCreate_RenderTargetable | TexCreate_ShaderResource, false ) );
	GRenderTargetPool.FindFreeElement( RHICmdList, Desc, PhysicalTexture, TEXT("PhysicalTexture") );
}

void FVirtualTextureTestType::ReleaseRHI()
{
	GRenderTargetPool.FreeUnusedResource( PhysicalTexture );
}




FRHITexture* FVirtualTextureTestType::GetPageTableTexture() const
{
#if CPU_TEST
// 	FVirtualTextureSpace* FSpace = (FVirtualTextureSpace*)TEMP_GetVirtualTextureSpaceResource();
// 	if (FSpace)
// 	{
// 		return FSpace->GetPageTableTexture();
// 	}
// 	else
// 	{
 		return GBlackTexture->TextureRHI; //todo black is probably not a valid 'dummy' pagetable format
// 	}
#else
	return Space->GetPageTableTexture();
#endif
}

FVirtualTextureTest::FVirtualTextureTest( uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ )
	: IVirtualTexture( InSizeX, InSizeY, InSizeY )
{}

FVirtualTextureTest::~FVirtualTextureTest()
{}

bool FVirtualTextureTest::RequestPageData(FRHICommandList& RHICmdList, uint8 vLevel, uint64 vAddress, void* RESTRICT& Location ) /*const*/
{
	/*
	const size_t VTHeaderSize = 0;

#if 0
	// Look up offset and size from offset table
	// Only required for variable bit-rate compression
	size_t Offsets[2] = { 0 };
	size_t DataRead = FileCache->Read( Offsets, VT->FileName, VTHeaderSize + VTPageIndex * sizeof( size_t ), sizeof( Offsets ), priority );
		
	size_t PageSize = Offsets[1] - Offsets[0];
	size_t PageOffset = Offsets[0];
#else
	size_t PageSize = 16;
	size_t PageOffset = VTHeaderSize + VTPageIndex * PageSize;
#endif

	// Need to know if the actual data is there
	size_t DataRead = FileCache->Read( Data, VT->FileName, PageOffset, PageSize, priority );
	*/

	return true;
}

class FVirtualTextureTestPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVirtualTextureTestPS);

	static bool ShouldCache( EShaderPlatform Platform )
	{
		return IsFeatureLevelSupported( Platform, ERHIFeatureLevel::SM5 );
	}

	FVirtualTextureTestPS() {}

public:
	FVirtualTextureTestPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
};

IMPLEMENT_SHADER_TYPE(, FVirtualTextureTestPS, TEXT("/Engine/Private/VTtest.usf"), TEXT("VirtualTextureTestPS"), SF_Pixel );


void FVirtualTextureTest::ProducePageData( FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint8 vLevel, uint64 vAddress, uint16 pAddress, void* Location ) /*const*/
{
	/*
#if 0
	// Copy data from file cache to ByteAddressBuffer for decompression on GPU
	uint8* Data = (uint8*)CompressedBuffer + CompressedBufferOffset;
	size_t DataRead = FileCache->Read( Data, VT->FileName, PageOffset, PageSize, priority );
	check( DataRead == PageSize );
	// TODO Dispatch decompress CS
#else
	// Copy data from file cache to staging texture
	uint8* Data = nullptr;
	size_t DataRead = FileCache->Read( Data, VT->FileName, PageOffset, PageSize, priority );
	check( DataRead == PageSize );
	// TODO Transfer CPU staging texture to GPU
#endif
	*/

	// TODO These should come from VT typedef
	const uint32 vPageSize = 128;
	const uint32 pPageBorder = 4;
	const uint32 pPageSize = vPageSize + 2 * pPageBorder;

	const uint32 vPageX = FMath::ReverseMortonCode2( vAddress );
	const uint32 vPageY = FMath::ReverseMortonCode2( vAddress >> 1 );

	const uint32 pPageX = FMath::ReverseMortonCode2( pAddress );
	const uint32 pPageY = FMath::ReverseMortonCode2( pAddress >> 1 );

	FIntRect SrcRect(
		( vPageX + (0 << vLevel) ) * vPageSize - ( pPageBorder << vLevel ),
		( vPageY + (0 << vLevel) ) * vPageSize - ( pPageBorder << vLevel ),
		( vPageX + (1 << vLevel) ) * vPageSize + ( pPageBorder << vLevel ),
		( vPageY + (1 << vLevel) ) * vPageSize + ( pPageBorder << vLevel )
	);

	FIntRect DstRect(
		( pPageX + 0 ) * pPageSize,
		( pPageY + 0 ) * pPageSize,
		( pPageX + 1 ) * pPageSize,
		( pPageY + 1 ) * pPageSize
	);

	FIntPoint SrcSize( 1024 * vPageSize, 1024 * vPageSize );
	FIntPoint DstSize( 64 * pPageSize, 64 * pPageSize );

	TRefCountPtr< IPooledRenderTarget >	PhysicalTexture = GVirtualTextureTestType.PhysicalTexture;

	SCOPED_DRAW_EVENT( RHICmdList, TestProducePageData );

	auto ShaderMap = GetGlobalShaderMap( FeatureLevel );

	FRHITexture* RenderTarget = PhysicalTexture->GetRHI();

	FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Load_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("ProducePageData"));
	{
		RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0.0f, DstRect.Max.X, DstRect.Max.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		TShaderMapRef< FScreenVS >				VertexShader(ShaderMap);
		TShaderMapRef< FVirtualTextureTestPS >	PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		DrawRectangle(
			RHICmdList,
			0, 0,
			DstRect.Width(), DstRect.Height(),
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Width(), SrcRect.Height(),
			DstRect.Size(),
			SrcSize,
			VertexShader,
			EDRF_UseTriangleOptimization);
	}
	RHICmdList.EndRenderPass();
	RHICmdList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::RTV, ERHIAccess::SRVMask));

	GVisualizeTexture.SetCheckPoint( RHICmdList, PhysicalTexture );
}

void FVirtualTextureTest::DumpToConsole(bool verbose)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Test Virtual Texture"));
}

#endif
