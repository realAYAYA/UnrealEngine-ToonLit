// Copyright Epic Games, Inc. All Rights Reserved. 

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/ColorVertexBuffer.h"
#include "DynamicMeshBuilder.h"
#include "Components/BaseDynamicMeshComponent.h"
#include "RayTracingGeometry.h"

#include "PhysicsEngine/AggregateGeom.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FDynamicMeshAttributeSet;
using UE::Geometry::FDynamicMeshUVOverlay;
using UE::Geometry::FDynamicMeshNormalOverlay;
using UE::Geometry::FDynamicMeshColorOverlay;
using UE::Geometry::FDynamicMeshMaterialAttribute;

class FDynamicPrimitiveUniformBuffer;
class FMaterialRenderProxy;
class UMaterialInterface;
struct FRayTracingMaterialGatheringContext;

/**
 * FMeshRenderBufferSet stores a set of RenderBuffers for a mesh
 */
class FMeshRenderBufferSet
{
public:
	/** Number of triangles in this renderbuffer set. Note that triangles may be split between IndexBuffer and SecondaryIndexBuffer. */
	int TriangleCount = 0;

	/** The buffer containing vertex data. */
	FStaticMeshVertexBuffer StaticMeshVertexBuffer;
	/** The buffer containing the position vertex data. */
	FPositionVertexBuffer PositionVertexBuffer;
	/** The buffer containing the vertex color data. */
	FColorVertexBuffer ColorVertexBuffer;

	/** triangle indices */
	FDynamicMeshIndexBuffer32 IndexBuffer;

	/** vertex factory */
	FLocalVertexFactory VertexFactory;

	/** Material to draw this mesh with */
	UMaterialInterface* Material = nullptr;

	/**
	 * Optional list of triangles stored in this buffer. Storing this allows us
	 * to rebuild the buffers if vertex data changes.
	 */
	TOptional<TArray<int>> Triangles;

	/**
	 * If secondary index buffer is enabled, we populate this index buffer with additional triangles indexing into the same vertex buffers
	 */
	bool bEnableSecondaryIndexBuffer = false;

	/**
	 * partition or subset of IndexBuffer that indexes into same vertex buffers
	 */
	FDynamicMeshIndexBuffer32 SecondaryIndexBuffer;

	/**
	 * configure whether raytracing should be enabled for this RenderBufferSet
	 */
	bool bEnableRaytracing = false;

#if RHI_RAYTRACING
	/**
	 * Raytracing buffers
	 */
	FRayTracingGeometry PrimaryRayTracingGeometry;
	FRayTracingGeometry SecondaryRayTracingGeometry;
	bool bIsRayTracingDataValid = false;
#endif

	/**
	 * In situations where we want to *update* the existing Vertex or Index buffers, we need to synchronize
	 * access between the Game and Render threads. We use this lock to do that.
	 */
	FCriticalSection BuffersLock;


	FMeshRenderBufferSet(ERHIFeatureLevel::Type FeatureLevelType)
		: VertexFactory(FeatureLevelType, "FMeshRenderBufferSet")
	{
		StaticMeshVertexBuffer.SetUseFullPrecisionUVs(true);
		StaticMeshVertexBuffer.SetUseHighPrecisionTangentBasis(true);
	}


	virtual ~FMeshRenderBufferSet()
	{
		if (TriangleCount > 0)
		{
			PositionVertexBuffer.ReleaseResource();
			StaticMeshVertexBuffer.ReleaseResource();
			ColorVertexBuffer.ReleaseResource();
			VertexFactory.ReleaseResource();
			if (IndexBuffer.IsInitialized())
			{
				IndexBuffer.ReleaseResource();
			}
			if (SecondaryIndexBuffer.IsInitialized())
			{
				SecondaryIndexBuffer.ReleaseResource();
			}

#if RHI_RAYTRACING
			if (bEnableRaytracing)
			{
				PrimaryRayTracingGeometry.ReleaseResource();
				SecondaryRayTracingGeometry.ReleaseResource();
			}
#endif
		}
	}


	/**
	 * Upload initialized mesh buffers. 
	 * @warning This can only be called on the Rendering Thread.
	 */
	void Upload()
	{
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		if (TriangleCount == 0)
		{
			return;
		}

		InitOrUpdateResource(RHICmdList, &this->PositionVertexBuffer);
		InitOrUpdateResource(RHICmdList, &this->StaticMeshVertexBuffer);
		InitOrUpdateResource(RHICmdList, &this->ColorVertexBuffer);

		FLocalVertexFactory::FDataType Data;
		this->PositionVertexBuffer.BindPositionVertexBuffer(&this->VertexFactory, Data);
		this->StaticMeshVertexBuffer.BindTangentVertexBuffer(&this->VertexFactory, Data);
		this->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&this->VertexFactory, Data);
		// currently no lightmaps support
		//this->StaticMeshVertexBuffer.BindLightMapVertexBuffer(&this->VertexFactory, Data, LightMapIndex);
		this->ColorVertexBuffer.BindColorVertexBuffer(&this->VertexFactory, Data);
		this->VertexFactory.SetData(RHICmdList, Data);

