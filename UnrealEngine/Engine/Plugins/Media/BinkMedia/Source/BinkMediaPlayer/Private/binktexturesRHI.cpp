// Copyright Epic Games, Inc. All Rights Reserved.

#define BINKRHIFUNCTIONS
#define BINKTEXTURESCLEANUP
#include "../../BinkMediaPlayerSDK/include/egttypes.h"
#include "../../BinkMediaPlayerSDK/include/binktiny.h"
#include "../../BinkMediaPlayerSDK/include/binktextures.h"

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "RHI.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Runtime/RHI/Public/RHIStaticStates.h"
#include "Runtime/RHI/Public/PipelineStateCache.h"
#include "Runtime/RenderCore/Public/ShaderParameterUtils.h"
#include "Runtime/RenderCore/Public/RenderResource.h"
#include "Runtime/Renderer/Public/MaterialShader.h"
#include "Runtime/RenderCore/Public/RenderGraphBuilder.h"
#include "Runtime/RenderCore/Public/RenderGraphResources.h"
#include "Runtime/RenderCore/Public/BinkShaders.h"


extern FRHITexture2D *BinkRHIRenderTarget;
extern ERenderTargetLoadAction BinkRenderTargetLoadAction;

FRDGTextureRef BinkRegisterExternalTexture(FRDGBuilder& GraphBuilder, FRHITexture* Texture, const TCHAR* NameIfUnregistered)
{
    if (FRDGTextureRef FoundTexture = GraphBuilder.FindExternalTexture(Texture))
    {
        return FoundTexture;
    }
    return GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Texture, NameIfUnregistered));
}

typedef struct BINKSHADERSRHI
{
	BINKSHADERS pub;
} BINKSHADERSRHI; 

typedef struct BINKTEXTURESRHI
{
	BINKTEXTURES pub;

	// RHI
	S32 video_width, video_height;
	S32 dirty;

	BINKSHADERSRHI * shaders;
	HBINK bink;
	// this is the Bink info on the textures
	BINKFRAMEBUFFERS bink_buffers;

	F32 Ax, Ay, Bx, By, Cx, Cy;
	F32 alpha;
	S32 draw_type;
	S32 draw_flags;
	F32 u0, v0, u1, v1;

	FTextureRHIRef Ytexture[BINKMAXFRAMEBUFFERS];
	FTextureRHIRef cRtexture[BINKMAXFRAMEBUFFERS];
	FTextureRHIRef cBtexture[BINKMAXFRAMEBUFFERS];
	FTextureRHIRef Atexture[BINKMAXFRAMEBUFFERS];
	FTextureRHIRef Htexture[BINKMAXFRAMEBUFFERS];

	// unused for now
	S32 tonemap;
	F32 exposure;
	F32 out_luma;

} BINKTEXTURESRHI;

#if defined( BINKTEXTURESINDIRECTBINKCALLS )
  RADDEFFUNC void indirectBinkGetFrameBuffersInfo( HBINK bink, BINKFRAMEBUFFERS * fbset );
  #define BinkGetFrameBuffersInfo indirectBinkGetFrameBuffersInfo
  RADDEFFUNC void indirectBinkRegisterFrameBuffers( HBINK bink, BINKFRAMEBUFFERS * fbset );
  #define BinkRegisterFrameBuffers indirectBinkRegisterFrameBuffers
  RADDEFFUNC S32 indirectBinkAllocateFrameBuffers( HBINK bp, BINKFRAMEBUFFERS * set, U32 minimum_alignment );
  #define BinkAllocateFrameBuffers indirectBinkAllocateFrameBuffers
  RADDEFFUNC void * indirectBinkUtilMalloc(U64 bytes);
  #define BinkUtilMalloc indirectBinkUtilMalloc
  RADDEFFUNC void indirectBinkUtilFree(void * ptr);
  #define BinkUtilFree indirectBinkUtilFree
#endif

