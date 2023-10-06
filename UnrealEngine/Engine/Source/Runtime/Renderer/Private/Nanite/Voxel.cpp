// Copyright Epic Games, Inc. All Rights Reserved.

// Experimental voxel code

#include "Voxel.h"
#include "CoreMinimal.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "SpriteIndexBuffer.h"
#include "ClearQuad.h"
#include "DataDrivenShaderPlatformInfo.h"

static TAutoConsoleVariable<int32> CVarVoxel(
	TEXT("r.Voxel"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarVoxelMethod(
	TEXT("r.VoxelMethod"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarVoxelLevel2(
	TEXT("r.VoxelLevel2"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

namespace Nanite
{
#if !UE_BUILD_SHIPPING

TGlobalResource< FSpriteIndexBuffer<64> > GSpriteIndexBuffer;

class FVoxelShader : public FGlobalShader
{
public:
	FVoxelShader() {}
	FVoxelShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader( Initializer )
	{}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM6 );
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.CompilerFlags.Add( CFLAG_WaveOperations );
	}
};

BEGIN_SHADER_PARAMETER_STRUCT( FBuildBricksParameters, )
	SHADER_PARAMETER_STRUCT_REF( FViewUniformShaderParameters, View )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER( FSceneTextureUniformParameters, SceneTextures )

	SHADER_PARAMETER( uint32, HashTableSize )

	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,		HashTable )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< uint >,		IndexBuffer )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FBrick >,	BrickBuffer )
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,				BlockBuffer )
	SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,				DispatchIndirectArgs )
	SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,				DrawIndirectArgs )

	SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,	RWHashTable )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< uint >,	RWIndexBuffer )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FBrick >,	RWBrickBuffer )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWByteAddressBuffer,			RWBlockBuffer )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,				RWDispatchIndirectArgs )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,				RWDrawIndirectArgs )

	RDG_BUFFER_ACCESS( IndirectArgs, ERHIAccess::IndirectArgs )
END_SHADER_PARAMETER_STRUCT()

#define IMPLEMENT_GLOBAL_SHADER_STRUCT( ShaderTypeName, StructTypeName, SourceFilename, FunctionName, Frequency ) \
	class ShaderTypeName : public FVoxelShader \
	{ \
		DECLARE_GLOBAL_SHADER( ShaderTypeName ); \
		SHADER_USE_PARAMETER_STRUCT( ShaderTypeName, FVoxelShader ); \
		using FParameters = StructTypeName; \
	}; \
	IMPLEMENT_GLOBAL_SHADER( ShaderTypeName, SourceFilename, FunctionName, Frequency )

IMPLEMENT_GLOBAL_SHADER_STRUCT( FVisibleBricksHashCS,	FBuildBricksParameters, "/Engine/Private/Nanite/Voxel/AutoVoxel.usf", "VisibleBricksHash",	SF_Compute );
IMPLEMENT_GLOBAL_SHADER_STRUCT( FFillArgsCS,			FBuildBricksParameters, "/Engine/Private/Nanite/Voxel/AutoVoxel.usf", "FillArgs",			SF_Compute );
IMPLEMENT_GLOBAL_SHADER_STRUCT( FAllocBlocksCS,			FBuildBricksParameters, "/Engine/Private/Nanite/Voxel/AutoVoxel.usf", "AllocBlocks",		SF_Compute );
IMPLEMENT_GLOBAL_SHADER_STRUCT( FFillBlocksCS,			FBuildBricksParameters, "/Engine/Private/Nanite/Voxel/AutoVoxel.usf", "FillBlocks",			SF_Compute );