		InitOrUpdateResource(RHICmdList, &this->VertexFactory);
		PositionVertexBuffer.InitResource(RHICmdList);
		StaticMeshVertexBuffer.InitResource(RHICmdList);
		ColorVertexBuffer.InitResource(RHICmdList);
		VertexFactory.InitResource(RHICmdList);

		if (IndexBuffer.Indices.Num() > 0)
		{
			IndexBuffer.InitResource(RHICmdList);
		}
		if (bEnableSecondaryIndexBuffer && SecondaryIndexBuffer.Indices.Num() > 0)
		{
			SecondaryIndexBuffer.InitResource(RHICmdList);
		}

		InvalidateRayTracingData();
		ValidateRayTracingData();		// currently we are immediately validating. This may be revisited in future.
	}


	/**
	 * Fast path to only update the primary and secondary index buffers. This can be used
	 * when (eg) the secondary index buffer is being used to highlight/hide a subset of triangles.
	 * @warning This can only be called on the Rendering Thread.
	 */
	void UploadIndexBufferUpdate()
	{
		// todo: can this be done with RHI locking and memcpy, like in TransferVertexUpdateToGPU?
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		if (IndexBuffer.Indices.Num() > 0)
		{
			InitOrUpdateResource(RHICmdList, &IndexBuffer);
		}
		if (bEnableSecondaryIndexBuffer && SecondaryIndexBuffer.Indices.Num() > 0)
		{
			InitOrUpdateResource(RHICmdList, &SecondaryIndexBuffer);
		}

		InvalidateRayTracingData();
		ValidateRayTracingData();		// currently we are immediately validating. This may be revisited in future.
	}


	/**
	 * Fast path to only update vertex buffers. This path rebuilds all the
	 * resources and reconfigures the vertex factory, so the counts/etc could be modified.
	 * @warning This can only be called on the Rendering Thread.
	 */
	void UploadVertexUpdate(bool bPositions, bool bMeshAttribs, bool bColors)
	{
		// todo: look at calls to this function, it seems possible that TransferVertexUpdateToGPU
		// could be used instead (which should be somewhat more efficient?). It's not clear if there
		// are any situations where we would change vertex buffer size w/o also updating the index
		// buffers (in which case we are fully rebuilding the buffers...)

		if (TriangleCount == 0)
		{
			return;
		}

		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

		if (bPositions)
		{
			InitOrUpdateResource(RHICmdList, &this->PositionVertexBuffer);
		}
		if (bMeshAttribs)
		{
			InitOrUpdateResource(RHICmdList, &this->StaticMeshVertexBuffer);
		}
		if (bColors)
		{
			InitOrUpdateResource(RHICmdList, &this->ColorVertexBuffer);
		}

		FLocalVertexFactory::FDataType Data;
		this->PositionVertexBuffer.BindPositionVertexBuffer(&this->VertexFactory, Data);
		this->StaticMeshVertexBuffer.BindTangentVertexBuffer(&this->VertexFactory, Data);
		this->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&this->VertexFactory, Data);
		this->ColorVertexBuffer.BindColorVertexBuffer(&this->VertexFactory, Data);
		this->VertexFactory.SetData(RHICmdList, Data);

		InitOrUpdateResource(RHICmdList, &this->VertexFactory);

		InvalidateRayTracingData();
		ValidateRayTracingData();		// currently we are immediately validating. This may be revisited in future.
	}


	/**
	 * Fast path to update various vertex buffers. This path does not support changing the
	 * size/counts of any of the sub-buffers, a direct memcopy from the CPU-side buffer to the RHI buffer is used.
	 * @warning This can only be called on the Rendering Thread.
	 */
	void TransferVertexUpdateToGPU(FRHICommandListBase& RHICmdList, bool bPositions, bool bNormals, bool bTexCoords, bool bColors)
	{
		if (TriangleCount == 0)
		{
			return;
		}

		if (bPositions)
		{
			FPositionVertexBuffer& VertexBuffer = this->PositionVertexBuffer;
			void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHICmdList.UnlockBuffer(VertexBuffer.VertexBufferRHI);
		}
		if (bNormals)
		{
			FStaticMeshVertexBuffer& VertexBuffer = this->StaticMeshVertexBuffer;
			void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTangentSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
			RHICmdList.UnlockBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
		}
		if (bColors)
		{
			FColorVertexBuffer& VertexBuffer = this->ColorVertexBuffer;
			void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			RHICmdList.UnlockBuffer(VertexBuffer.VertexBufferRHI);
		}
		if (bTexCoords)
		{
			FStaticMeshVertexBuffer& VertexBuffer = this->StaticMeshVertexBuffer;
			void* VertexBufferData = RHICmdList.LockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTexCoordSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), VertexBuffer.GetTexCoordSize());
			RHICmdList.UnlockBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
		}

		InvalidateRayTracingData();
		ValidateRayTracingData();		// currently we are immediately validating. This may be revisited in future.
	}

	UE_DEPRECATED(5.3, "TransferVertexUpdateToGPU now requires a command list")
	void TransferVertexUpdateToGPU(bool bPositions, bool bNormals, bool bTexCoords, bool bColors)
	{
		TransferVertexUpdateToGPU(FRHICommandListImmediate::Get(), bPositions, bNormals, bTexCoords, bColors);
	}

	void InvalidateRayTracingData()
	{
#if RHI_RAYTRACING
		bIsRayTracingDataValid = false;
#endif
	}

	// Verify that valid raytracing data is available. This will cause a rebuild of the
	// raytracing data if any of our buffers have been modified. Currently this is called
	// by GetDynamicRayTracingInstances to ensure the RT data is available when needed.
	void ValidateRayTracingData()
	{
#if RHI_RAYTRACING
		if (bIsRayTracingDataValid == false && IsRayTracingEnabled() && bEnableRaytracing)
		{
			UpdateRaytracingGeometryIfEnabled();

			bIsRayTracingDataValid = true;
		}
#endif
	}