static BINKTEXTURES * Create_textures( BINKSHADERS * pshaders, HBINK bink, void * user_ptr );
static void Free_shaders( BINKSHADERS * pshaders );
static void Free_textures( BINKTEXTURES * ptextures );
static void Start_texture_update( BINKTEXTURES * ptextures );
static void Finish_texture_update( BINKTEXTURES * ptextures );
static void Draw_textures( BINKTEXTURES * ptextures, BINKSHADERS * pshaders, void * graphics_context );
static void Set_draw_position( BINKTEXTURES * ptextures, F32 x0, F32 y0, F32 x1, F32 y1 );
static void Set_draw_corners( BINKTEXTURES * ptextures, F32 Ax, F32 Ay, F32 Bx, F32 By, F32 Cx, F32 Cy );
static void Set_source_rect( BINKTEXTURES * ptextures, F32 u0, F32 v0, F32 u1, F32 v1 );
static void Set_alpha_settings( BINKTEXTURES * ptextures, F32 alpha_value, S32 draw_type );
static void Set_hdr_settings( BINKTEXTURES * ptextures, S32 tonemap, F32 exposure, S32 out_nits  );

//-----------------------------------------------------------------------------
RADDEFFUNC BINKSHADERS * Create_Bink_shaders(void * dummy_device)
{
	BINKSHADERSRHI * pshaders;

	pshaders = (BINKSHADERSRHI*)FMemory::Malloc(sizeof(*pshaders));
	if (pshaders == 0)
	{
		return 0;
	}

	FMemory::Memset(pshaders, 0, sizeof(*pshaders));

	pshaders->pub.Create_textures = Create_textures;
	pshaders->pub.Free_shaders = Free_shaders;

	return &pshaders->pub;
}

//-----------------------------------------------------------------------------
static void Free_shaders( BINKSHADERS * pshaders )
{
	FMemory::Free(pshaders);
}