#if 1
BEGIN_SHADER_PARAMETER_STRUCT( FTileBricksParameters, )
	SHADER_PARAMETER_STRUCT_REF( FViewUniformShaderParameters, View )

	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FBrick >,	BrickBuffer )
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,				BlockBuffer )
	
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FTileListElement >,	TileListBuffer )
	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FTileArrayElement >,	TileArrayBuffer )
	SHADER_PARAMETER_RDG_TEXTURE( Texture2D< uint >,		TileHead )
	SHADER_PARAMETER_RDG_TEXTURE( Texture2D< uint >,		TileCount )
	SHADER_PARAMETER_RDG_TEXTURE( Texture2D< uint >,		TileOffset )
	SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,		DispatchIndirectArgs )

	SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FTileListElement >,	RWTileListBuffer )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWStructuredBuffer< FTileArrayElement >,	RWTileArrayBuffer )
	SHADER_PARAMETER_RDG_TEXTURE_UAV( RWTexture2D< uint >,	RWTileHead )
	SHADER_PARAMETER_RDG_TEXTURE_UAV( RWTexture2D< uint >,	RWTileCount )
	SHADER_PARAMETER_RDG_TEXTURE_UAV( RWTexture2D< uint >,	RWTileOffset )
	SHADER_PARAMETER_RDG_BUFFER_UAV( RWBuffer< uint >,		RWDispatchIndirectArgs )

	SHADER_PARAMETER_RDG_TEXTURE_UAV( RWTexture2D, OutSceneColor )

	RDG_BUFFER_ACCESS( IndirectArgs, ERHIAccess::IndirectArgs )
END_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_STRUCT( FBinBricksInTilesCS,	FTileBricksParameters, "/Engine/Private/Nanite/Voxel/TileBricks.usf", "BinBricksInTiles",	SF_Compute );
IMPLEMENT_GLOBAL_SHADER_STRUCT( FBuildTileArraysCS,		FTileBricksParameters, "/Engine/Private/Nanite/Voxel/TileBricks.usf", "BuildTileArrays",	SF_Compute );
IMPLEMENT_GLOBAL_SHADER_STRUCT( FRayCastTilesCS,		FTileBricksParameters, "/Engine/Private/Nanite/Voxel/TileBricks.usf", "RayCastTiles",		SF_Compute );

IMPLEMENT_GLOBAL_SHADER_STRUCT( FScatterVoxelsCS,		FTileBricksParameters, "/Engine/Private/Nanite/Voxel/ScatterBricks.usf", "ScatterVoxels",	SF_Compute );
IMPLEMENT_GLOBAL_SHADER_STRUCT( FScatterResolveCS,		FTileBricksParameters, "/Engine/Private/Nanite/Voxel/ScatterBricks.usf", "ScatterResolve",	SF_Compute );
#endif

BEGIN_SHADER_PARAMETER_STRUCT( FDrawBricksParameters, )
	SHADER_PARAMETER_STRUCT_REF( FViewUniformShaderParameters, View )

	SHADER_PARAMETER_RDG_BUFFER_SRV( StructuredBuffer< FBrick >,	BrickBuffer )
	SHADER_PARAMETER_RDG_BUFFER_SRV( ByteAddressBuffer,				BlockBuffer )
	SHADER_PARAMETER_RDG_BUFFER_SRV( Buffer< uint >,				DispatchIndirectArgs )

	RDG_BUFFER_ACCESS( IndirectArgs, ERHIAccess::IndirectArgs )
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FDrawBricksVS : public FVoxelShader
{
	DECLARE_GLOBAL_SHADER( FDrawBricksVS );
	SHADER_USE_PARAMETER_STRUCT( FDrawBricksVS, FVoxelShader );

	using FPermutationDomain = TShaderPermutationDomain<>;
	using FParameters = FDrawBricksParameters;
};

class FDrawBricksPS : public FVoxelShader
{
	DECLARE_GLOBAL_SHADER( FDrawBricksPS );
	SHADER_USE_PARAMETER_STRUCT( FDrawBricksPS, FVoxelShader );

	using FPermutationDomain = TShaderPermutationDomain<>;
	using FParameters = FDrawBricksParameters;
};

IMPLEMENT_SHADER_TYPE(, FDrawBricksVS, TEXT("/Engine/Private/Nanite/Voxel/RasterizeBricks.usf"), TEXT("DrawBricksVS"), SF_Vertex );
IMPLEMENT_SHADER_TYPE(, FDrawBricksPS, TEXT("/Engine/Private/Nanite/Voxel/RasterizeBricks.usf"), TEXT("DrawBricksPS"), SF_Pixel );