protected:

	// rebuild raytracing data for current buffers
	void UpdateRaytracingGeometryIfEnabled()
	{
#if RHI_RAYTRACING
		// do we always want to do this?
		PrimaryRayTracingGeometry.ReleaseResource();		
		SecondaryRayTracingGeometry.ReleaseResource();
		FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
			
		for (int32 k = 0; k < 2; ++k)
		{
			FDynamicMeshIndexBuffer32& UseIndexBuffer = (k == 0) ? IndexBuffer : SecondaryIndexBuffer;
			if (UseIndexBuffer.Indices.Num() == 0)
			{
				continue;
			}

			FRayTracingGeometry& RayTracingGeometry = (k == 0) ? PrimaryRayTracingGeometry : SecondaryRayTracingGeometry;

			FRayTracingGeometryInitializer Initializer;
			Initializer.IndexBuffer = UseIndexBuffer.IndexBufferRHI;
			Initializer.TotalPrimitiveCount = UseIndexBuffer.Indices.Num() / 3;
			Initializer.GeometryType = RTGT_Triangles;
			Initializer.bFastBuild = true;
			Initializer.bAllowUpdate = false;

			FRayTracingGeometrySegment Segment;
			Segment.VertexBuffer = PositionVertexBuffer.VertexBufferRHI;
			Segment.NumPrimitives = Initializer.TotalPrimitiveCount;
			Segment.MaxVertices = PositionVertexBuffer.GetNumVertices();

			Initializer.Segments.Add(Segment);

			RayTracingGeometry.SetInitializer(MoveTemp(Initializer));
			RayTracingGeometry.InitResource(RHICmdList);
		}
#endif
	}



	/**
	 * Initializes a render resource, or update it if already initialized.
	 * @warning This function can only be called on the Render Thread
	 */
	void InitOrUpdateResource(FRHICommandListBase& RHICmdList, FRenderResource* Resource)
	{
		if (!Resource->IsInitialized())
		{
			Resource->InitResource(RHICmdList);
		}
		else
		{
			Resource->UpdateRHI(RHICmdList);
		}
	}




protected:
	friend class FBaseDynamicMeshSceneProxy;

	/**
	 * Enqueue a command on the Render Thread to destroy the passed in buffer set.
	 * At this point the buffer set should be considered invalid.
	 */
	static void DestroyRenderBufferSet(FMeshRenderBufferSet* BufferSet)
	{
		if (BufferSet->TriangleCount == 0)
		{
			return;
		}

		delete BufferSet;
	}


};




/**
 * FBaseDynamicMeshSceneProxy is an abstract base class for a Render Proxy
 * for a UBaseDynamicMeshComponent, where the assumption is that mesh data
 * will be stored in FMeshRenderBufferSet instances
 */
class FBaseDynamicMeshSceneProxy : public FPrimitiveSceneProxy
{
	using FIndex2i = UE::Geometry::FIndex2i;
	using FIndex3i = UE::Geometry::FIndex3i;
public:
	UBaseDynamicMeshComponent* ParentBaseComponent;

	/**
	 * Constant color assigned to vertices if no other vertex color is specified
	 */
	FColor ConstantVertexColor = FColor::White;

	/**
	 * If true, vertex colors on the FDynamicMesh will be ignored
	 */
	bool bIgnoreVertexColors = false;

	/**
	 * If true, a per-triangle color is used to set vertex colors
	 */
	bool bUsePerTriangleColor = false;

	/**
	 * Per-triangle color function. Only called if bUsePerTriangleColor=true
	 */
	TFunction<FColor(const FDynamicMesh3*, int)> PerTriangleColorFunc = nullptr;