//-----------------------------------------------------------------------------
static BINKTEXTURES * Create_textures(BINKSHADERS * pshaders, HBINK bink, void * user_ptr)
{
	BINKTEXTURESRHI * textures;
	BINKFRAMEBUFFERS * bb;

	textures = (BINKTEXTURESRHI *)FMemory::Malloc(sizeof(*textures));
	if (textures == 0)
		return 0;

	FMemory::Memset(textures, 0, sizeof(*textures));

	textures->pub.user_ptr = user_ptr;
	textures->shaders = (BINKSHADERSRHI*)pshaders;
	textures->bink = bink;
	textures->video_width = bink->Width;
	textures->video_height = bink->Height;

	bb = &textures->bink_buffers;

	BinkGetFrameBuffersInfo(bink, bb);

#if !PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS
	// allocate the system memory buffers if not allocated
	if (!BinkAllocateFrameBuffers(bink, bb, 0))
	{
		FMemory::Free(textures);
		return 0;
	}
#endif

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
	EPixelFormat format = PF_R8;
#if PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS 
	ETextureCreateFlags TexCreateFlags = TexCreate_Dynamic | TexCreate_CPUReadback | TexCreate_CPUWritable | TexCreate_OfflineProcessed | TexCreate_NoTiling;
#else
	ETextureCreateFlags TexCreateFlags = TexCreate_Dynamic | TexCreate_NoTiling;
#endif

	const FRHITextureCreateDesc YADesc =
		FRHITextureCreateDesc::Create2D(TEXT("BINK"), bb->YABufferWidth, bb->YABufferHeight, format)
		.SetFlags(TexCreateFlags);

	const FRHITextureCreateDesc cRcBDesc =
		FRHITextureCreateDesc::Create2D(TEXT("BINK"), bb->cRcBBufferWidth, bb->cRcBBufferHeight, format)
		.SetFlags(TexCreateFlags);

	for (int i = 0; i < bb->TotalFrames; ++i)
	{
		BINKFRAMEPLANESET *bp_src = &bb->Frames[i];

		if (bp_src->YPlane.Allocate)
		{
			textures->Ytexture[i] = RHICreateTexture(YADesc);
#if PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS 
			bp_src->YPlane.Buffer = GDynamicRHI->LockTexture2D_RenderThread(RHICmdList, textures->Ytexture[i], 0, PLATFORM_DIRECT_TEXTURE_MEMORY_ACCESS_LOCK_MODE, bp_src->YPlane.BufferPitch, false, false);
			bp_src->YPlane.BufferPitch = bp_src->YPlane.BufferPitch ? bp_src->YPlane.BufferPitch : ((bb->YABufferWidth+255)&-256);
#endif
		}

		if (bp_src->cRPlane.Allocate)
		{
			textures->cRtexture[i] = RHICreateTexture(cRcBDesc);
#if PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS 
			bp_src->cRPlane.Buffer = GDynamicRHI->LockTexture2D_RenderThread(RHICmdList, textures->cRtexture[i], 0, PLATFORM_DIRECT_TEXTURE_MEMORY_ACCESS_LOCK_MODE, bp_src->cRPlane.BufferPitch, false, false);
			bp_src->cRPlane.BufferPitch = bp_src->cRPlane.BufferPitch ? bp_src->cRPlane.BufferPitch : ((bb->cRcBBufferWidth+255)&-256);
#endif
		}

		if (bp_src->cBPlane.Allocate)
		{
			textures->cBtexture[i] = RHICreateTexture(cRcBDesc);
#if PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS 
			bp_src->cBPlane.Buffer = GDynamicRHI->LockTexture2D_RenderThread(RHICmdList, textures->cBtexture[i], 0, PLATFORM_DIRECT_TEXTURE_MEMORY_ACCESS_LOCK_MODE, bp_src->cBPlane.BufferPitch, false, false);
			bp_src->cBPlane.BufferPitch = bp_src->cBPlane.BufferPitch ? bp_src->cBPlane.BufferPitch : ((bb->cRcBBufferWidth+255)&-256);
#endif
		}

		if (bp_src->APlane.Allocate)
		{
			textures->Atexture[i] = RHICreateTexture(YADesc);
#if PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS 
			bp_src->APlane.Buffer = GDynamicRHI->LockTexture2D_RenderThread(RHICmdList, textures->Atexture[i], 0, PLATFORM_DIRECT_TEXTURE_MEMORY_ACCESS_LOCK_MODE, bp_src->APlane.BufferPitch, false, false);
			bp_src->APlane.BufferPitch = bp_src->APlane.BufferPitch ? bp_src->APlane.BufferPitch : ((bb->YABufferWidth+255)&-256);
#endif
		}

		if (bp_src->HPlane.Allocate)
		{
			textures->Htexture[i] = RHICreateTexture(YADesc);
#if PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS 
			bp_src->HPlane.Buffer = GDynamicRHI->LockTexture2D_RenderThread(RHICmdList, textures->Htexture[i], 0, PLATFORM_DIRECT_TEXTURE_MEMORY_ACCESS_LOCK_MODE, bp_src->HPlane.BufferPitch, false, false);
			bp_src->HPlane.BufferPitch = bp_src->HPlane.BufferPitch ? bp_src->HPlane.BufferPitch : ((bb->YABufferWidth+255)&-256);
#endif
		}
	}

	// Register our locked texture pointers with Bink
	BinkRegisterFrameBuffers(bink, bb);

	Set_draw_corners(&textures->pub, 0, 0, 1, 0, 0, 1);
	Set_source_rect(&textures->pub, 0, 0, 1, 1);
	Set_alpha_settings(&textures->pub, 1, 0);
	Set_hdr_settings(&textures->pub, 0, 1.0f, 80);

	textures->pub.Free_textures = Free_textures;
	textures->pub.Start_texture_update = Start_texture_update;
	textures->pub.Finish_texture_update = Finish_texture_update;
	textures->pub.Draw_textures = Draw_textures;
	textures->pub.Set_draw_position = Set_draw_position;
	textures->pub.Set_draw_corners = Set_draw_corners;
	textures->pub.Set_source_rect = Set_source_rect;
	textures->pub.Set_alpha_settings = Set_alpha_settings;
	textures->pub.Set_hdr_settings = Set_hdr_settings;

	return &textures->pub;
}

