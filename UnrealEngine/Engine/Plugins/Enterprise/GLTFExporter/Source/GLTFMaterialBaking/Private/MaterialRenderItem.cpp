// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialRenderItem.h"
#include "GLTFMaterialBakingStructures.h"

#include "EngineModule.h"
#include "StaticMeshAttributes.h"
#include "DynamicMeshBuilder.h"
#include "MeshPassProcessor.h"
#include "CanvasRender.h"

#define SHOW_WIREFRAME_MESH 0

FGLTFMeshMaterialRenderItem::FGLTFMeshMaterialRenderItem(
	const FIntPoint& InTextureSize,
	const FGLTFMeshRenderData* InMeshSettings,
	FDynamicMeshBufferAllocator* InDynamicMeshBufferAllocator)
	: MeshSettings(InMeshSettings)
	, TextureSize(InTextureSize)
	, MaterialRenderProxy(nullptr)
	, ViewFamily(nullptr)
	, bMeshElementDirty(true)
	, DynamicMeshBufferAllocator(InDynamicMeshBufferAllocator)
{
	GenerateRenderData();
	LCI = new FGLTFMeshRenderInfo(InMeshSettings->LightMap, nullptr, nullptr, InMeshSettings->LightmapResourceCluster);
}

bool FGLTFMeshMaterialRenderItem::Render_RenderThread(FCanvasRenderContext& RenderContext, FMeshPassProcessorRenderState& DrawRenderState, const FCanvas* Canvas)
{
	checkSlow(ViewFamily && MeshSettings && MaterialRenderProxy);
	// current render target set for the canvas
	const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
	const FIntRect ViewRect(FIntPoint(0, 0), CanvasRenderTarget->GetSizeXY());

	// make a temporary view
	FSceneViewInitOptions ViewInitOptions;
	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.SetViewRectangle(ViewRect);
	ViewInitOptions.ViewOrigin = FVector::ZeroVector;
	ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
	ViewInitOptions.ProjectionMatrix = Canvas->GetTransformStack().Top().GetMatrix();
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.OverlayColor = FLinearColor::White;

	FSceneView View(ViewInitOptions);
	View.FinalPostProcessSettings.bOverride_IndirectLightingIntensity = 1;
	View.FinalPostProcessSettings.IndirectLightingIntensity = 0.0f;

	// TODO: The 2 lines below has been disabled because of compilation errors:
	//		RHINeedsToSwitchVerticalAxis does nt exist anymore
	//		GetAllowSwitchVerticalAxis is not a member of FCanvas anymore
	//const bool bNeedsToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(Canvas->GetShaderPlatform()) && !Canvas->GetAllowSwitchVerticalAxis();
	//check(bNeedsToSwitchVerticalAxis == false);

	if (Vertices.Num() && Indices.Num())
	{
		FMeshPassProcessorRenderState LocalDrawRenderState;

		// disable depth test & writes
		LocalDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
		LocalDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		QueueMaterial(RenderContext, LocalDrawRenderState, &View);
	}

	return true;
}

bool FGLTFMeshMaterialRenderItem::Render_GameThread(const FCanvas* Canvas, FCanvasRenderThreadScope& RenderScope)
{
	RenderScope.EnqueueRenderCommand(
		[this, Canvas](FCanvasRenderContext& RenderContext)
		{
			// Render_RenderThread uses its own render state
			FMeshPassProcessorRenderState DummyRenderState;
			Render_RenderThread(RenderContext, DummyRenderState, Canvas);
		}
	);

	return true;
}

void FGLTFMeshMaterialRenderItem::GenerateRenderData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMaterialRenderItem::GenerateRenderData)

	// Reset array without resizing
	Vertices.SetNum(0, false);
	Indices.SetNum(0, false);
	if (MeshSettings->MeshDescription)
	{
		// Use supplied FMeshDescription data to populate render data
		PopulateWithMeshData();
	}
	else
	{
		// Use simple rectangle
		PopulateWithQuadData();
	}

	bMeshElementDirty = true;
}

FGLTFMeshMaterialRenderItem::~FGLTFMeshMaterialRenderItem()
{
	// Send the release of the buffers to the render thread
	ENQUEUE_RENDER_COMMAND(ReleaseResources)(
		[ToRelease = MoveTemp(MeshBuilderResources)](FRHICommandListImmediate& RHICmdList) {}
	);
}