	/**
	 * If true, VertexColorRemappingFunc is called on Vertex Colors provided from Mesh to remap them to a different color
	 */
	bool bApplyVertexColorRemapping = false;

	/**
	 * Vertex color remapping function. Only called if bApplyVertexColorRemapping == true, for mesh vertex colors
	 */
	TUniqueFunction<void(FVector4f&)> VertexColorRemappingFunc = nullptr;

	/**
	 * Color Space Transform/Conversion applied to Vertex Colors provided from Mesh Color Overlay Attribute
	 * Color Space Conversion is applied after any Vertex Color Remapping.
	 */
	EDynamicMeshVertexColorTransformMode ColorSpaceTransformMode = EDynamicMeshVertexColorTransformMode::NoTransform;

	/**
	* If true, a facet normals are used instead of mesh normals
	*/
	bool bUsePerTriangleNormals = false;

	/**
	 * If true, populate secondary buffers using SecondaryTriFilterFunc
	 */
	bool bUseSecondaryTriBuffers = false;

	/**
	 * Filter predicate for secondary triangle index buffer. Only called if bUseSecondaryTriBuffers=true
	 */
	TUniqueFunction<bool(const FDynamicMesh3*, int32)> SecondaryTriFilterFunc = nullptr;

protected:
	// Set of currently-allocated RenderBuffers. We own these pointers and must clean them up.
	// Must guard access with AllocatedSetsLock!!
	TSet<FMeshRenderBufferSet*> AllocatedBufferSets;

	// use to control access to AllocatedBufferSets 
	FCriticalSection AllocatedSetsLock;

	// control raytracing support
	bool bEnableRaytracing = false;

	// Allow view-mode overrides. 
	bool bEnableViewModeOverrides = true;

public:
	GEOMETRYFRAMEWORK_API FBaseDynamicMeshSceneProxy(UBaseDynamicMeshComponent* Component);

	GEOMETRYFRAMEWORK_API virtual ~FBaseDynamicMeshSceneProxy();


	//
	// FBaseDynamicMeshSceneProxy API - subclasses must implement these functions
	//


	/**
	 * Return set of active renderbuffers. Must be implemented by subclass.
	 * This is the set of render buffers that will be drawn by GetDynamicMeshElements
	 */
	virtual void GetActiveRenderBufferSets(TArray<FMeshRenderBufferSet*>& Buffers) const = 0;



	//
	// RenderBuffer management
	//


	/**
	 * Allocates a set of render buffers. FPrimitiveSceneProxy will keep track of these
	 * buffers and destroy them on destruction.
	 */
	GEOMETRYFRAMEWORK_API virtual FMeshRenderBufferSet* AllocateNewRenderBufferSet();

	/**
	 * Explicitly release a set of RenderBuffers
	 */
	GEOMETRYFRAMEWORK_API virtual void ReleaseRenderBufferSet(FMeshRenderBufferSet* BufferSet);


	/**
	 * Initialize rendering buffers from given attribute overlays.
	 * Creates three vertices per triangle, IE no shared vertices in buffers.
	 */
	template<typename TriangleEnumerable>
	void InitializeBuffersFromOverlays(
		FMeshRenderBufferSet* RenderBuffers,
		const FDynamicMesh3* Mesh,
		int NumTriangles, TriangleEnumerable Enumerable,
		const FDynamicMeshUVOverlay* UVOverlay,
		const FDynamicMeshNormalOverlay* NormalOverlay,
		const FDynamicMeshColorOverlay* ColorOverlay,
		TFunctionRef<void(int, int, int, const FVector3f&, FVector3f&, FVector3f&)> TangentsFunc,
		bool bTrackTriangles = false)
	{
		TArray<const FDynamicMeshUVOverlay*> UVOverlays;
		UVOverlays.Add(UVOverlay);
		InitializeBuffersFromOverlays(RenderBuffers, Mesh, NumTriangles, Enumerable,
			UVOverlays, NormalOverlay, ColorOverlay, TangentsFunc, bTrackTriangles);
	}