//-----------------------------------------------------------------------------

static void Free_textures(BINKTEXTURES* ptextures)
{
	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
	BINKTEXTURESRHI* textures = (BINKTEXTURESRHI*)ptextures;
	BINKFRAMEBUFFERS *bb = &textures->bink_buffers;

	/*
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	RHIFlushResources();
	RHICmdList.SubmitCommandsHint();
	FPlatformMisc::MemoryBarrier();
	*/

	for (int i = 0; i < bb->TotalFrames; ++i)
	{
		BINKFRAMEPLANESET *bp_src = &bb->Frames[i];
#if PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS 
		if (bp_src->YPlane.Buffer)  GDynamicRHI->UnlockTexture2D_RenderThread(RHICmdList, textures->Ytexture[i], 0, false);
		if (bp_src->cRPlane.Buffer) GDynamicRHI->UnlockTexture2D_RenderThread(RHICmdList, textures->cRtexture[i], 0, false);
		if (bp_src->cBPlane.Buffer) GDynamicRHI->UnlockTexture2D_RenderThread(RHICmdList, textures->cBtexture[i], 0, false);
		if (bp_src->APlane.Buffer)  GDynamicRHI->UnlockTexture2D_RenderThread(RHICmdList, textures->Atexture[i], 0, false);
		if (bp_src->HPlane.Buffer)  GDynamicRHI->UnlockTexture2D_RenderThread(RHICmdList, textures->Htexture[i], 0, false);
#endif
		if (textures->Ytexture[i].IsValid())  textures->Ytexture[i].SafeRelease();
		if (textures->cRtexture[i].IsValid()) textures->cRtexture[i].SafeRelease();
		if (textures->cBtexture[i].IsValid()) textures->cBtexture[i].SafeRelease();
		if (textures->Atexture[i].IsValid())  textures->Atexture[i].SafeRelease();
		if (textures->Htexture[i].IsValid())  textures->Htexture[i].SafeRelease();
	}

	FMemory::Free(textures);
}

static void Start_texture_update( BINKTEXTURES * ptextures )
{
	//BINKTEXTURESRHI *textures = (BINKTEXTURESRHI*)ptextures;
}

static void Finish_texture_update( BINKTEXTURES * ptextures )
{
	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
	BINKTEXTURESRHI * textures = (BINKTEXTURESRHI*)ptextures;
	BINKFRAMEBUFFERS *bb = &textures->bink_buffers;
	int frame_num = bb->FrameNum;
	BINKFRAMEPLANESET *bp_src = &bb->Frames[frame_num];

#if !PLATFORM_HAS_DIRECT_TEXTURE_MEMORY_ACCESS
	textures->dirty = 1;
#endif

	textures->pub.Ytexture = (void*)(UINTa)&textures->Ytexture[frame_num];
	textures->pub.cRtexture = (void*)(UINTa)&textures->cRtexture[frame_num];
	textures->pub.cBtexture = (void*)(UINTa)&textures->cBtexture[frame_num];
	textures->pub.Atexture = (void*)(UINTa)&textures->Atexture[frame_num];
	textures->pub.Htexture = (void*)(UINTa)&textures->Htexture[frame_num];
}

//-----------------------------------------------------------------------------