#endif


void DrawVisibleBricks(
	FRDGBuilder& GraphBuilder,
	FScene& Scene,
	const FViewInfo& View,
	FMinimalSceneTextures& SceneTextures )
{
#if !UE_BUILD_SHIPPING
	if( CVarVoxel.GetValueOnRenderThread() == 0 )
		return;

	RDG_EVENT_SCOPE( GraphBuilder, "Voxel" );

	uint32 HashTableSize = View.ViewRect.Area();
	//uint32 HashTableSize = FMath::RoundUpToPowerOfTwo( View.ViewRect.Area() );

	FRDGBufferRef HashTable		= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc(  4, HashTableSize ), TEXT("Voxel.HashTable") );
	FRDGBufferRef IndexBuffer	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc(  4, HashTableSize ), TEXT("Voxel.IndexBuffer") );
	//FRDGBufferRef BrickBuffer	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc(  8, HashTableSize ), TEXT("Voxel.BrickBuffer") );
	FRDGBufferRef BrickBuffer	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc( 16, HashTableSize ), TEXT("Voxel.BrickBuffer") );

	FRDGBufferRef BlockBufferL1	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateByteAddressDesc( 8 * HashTableSize ), TEXT("Voxel.BlockBufferL1") );
	FRDGBufferRef BlockBufferL2	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateByteAddressDesc( 8 * 2 * View.ViewRect.Area() ), TEXT("Voxel.BlockBufferL2") );

	FRDGBufferRef DispatchIndirectArgs	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateIndirectDesc(4), TEXT("Voxel.DispatchIndirectArgs") );
	FRDGBufferRef DrawIndirectArgs		= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateIndirectDesc(6), TEXT("Voxel.DrawIndirectArgs") );

	AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( HashTable ), 0 );
	AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( DispatchIndirectArgs ), 0 );
	AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( DrawIndirectArgs ), 0 );

	AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( BlockBufferL1 ), 0 );
	AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( BlockBufferL2 ), 0 );

	FRDGBufferUAVDesc BrickUAV( BrickBuffer );
	//BrickUAV.bSupportsAtomicCounter = true;
	
	{
		FBuildBricksParameters* PassParameters = GraphBuilder.AllocParameters< FBuildBricksParameters >();

		PassParameters->View			= View.ViewUniformBuffer;
		PassParameters->SceneTextures	= SceneTextures.UniformBuffer;

		PassParameters->HashTableSize	= HashTableSize;

		PassParameters->RWHashTable		= GraphBuilder.CreateUAV( HashTable );
		PassParameters->RWIndexBuffer	= GraphBuilder.CreateUAV( IndexBuffer );
		PassParameters->RWBrickBuffer	= GraphBuilder.CreateUAV( BrickBuffer );
		//PassParameters->RWBrickBuffer	= GraphBuilder.CreateUAV( BrickUAV, ERDGUnorderedAccessViewFlags::InitializeCount );
		PassParameters->RWBlockBuffer	= GraphBuilder.CreateUAV( BlockBufferL1 );

		PassParameters->RWDispatchIndirectArgs	= GraphBuilder.CreateUAV( DispatchIndirectArgs );

		auto ComputeShader = View.ShaderMap->GetShader< FVisibleBricksHashCS >();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Visible Bricks Hash"),
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount( View.ViewRect.Size(), 8 )
		);
	}

	{
		FBuildBricksParameters* PassParameters = GraphBuilder.AllocParameters< FBuildBricksParameters >();

		PassParameters->RWBrickBuffer			= GraphBuilder.CreateUAV( BrickUAV );
		PassParameters->RWDispatchIndirectArgs	= GraphBuilder.CreateUAV( DispatchIndirectArgs );
		PassParameters->RWDrawIndirectArgs		= GraphBuilder.CreateUAV( DrawIndirectArgs );

		auto ComputeShader = View.ShaderMap->GetShader< FFillArgsCS >();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Fill Args"),
			ComputeShader,
			PassParameters,
			FIntVector( 1, 1, 1 )
		);
	}
	
	FRDGBufferRef BlockBuffer = BlockBufferL1;

	if( CVarVoxelLevel2.GetValueOnRenderThread() )
	{
		{
			FBuildBricksParameters* PassParameters = GraphBuilder.AllocParameters< FBuildBricksParameters >();

			PassParameters->RWBrickBuffer	= GraphBuilder.CreateUAV( BrickBuffer );
			PassParameters->BlockBuffer		= GraphBuilder.CreateSRV( BlockBufferL1 );
			PassParameters->RWBlockBuffer	= GraphBuilder.CreateUAV( BlockBufferL2 );

			PassParameters->DispatchIndirectArgs	= GraphBuilder.CreateSRV( DispatchIndirectArgs );
			PassParameters->RWDrawIndirectArgs		= GraphBuilder.CreateUAV( DrawIndirectArgs );
		
			PassParameters->IndirectArgs			= DispatchIndirectArgs;

			auto ComputeShader = View.ShaderMap->GetShader< FAllocBlocksCS >();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Alloc Blocks"),
				ComputeShader,
				PassParameters,
				PassParameters->IndirectArgs,
				0
			);
		}

		{
			FBuildBricksParameters* PassParameters = GraphBuilder.AllocParameters< FBuildBricksParameters >();

			PassParameters->View			= View.ViewUniformBuffer;
			PassParameters->SceneTextures	= SceneTextures.UniformBuffer;

			PassParameters->HashTableSize	= HashTableSize;

			PassParameters->HashTable		= GraphBuilder.CreateSRV( HashTable );
			PassParameters->IndexBuffer		= GraphBuilder.CreateSRV( IndexBuffer );
			PassParameters->BrickBuffer		= GraphBuilder.CreateSRV( BrickBuffer );
			PassParameters->RWBlockBuffer	= GraphBuilder.CreateUAV( BlockBufferL2 );

			auto ComputeShader = View.ShaderMap->GetShader< FFillBlocksCS >();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Fill Blocks"),
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount( View.ViewRect.Size(), 8 )
			);
		}

		BlockBuffer = BlockBufferL2;
	}

	int32 VoxelMethod = CVarVoxelMethod.GetValueOnRenderThread();

	switch( VoxelMethod )
	{
	default:
	case 0:		// Rasterize bricks
		{
			FDrawBricksParameters* PassParameters = GraphBuilder.AllocParameters< FDrawBricksParameters >();

			PassParameters->View = View.ViewUniformBuffer;

			PassParameters->BrickBuffer			= GraphBuilder.CreateSRV( BrickBuffer );
			PassParameters->BlockBuffer			= GraphBuilder.CreateSRV( BlockBuffer );
			PassParameters->DispatchIndirectArgs= GraphBuilder.CreateSRV( DispatchIndirectArgs );

			PassParameters->IndirectArgs		= DrawIndirectArgs;

			PassParameters->RenderTargets[0] = FRenderTargetBinding( SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad );
			PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding( SceneTextures.Depth.Target, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop );

			const bool bReverseCulling	= View.bReverseCulling;
			const FIntRect& ViewRect	= View.ViewRect;

			auto VertexShader = View.ShaderMap->GetShader< FDrawBricksVS >();
			auto PixelShader  = View.ShaderMap->GetShader< FDrawBricksPS >();

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Draw Bricks"),
				PassParameters,
				ERDGPassFlags::Raster,
				[ VertexShader, PixelShader, PassParameters, bReverseCulling, &ViewRect ]( FRHICommandListImmediate& RHICmdList )
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets( GraphicsPSOInit );
					RHICmdList.SetViewport( ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f );

				#if 0
					GraphicsPSOInit.BlendState = TStaticBlendState< CW_RGBA, BO_Add, BF_One, BF_One >::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				#else
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					GraphicsPSOInit.RasterizerState = bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
				#endif
					GraphicsPSOInit.PrimitiveType = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState( RHICmdList, GraphicsPSOInit, 0 );
			
					SetShaderParameters( RHICmdList, VertexShader, VertexShader.GetVertexShader(), *PassParameters );
					SetShaderParameters( RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters );

					RHICmdList.SetStreamSource( 0, nullptr, 0 );
					
					if( GRHISupportsRectTopology )
					{
						RHICmdList.DrawPrimitiveIndirect( PassParameters->IndirectArgs->GetIndirectRHICallBuffer(), 0 );
					}
					else
					{
						RHICmdList.DrawIndexedPrimitiveIndirect( GSpriteIndexBuffer.IndexBufferRHI, PassParameters->IndirectArgs->GetIndirectRHICallBuffer(), 0 );
					}
				} );
		}
		break;

	case 1:		// Tiled
		{
			uint32 TileSize = 8;
			uint32 TilesX = FMath::DivideAndRoundUp< uint32 >( View.ViewRect.Size().X, TileSize );
			uint32 TilesY = FMath::DivideAndRoundUp< uint32 >( View.ViewRect.Size().Y, TileSize );
			
			uint32 MaxTileElements = View.ViewRect.Area();

			FRDGBufferRef TileListBuffer	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc( 12, MaxTileElements ), TEXT("Voxel.TileListBuffer") );
			FRDGBufferRef TileArrayBuffer	= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateStructuredDesc(  8, MaxTileElements ), TEXT("Voxel.TileArrayBuffer") );
			FRDGBufferRef TileArgs			= GraphBuilder.CreateBuffer( FRDGBufferDesc::CreateIndirectDesc(5), TEXT("Voxel.TileArgs") );

			FRDGBufferUAVDesc TileListUAV( TileListBuffer );
			//TileListUAV.bSupportsAtomicCounter = true;

			AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( TileArgs ), 0 );

			FRDGTextureDesc TileDesc( FRDGTextureDesc::Create2D( FIntPoint( TilesX, TilesY ), PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV ) );

			FRDGTextureRef TileHead		= GraphBuilder.CreateTexture( TileDesc, TEXT("TileHead") );
			FRDGTextureRef TileCount	= GraphBuilder.CreateTexture( TileDesc, TEXT("TileCount") );
			FRDGTextureRef TileOffset	= GraphBuilder.CreateTexture( TileDesc, TEXT("TileOffset") );

			const uint32 ClearValue[4] = { 0, 0, 0, 0 };
			AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( TileHead ),  ClearValue );
			AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( TileCount ), ClearValue );

			FRDGTextureDesc SceneColorDesc = SceneTextures.Color.Target->Desc;
			SceneColorDesc.Flags |= TexCreate_UAV;
			FRDGTextureRef OutSceneColor = GraphBuilder.CreateTexture( SceneColorDesc, TEXT("SceneColorVoxel") );

			{
				FTileBricksParameters* PassParameters = GraphBuilder.AllocParameters< FTileBricksParameters >();

				PassParameters->View = View.ViewUniformBuffer;

				PassParameters->BrickBuffer			= GraphBuilder.CreateSRV( BrickBuffer );
				PassParameters->BlockBuffer			= GraphBuilder.CreateSRV( BlockBuffer );
				PassParameters->RWTileListBuffer	= GraphBuilder.CreateUAV( TileListUAV );	//, ERDGUnorderedAccessViewFlags::InitializeCount );
				PassParameters->RWTileHead			= GraphBuilder.CreateUAV( TileHead );
				PassParameters->RWTileCount			= GraphBuilder.CreateUAV( TileCount );

				PassParameters->DispatchIndirectArgs	= GraphBuilder.CreateSRV( DispatchIndirectArgs );
				PassParameters->RWDispatchIndirectArgs	= GraphBuilder.CreateUAV( TileArgs );
				PassParameters->IndirectArgs			= DispatchIndirectArgs;

				auto ComputeShader = View.ShaderMap->GetShader< FBinBricksInTilesCS >();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Bin Blocks"),
					ComputeShader,
					PassParameters,
					PassParameters->IndirectArgs,
					0
				);
			}

			if(0)
			{
				FTileBricksParameters* PassParameters = GraphBuilder.AllocParameters< FTileBricksParameters >();

				PassParameters->View = View.ViewUniformBuffer;

				PassParameters->TileListBuffer			= GraphBuilder.CreateSRV( TileListBuffer );
				PassParameters->RWTileArrayBuffer		= GraphBuilder.CreateUAV( TileArrayBuffer );
				PassParameters->TileHead				= TileHead;
				PassParameters->TileCount				= TileCount;
				PassParameters->RWTileOffset			= GraphBuilder.CreateUAV( TileOffset );
				PassParameters->RWDispatchIndirectArgs	= GraphBuilder.CreateUAV( TileArgs );

				auto ComputeShader = View.ShaderMap->GetShader< FBuildTileArraysCS >();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Build Tile Arrays"),
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount( View.ViewRect.Size(), TileSize )
				);
			}

			{
				FTileBricksParameters* PassParameters = GraphBuilder.AllocParameters< FTileBricksParameters >();

				PassParameters->View = View.ViewUniformBuffer;

				PassParameters->BrickBuffer			= GraphBuilder.CreateSRV( BrickBuffer );
				PassParameters->BlockBuffer			= GraphBuilder.CreateSRV( BlockBuffer );
				PassParameters->TileListBuffer		= GraphBuilder.CreateSRV( TileListBuffer );
				//PassParameters->TileArrayBuffer		= GraphBuilder.CreateSRV( TileArrayBuffer );
				PassParameters->TileHead			= TileHead;
				PassParameters->TileCount			= TileCount;
				PassParameters->TileOffset			= TileOffset;
				PassParameters->OutSceneColor		= GraphBuilder.CreateUAV( OutSceneColor );

				auto ComputeShader = View.ShaderMap->GetShader< FRayCastTilesCS >();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Ray Cast Tiles"),
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount( View.ViewRect.Size(), TileSize )
				);
			}

			SceneTextures.Color = OutSceneColor;
		}
		break;
	case 2:		// Scatter
		{
			FRDGTextureDesc VisBufferDesc( FRDGTextureDesc::Create2D( View.ViewRect.Size(), PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV ) );

			FRDGTextureRef TileCount	= GraphBuilder.CreateTexture( VisBufferDesc, TEXT("TileCount") );

			const uint32 ClearValue[4] = { 0, 0, 0, 0 };
			AddClearUAVPass( GraphBuilder, GraphBuilder.CreateUAV( TileCount ), ClearValue );

			FRDGTextureDesc SceneColorDesc = SceneTextures.Color.Target->Desc;
			SceneColorDesc.Flags |= TexCreate_UAV;
			FRDGTextureRef OutSceneColor = GraphBuilder.CreateTexture( SceneColorDesc, TEXT("SceneColorVoxel") );

			{
				FTileBricksParameters* PassParameters = GraphBuilder.AllocParameters< FTileBricksParameters >();

				PassParameters->View = View.ViewUniformBuffer;

				PassParameters->BrickBuffer			= GraphBuilder.CreateSRV( BrickBuffer );
				PassParameters->BlockBuffer			= GraphBuilder.CreateSRV( BlockBufferL1 );
				PassParameters->RWTileCount			= GraphBuilder.CreateUAV( TileCount );

				PassParameters->DispatchIndirectArgs	= GraphBuilder.CreateSRV( DispatchIndirectArgs );
				PassParameters->IndirectArgs			= DispatchIndirectArgs;

				auto ComputeShader = View.ShaderMap->GetShader< FScatterVoxelsCS >();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Scatter Voxels"),
					ComputeShader,
					PassParameters,
					PassParameters->IndirectArgs,
					0
				);
			}

			{
				FTileBricksParameters* PassParameters = GraphBuilder.AllocParameters< FTileBricksParameters >();

				PassParameters->View = View.ViewUniformBuffer;

				PassParameters->TileCount			= TileCount;
				PassParameters->OutSceneColor		= GraphBuilder.CreateUAV( OutSceneColor );

				auto ComputeShader = View.ShaderMap->GetShader< FScatterResolveCS >();

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Scatter Resolve"),
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount( View.ViewRect.Size(), 8 )
				);
			}

			SceneTextures.Color = OutSceneColor;
		}
		break;
	}
#endif
}

} // namespace Nanite