	/**
	 * Initialize rendering buffers from given attribute overlays.
	 * Creates three vertices per triangle, IE no shared vertices in buffers.
	 */
	template<typename TriangleEnumerable, typename UVOverlayListAllocator>
	void InitializeBuffersFromOverlays(
		FMeshRenderBufferSet* RenderBuffers,
		const FDynamicMesh3* Mesh,
		int NumTriangles, TriangleEnumerable Enumerable,
		const TArray<const FDynamicMeshUVOverlay*, UVOverlayListAllocator>& UVOverlays,
		const FDynamicMeshNormalOverlay* NormalOverlay,
		const FDynamicMeshColorOverlay* ColorOverlay,
		TFunctionRef<void(int, int, int, const FVector3f&, FVector3f&, FVector3f&)> TangentsFunc,
		bool bTrackTriangles = false)
	{
		RenderBuffers->TriangleCount = NumTriangles;
		if (NumTriangles == 0)
		{
			return;
		}

		bool bHaveColors = (ColorOverlay != nullptr) && (bIgnoreVertexColors == false);

		int NumVertices = NumTriangles * 3;
		int NumUVOverlays = UVOverlays.Num();
		int NumTexCoords = FMath::Max(1, NumUVOverlays);		// must have at least one tex coord
		TArray<FIndex3i, TFixedAllocator<MAX_STATIC_TEXCOORDS>> UVTriangles;
		UVTriangles.SetNum(NumTexCoords);

		{
			RenderBuffers->PositionVertexBuffer.Init(NumVertices);
			RenderBuffers->StaticMeshVertexBuffer.Init(NumVertices, NumTexCoords );
			RenderBuffers->ColorVertexBuffer.Init(NumVertices);
			RenderBuffers->IndexBuffer.Indices.AddUninitialized(NumTriangles * 3);
		}

		// build triangle list if requested, or if we are using secondary buffers in which case we need it to filter later
		bool bBuildTriangleList = bTrackTriangles || bUseSecondaryTriBuffers;
		if (bBuildTriangleList)
		{
			RenderBuffers->Triangles = TArray<int32>();
		}

		int TriIdx = 0, VertIdx = 0;
		FVector3f TangentX, TangentY;
		for (int TriangleID : Enumerable)
		{
			FIndex3i Tri = Mesh->GetTriangle(TriangleID);
			for (int32 k = 0; k < NumTexCoords; ++k)
			{
				UVTriangles[k] = (k < NumUVOverlays && UVOverlays[k] != nullptr) ? UVOverlays[k]->GetTriangle(TriangleID) : FIndex3i::Invalid();
			}
			FIndex3i TriNormal = (NormalOverlay != nullptr) ? NormalOverlay->GetTriangle(TriangleID) : FIndex3i::Zero();
			FIndex3i TriColor = (ColorOverlay != nullptr) ? ColorOverlay->GetTriangle(TriangleID) : FIndex3i::Zero();

			FColor UniformTriColor = ConstantVertexColor;
			if (bUsePerTriangleColor && PerTriangleColorFunc != nullptr)
			{
				UniformTriColor = PerTriangleColorFunc(Mesh, TriangleID);
				bHaveColors = false;
			}

			for (int j = 0; j < 3; ++j)
			{
				RenderBuffers->PositionVertexBuffer.VertexPosition(VertIdx) = (FVector3f)Mesh->GetVertex(Tri[j]);

				FVector3f Normal;
				if (bUsePerTriangleNormals)
				{
					Normal = (FVector3f)Mesh->GetTriNormal(TriangleID);
				}
				else
				{
					Normal = (NormalOverlay != nullptr && TriNormal[j] != FDynamicMesh3::InvalidID) ?
						NormalOverlay->GetElement(TriNormal[j]) : Mesh->GetVertexNormal(Tri[j]);
				}

				// get tangents
				TangentsFunc(Tri[j], TriangleID, j, Normal, TangentX, TangentY);

				RenderBuffers->StaticMeshVertexBuffer.SetVertexTangents(VertIdx, TangentX, TangentY, Normal);

				for (int32 k = 0; k < NumTexCoords; ++k)
				{
					FVector2f UV = (UVTriangles[k][j] != FDynamicMesh3::InvalidID) ?
						UVOverlays[k]->GetElement(UVTriangles[k][j]) : FVector2f::Zero();
					RenderBuffers->StaticMeshVertexBuffer.SetVertexUV(VertIdx, k, UV);
				}

				FColor VertexFColor = (bHaveColors && TriColor[j] != FDynamicMesh3::InvalidID) ?
					GetOverlayColorAsFColor(ColorOverlay, TriColor[j]) : UniformTriColor;

				RenderBuffers->ColorVertexBuffer.VertexColor(VertIdx) = VertexFColor;

				RenderBuffers->IndexBuffer.Indices[TriIdx++] = VertIdx;		// currently TriIdx == VertIdx so we don't really need both...
				VertIdx++;
			}

			if (bBuildTriangleList)
			{
				RenderBuffers->Triangles->Add(TriangleID);
			}
		}

		// split triangles into secondary buffer (at bit redudant since we just built IndexBuffer, but we may optionally duplicate triangles in the future
		if (bUseSecondaryTriBuffers)
		{
			RenderBuffers->bEnableSecondaryIndexBuffer = true;
			UpdateSecondaryTriangleBuffer(RenderBuffers, Mesh, false);
		}
	}


