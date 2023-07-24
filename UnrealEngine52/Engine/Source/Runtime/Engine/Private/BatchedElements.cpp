// Copyright Epic Games, Inc. All Rights Reserved.


#include "BatchedElements.h"

#include "SimpleElementShaders.h"
#include "RHIStaticStates.h"
#include "MeshPassProcessor.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "PipelineStateCache.h"
#include "SceneRelativeViewMatrices.h"
#include "GlobalRenderResources.h"
#include "HDRHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogBatchedElements, Log, All);

FSimpleElementVertex::FSimpleElementVertex() = default;

FSimpleElementVertex::FSimpleElementVertex(const FVector4f& InPosition, const FVector2f& InTextureCoordinate, const FLinearColor& InColor, const FColor& InHitProxyColor)
	: RelativePosition(InPosition)
	, TilePosition(ForceInitToZero)
	, TextureCoordinate(InTextureCoordinate)
	, Color(InColor)
	, HitProxyIdColor(InHitProxyColor)
{
}

FSimpleElementVertex::FSimpleElementVertex(const FVector4f& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, const FColor& InHitProxyColor)
	: RelativePosition(InPosition)
	, TilePosition(ForceInitToZero)
	, TextureCoordinate(InTextureCoordinate)
	, Color(InColor)
	, HitProxyIdColor(InHitProxyColor)
{
}

FSimpleElementVertex::FSimpleElementVertex(const FVector3f& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, const FColor& InHitProxyColor)
	: RelativePosition(InPosition)
	, TilePosition(ForceInitToZero)
	, TextureCoordinate(FVector2f(InTextureCoordinate))
	, Color(InColor)
	, HitProxyIdColor(InHitProxyColor)
{
}

FSimpleElementVertex::FSimpleElementVertex(const FVector4d& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, const FColor& InHitProxyColor)
	: TextureCoordinate(InTextureCoordinate)
	, Color(InColor)
	, HitProxyIdColor(InHitProxyColor)
{
	const FLargeWorldRenderPosition AbsolutePosition(InPosition);
	RelativePosition = FVector4f(AbsolutePosition.GetOffset(), (float)InPosition.W); // Don't bother with LWC W-component
	TilePosition = FVector4f(AbsolutePosition.GetTile(), 0.0f);
}

FSimpleElementVertex::FSimpleElementVertex(const FVector3d& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, const FColor& InHitProxyColor)
	: TextureCoordinate(InTextureCoordinate)
	, Color(InColor)
	, HitProxyIdColor(InHitProxyColor)
{
	const FLargeWorldRenderPosition AbsolutePosition(InPosition);
	RelativePosition = FVector4f(AbsolutePosition.GetOffset(), 1.0f);
	TilePosition = FVector4f(AbsolutePosition.GetTile(), 0.0f);
}

FSimpleElementVertex::FSimpleElementVertex(const FVector4f& InPosition, const FVector2f& InTextureCoordinate, const FLinearColor& InColor, FHitProxyId InHitProxyId)
	: FSimpleElementVertex(InPosition, InTextureCoordinate, InColor, InHitProxyId.GetColor())
{
}

FSimpleElementVertex::FSimpleElementVertex(const FVector4f& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, FHitProxyId InHitProxyId)
	: FSimpleElementVertex(InPosition, InTextureCoordinate, InColor, InHitProxyId.GetColor())
{
}

FSimpleElementVertex::FSimpleElementVertex(const FVector3f& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, FHitProxyId InHitProxyId)
	: FSimpleElementVertex(InPosition, InTextureCoordinate, InColor, InHitProxyId.GetColor())
{
}

FSimpleElementVertex::FSimpleElementVertex(const FVector4d& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, FHitProxyId InHitProxyId)
	: FSimpleElementVertex(InPosition, InTextureCoordinate, InColor, InHitProxyId.GetColor())
{
}

FSimpleElementVertex::FSimpleElementVertex(const FVector3d& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, FHitProxyId InHitProxyId)
	: FSimpleElementVertex(InPosition, InTextureCoordinate, InColor, InHitProxyId.GetColor())
{
}

FSimpleElementVertexDeclaration::FSimpleElementVertexDeclaration() = default;
FSimpleElementVertexDeclaration::FSimpleElementVertexDeclaration(FSimpleElementVertexDeclaration&&) = default;
FSimpleElementVertexDeclaration::~FSimpleElementVertexDeclaration() = default;

void FSimpleElementVertexDeclaration::InitRHI()
{
	FVertexDeclarationElementList Elements;
	uint16 Stride = sizeof(FSimpleElementVertex);
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSimpleElementVertex, RelativePosition), VET_Float4, 0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSimpleElementVertex, TilePosition), VET_Float4, 1, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSimpleElementVertex, TextureCoordinate), VET_Float2, 2, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSimpleElementVertex, Color), VET_Float4, 3, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(FSimpleElementVertex, HitProxyIdColor), VET_Color, 4, Stride));
	VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
}

void FSimpleElementVertexDeclaration::ReleaseRHI()
{
	VertexDeclarationRHI.SafeRelease();
}

/** The simple element vertex declaration. */
TGlobalResource<FSimpleElementVertexDeclaration> GSimpleElementVertexDeclaration;

EBlendModeFilter::Type GetBlendModeFilter(ESimpleElementBlendMode BlendMode)
{
	if (BlendMode == SE_BLEND_Opaque || BlendMode == SE_BLEND_Masked || BlendMode == SE_BLEND_MaskedDistanceField || BlendMode == SE_BLEND_MaskedDistanceFieldShadowed)
	{
		return EBlendModeFilter::OpaqueAndMasked;
	}
	else
	{
		return EBlendModeFilter::Translucent;
	}
}

FBatchedElements::FBatchedElements()
	: WireTriVerts(/*InNeedsCPUAccess*/true) // Keep vertices on buffer creation
	, MaxMeshIndicesAllowed(GDrawUPIndexCheckCount / sizeof(int32))
	// the index buffer is 2 bytes, so make sure we only address 0xFFFF vertices in the index buffer
	, MaxMeshVerticesAllowed(FMath::Min<uint32>(0xFFFF, GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)))
	, bEnableHDREncoding(true)
{
}