static void update_plane_texture_rect(FRHICommandListImmediate& RHI, FRHITexture2D* RHITexture, BINKPLANE const* plane, unsigned w, unsigned h)
{
	uint32 Stride = 0;
	unsigned char* TextureMemory = (unsigned char*)GDynamicRHI->LockTexture2D_RenderThread(RHI, RHITexture, 0, RLM_WriteOnly, Stride, false);

	if (TextureMemory)
	{
		check(Stride >= w);
		if (Stride == w)
		{
			FMemory::Memcpy(TextureMemory, plane->Buffer, w * h);
		}
		else
		{
			for (unsigned i = 0; i < h; ++i)
			{
				FMemory::Memcpy(TextureMemory + i * Stride, (char*)plane->Buffer + i * w, w);
			}
		}
		GDynamicRHI->UnlockTexture2D_RenderThread(RHI, RHITexture, 0, false);
	}
}

static void Draw_textures(BINKTEXTURES* ptextures, BINKSHADERS* pshaders, void* graphics_context)
{
	BINKTEXTURESRHI* textures = (BINKTEXTURESRHI*)ptextures;
	BINKSHADERSRHI* bshaders = (BINKSHADERSRHI*)pshaders;
	BINKFRAMEBUFFERS *bb = &textures->bink_buffers;
	int frame_num = bb->FrameNum;
	BINKFRAMEPLANESET *bp_src = &bb->Frames[frame_num];

	// nowhere to render to?
	if (BinkRHIRenderTarget == 0)
		return;

	if (bshaders == 0)
		bshaders = textures->shaders;

	int hasAPlane = bp_src->APlane.Allocate;
	int hasHPlane = bp_src->HPlane.Allocate;

	FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
	FRDGBuilder BinkGraphBuilder(RHICmdList);

	FBinkParameters *consts = BinkGraphBuilder.AllocParameters<FBinkParameters>();

	FRDGTexture *BinkRDGTexture = BinkRegisterExternalTexture(BinkGraphBuilder, BinkRHIRenderTarget, TEXT("Bink_RT"));
	consts->RenderTargets[0] = FRenderTargetBinding(BinkRDGTexture, BinkRenderTargetLoadAction, 0);
	consts->tex0 = BinkRegisterExternalTexture(BinkGraphBuilder, textures->Ytexture[frame_num], TEXT("BinkY"));
	consts->tex1 = BinkRegisterExternalTexture(BinkGraphBuilder, textures->cRtexture[frame_num], TEXT("BinkCr"));
	consts->tex2 = BinkRegisterExternalTexture(BinkGraphBuilder, textures->cBtexture[frame_num], TEXT("BinkCb"));
	consts->tex3 = BinkRegisterExternalTexture(BinkGraphBuilder, hasAPlane ? textures->Atexture[frame_num] : textures->Ytexture[frame_num], TEXT("BinkA"));
	consts->tex4 = BinkRegisterExternalTexture(BinkGraphBuilder, hasHPlane ? textures->Htexture[frame_num] : textures->Ytexture[frame_num], TEXT("BinkH"));
	consts->samp0 = TStaticSamplerState<SF_Bilinear>::GetRHI();
	consts->samp1 = TStaticSamplerState<SF_Bilinear>::GetRHI();
	consts->samp2 = TStaticSamplerState<SF_Bilinear>::GetRHI();
	consts->samp3 = TStaticSamplerState<SF_Bilinear>::GetRHI();
	consts->samp4 = TStaticSamplerState<SF_Bilinear>::GetRHI();

	// Pixel shader alpha constants
	if (textures->draw_type == 3)
	{
		unsigned BinkColorA = 0xffffffff;
		unsigned BinkColorB = 0xffc0c0c0;
		consts->consta.X = ((F32)(S32)((BinkColorA >> 0) & 255)) * (1.0f / 255.0f);
		consts->consta.Y = ((F32)(S32)((BinkColorA >> 8) & 255)) * (1.0f / 255.0f);
		consts->consta.Z = ((F32)(S32)((BinkColorA >> 16) & 255)) * (1.0f / 255.0f);
		consts->consta.W = ((F32)(S32)((BinkColorA >> 24) & 255)) * (1.0f / 255.0f);
		consts->crc.X = ((F32)(S32)((BinkColorB >> 0) & 255)) * (1.0f / 255.0f);
		consts->crc.Y = ((F32)(S32)((BinkColorB >> 8) & 255)) * (1.0f / 255.0f);
		consts->crc.Z = ((F32)(S32)((BinkColorB >> 16) & 255)) * (1.0f / 255.0f);
		consts->crc.W = ((F32)(S32)((BinkColorB >> 24) & 255)) * (1.0f / 255.0f);
		consts->cbc.X = ((F32)textures->bink->Width) / 8.0f;
		consts->cbc.Y = ((F32)textures->bink->Height) / 8.0f;
		consts->cbc.Z = 0.f;
		consts->cbc.W = 0.f;
	}
	else
	{
		// Pixel shader alpha constants
		consts->consta.Z = consts->consta.Y = consts->consta.X = textures->draw_type == 1 ? textures->alpha : 1.0f;
		consts->consta.W = textures->alpha;

		if (hasHPlane)
		{
			// HDR stuff
			consts->crc.X = textures->bink->ColorSpace[0];
			consts->crc.Y = textures->exposure;
			consts->crc.Z = textures->out_luma;
			consts->crc.W = 0;
			consts->cbc.X = textures->bink->ColorSpace[1];
			consts->cbc.Y = textures->bink->ColorSpace[2];
			consts->cbc.Z = textures->bink->ColorSpace[3];
			consts->cbc.W = textures->bink->ColorSpace[4];
		}
		else
		{
			// set the constants for the type of ycrcb we have
			consts->crc.X = textures->bink->ColorSpace[0];
			consts->crc.Y = textures->bink->ColorSpace[1];
			consts->crc.Z = textures->bink->ColorSpace[2];
			consts->crc.W = textures->bink->ColorSpace[3];
			consts->cbc.X = textures->bink->ColorSpace[4];
			consts->cbc.Y = textures->bink->ColorSpace[5];
			consts->cbc.Z = textures->bink->ColorSpace[6];
			consts->cbc.W = textures->bink->ColorSpace[7];
			consts->adj.X = textures->bink->ColorSpace[8];
			consts->adj.Y = textures->bink->ColorSpace[9];
			consts->adj.Z = textures->bink->ColorSpace[10];
			consts->adj.W = textures->bink->ColorSpace[11];
			consts->yscale.X = textures->bink->ColorSpace[12];
			consts->yscale.Y = textures->bink->ColorSpace[13];
			consts->yscale.Z = textures->bink->ColorSpace[14];
			consts->yscale.W = textures->bink->ColorSpace[15];
		}
	}

	// Vertex shader constants
	consts->xy_xform0.X = (textures->Bx - textures->Ax) * 2.0f;
	consts->xy_xform0.Y = (textures->Cx - textures->Ax) * 2.0f;
	consts->xy_xform0.Z = (textures->By - textures->Ay) * -2.0f;
	consts->xy_xform0.W = (textures->Cy - textures->Ay) * -2.0f; // view space has +y = up, our coords have +y = down
	consts->xy_xform1.X = textures->Ax * 2.0f - 1.0f;
	consts->xy_xform1.Y = 1.0f - textures->Ay * 2.0f;
	consts->xy_xform1.Z = 0.0f;
	consts->xy_xform1.W = 0.0f;

	// UV matrix
	{
		F32 luma_u_scale = (F32)textures->video_width / (F32)bb->YABufferWidth;
		F32 luma_v_scale = (F32)textures->video_height / (F32)bb->YABufferHeight;
		F32 chroma_u_scale = (F32)(textures->video_width / 2) / (F32)bb->cRcBBufferWidth;
		F32 chroma_v_scale = (F32)(textures->video_height / 2) / (F32)bb->cRcBBufferHeight;

		// Set up matrix columns for UV transform (currently just scale+translate, could add rotation)
		// X column
		consts->uv_xform0.X = (textures->u1 - textures->u0) * luma_u_scale;
		consts->uv_xform0.Y = 0.0f;
		consts->uv_xform0.Z = (textures->u1 - textures->u0) * chroma_u_scale;
		consts->uv_xform0.W = 0.0f;

		// Y column
		consts->uv_xform1.X = 0.0f;
		consts->uv_xform1.Y = (textures->v1 - textures->v0) * luma_v_scale;
		consts->uv_xform1.Z = 0.0f;
		consts->uv_xform1.W = (textures->v1 - textures->v0) * chroma_v_scale;

		// W column (translation)
		consts->uv_xform2.X = textures->u0 * luma_u_scale;
		consts->uv_xform2.Y = textures->v0 * luma_v_scale;
		consts->uv_xform2.Z = textures->u0 * chroma_u_scale;
		consts->uv_xform2.W = textures->v0 * chroma_v_scale;
	}

	FBinkDrawVS::FParameters* vert_params = BinkGraphBuilder.AllocParameters<FBinkDrawVS::FParameters>();
	vert_params->BinkParameters = *consts;

	FBinkDrawICtCpPS::FParameters* ictcp_params = BinkGraphBuilder.AllocParameters<FBinkDrawICtCpPS::FParameters>();
	ictcp_params->BinkParameters = *consts;

	FBinkDrawYCbCrPS::FParameters* ycbcr_params = BinkGraphBuilder.AllocParameters<FBinkDrawYCbCrPS::FParameters>();
	ycbcr_params->BinkParameters = *consts;

	auto* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FBinkDrawVS> DrawVS(ShaderMap);

	FBinkDrawICtCpPS::FPermutationDomain ictcp_pv;
	ictcp_pv.Set<FBinkDrawICtCpPS::FALPHA>(!!hasAPlane);
	ictcp_pv.Set<FBinkDrawICtCpPS::FTONEMAP>(textures->tonemap == 1);
	ictcp_pv.Set<FBinkDrawICtCpPS::FST2084>(textures->tonemap == 2);
	TShaderMapRef<FBinkDrawICtCpPS> DrawICtCpPS(ShaderMap, ictcp_pv);

	FBinkDrawYCbCrPS::FPermutationDomain ycbcr_pv;
	ycbcr_pv.Set<FBinkDrawYCbCrPS::FALPHA>(!!hasAPlane);
	ycbcr_pv.Set<FBinkDrawYCbCrPS::FSRGB>(!!(textures->draw_flags & 0x80000000));
	TShaderMapRef<FBinkDrawYCbCrPS> DrawYCbCrPS(ShaderMap, ycbcr_pv);

	FRDGEventName EventName(TEXT("Bink"));
	BinkGraphBuilder.AddPass(
		MoveTemp(EventName),
		consts,
		ERDGPassFlags::Raster,
		[&](FRHICommandListImmediate& RHICmdList)
		{
			if (textures->dirty)
			{
				textures->dirty = 0;

				FUpdateTextureRegion2D region_YAH(0, 0, 0, 0, bb->YABufferWidth, bb->YABufferHeight);
				FUpdateTextureRegion2D region_cRcB(0, 0, 0, 0, bb->cRcBBufferWidth, bb->cRcBBufferHeight);

				GDynamicRHI->UpdateTexture2D_RenderThread(RHICmdList, textures->Ytexture[frame_num], 0, region_YAH, bp_src->YPlane.BufferPitch, (uint8*)bp_src->YPlane.Buffer);
				GDynamicRHI->UpdateTexture2D_RenderThread(RHICmdList, textures->cRtexture[frame_num], 0, region_cRcB, bp_src->cRPlane.BufferPitch, (uint8*)bp_src->cRPlane.Buffer);
				GDynamicRHI->UpdateTexture2D_RenderThread(RHICmdList, textures->cBtexture[frame_num], 0, region_cRcB, bp_src->cBPlane.BufferPitch, (uint8*)bp_src->cBPlane.Buffer);
				if (hasAPlane) GDynamicRHI->UpdateTexture2D_RenderThread(RHICmdList, textures->Atexture[frame_num], 0, region_YAH, bp_src->APlane.BufferPitch, (uint8*)bp_src->APlane.Buffer);
				if (hasHPlane) GDynamicRHI->UpdateTexture2D_RenderThread(RHICmdList, textures->Htexture[frame_num], 0, region_YAH, bp_src->HPlane.BufferPitch, (uint8*)bp_src->HPlane.Buffer);
			}

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			// Set up blending 
			if ((!hasAPlane && textures->alpha >= 0.999f) || textures->draw_type == 2)
			{
				// opaque
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			}
			else if (textures->draw_type == 1)
			{
				// alpha pre-multiplied
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_Zero>::GetRHI();
			}
			else
			{
				// normal alpha
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_Zero>::GetRHI();
			}

			// Disable backface culling
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, false, false>::GetRHI();

			// Disable Depth/Stencil test
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			FVertexDeclarationElementList Elements;
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = DrawVS.GetVertexShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			// Set VS/PS shaders
			if (hasHPlane)
			{
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = DrawICtCpPS.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
				SetShaderParameters(RHICmdList, DrawICtCpPS, DrawICtCpPS.GetPixelShader(), *ictcp_params);
			}
			else
			{
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = DrawYCbCrPS.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
				SetShaderParameters(RHICmdList, DrawYCbCrPS, DrawYCbCrPS.GetPixelShader(), *ycbcr_params);
			}

			SetShaderParameters(RHICmdList, DrawVS, DrawVS.GetVertexShader(), *vert_params);

			RHICmdList.DrawPrimitive(0, 2, 1);
		});

	BinkGraphBuilder.Execute();
}