	FColor GetOverlayColorAsFColor(
		const FDynamicMeshColorOverlay* ColorOverlay,
		int32 ElementID)
	{
		checkSlow(ColorOverlay);
		FVector4f UseColor = ColorOverlay->GetElement(ElementID);

		if (bApplyVertexColorRemapping)
		{
			VertexColorRemappingFunc(UseColor);
		}

		if (ColorSpaceTransformMode == EDynamicMeshVertexColorTransformMode::SRGBToLinear)
		{
			// is there a better way to do this? 
			FColor QuantizedSRGBColor = ((FLinearColor)UseColor).ToFColor(false);
			return FLinearColor(QuantizedSRGBColor).ToFColor(false);
		}
		else
		{
			bool bConvertToSRGB = (ColorSpaceTransformMode == EDynamicMeshVertexColorTransformMode::LinearToSRGB);
			return ((FLinearColor)UseColor).ToFColor(bConvertToSRGB);
		}
	}


	/**
	 * Filter the triangles in a FMeshRenderBufferSet into the SecondaryIndexBuffer.
	 * Requires that RenderBuffers->Triangles has been initialized.
	 * @param bDuplicate if set, then primary IndexBuffer is unmodified and SecondaryIndexBuffer contains duplicates. Otherwise triangles are sorted via predicate into either primary or secondary.
	 */
	void UpdateSecondaryTriangleBuffer(
		FMeshRenderBufferSet* RenderBuffers,
		const FDynamicMesh3* Mesh,
		bool bDuplicate)
	{
		if (ensure(bUseSecondaryTriBuffers == true && RenderBuffers->Triangles.IsSet()) == false)
		{
			return;
		}

		const TArray<int32>& TriangleIDs = RenderBuffers->Triangles.GetValue();
		int NumTris = TriangleIDs.Num();
		TArray<uint32>& Indices = RenderBuffers->IndexBuffer.Indices;
		TArray<uint32>& SecondaryIndices = RenderBuffers->SecondaryIndexBuffer.Indices;

		RenderBuffers->SecondaryIndexBuffer.Indices.Reset();
		if (bDuplicate == false)
		{
			RenderBuffers->IndexBuffer.Indices.Reset();
		}
		for ( int k = 0; k < NumTris; ++k)
		{
			int TriangleID = TriangleIDs[k];
			bool bInclude = SecondaryTriFilterFunc(Mesh, TriangleID);
			if (bInclude)
			{
				SecondaryIndices.Add(3*k);
				SecondaryIndices.Add(3*k + 1);
				SecondaryIndices.Add(3*k + 2);
			} 
			else if (bDuplicate == false)
			{
				Indices.Add(3*k);
				Indices.Add(3*k + 1);
				Indices.Add(3*k + 2);
			}
		}
	}


	/**
	 * RecomputeRenderBufferTriangleIndexSets re-sorts the existing set of triangles in a FMeshRenderBufferSet
	 * into primary and secondary index buffers. Note that UploadIndexBufferUpdate() must be called
	 * after this function!
	 */
	void RecomputeRenderBufferTriangleIndexSets(
		FMeshRenderBufferSet* RenderBuffers, 
		const FDynamicMesh3* Mesh)
	{
		if (RenderBuffers->TriangleCount == 0)
		{
			return;
		}
		if (ensure(RenderBuffers->Triangles.IsSet() && RenderBuffers->Triangles->Num() > 0) == false)
		{
			return;
		}

		//bool bDuplicate = false;		// flag for future use, in case we want to draw all triangles in primary and duplicates in secondary...
		RenderBuffers->IndexBuffer.Indices.Reset();
		RenderBuffers->SecondaryIndexBuffer.Indices.Reset();
		
		TArray<uint32>& Indices = RenderBuffers->IndexBuffer.Indices;
		TArray<uint32>& SecondaryIndices = RenderBuffers->SecondaryIndexBuffer.Indices;
		const TArray<int32>& TriangleIDs = RenderBuffers->Triangles.GetValue();

		int NumTris = TriangleIDs.Num();
		for (int k = 0; k < NumTris; ++k)
		{
			int TriangleID = TriangleIDs[k];
			bool bInclude = SecondaryTriFilterFunc(Mesh, TriangleID);
			if (bInclude)
			{
				SecondaryIndices.Add(3 * k);
				SecondaryIndices.Add(3 * k + 1);
				SecondaryIndices.Add(3 * k + 2);
			}
			else // if (bDuplicate == false)
			{
				Indices.Add(3 * k);
				Indices.Add(3 * k + 1);
				Indices.Add(3 * k + 2);
			}
		}
	}