void FGLTFMeshMaterialRenderItem::QueueMaterial(FCanvasRenderContext& RenderContext, FMeshPassProcessorRenderState& DrawRenderState, const FSceneView* View)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMeshMaterialRenderItem::QueueMaterial)

	if (bMeshElementDirty)
	{
		MeshBuilderResources.Clear();
		FDynamicMeshBuilder DynamicMeshBuilder(View->GetFeatureLevel(), MAX_STATIC_TEXCOORDS, MeshSettings->LightMapIndex, false, DynamicMeshBufferAllocator);
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CopyData);
			DynamicMeshBuilder.AddVertices(Vertices);
			DynamicMeshBuilder.AddTriangles(Indices);
		}

		DynamicMeshBuilder.GetMeshElement(FMatrix::Identity, MaterialRenderProxy, SDPG_Foreground, true, false, 0, MeshBuilderResources, MeshElement);

		// NOTE: Since we can't pass these parameters during creation, we overwrite them here
		const FGLTFPrimitiveData* PrimitiveData = MeshSettings->PrimitiveData;
		if (PrimitiveData != nullptr)
		{
			FPrimitiveUniformShaderParameters PrimitiveParams = FPrimitiveUniformShaderParametersBuilder{}
				.Defaults()
					.LocalToWorld(FMatrix::Identity)
					.ActorWorldPosition(PrimitiveData->ActorPosition)
					.WorldBounds(PrimitiveData->ObjectBounds)
					.LocalBounds(PrimitiveData->ObjectLocalBounds)
					.PreSkinnedLocalBounds(PrimitiveData->PreSkinnedLocalBounds)
					.CustomPrimitiveData(PrimitiveData->CustomPrimitiveData)
					.ReceivesDecals(false)
					//.DrawsVelocity(false)
				.Build();

			// Overwrite object orientation, since original calculation is derived from LocalToWorld matrix,
			// which currently needs to be identity matrix to avoid transforming the "2D" vertices used for baking
			PrimitiveParams.ObjectOrientation = FVector3f(PrimitiveData->ObjectOrientation);

			const_cast<TUniformBuffer<FPrimitiveUniformShaderParameters>*>(MeshElement.Elements[0].PrimitiveUniformBufferResource)->SetContents(PrimitiveParams);
		}

		check(MeshBuilderResources.IsValidForRendering());
		bMeshElementDirty = false;
	}

	MeshElement.MaterialRenderProxy = MaterialRenderProxy;

	LCI->CreatePrecomputedLightingUniformBuffer_RenderingThread(View->GetFeatureLevel());
	MeshElement.LCI = LCI;

#if SHOW_WIREFRAME_MESH
	MeshElement.bWireframe = true;
#endif

	const int32 NumTris = FMath::TruncToInt((float)Indices.Num() / 3);
	if (NumTris == 0)
	{
		// there's nothing to do here
		return;
	}

	// Bake the material out to a tile
	GetRendererModule().DrawTileMesh(RenderContext, DrawRenderState, *View, MeshElement, false /*bIsHitTesting*/, FHitProxyId());
}

void FGLTFMeshMaterialRenderItem::PopulateWithQuadData()
{
	Vertices.Empty(4);
	Indices.Empty(6);

	const float U = MeshSettings->TextureCoordinateBox.Min.X;
	const float V = MeshSettings->TextureCoordinateBox.Min.Y;
	const float SizeU = MeshSettings->TextureCoordinateBox.Max.X - MeshSettings->TextureCoordinateBox.Min.X;
	const float SizeV = MeshSettings->TextureCoordinateBox.Max.Y - MeshSettings->TextureCoordinateBox.Min.Y;
	const float ScaleX = TextureSize.X;
	const float ScaleY = TextureSize.Y;

	// add vertices
	for (int32 VertIndex = 0; VertIndex < 4; VertIndex++)
	{
		FDynamicMeshVertex* Vert = new(Vertices)FDynamicMeshVertex();
		const int32 X = VertIndex & 1;
		const int32 Y = (VertIndex >> 1) & 1;
		Vert->Position.Set(ScaleX * X, ScaleY * Y, 0);
		Vert->SetTangents(FVector3f(1, 0, 0), FVector3f(0, 1, 0), FVector3f(0, 0, 1));
		FMemory::Memzero(&Vert->TextureCoordinate, sizeof(Vert->TextureCoordinate));
		for (int32 TexcoordIndex = 0; TexcoordIndex < MAX_STATIC_TEXCOORDS; TexcoordIndex++)
		{
			Vert->TextureCoordinate[TexcoordIndex].Set(U + SizeU * X, V + SizeV * Y);
		}
		Vert->Color = FColor::White;
	}

	// add indices
	static const uint32 TriangleIndices[6] = { 0, 2, 1, 2, 3, 1 };
	Indices.Append(TriangleIndices, 6);
}