void FBatchedElements::AddLine(const FVector& Start, const FVector& End, const FLinearColor& Color, FHitProxyId HitProxyId, float Thickness, float DepthBias, bool bScreenSpace)
{
	// Ensure the line isn't masked out.  Some legacy code relies on Color.A being ignored.
	FLinearColor OpaqueColor(Color);
	OpaqueColor.A = 1;

	if (Thickness == 0.0f)
	{
		if (DepthBias == 0.0f)
		{
			new(LineVertices) FSimpleElementVertex(Start, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(LineVertices) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
		}
		else
		{
			// Draw degenerate triangles in wireframe mode to support depth bias (d3d11 and opengl3 don't support depth bias on line primitives, but do on wireframes)
			FBatchedWireTris* WireTri = new(WireTris) FBatchedWireTris();
			WireTri->DepthBias = DepthBias;
			new(WireTriVerts) FSimpleElementVertex(Start, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, OpaqueColor, HitProxyId);
		}
	}
	else
	{
		FBatchedThickLines* ThickLine = new(ThickLines) FBatchedThickLines;
		ThickLine->Start = Start;
		ThickLine->End = End;
		ThickLine->Thickness = Thickness;
		ThickLine->Color = OpaqueColor;
		ThickLine->HitProxyColor = HitProxyId.GetColor();
		ThickLine->DepthBias = DepthBias;
		ThickLine->bScreenSpace = bScreenSpace;
	}
}

void FBatchedElements::AddTranslucentLine(const FVector& Start, const FVector& End, const FLinearColor& Color, FHitProxyId HitProxyId, float Thickness, float DepthBias, bool bScreenSpace)
{
	if (Thickness == 0.0f)
	{
		if (DepthBias == 0.0f)
		{
			new(LineVertices) FSimpleElementVertex(Start, FVector2D::ZeroVector, Color, HitProxyId);
			new(LineVertices) FSimpleElementVertex(End, FVector2D::ZeroVector, Color, HitProxyId);
		}
		else
		{
			// Draw degenerate triangles in wireframe mode to support depth bias (d3d11 and opengl3 don't support depth bias on line primitives, but do on wireframes)
			FBatchedWireTris* WireTri = new(WireTris) FBatchedWireTris();
			WireTri->DepthBias = DepthBias;
			new(WireTriVerts) FSimpleElementVertex(Start, FVector2D::ZeroVector, Color, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, Color, HitProxyId);
			new(WireTriVerts) FSimpleElementVertex(End, FVector2D::ZeroVector, Color, HitProxyId);
		}
	}
	else
	{
		FBatchedThickLines* ThickLine = new(ThickLines) FBatchedThickLines;
		ThickLine->Start = Start;
		ThickLine->End = End;
		ThickLine->Thickness = Thickness;
		ThickLine->Color = Color;
		ThickLine->HitProxyColor = HitProxyId.GetColor();
		ThickLine->DepthBias = DepthBias;
		ThickLine->bScreenSpace = bScreenSpace;

	}
}

void FBatchedElements::AddPoint(const FVector& Position,float Size,const FLinearColor& Color,FHitProxyId HitProxyId)
{
	// Ensure the point isn't masked out.  Some legacy code relies on Color.A being ignored.
	FLinearColor OpaqueColor(Color);
	OpaqueColor.A = 1;

	FBatchedPoint* Point = new(Points) FBatchedPoint;
	Point->Position = Position;
	Point->Size = Size;
	Point->Color = OpaqueColor.ToFColor(true);
	Point->HitProxyColor = HitProxyId.GetColor();
}

int32 FBatchedElements::AddVertex(const FVector4& InPosition, const FVector2D& InTextureCoordinate, const FLinearColor& InColor, FHitProxyId HitProxyId)
{
	int32 VertexIndex = MeshVertices.Num();
	new(MeshVertices) FSimpleElementVertex(InPosition, InTextureCoordinate, InColor, HitProxyId);
	return VertexIndex;
}

int32 FBatchedElements::AddVertexf(const FVector4f& InPosition,const FVector2f& InTextureCoordinate,const FLinearColor& InColor,FHitProxyId HitProxyId)
{
	int32 VertexIndex = MeshVertices.Num();
	new(MeshVertices) FSimpleElementVertex(InPosition,InTextureCoordinate,InColor,HitProxyId);
	return VertexIndex;
}

/** Adds a triangle to the batch. */
void FBatchedElements::AddTriangle(int32 V0,int32 V1,int32 V2,const FTexture* Texture,EBlendMode BlendMode)
{
	ESimpleElementBlendMode SimpleElementBlendMode = SE_BLEND_Opaque;
	switch (BlendMode)
	{
	case BLEND_Opaque:
		SimpleElementBlendMode = SE_BLEND_Opaque; 
		break;
	case BLEND_Masked:
	case BLEND_Translucent:
		SimpleElementBlendMode = SE_BLEND_Translucent; 
		break;
	case BLEND_Additive:
		SimpleElementBlendMode = SE_BLEND_Additive; 
		break;
	case BLEND_Modulate:
		SimpleElementBlendMode = SE_BLEND_Modulate; 
		break;
	case BLEND_AlphaComposite:
		SimpleElementBlendMode = SE_BLEND_AlphaComposite;
		break;
	case BLEND_AlphaHoldout:
		SimpleElementBlendMode = SE_BLEND_AlphaHoldout;
		break;
	};
	AddTriangle(V0,V1,V2,Texture,SimpleElementBlendMode);
}

void FBatchedElements::AddTriangle(int32 V0, int32 V1, int32 V2, const FTexture* Texture, ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	AddTriangleExtensive( V0, V1, V2, NULL, Texture, BlendMode, GlowInfo );
}

	
void FBatchedElements::AddTriangle(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,ESimpleElementBlendMode BlendMode)
{
	AddTriangleExtensive( V0, V1, V2, BatchedElementParameters, GWhiteTexture, BlendMode );
}


void FBatchedElements::AddTriangleExtensive(int32 V0,int32 V1,int32 V2,FBatchedElementParameters* BatchedElementParameters,const FTexture* Texture,ESimpleElementBlendMode BlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	check(Texture);

	// Find an existing mesh element for the given texture and blend mode
	FBatchedMeshElement* MeshElement = NULL;
	for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
	{
		const FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if( CurMeshElement.Texture == Texture && 
			CurMeshElement.BatchedElementParameters.GetReference() == BatchedElementParameters &&
			CurMeshElement.BlendMode == BlendMode &&
			// make sure we are not overflowing on indices
			(CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed &&
			CurMeshElement.GlowInfo == GlowInfo )
		{
			// make sure we are not overflowing on vertices
			int32 DeltaV0 = (V0 - (int32)CurMeshElement.MinVertex);
			int32 DeltaV1 = (V1 - (int32)CurMeshElement.MinVertex);
			int32 DeltaV2 = (V2 - (int32)CurMeshElement.MinVertex);
			if( DeltaV0 >= 0 && DeltaV0 < MaxMeshVerticesAllowed &&
				DeltaV1 >= 0 && DeltaV1 < MaxMeshVerticesAllowed &&
				DeltaV2 >= 0 && DeltaV2 < MaxMeshVerticesAllowed )
			{
				MeshElement = &MeshElements[MeshIndex];
				break;
			}			
		}
	}
	if(!MeshElement)
	{
		// make sure that vertex indices are close enough to fit within MaxVerticesAllowed
		if( FMath::Abs(V0 - V1) >= MaxMeshVerticesAllowed ||
			FMath::Abs(V0 - V2) >= MaxMeshVerticesAllowed )
		{
			UE_LOG(LogBatchedElements, Warning, TEXT("Omitting FBatchedElements::AddTriangle due to sparce vertices V0=%i,V1=%i,V2=%i"),V0,V1,V2);
		}
		else
		{
			// Create a new mesh element for the texture if this is the first triangle encountered using it.
			MeshElement = new(MeshElements) FBatchedMeshElement;
			MeshElement->Texture = Texture;
			MeshElement->BatchedElementParameters = BatchedElementParameters;
			MeshElement->BlendMode = BlendMode;
			MeshElement->GlowInfo = GlowInfo;
			MeshElement->MaxVertex = V0;
			// keep track of the min vertex index used
			MeshElement->MinVertex = FMath::Min(FMath::Min(V0,V1),V2);
		}
	}

	if( MeshElement )
	{
		// Add the triangle's indices to the mesh element's index array.
		MeshElement->Indices.Add(V0 - MeshElement->MinVertex);
		MeshElement->Indices.Add(V1 - MeshElement->MinVertex);
		MeshElement->Indices.Add(V2 - MeshElement->MinVertex);

		// keep track of max vertex used in this mesh batch
		MeshElement->MaxVertex = FMath::Max(FMath::Max(FMath::Max(V0,(int32)MeshElement->MaxVertex),V1),V2);
	}
}

/** 
* Reserves space in mesh vertex array
* 
* @param NumMeshVerts - number of verts to reserve space for
* @param Texture - used to find the mesh element entry
* @param BlendMode - used to find the mesh element entry
*/
void FBatchedElements::AddReserveVertices(int32 NumMeshVerts)
{
	MeshVertices.Reserve( MeshVertices.Num() + NumMeshVerts );
}

void FBatchedElements::ReserveVertices(int32 NumMeshVerts)
{
	MeshVertices.Reserve( NumMeshVerts );
}

/** 
 * Reserves space in line vertex array
 * 
 * @param NumLines - number of lines to reserve space for
 * @param bDepthBiased - whether reserving depth-biased lines or non-biased lines
 * @param bThickLines - whether reserving regular lines or thick lines
 */
void FBatchedElements::AddReserveLines(int32 NumLines, bool bDepthBiased, bool bThickLines)
{
	if (!bThickLines)
	{
		if (!bDepthBiased)
		{
			LineVertices.Reserve(LineVertices.Num() + NumLines * 2);
		}
		else
		{
			WireTris.Reserve(WireTris.Num() + NumLines);
			WireTriVerts.Reserve(WireTriVerts.Num() + NumLines * 3);
		}
	}
	else
	{
		ThickLines.Reserve(ThickLines.Num() + NumLines * 2);
	}
}

/**
* Reserves space in triangle arrays 
* 
* @param NumMeshTriangles - number of triangles to reserve space for
* @param Texture - used to find the mesh element entry
* @param BlendMode - used to find the mesh element entry
*/
void FBatchedElements::AddReserveTriangles(int32 NumMeshTriangles,const FTexture* Texture,ESimpleElementBlendMode BlendMode)
{
	for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
	{
		FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if( CurMeshElement.Texture == Texture && 
			CurMeshElement.BatchedElementParameters.GetReference() == NULL &&
			CurMeshElement.BlendMode == BlendMode &&
			(CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed )
		{
			CurMeshElement.Indices.Reserve( CurMeshElement.Indices.Num() + NumMeshTriangles );
			break;
		}
	}	
}

void FBatchedElements::ReserveTriangles(int32 NumMeshTriangles, const FTexture* Texture, ESimpleElementBlendMode BlendMode)
{
	for (int32 MeshIndex = 0; MeshIndex < MeshElements.Num(); MeshIndex++)
	{
		FBatchedMeshElement& CurMeshElement = MeshElements[MeshIndex];
		if (CurMeshElement.Texture == Texture &&
		   CurMeshElement.BatchedElementParameters.GetReference() == NULL &&
		   CurMeshElement.BlendMode == BlendMode &&
		   (CurMeshElement.Indices.Num()+3) < MaxMeshIndicesAllowed)
		{
			CurMeshElement.Indices.Reserve( NumMeshTriangles );
			break;
		}
	}
}

void FBatchedElements::AddSprite(
	const FVector& Position,
	float SizeX,
	float SizeY,
	const FTexture* Texture,
	const FLinearColor& Color,
	FHitProxyId HitProxyId,
	float U,
	float UL,
	float V,
	float VL,
	uint8 BlendMode,
	float OpacityMaskRefVal
	)
{
	check(Texture);

	FBatchedSprite* Sprite = new(Sprites) FBatchedSprite;
	Sprite->Position = Position;
	Sprite->SizeX = SizeX;
	Sprite->SizeY = SizeY;
	Sprite->Texture = Texture;
	Sprite->Color = Color;
	Sprite->HitProxyColor = HitProxyId.GetColor();
	Sprite->U = U;
	Sprite->UL = UL == 0.f ? Texture->GetSizeX() : UL;
	Sprite->V = V;
	Sprite->VL = VL == 0.f ? Texture->GetSizeY() : VL;
	Sprite->OpacityMaskRefVal = OpacityMaskRefVal;
	Sprite->BlendMode = BlendMode;
}

/** Translates a ESimpleElementBlendMode into a RHI state change for rendering a mesh with the blend mode normally. */
static void SetBlendState(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ESimpleElementBlendMode BlendMode)
{
	// Override blending operations to accumulate alpha
	static const auto CVarCompositeMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.UI.CompositeMode"));

	const bool bCompositeUI = GRHISupportsHDROutput
		&& CVarCompositeMode && CVarCompositeMode->GetValueOnRenderThread() != 0 
		&& IsHDREnabled();

	if (bCompositeUI)
	{
		// Compositing to offscreen buffer, so alpha needs to be accumulated in a sensible manner
		switch (BlendMode)
		{
		case SE_BLEND_Translucent:
		case SE_BLEND_TranslucentDistanceField:
		case SE_BLEND_TranslucentDistanceFieldShadowed:
		case SE_BLEND_TranslucentAlphaOnly:
			BlendMode = SE_BLEND_AlphaBlend;
			break;

		default:
			// Blend mode is reasonable as-is
			break;
		};
	}

	switch(BlendMode)
	{
	case SE_BLEND_Opaque:
	case SE_BLEND_Masked:
	case SE_BLEND_MaskedDistanceField:
	case SE_BLEND_MaskedDistanceFieldShadowed:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case SE_BLEND_Translucent:
	case SE_BLEND_TranslucentDistanceField:
	case SE_BLEND_TranslucentDistanceFieldShadowed:
	case SE_BLEND_TranslucentAlphaOnly:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
		break;
	case SE_BLEND_Additive:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI();
		break;
	case SE_BLEND_Modulate:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_AlphaComposite:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		break;
	case SE_BLEND_AlphaHoldout:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
		break;
	case SE_BLEND_TranslucentAlphaOnlyWriteAlpha:
	case SE_BLEND_AlphaBlend:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI();
		break;
	case SE_BLEND_RGBA_MASK_END:
	case SE_BLEND_RGBA_MASK_START:
		break;
	}
}

/** Translates a ESimpleElementBlendMode into a RHI state change for rendering a mesh with the blend mode for hit testing. */
static void SetHitTestingBlendState(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ESimpleElementBlendMode BlendMode)
{
	switch(BlendMode)
	{
	case SE_BLEND_Opaque:
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		break;
	case SE_BLEND_Masked:
	case SE_BLEND_MaskedDistanceField:
	case SE_BLEND_MaskedDistanceFieldShadowed:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_AlphaComposite:
	case SE_BLEND_AlphaHoldout:
	case SE_BLEND_AlphaBlend:
	case SE_BLEND_Translucent:
	case SE_BLEND_TranslucentDistanceField:
	case SE_BLEND_TranslucentDistanceFieldShadowed:
	case SE_BLEND_TranslucentAlphaOnly:
	case SE_BLEND_TranslucentAlphaOnlyWriteAlpha:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_Additive:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_Modulate:
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		break;
	case SE_BLEND_RGBA_MASK_END:
	case SE_BLEND_RGBA_MASK_START:
		break;
	}
}

/** Global smoothing width for rendering batched elements with distance field blend modes */
float GBatchedElementSmoothWidth = 4;


FAutoConsoleVariableRef CVarWellCanvasDistanceFieldSmoothWidth(TEXT("Canvas.DistanceFieldSmoothness"), GBatchedElementSmoothWidth, TEXT("Global sharpness of distance field fonts/shapes rendered by canvas."), ECVF_Default);

template<class TSimpleElementPixelShader>
static TShaderRef<TSimpleElementPixelShader> GetPixelShader(ESimpleElementBlendMode BlendMode, ERHIFeatureLevel::Type FeatureLevel)
{
	return TShaderMapRef<TSimpleElementPixelShader>(GetGlobalShaderMap(FeatureLevel));
}

/**
 * Sets the appropriate vertex and pixel shader.
 */
void FBatchedElements::PrepareShaders(
	FRHICommandList& RHICmdList,
	FGraphicsPipelineStateInitializer& GraphicsPSOInit,
	uint32 StencilRef,
	ERHIFeatureLevel::Type FeatureLevel,
	ESimpleElementBlendMode BlendMode,
	const FRelativeViewMatrices& ViewMatrices,
	FBatchedElementParameters* BatchedElementParameters,
	const FTexture* Texture,
	bool bHitTesting,
	float Gamma,
	const FDepthFieldGlowInfo* GlowInfo,
	const FSceneView* View,
	float OpacityMaskRefVal
	) const
{
	// used to mask individual channels and desaturate
	FMatrix ColorWeights( FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 1, 0), FPlane(0, 0, 0, 0) );

	float GammaToUse = Gamma;

	ESimpleElementBlendMode MaskedBlendMode = SE_BLEND_Opaque;

	if(BlendMode >= SE_BLEND_RGBA_MASK_START && BlendMode <= SE_BLEND_RGBA_MASK_END)
	{
		/*
		* Red, Green, Blue and Alpha color weights all initialized to 0
		*/
		FPlane R( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane G( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane B( 0.0f, 0.0f, 0.0f, 0.0f );
		FPlane A( 0.0f,	0.0f, 0.0f, 0.0f );

		/*
		* Extract the color components from the in BlendMode to determine which channels should be active
		*/
		uint32 BlendMask = ( ( uint32 )BlendMode ) - SE_BLEND_RGBA_MASK_START;

		bool bRedChannel = ( BlendMask & ( 1 << 0 ) ) != 0;
		bool bGreenChannel = ( BlendMask & ( 1 << 1 ) ) != 0;
		bool bBlueChannel = ( BlendMask & ( 1 << 2 ) ) != 0;
		bool bAlphaChannel = ( BlendMask & ( 1 << 3 ) ) != 0;
		bool bDesaturate = ( BlendMask & ( 1 << 4 ) ) != 0;
		bool bAlphaOnly = bAlphaChannel && !bRedChannel && !bGreenChannel && !bBlueChannel;
		uint32 NumChannelsOn = ( bRedChannel ? 1 : 0 ) + ( bGreenChannel ? 1 : 0 ) + ( bBlueChannel ? 1 : 0 );
		GammaToUse = bAlphaOnly? 1.0f: Gamma;
		
		// If we are only to draw the alpha channel, make the Blend state opaque, to allow easy identification of the alpha values
		if( bAlphaOnly )
		{
			MaskedBlendMode = SE_BLEND_Opaque;
			SetBlendState(RHICmdList, GraphicsPSOInit, MaskedBlendMode);
			
			R.W = G.W = B.W = 1.0f;
		}
		else
		{
			MaskedBlendMode = !bAlphaChannel ? SE_BLEND_Opaque : SE_BLEND_Translucent;  // If alpha channel is disabled, do not allow alpha blending
			SetBlendState(RHICmdList, GraphicsPSOInit, MaskedBlendMode);

			// Determine the red, green, blue and alpha components of their respective weights to enable that colours prominence
			R.X = bRedChannel ? 1.0f : 0.0f;
			G.Y = bGreenChannel ? 1.0f : 0.0f;
			B.Z = bBlueChannel ? 1.0f : 0.0f;
			A.W = bAlphaChannel ? 1.0f : 0.0f;

			/*
			* Determine if desaturation is enabled, if so, we determine the output colour based on the number of active channels that are displayed
			* e.g. if Only the red and green channels are being viewed, then the desaturation of the image will be divided by 2
			*/
			if( bDesaturate && NumChannelsOn )
			{
				float ValR, ValG, ValB;
				ValR = R.X / NumChannelsOn;
				ValG = G.Y / NumChannelsOn;
				ValB = B.Z / NumChannelsOn;
				R = FPlane( ValR, ValG, ValB, 0 );
				G = FPlane( ValR, ValG, ValB, 0 );
				B = FPlane( ValR, ValG, ValB, 0 );
			}
		}

		ColorWeights = FMatrix( R, G, B, A );
	}
	
	if( BatchedElementParameters != NULL )
	{
		// Use the vertex/pixel shader that we were given
		ensure(ViewMatrices.TilePosition.IsZero());
		BatchedElementParameters->BindShaders(RHICmdList, GraphicsPSOInit, FeatureLevel, FMatrix(ViewMatrices.RelativeWorldToClip), GammaToUse, ColorWeights, Texture);
	}
	else
	{
		TShaderMapRef<FSimpleElementVS> VertexShader(GetGlobalShaderMap(FeatureLevel));
			
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GSimpleElementVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

		if (bHitTesting)
		{
			SetHitTestingBlendState(RHICmdList, GraphicsPSOInit, BlendMode);

			TShaderMapRef<FSimpleElementHitProxyPS> HitTestingPixelShader(GetGlobalShaderMap(FeatureLevel));
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = HitTestingPixelShader.GetPixelShader();

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

			HitTestingPixelShader->SetParameters(RHICmdList, Texture);
		}
		else
		{
			if (BlendMode == SE_BLEND_Masked)
			{
				// use clip() in the shader instead of alpha testing as cards that don't support floating point blending
				// also don't support alpha testing to floating point render targets
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();

				if (Texture->bSRGB)
				{
					auto MaskedPixelShader = GetPixelShader<FSimpleElementMaskedGammaPS_SRGB>(BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = MaskedPixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

					MaskedPixelShader->SetEditorCompositingParameters(RHICmdList, View);
					MaskedPixelShader->SetParameters(RHICmdList, Texture, Gamma, OpacityMaskRefVal, BlendMode);
				}
				else
				{
					auto MaskedPixelShader = GetPixelShader<FSimpleElementMaskedGammaPS_Linear>(BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = MaskedPixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

					MaskedPixelShader->SetEditorCompositingParameters(RHICmdList, View);
					MaskedPixelShader->SetParameters(RHICmdList, Texture, Gamma, OpacityMaskRefVal, BlendMode);
				}
			}
			// render distance field elements
			else if (
				BlendMode == SE_BLEND_MaskedDistanceField || 
				BlendMode == SE_BLEND_MaskedDistanceFieldShadowed ||
				BlendMode == SE_BLEND_TranslucentDistanceField	||
				BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed)
			{
				float AlphaRefVal = OpacityMaskRefVal;
				if (BlendMode == SE_BLEND_TranslucentDistanceField ||
					BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed)
				{
					// enable alpha blending and disable clip ref value for translucent rendering
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
					AlphaRefVal = 0.0f;
				}
				else
				{
					// clip is done in shader so just render opaque
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				}
				
				TShaderMapRef<FSimpleElementDistanceFieldGammaPS> DistanceFieldPixelShader(GetGlobalShaderMap(FeatureLevel));
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = DistanceFieldPixelShader.GetPixelShader();

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

				// @todo - expose these as options for batch rendering
				static FVector2D ShadowDirection(-1.0f/Texture->GetSizeX(),-1.0f/Texture->GetSizeY());
				static FLinearColor ShadowColor(FLinearColor::Black);
				const float ShadowSmoothWidth = (GBatchedElementSmoothWidth * 2) / Texture->GetSizeX();
				
				const bool EnableShadow = (
					BlendMode == SE_BLEND_MaskedDistanceFieldShadowed || 
					BlendMode == SE_BLEND_TranslucentDistanceFieldShadowed
					);
				
				DistanceFieldPixelShader->SetParameters(
					RHICmdList, 
					Texture,
					Gamma,
					AlphaRefVal,
					GBatchedElementSmoothWidth,
					EnableShadow,
					ShadowDirection,
					ShadowColor,
					ShadowSmoothWidth,
					(GlowInfo != NULL) ? *GlowInfo : FDepthFieldGlowInfo(),
					BlendMode
					);
			}
			else if(BlendMode == SE_BLEND_TranslucentAlphaOnly || BlendMode == SE_BLEND_TranslucentAlphaOnlyWriteAlpha)
			{
				SetBlendState(RHICmdList, GraphicsPSOInit, BlendMode);

				if (FMath::Abs(Gamma - 1.0f) < UE_KINDA_SMALL_NUMBER)
				{
					auto AlphaOnlyPixelShader = GetPixelShader<FSimpleElementAlphaOnlyPS>(BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = AlphaOnlyPixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

					AlphaOnlyPixelShader->SetParameters(RHICmdList, Texture);
					AlphaOnlyPixelShader->SetEditorCompositingParameters(RHICmdList, View);
				}
				else
				{
					auto GammaAlphaOnlyPixelShader = GetPixelShader<FSimpleElementGammaAlphaOnlyPS>(BlendMode, FeatureLevel);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GammaAlphaOnlyPixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

					GammaAlphaOnlyPixelShader->SetParameters(RHICmdList, Texture, Gamma, BlendMode);
					GammaAlphaOnlyPixelShader->SetEditorCompositingParameters(RHICmdList, View);
				}
			}
			else if(BlendMode >= SE_BLEND_RGBA_MASK_START && BlendMode <= SE_BLEND_RGBA_MASK_END)
			{
				TShaderMapRef<FSimpleElementColorChannelMaskPS> ColorChannelMaskPixelShader(GetGlobalShaderMap(FeatureLevel));
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ColorChannelMaskPixelShader.GetPixelShader();

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);
			
				ColorChannelMaskPixelShader->SetParameters(RHICmdList, Texture, ColorWeights, GammaToUse );
			}
			else
			{
				SetBlendState(RHICmdList, GraphicsPSOInit, BlendMode);
	
				if (FMath::Abs(Gamma - 1.0f) < UE_KINDA_SMALL_NUMBER)
				{
					TShaderMapRef<FSimpleElementPS> PixelShader(GetGlobalShaderMap(FeatureLevel));
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

					PixelShader->SetParameters(RHICmdList, Texture);
					PixelShader->SetEditorCompositingParameters(RHICmdList, View);
				}
				else
				{
					TShaderRef<FSimpleElementGammaBasePS> BasePixelShader;
					if (Texture->bSRGB)
					{
						TShaderMapRef<FSimpleElementGammaPS_SRGB> PixelShader_SRGB(GetGlobalShaderMap(FeatureLevel));
						BasePixelShader = PixelShader_SRGB;
					}
					else
					{
						TShaderMapRef<FSimpleElementGammaPS_Linear> PixelShader_Linear(GetGlobalShaderMap(FeatureLevel));
						BasePixelShader = PixelShader_Linear;
					}

					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = BasePixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

					BasePixelShader->SetParameters(RHICmdList, Texture, Gamma, BlendMode);
					BasePixelShader->SetEditorCompositingParameters(RHICmdList, View);
				}
			}
		}

		// Set the simple element vertex shader parameters
		VertexShader->SetParameters(RHICmdList, ViewMatrices);
	}
}

void FBatchedElements::DrawPointElements(FRHICommandList& RHICmdList, const FMatrix& Transform, const uint32 ViewportSizeX, const uint32 ViewportSizeY, const FVector& CameraX, const FVector& CameraY) const
{
	// Draw the point elements.
	if( Points.Num() > 0 )
	{
		// preallocate some memory to directly fill out
		const uint32 NumTris = ((uint32)Points.Num()) * 2; // even if Points.Num() == INT32_MAX, this won't overflow a uint32
		const uint32 NumVertices = NumTris * 3; // but this could

		// Prevent integer overflow to buffer overflow.
		if (NumTris > (UINT32_MAX / 3) ||
			NumVertices > UINT32_MAX / sizeof(FSimpleElementVertex))
		{
			UE_LOG(LogBatchedElements, Error, TEXT("Too many points. Will overflow uint32 buffer size. NumPoints: %d"), Points.Num());
			return;
		}

		FRHIResourceCreateInfo CreateInfo(TEXT("FBatchedElements_Points"));
		FBufferRHIRef VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FSimpleElementVertex) * NumVertices, BUF_VertexBuffer | BUF_Volatile, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
		void* VerticesPtr = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FSimpleElementVertex) * NumVertices, RLM_WriteOnly);

		FSimpleElementVertex* PointVertices = (FSimpleElementVertex*)VerticesPtr;

		uint32 VertIdx = 0;
		for(int32 PointIndex = 0; PointIndex < Points.Num(); PointIndex++)
		{
			// TODO: Support quad primitives here
			const FBatchedPoint& Point = Points[PointIndex];
			FVector4 TransformedPosition = Transform.TransformFVector4(Point.Position);

			// Generate vertices for the point such that the post-transform point size is constant.
			const uint32 ViewportMajorAxis = ViewportSizeX;//FMath::Max(ViewportSizeX, ViewportSizeY);
			const FVector WorldPointX = CameraX * Point.Size / ViewportMajorAxis * TransformedPosition.W;
			const FVector WorldPointY = CameraY * -Point.Size / ViewportMajorAxis * TransformedPosition.W;
					
			PointVertices[VertIdx + 0] = FSimpleElementVertex(Point.Position + WorldPointX - WorldPointY,FVector2D(1,0),Point.Color,Point.HitProxyColor);
			PointVertices[VertIdx + 1] = FSimpleElementVertex(Point.Position + WorldPointX + WorldPointY,FVector2D(1,1),Point.Color,Point.HitProxyColor);
			PointVertices[VertIdx + 2] = FSimpleElementVertex(Point.Position - WorldPointX - WorldPointY,FVector2D(0,0),Point.Color,Point.HitProxyColor);
			PointVertices[VertIdx + 3] = FSimpleElementVertex(Point.Position + WorldPointX + WorldPointY,FVector2D(1,1),Point.Color,Point.HitProxyColor);
			PointVertices[VertIdx + 4] = FSimpleElementVertex(Point.Position - WorldPointX - WorldPointY,FVector2D(0,0),Point.Color,Point.HitProxyColor);
			PointVertices[VertIdx + 5] = FSimpleElementVertex(Point.Position - WorldPointX + WorldPointY,FVector2D(0,1),Point.Color,Point.HitProxyColor);

			VertIdx += 6;
		}

		RHICmdList.UnlockBuffer(VertexBufferRHI);
		RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
		RHICmdList.DrawPrimitive(0, NumTris, 1);
	}
}

FSceneView FBatchedElements::CreateProxySceneView(const FMatrix& ProjectionMatrix, const FIntRect& ViewRect)
{
	FSceneViewInitOptions ProxyViewInitOptions;
	ProxyViewInitOptions.SetViewRectangle(ViewRect);
	ProxyViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ProxyViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ProxyViewInitOptions.ProjectionMatrix = ProjectionMatrix;

	return FSceneView(ProxyViewInitOptions);
}

bool FBatchedElements::Draw(FRHICommandList& RHICmdList, const FMeshPassProcessorRenderState& DrawRenderState, ERHIFeatureLevel::Type FeatureLevel, const FSceneView& View, bool bHitTesting, float Gamma /* = 1.0f */, EBlendModeFilter::Type Filter /* = EBlendModeFilter::All */) const
{
	const FRelativeViewMatrices RelativeMatrices = FRelativeViewMatrices::Create(View.ViewMatrices);
	const FMatrix& WorldToClip = View.ViewMatrices.GetViewProjectionMatrix();
	const FMatrix& ClipToWorld = View.ViewMatrices.GetInvViewProjectionMatrix();
	const uint32 ViewportSizeX = View.UnscaledViewRect.Width();
	const uint32 ViewportSizeY = View.UnscaledViewRect.Height();

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	DrawRenderState.ApplyToPSO(GraphicsPSOInit);
	uint32 StencilRef = DrawRenderState.GetStencilRef();

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

	if (UNLIKELY(!FApp::CanEverRender()))
	{
		return false;
	}

	if( HasPrimsToDraw() )
	{
		FVector CameraX = ClipToWorld.TransformVector(FVector(1,0,0)).GetSafeNormal();
		FVector CameraY = ClipToWorld.TransformVector(FVector(0,1,0)).GetSafeNormal();
		FVector CameraZ = ClipToWorld.TransformVector(FVector(0,0,1)).GetSafeNormal();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

		if( (LineVertices.Num() > 0 || Points.Num() > 0 || ThickLines.Num() > 0 || WireTris.Num() > 0)
			&& (Filter & EBlendModeFilter::OpaqueAndMasked))
		{
			// Lines/points don't support batched element parameters (yet!)
			FBatchedElementParameters* BatchedElementParameters = NULL;

			// Draw the line elements.
			if( LineVertices.Num() > 0 )
			{
				// Prevent integer overflow to buffer overflow.
				if (LineVertices.Num() > UINT32_MAX / sizeof(FSimpleElementVertex))
				{
					UE_LOG(LogBatchedElements, Error, TEXT("Too many line vertices. Will overflow uint32 buffer size. LineVertices: %d"), LineVertices.Num());
					return false;
				}

				GraphicsPSOInit.PrimitiveType = PT_LineList;

				// Set the appropriate pixel shader parameters & shader state for the non-textured elements.
				PrepareShaders(RHICmdList, GraphicsPSOInit, StencilRef, FeatureLevel, SE_BLEND_Opaque, RelativeMatrices, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View);

				FRHIResourceCreateInfo CreateInfo(TEXT("Lines"));
				FBufferRHIRef VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FSimpleElementVertex) * LineVertices.Num(), BUF_VertexBuffer | BUF_Volatile, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
				void* VoidPtr = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FSimpleElementVertex) * LineVertices.Num(), RLM_WriteOnly);

				FMemory::Memcpy(VoidPtr, LineVertices.GetData(), sizeof(FSimpleElementVertex) * LineVertices.Num());
				RHICmdList.UnlockBuffer(VertexBufferRHI);

				RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);

				int32 MaxVerticesAllowed = ((GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)) / 2) * 2;
				/*
				hack to avoid a crash when trying to render large numbers of line segments.
				*/
				MaxVerticesAllowed = FMath::Min(MaxVerticesAllowed, 64 * 1024);

				int32 MinVertex=0;
				int32 TotalVerts = (LineVertices.Num() / 2) * 2;
				while( MinVertex < TotalVerts )
				{
					int32 NumLinePrims = FMath::Min( MaxVerticesAllowed, TotalVerts - MinVertex ) / 2;
					RHICmdList.DrawPrimitive(MinVertex, NumLinePrims, 1);
					MinVertex += NumLinePrims * 2;
				}

				VertexBufferRHI.SafeRelease();
			}

			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Set the appropriate pixel shader parameters & shader state for the non-textured elements.
			PrepareShaders(RHICmdList, GraphicsPSOInit, StencilRef, FeatureLevel, SE_BLEND_Opaque, RelativeMatrices, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View);

			// Draw points
			DrawPointElements(RHICmdList, WorldToClip, ViewportSizeX, ViewportSizeY, CameraX, CameraY);

			if ( ThickLines.Num() > 0 )
			{
				float OrthoZoomFactor = 1.0f;

				const bool bIsPerspective = View.ViewMatrices.GetProjectionMatrix().M[3][3] < 1.0f ? true : false;
				if (!bIsPerspective)
				{
					OrthoZoomFactor = 1.0f / View.ViewMatrices.GetProjectionMatrix().M[0][0];
				}

				int32 LineIndex = 0;
				const int32 MaxLinesPerBatch = 2048;
				while (LineIndex < ThickLines.Num())
				{
					int32 FirstLineThisBatch = LineIndex;
					float DepthBiasThisBatch = ThickLines[LineIndex].DepthBias;
					while (++LineIndex < ThickLines.Num())
					{
						if ((ThickLines[LineIndex].DepthBias != DepthBiasThisBatch)
							|| ((LineIndex - FirstLineThisBatch) >= MaxLinesPerBatch))
						{
							break;
						}
					}
					int32 NumLinesThisBatch = LineIndex - FirstLineThisBatch;
					check(NumLinesThisBatch > 0);

					const bool bEnableMSAA = true;
					const bool bEnableLineAA = false;
					FRasterizerStateInitializerRHI Initializer(FM_Solid, CM_None, 0, DepthBiasThisBatch, ERasterizerDepthClipMode::DepthClip, bEnableMSAA, bEnableLineAA);
					auto RasterState = RHICreateRasterizerState(Initializer);
					GraphicsPSOInit.RasterizerState = RasterState.GetReference();
					PrepareShaders(RHICmdList, GraphicsPSOInit, StencilRef, FeatureLevel, SE_BLEND_AlphaBlend, RelativeMatrices, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View);

					FRHIResourceCreateInfo CreateInfo(TEXT("ThickLines"));
					FBufferRHIRef VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FSimpleElementVertex) * 8 * 3 * NumLinesThisBatch, BUF_VertexBuffer | BUF_Volatile, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
					void* ThickVertexData = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FSimpleElementVertex) * 8 * 3 * NumLinesThisBatch, RLM_WriteOnly);
					FSimpleElementVertex* ThickVertices = (FSimpleElementVertex*)ThickVertexData;
					check(ThickVertices);

					for (int i = 0; i < NumLinesThisBatch; ++i)
					{
						const FBatchedThickLines& Line = ThickLines[FirstLineThisBatch + i];
						const float Thickness = FMath::Abs( Line.Thickness );

						const float StartW			= WorldToClip.TransformFVector4(Line.Start).W;
						const float EndW			= WorldToClip.TransformFVector4(Line.End).W;

						// Negative thickness means that thickness is calculated in screen space, positive thickness should be used for world space thickness.
						const float ScalingStart	= Line.bScreenSpace ? StartW / ViewportSizeX : 1.0f;
						const float ScalingEnd		= Line.bScreenSpace ? EndW   / ViewportSizeX : 1.0f;

						const float CurrentOrthoZoomFactor = Line.bScreenSpace ? OrthoZoomFactor : 1.0f;

						const float ScreenSpaceScaling = Line.bScreenSpace ? 2.0f : 1.0f;

						const float StartThickness	= Thickness * ScreenSpaceScaling * CurrentOrthoZoomFactor * ScalingStart;
						const float EndThickness	= Thickness * ScreenSpaceScaling * CurrentOrthoZoomFactor * ScalingEnd;

						const FVector WorldPointXS	= CameraX * StartThickness * 0.5f;
						const FVector WorldPointYS	= CameraY * StartThickness * 0.5f;

						const FVector WorldPointXE	= CameraX * EndThickness * 0.5f;
						const FVector WorldPointYE	= CameraY * EndThickness * 0.5f;

						// Generate vertices for the point such that the post-transform point size is constant.
						const FVector WorldPointX = CameraX * Thickness * StartW / ViewportSizeX;
						const FVector WorldPointY = CameraY * Thickness * StartW / ViewportSizeX;

						// Begin point
						ThickVertices[0] = FSimpleElementVertex(Line.Start + WorldPointXS - WorldPointYS,FVector2D(1,0),Line.Color,Line.HitProxyColor); // 0S
						ThickVertices[1] = FSimpleElementVertex(Line.Start + WorldPointXS + WorldPointYS,FVector2D(1,1),Line.Color,Line.HitProxyColor); // 1S
						ThickVertices[2] = FSimpleElementVertex(Line.Start - WorldPointXS - WorldPointYS,FVector2D(0,0),Line.Color,Line.HitProxyColor); // 2S
					
						ThickVertices[3] = FSimpleElementVertex(Line.Start + WorldPointXS + WorldPointYS,FVector2D(1,1),Line.Color,Line.HitProxyColor); // 1S
						ThickVertices[4] = FSimpleElementVertex(Line.Start - WorldPointXS - WorldPointYS,FVector2D(0,0),Line.Color,Line.HitProxyColor); // 2S
						ThickVertices[5] = FSimpleElementVertex(Line.Start - WorldPointXS + WorldPointYS,FVector2D(0,1),Line.Color,Line.HitProxyColor); // 3S

						// Ending point
						ThickVertices[0+ 6] = FSimpleElementVertex(Line.End + WorldPointXE - WorldPointYE,FVector2D(1,0),Line.Color,Line.HitProxyColor); // 0E
						ThickVertices[1+ 6] = FSimpleElementVertex(Line.End + WorldPointXE + WorldPointYE,FVector2D(1,1),Line.Color,Line.HitProxyColor); // 1E
						ThickVertices[2+ 6] = FSimpleElementVertex(Line.End - WorldPointXE - WorldPointYE,FVector2D(0,0),Line.Color,Line.HitProxyColor); // 2E
																																							  
						ThickVertices[3+ 6] = FSimpleElementVertex(Line.End + WorldPointXE + WorldPointYE,FVector2D(1,1),Line.Color,Line.HitProxyColor); // 1E
						ThickVertices[4+ 6] = FSimpleElementVertex(Line.End - WorldPointXE - WorldPointYE,FVector2D(0,0),Line.Color,Line.HitProxyColor); // 2E
						ThickVertices[5+ 6] = FSimpleElementVertex(Line.End - WorldPointXE + WorldPointYE,FVector2D(0,1),Line.Color,Line.HitProxyColor); // 3E

						// First part of line
						ThickVertices[0+12] = FSimpleElementVertex(Line.Start - WorldPointXS - WorldPointYS,FVector2D(0,0),Line.Color,Line.HitProxyColor); // 2S
						ThickVertices[1+12] = FSimpleElementVertex(Line.Start + WorldPointXS + WorldPointYS,FVector2D(1,1),Line.Color,Line.HitProxyColor); // 1S
						ThickVertices[2+12] = FSimpleElementVertex(Line.End   - WorldPointXE - WorldPointYE,FVector2D(0,0),Line.Color,Line.HitProxyColor); // 2E

						ThickVertices[3+12] = FSimpleElementVertex(Line.Start + WorldPointXS + WorldPointYS,FVector2D(1,1),Line.Color,Line.HitProxyColor); // 1S
						ThickVertices[4+12] = FSimpleElementVertex(Line.End   + WorldPointXE + WorldPointYE,FVector2D(1,1),Line.Color,Line.HitProxyColor); // 1E
						ThickVertices[5+12] = FSimpleElementVertex(Line.End   - WorldPointXE - WorldPointYE,FVector2D(0,0),Line.Color,Line.HitProxyColor); // 2E

						// Second part of line
						ThickVertices[0+18] = FSimpleElementVertex(Line.Start - WorldPointXS + WorldPointYS,FVector2D(0,1),Line.Color,Line.HitProxyColor); // 3S
						ThickVertices[1+18] = FSimpleElementVertex(Line.Start + WorldPointXS - WorldPointYS,FVector2D(1,0),Line.Color,Line.HitProxyColor); // 0S
						ThickVertices[2+18] = FSimpleElementVertex(Line.End   - WorldPointXE + WorldPointYE,FVector2D(0,1),Line.Color,Line.HitProxyColor); // 3E

						ThickVertices[3+18] = FSimpleElementVertex(Line.Start + WorldPointXS - WorldPointYS,FVector2D(1,0),Line.Color,Line.HitProxyColor); // 0S
						ThickVertices[4+18] = FSimpleElementVertex(Line.End   + WorldPointXE - WorldPointYE,FVector2D(1,0),Line.Color,Line.HitProxyColor); // 0E
						ThickVertices[5+18] = FSimpleElementVertex(Line.End   - WorldPointXE + WorldPointYE,FVector2D(0,1),Line.Color,Line.HitProxyColor); // 3E

						ThickVertices += 24;
					}

					RHICmdList.UnlockBuffer(VertexBufferRHI);
					RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
					RHICmdList.DrawPrimitive(0, 8 * NumLinesThisBatch, 1);
				}

				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			}
			// Draw the wireframe triangles.
			if (WireTris.Num() > 0)
			{
				check(WireTriVerts.Num() == WireTris.Num() * 3);

				FRHIResourceCreateInfo CreateInfo(TEXT("WireTris"), &WireTriVerts);
				FBufferRHIRef VertexBufferRHI = RHICmdList.CreateBuffer(WireTriVerts.GetResourceDataSize(), BUF_VertexBuffer | BUF_Volatile, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);

				RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);

				const bool bEnableMSAA = true;
				const bool bEnableLineAA = false;
				FRasterizerStateInitializerRHI Initializer(FM_Wireframe, CM_None, bEnableMSAA, bEnableLineAA);

				int32 MaxVerticesAllowed = ((GDrawUPVertexCheckCount / sizeof(FSimpleElementVertex)) / 3) * 3;
				/*
				hack to avoid a crash when trying to render large numbers of line segments.
				*/
				MaxVerticesAllowed = FMath::Min(MaxVerticesAllowed, 64 * 1024);

				const int32 MaxTrisAllowed = MaxVerticesAllowed / 3;

				int32 MinTri=0;
				int32 TotalTris = WireTris.Num();
				while( MinTri < TotalTris )
				{
					int32 MaxTri = FMath::Min(MinTri + MaxTrisAllowed, TotalTris);
					float DepthBias = WireTris[MinTri].DepthBias;
					for (int32 i = MinTri + 1; i < MaxTri; ++i)
					{
						if (DepthBias != WireTris[i].DepthBias)
						{
							MaxTri = i;
							break;
						}
					}

					Initializer.DepthBias = DepthBias;
					auto RasterState = RHICreateRasterizerState(Initializer);
					GraphicsPSOInit.RasterizerState = RasterState.GetReference();
					PrepareShaders(RHICmdList, GraphicsPSOInit, StencilRef, FeatureLevel, SE_BLEND_Opaque, RelativeMatrices, BatchedElementParameters, GWhiteTexture, bHitTesting, Gamma, NULL, &View);

					int32 NumTris = MaxTri - MinTri;
					RHICmdList.DrawPrimitive(MinTri * 3, NumTris, 1);
					MinTri = MaxTri;
				}
				VertexBufferRHI.SafeRelease();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			}
		}

		// Draw the sprites.
		if( Sprites.Num() > 0 )
		{
			// Sprites don't support batched element parameters (yet!)
			FBatchedElementParameters* BatchedElementParameters = NULL;

			//Sort sprites by blend mode and texture
			struct FCompareSprite
			{
				explicit FCompareSprite(EBlendModeFilter::Type InFilter) : ValidBlendFilter(InFilter) {}

				FORCEINLINE bool operator()( const FBatchedElements::FBatchedSprite& SpriteA, const FBatchedElements::FBatchedSprite& SpriteB ) const
				{
					if (SpriteA.BlendMode != SpriteB.BlendMode)
					{
						const bool bBlendAValid = (ValidBlendFilter & SpriteA.BlendMode) != 0;
						const bool bBlendBValid = (ValidBlendFilter & SpriteB.BlendMode) != 0;
						if (bBlendAValid != bBlendBValid) return bBlendAValid; // we want valid blend modes to sort in front of invalid blend modes
						return SpriteA.BlendMode < SpriteB.BlendMode;
					}
					return SpriteA.Texture < SpriteB.Texture;
				}

				EBlendModeFilter::Type ValidBlendFilter;
			};

			TArray<FBatchedSprite> SortedSprites = Sprites;

			SortedSprites.Sort(FCompareSprite(Filter));

			// count the number of sprites that have valid blend modes
			// (they have been sorted to the front of the list)
			int32 ValidSpriteCount = 0;
			while (ValidSpriteCount < SortedSprites.Num() &&
				(Filter & GetBlendModeFilter((ESimpleElementBlendMode)SortedSprites[ValidSpriteCount].BlendMode)))
			{
				++ValidSpriteCount;
			}

			if (ValidSpriteCount > 0)
			{
				// Prevent integer overflow to buffer overflow.
				if (ValidSpriteCount > UINT32_MAX / sizeof(FSimpleElementVertex) * 6)
				{
					UE_LOG(LogBatchedElements, Error, TEXT("Too many sprites. Will overflow uint32 buffer size. ValidSpriteCount: %d"), ValidSpriteCount);
					return false;
				}

				FRHIResourceCreateInfo CreateInfo(TEXT("Sprites"));
				FBufferRHIRef VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FSimpleElementVertex) * ValidSpriteCount * 6, BUF_VertexBuffer | BUF_Volatile, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
				void* VoidPtr = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FSimpleElementVertex) * ValidSpriteCount * 6, RLM_WriteOnly);
				FSimpleElementVertex* SpriteList = reinterpret_cast<FSimpleElementVertex*>(VoidPtr);

				for (int32 SpriteIndex = 0; SpriteIndex < ValidSpriteCount; SpriteIndex++)
				{
					const FBatchedSprite& Sprite = SortedSprites[SpriteIndex];
					FSimpleElementVertex* Vertex = &SpriteList[SpriteIndex * 6];

					// Compute the sprite vertices.
					const FVector WorldSpriteX = CameraX * Sprite.SizeX;
					const FVector WorldSpriteY = CameraY * -Sprite.SizeY * GProjectionSignY;

					const float UStart = Sprite.U / Sprite.Texture->GetSizeX();
					const float UEnd = (Sprite.U + Sprite.UL) / Sprite.Texture->GetSizeX();
					const float VStart = Sprite.V / Sprite.Texture->GetSizeY();
					const float VEnd = (Sprite.V + Sprite.VL) / Sprite.Texture->GetSizeY();

					Vertex[0] = FSimpleElementVertex(Sprite.Position + WorldSpriteX - WorldSpriteY, FVector2D(UEnd, VStart), Sprite.Color, Sprite.HitProxyColor);
					Vertex[1] = FSimpleElementVertex(Sprite.Position + WorldSpriteX + WorldSpriteY, FVector2D(UEnd, VEnd), Sprite.Color, Sprite.HitProxyColor);
					Vertex[2] = FSimpleElementVertex(Sprite.Position - WorldSpriteX - WorldSpriteY, FVector2D(UStart, VStart), Sprite.Color, Sprite.HitProxyColor);

					Vertex[3] = FSimpleElementVertex(Sprite.Position + WorldSpriteX + WorldSpriteY, FVector2D(UEnd, VEnd), Sprite.Color, Sprite.HitProxyColor);
					Vertex[4] = FSimpleElementVertex(Sprite.Position - WorldSpriteX - WorldSpriteY, FVector2D(UStart, VStart), Sprite.Color, Sprite.HitProxyColor);
					Vertex[5] = FSimpleElementVertex(Sprite.Position - WorldSpriteX + WorldSpriteY, FVector2D(UStart, VEnd), Sprite.Color, Sprite.HitProxyColor);
				}
				RHICmdList.UnlockBuffer(VertexBufferRHI);

				//First time init
				const FTexture* CurrentTexture = SortedSprites[0].Texture;
				ESimpleElementBlendMode CurrentBlendMode = (ESimpleElementBlendMode)SortedSprites[0].BlendMode;
				int32 BatchStartIndex = 0;
				float CurrentOpacityMask = SortedSprites[0].OpacityMaskRefVal;

				// Start loop at 1, since we've already started the first batch with the first sprite in the list
				for (int32 SpriteIndex = 1; SpriteIndex < ValidSpriteCount + 1; SpriteIndex++)
				{
					// Need to flush the current batch once we hit the end of the list, or if state of this sprite doesn't match current batch
					if (SpriteIndex == ValidSpriteCount ||
						CurrentTexture != SortedSprites[SpriteIndex].Texture ||
						CurrentBlendMode != SortedSprites[SpriteIndex].BlendMode ||
						CurrentOpacityMask != SortedSprites[SpriteIndex].OpacityMaskRefVal)
					{
						const int32 SpriteNum = SpriteIndex - BatchStartIndex;
						const int32 BaseVertex = BatchStartIndex * 6;
						const int32 PrimCount = SpriteNum * 2;
						PrepareShaders(RHICmdList, GraphicsPSOInit, StencilRef, FeatureLevel, CurrentBlendMode, RelativeMatrices, BatchedElementParameters, CurrentTexture, bHitTesting, Gamma, NULL, &View, CurrentOpacityMask);
						RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
						RHICmdList.DrawPrimitive(BaseVertex, PrimCount, 1);

						// begin the next batch
						if (SpriteIndex < ValidSpriteCount)
						{
							BatchStartIndex = SpriteIndex;
							CurrentTexture = SortedSprites[SpriteIndex].Texture;
							CurrentBlendMode = (ESimpleElementBlendMode)SortedSprites[SpriteIndex].BlendMode;
							CurrentOpacityMask = SortedSprites[SpriteIndex].OpacityMaskRefVal;
						}
					}
				}
				VertexBufferRHI.SafeRelease();
			}
		}

		if( MeshElements.Num() > 0)
		{
			// Prevent integer overflow to buffer overflow.
			if (MeshVertices.Num() > UINT32_MAX / sizeof(FSimpleElementVertex))
			{
				UE_LOG(LogBatchedElements, Error, TEXT("Too many mesh vertices. Will overflow uint32 buffer size. MeshVertices.Num(): %d"), MeshVertices.Num());
				return false;
			}

			FRHIResourceCreateInfo CreateInfo(TEXT("MeshElements"));
			FBufferRHIRef VertexBufferRHI = RHICmdList.CreateBuffer(sizeof(FSimpleElementVertex) * MeshVertices.Num(), BUF_VertexBuffer | BUF_Volatile, 0, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
			void* VoidPtr = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FSimpleElementVertex) * MeshVertices.Num(), RLM_WriteOnly);
			FPlatformMemory::Memcpy(VoidPtr, MeshVertices.GetData(), sizeof(FSimpleElementVertex) * MeshVertices.Num());
			RHICmdList.UnlockBuffer(VertexBufferRHI);

			// Draw the mesh elements.
			for(int32 MeshIndex = 0;MeshIndex < MeshElements.Num();MeshIndex++)
			{
				const FBatchedMeshElement& MeshElement = MeshElements[MeshIndex];
				const EBlendModeFilter::Type MeshFilter = GetBlendModeFilter(MeshElement.BlendMode);

				// Only render blend modes in the filter
				if (Filter & MeshFilter)
				{
					// UE-50111
					checkf(
						MeshElement.Texture && MeshElement.Texture->SamplerStateRHI.GetReference(),
						TEXT("MeshElement.Texture=%p, bInitialized=%d, name=%s"),
						MeshElement.Texture,
						MeshElement.Texture ? MeshElement.Texture->IsInitialized() : -1,
						MeshElement.Texture ? *MeshElement.Texture->GetFriendlyName() : TEXT(""));

					// Set the appropriate pixel shader for the mesh.
					PrepareShaders(RHICmdList, GraphicsPSOInit, StencilRef, FeatureLevel, MeshElement.BlendMode, RelativeMatrices, MeshElement.BatchedElementParameters, MeshElement.Texture, bHitTesting, Gamma, &MeshElement.GlowInfo, &View);

					FBufferRHIRef IndexBufferRHI = RHICmdList.CreateBuffer(sizeof(uint16) * MeshElement.Indices.Num(), BUF_IndexBuffer | BUF_Volatile, sizeof(uint16), ERHIAccess::VertexOrIndexBuffer, CreateInfo);
					void* VoidPtr2 = RHICmdList.LockBuffer(IndexBufferRHI, 0, sizeof(uint16) * MeshElement.Indices.Num(), RLM_WriteOnly);
					FPlatformMemory::Memcpy(VoidPtr2, MeshElement.Indices.GetData(), sizeof(uint16) * MeshElement.Indices.Num());
					RHICmdList.UnlockBuffer(IndexBufferRHI);

					// Draw the mesh.
					RHICmdList.SetStreamSource(0, VertexBufferRHI, MeshElement.MinVertex * sizeof(FSimpleElementVertex));
					RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, MeshElement.MaxVertex - MeshElement.MinVertex + 1, 0, MeshElement.Indices.Num() / 3, 1);

					IndexBufferRHI.SafeRelease();
				}
			}

			VertexBufferRHI.SafeRelease();
		}

		return true;
	}
	else
	{
		return false;
	}
}

void FBatchedElements::Clear()
{
	LineVertices.Empty();
	Points.Empty();
	Sprites.Empty();
	MeshElements.Empty();
	ThickLines.Empty();
	MeshVertices.Empty();
}