	/**
	 * Update vertex positions/normals/colors of an existing set of render buffers.
	 * Assumes that buffers were created with unshared vertices, ie three vertices per triangle, eg by InitializeBuffersFromOverlays()
	 */
	template<typename TriangleEnumerable>
	void UpdateVertexBuffersFromOverlays(
		FMeshRenderBufferSet* RenderBuffers,
		const FDynamicMesh3* Mesh,
		int NumTriangles, TriangleEnumerable Enumerable,
		const FDynamicMeshNormalOverlay* NormalOverlay,
		const FDynamicMeshColorOverlay* ColorOverlay,
		TFunctionRef<void(int, int, int, const FVector3f&, FVector3f&, FVector3f&)> TangentsFunc,
		bool bUpdatePositions = true,
		bool bUpdateNormals = false,
		bool bUpdateColors = false)
	{
		if (RenderBuffers->TriangleCount == 0)
		{
			return;
		}

		bool bHaveColors = (ColorOverlay != nullptr) && (bIgnoreVertexColors == false);

		int NumVertices = NumTriangles * 3;
		if ( (bUpdatePositions && ensure(RenderBuffers->PositionVertexBuffer.GetNumVertices() == NumVertices) == false )
			|| (bUpdateNormals && ensure(RenderBuffers->StaticMeshVertexBuffer.GetNumVertices() == NumVertices) == false )
			|| (bUpdateColors && ensure(RenderBuffers->ColorVertexBuffer.GetNumVertices() == NumVertices) == false ) )
		{
			return;
		}

		int VertIdx = 0;
		FVector3f TangentX, TangentY;
		for (int TriangleID : Enumerable)
		{
			FIndex3i Tri = Mesh->GetTriangle(TriangleID);

			FIndex3i TriNormal = (bUpdateNormals && NormalOverlay != nullptr) ? NormalOverlay->GetTriangle(TriangleID) : FIndex3i::Zero();
			FIndex3i TriColor = (bUpdateColors && ColorOverlay != nullptr) ? ColorOverlay->GetTriangle(TriangleID) : FIndex3i::Zero();

			FColor UniformTriColor = ConstantVertexColor;
			if (bUpdateColors && bUsePerTriangleColor && PerTriangleColorFunc != nullptr)
			{
				UniformTriColor = PerTriangleColorFunc(Mesh, TriangleID);
				bHaveColors = false;
			}

			for (int j = 0; j < 3; ++j)
			{
				if (bUpdatePositions)
				{
					RenderBuffers->PositionVertexBuffer.VertexPosition(VertIdx) = (FVector3f)Mesh->GetVertex(Tri[j]);
				}

				if (bUpdateNormals)
				{
					// get normal and tangent
					FVector3f Normal;
					if (bUsePerTriangleNormals)
					{
						Normal = (FVector3f)Mesh->GetTriNormal(TriangleID);
					}
					else
					{
						Normal = (NormalOverlay != nullptr && TriNormal[j] != FDynamicMesh3::InvalidID) ?
							NormalOverlay->GetElement(TriNormal[j]) : Mesh->GetVertexNormal(Tri[j]);
					}

					TangentsFunc(Tri[j], TriangleID, j, Normal, TangentX, TangentY);

					RenderBuffers->StaticMeshVertexBuffer.SetVertexTangents(VertIdx, (FVector3f)TangentX, (FVector3f)TangentY, (FVector3f)Normal);
				}

				if (bUpdateColors)
				{
					FColor VertexFColor = (bHaveColors  && TriColor[j] != FDynamicMesh3::InvalidID) ?
						GetOverlayColorAsFColor(ColorOverlay, TriColor[j]) : UniformTriColor;
					RenderBuffers->ColorVertexBuffer.VertexColor(VertIdx) = VertexFColor;
				}

				VertIdx++;
			}
		}
	}

	/**
	 * Update vertex uvs of an existing set of render buffers.
	 * Assumes that buffers were created with unshared vertices, ie three vertices per triangle, eg by InitializeBuffersFromOverlays()
	 */
	template<typename TriangleEnumerable, typename UVOverlayListAllocator>
	void UpdateVertexUVBufferFromOverlays(
		FMeshRenderBufferSet* RenderBuffers,
		const FDynamicMesh3* Mesh,
		int32 NumTriangles, TriangleEnumerable Enumerable,
		const TArray<const FDynamicMeshUVOverlay*, UVOverlayListAllocator>& UVOverlays)
	{
		// We align the update to the way we set UV's in InitializeBuffersFromOverlays.

		if (RenderBuffers->TriangleCount == 0)
		{
			return;
		}
		int NumVertices = NumTriangles * 3;
		if (ensure(RenderBuffers->StaticMeshVertexBuffer.GetNumVertices() == NumVertices) == false)
		{
			return;
		}

		int NumUVOverlays = UVOverlays.Num();
		int NumTexCoords = RenderBuffers->StaticMeshVertexBuffer.GetNumTexCoords();
		if (!ensure(NumUVOverlays <= NumTexCoords))
		{
			return;
		}

		// Temporarily stores the UV element indices for all UV channels of a single triangle
		TArray<FIndex3i, TFixedAllocator<MAX_STATIC_TEXCOORDS>> UVTriangles;
		UVTriangles.SetNum(NumTexCoords);

		int VertIdx = 0;
		for (int TriangleID : Enumerable)
		{
			for (int32 k = 0; k < NumTexCoords; ++k)
			{
				UVTriangles[k] = (k < NumUVOverlays && UVOverlays[k] != nullptr) ? UVOverlays[k]->GetTriangle(TriangleID) : FIndex3i::Invalid();
			}

			for (int j = 0; j < 3; ++j)
			{
				for (int32 k = 0; k < NumTexCoords; ++k)
				{
					FVector2f UV = (UVTriangles[k][j] != FDynamicMesh3::InvalidID) ?
						UVOverlays[k]->GetElement(UVTriangles[k][j]) : FVector2f::Zero();
					RenderBuffers->StaticMeshVertexBuffer.SetVertexUV(VertIdx, k, UV);
				}

				++VertIdx;
			}
		}
	}