void FGLTFMeshMaterialRenderItem::PopulateWithMeshData()
{
	const FMeshDescription* RawMesh = MeshSettings->MeshDescription;

	FStaticMeshConstAttributes Attributes(*RawMesh);
	TArrayView<const FVector3f> VertexPositions = Attributes.GetVertexPositions().GetRawArray();
	TArrayView<const FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals().GetRawArray();
	TArrayView<const FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents().GetRawArray();
	TArrayView<const float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns().GetRawArray();
	TArrayView<const FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors().GetRawArray();

	TVertexInstanceAttributesConstRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
	const int32 NumVerts = RawMesh->Vertices().Num();

	// reserve renderer data
	Vertices.Empty(NumVerts);
	Indices.Empty(NumVerts >> 1);

	const float ScaleX = TextureSize.X;
	const float ScaleY = TextureSize.Y;

	const static int32 VertexPositionStoredUVChannel = 6;
	// count number of texture coordinates for this mesh
	const int32 NumTexcoords = [&]()
	{
		return FMath::Min(VertexInstanceUVs.GetNumChannels(), VertexPositionStoredUVChannel);
	}();

	// check if we should use NewUVs or original UV set
	const bool bUseNewUVs = MeshSettings->CustomTextureCoordinates.Num() > 0;
	if (bUseNewUVs)
	{
		check(MeshSettings->CustomTextureCoordinates.Num() == VertexInstanceUVs.GetNumElements() && VertexInstanceUVs.GetNumChannels() > MeshSettings->TextureCoordinateIndex);
	}

	// add vertices
	int32 VertIndex = 0;
	int32 FaceIndex = 0;
	for(const FTriangleID& TriangleID : RawMesh->Triangles().GetElementIDs())
	{
		const FPolygonGroupID PolygonGroupID = RawMesh->GetTrianglePolygonGroup(TriangleID);
		if (MeshSettings->MaterialIndices.Contains(PolygonGroupID.GetValue()))
		{
			const int32 NUM_VERTICES = 3;
			for (int32 Corner = 0; Corner < NUM_VERTICES; Corner++)
			{
				// Swap vertices order if mesh is mirrored
				const int32 CornerIdx = !MeshSettings->bMirrored ? Corner : NUM_VERTICES - Corner - 1;

				const int32 SrcVertIndex = FaceIndex * NUM_VERTICES + CornerIdx;
				const FVertexInstanceID SrcVertexInstanceID = RawMesh->GetTriangleVertexInstance(TriangleID, Corner);
				const FVertexID SrcVertexID = RawMesh->GetVertexInstanceVertex(SrcVertexInstanceID);

				// add vertex
				FDynamicMeshVertex* Vert = new(Vertices)FDynamicMeshVertex();
				if (!bUseNewUVs)
				{
					// compute vertex position from original UV
					const FVector2D& UV = FVector2D(VertexInstanceUVs.Get(SrcVertexInstanceID, MeshSettings->TextureCoordinateIndex));
					Vert->Position.Set(UV.X * ScaleX, UV.Y * ScaleY, 0);
				}
				else
				{
					const FVector2D& UV = MeshSettings->CustomTextureCoordinates[SrcVertIndex];
					Vert->Position.Set(UV.X * ScaleX, UV.Y * ScaleY, 0);
				}
				FVector3f TangentX = VertexInstanceTangents[SrcVertexInstanceID];
				FVector3f TangentZ = VertexInstanceNormals[SrcVertexInstanceID];
				FVector3f TangentY = FVector3f::CrossProduct(TangentZ, TangentX).GetSafeNormal() * VertexInstanceBinormalSigns[SrcVertexInstanceID];
				Vert->SetTangents(TangentX, TangentY, TangentZ);
				for (int32 TexcoordIndex = 0; TexcoordIndex < NumTexcoords; TexcoordIndex++)
				{
					Vert->TextureCoordinate[TexcoordIndex] = VertexInstanceUVs.Get(SrcVertexInstanceID, TexcoordIndex);
				}

				if (NumTexcoords < VertexPositionStoredUVChannel)
				{
					for (int32 TexcoordIndex = NumTexcoords; TexcoordIndex < VertexPositionStoredUVChannel; TexcoordIndex++)
					{
						Vert->TextureCoordinate[TexcoordIndex] = Vert->TextureCoordinate[FMath::Max(NumTexcoords - 1, 0)];
					}
				}
				// Store original vertex positions in texture coordinate data
				Vert->TextureCoordinate[6].X = VertexPositions[SrcVertexID].X;
				Vert->TextureCoordinate[6].Y = VertexPositions[SrcVertexID].Y;
				Vert->TextureCoordinate[7].X = VertexPositions[SrcVertexID].Z;
				Vert->TextureCoordinate[7].Y = 0.0f;

				Vert->Color = FLinearColor(VertexInstanceColors[SrcVertexInstanceID]).ToFColor(true);
				// add index
				Indices.Add(VertIndex);
				VertIndex++;
			}
		}
		FaceIndex++;
	}
}