static void Set_draw_position(BINKTEXTURES * ptextures, F32 x0, F32 y0, F32 x1, F32 y1)
{
	BINKTEXTURESRHI * textures = (BINKTEXTURESRHI*)ptextures;
	textures->Ax = x0;
	textures->Ay = y0;
	textures->Bx = x1;
	textures->By = y0;
	textures->Cx = x0;
	textures->Cy = y1;
}

static void Set_draw_corners(BINKTEXTURES * ptextures, F32 Ax, F32 Ay, F32 Bx, F32 By, F32 Cx, F32 Cy)
{
	BINKTEXTURESRHI * textures = (BINKTEXTURESRHI*)ptextures;
	textures->Ax = Ax;
	textures->Ay = Ay;
	textures->Bx = Bx;
	textures->By = By;
	textures->Cx = Cx;
	textures->Cy = Cy;
}

static void Set_source_rect(BINKTEXTURES * ptextures, F32 u0, F32 v0, F32 u1, F32 v1)
{
	BINKTEXTURESRHI * textures = (BINKTEXTURESRHI*)ptextures;
	textures->u0 = u0;
	textures->v0 = v0;
	textures->u1 = u1;
	textures->v1 = v1;
}

static void Set_alpha_settings(BINKTEXTURES * ptextures, F32 alpha_value, S32 draw_type)
{
	BINKTEXTURESRHI * textures = (BINKTEXTURESRHI*)ptextures;
	textures->alpha = alpha_value;
	textures->draw_type = draw_type & 0x0FFFFFFF;
	textures->draw_flags = draw_type & 0xF0000000;
}

static void Set_hdr_settings(BINKTEXTURES * ptextures, S32 tonemap, F32 exposure, S32 out_nits)
{
	BINKTEXTURESRHI * textures = (BINKTEXTURESRHI*)ptextures;
	textures->tonemap = tonemap;
	textures->exposure = exposure;
	textures->out_luma = ((F32)out_nits) / 80.f;
}