	/**
	 * @return number of active materials
	 */
	GEOMETRYFRAMEWORK_API virtual int32 GetNumMaterials() const;

	/**
	 * Safe GetMaterial function that will never return nullptr
	 */
	GEOMETRYFRAMEWORK_API virtual UMaterialInterface* GetMaterial(int32 k) const;

	/**
	 * Set whether or not to validate mesh batch materials against the component materials.
	 */
	void SetVerifyUsedMaterials(const bool bState)
	{
		bVerifyUsedMaterials = bState;
	}


	/**
	 * This needs to be called if the set of active materials changes, otherwise
	 * the check in FPrimitiveSceneProxy::VerifyUsedMaterial() will fail if an override
	 * material is set, if materials change, etc, etc
	 */
	GEOMETRYFRAMEWORK_API virtual void UpdatedReferencedMaterials();


	//
	// FBaseDynamicMeshSceneProxy implementation
	//

	/**
	 * If EngineShowFlags request vertex color rendering, returns the appropriate vertex color override material's render proxy.  Otherwise returns nullptr.
	 */
	GEOMETRYFRAMEWORK_API static FMaterialRenderProxy* GetEngineVertexColorMaterialProxy(FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags, bool bProxyIsSelected, bool bIsHovered);

	/**
	 * Render set of active RenderBuffers returned by GetActiveRenderBufferSets
	 */
	GEOMETRYFRAMEWORK_API virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views, 
		const FSceneViewFamily& ViewFamily, 
		uint32 VisibilityMap, 
		FMeshElementCollector& Collector) const override;

protected:
	/**
	 * Helper called by GetDynamicMeshElements to process collision debug drawing
	 */
	GEOMETRYFRAMEWORK_API virtual void GetCollisionDynamicMeshElements(TArray<FMeshRenderBufferSet*>& Buffers,
		const FEngineShowFlags& EngineShowFlags, bool bDrawCollisionView, bool bDrawSimpleCollision, bool bDrawComplexCollision,
		bool bProxyIsSelected,
		const TArray<const FSceneView*>& Views, uint32 VisibilityMap,
		FMeshElementCollector& Collector) const;
public:

	/**
	 * Draw a single-frame FMeshBatch for a FMeshRenderBufferSet
	 */
	GEOMETRYFRAMEWORK_API virtual void DrawBatch(FMeshElementCollector& Collector,
		const FMeshRenderBufferSet& RenderBuffers,
		const FDynamicMeshIndexBuffer32& IndexBuffer,
		FMaterialRenderProxy* UseMaterial,
		bool bWireframe,
		ESceneDepthPriorityGroup DepthPriority,
		int ViewIndex,
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer) const;

#if RHI_RAYTRACING

	GEOMETRYFRAMEWORK_API virtual bool IsRayTracingRelevant() const override;
	GEOMETRYFRAMEWORK_API virtual bool HasRayTracingRepresentation() const override;

	GEOMETRYFRAMEWORK_API virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override;


	/**
	* Draw a single-frame raytracing FMeshBatch for a FMeshRenderBufferSet
	*/
	GEOMETRYFRAMEWORK_API virtual void DrawRayTracingBatch(
		FRayTracingMaterialGatheringContext& Context,
		const FMeshRenderBufferSet& RenderBuffers,
		const FDynamicMeshIndexBuffer32& IndexBuffer,
		FRayTracingGeometry& RayTracingGeometry,
		FMaterialRenderProxy* UseMaterialProxy,
		ESceneDepthPriorityGroup DepthPriority,
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer,
		TArray<FRayTracingInstance>& OutRayTracingInstances) const;


#endif // RHI_RAYTRACING

public:
	// Set the collision data to use for debug drawing, or do nothing if debug drawing is not enabled
	GEOMETRYFRAMEWORK_API void SetCollisionData();

#if UE_ENABLE_DEBUG_DRAWING
private:
	// If debug drawing is enabled, we store collision data here so that collision shapes can be rendered when requested by showflags

	bool bOwnerIsNull = true;
	/** Whether the collision data has been set up for rendering */
	bool bHasCollisionData = false;
	/** Collision trace flags */
	ECollisionTraceFlag		CollisionTraceFlag;
	/** Collision Response of this component */
	FCollisionResponseContainer CollisionResponse;
	/** Cached AggGeom holding the collision shapes to render */
	FKAggregateGeom CachedAggGeom;

#endif

	GEOMETRYFRAMEWORK_API bool IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const;

};
