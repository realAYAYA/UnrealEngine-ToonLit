// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/PositionVertexBuffer.h"

#include "CoreMinimal.h"
#include "RHI.h"
#include "Components.h"

#include "StaticMeshVertexData.h"
#include "GPUSkinCache.h"

/*-----------------------------------------------------------------------------
FPositionVertexBuffer
-----------------------------------------------------------------------------*/

/** The implementation of the static mesh position-only vertex data storage type. */
class FPositionVertexData :
	public TStaticMeshVertexData<FPositionVertex>
{
public:
	FPositionVertexData(bool InNeedsCPUAccess = false)
		: TStaticMeshVertexData<FPositionVertex>(InNeedsCPUAccess)
	{
	}
};


FPositionVertexBuffer::FPositionVertexBuffer():
	VertexData(NULL),
	Data(NULL),
	Stride(0),
	NumVertices(0)
{}

FPositionVertexBuffer::~FPositionVertexBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FPositionVertexBuffer::CleanUp()
{
	if (VertexData)
	{
		delete VertexData;
		VertexData = nullptr;
	}
}

void FPositionVertexBuffer::Init(uint32 InNumVertices, bool bInNeedsCPUAccess)
{
	NumVertices = InNumVertices;
	bNeedsCPUAccess = bInNeedsCPUAccess;

	// Allocate the vertex data storage type.
	AllocateData(bInNeedsCPUAccess);

	// Allocate the vertex data buffer.
	VertexData->ResizeBuffer(NumVertices);
	Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
}

/**
* Initializes the buffer with the given vertices, used to convert legacy layouts.
* @param InVertices - The vertices to initialize the buffer with.
*/
void FPositionVertexBuffer::Init(const TArray<FStaticMeshBuildVertex>& InVertices, bool bInNeedsCPUAccess)
{
	Init(InVertices.Num(), bInNeedsCPUAccess);

	// Copy the vertices into the buffer.
	for(int32 VertexIndex = 0;VertexIndex < InVertices.Num();VertexIndex++)
	{
		const FStaticMeshBuildVertex& SourceVertex = InVertices[VertexIndex];
		const uint32 DestVertexIndex = VertexIndex;
		VertexPosition(DestVertexIndex) = SourceVertex.Position;
	}
}

/**
* Initializes this vertex buffer with the contents of the given vertex buffer.
* @param InVertexBuffer - The vertex buffer to initialize from.
*/
void FPositionVertexBuffer::Init(const FPositionVertexBuffer& InVertexBuffer, bool bInNeedsCPUAccess)
{
	bNeedsCPUAccess = bInNeedsCPUAccess;
	if ( InVertexBuffer.GetNumVertices() )
	{
		Init(InVertexBuffer.GetNumVertices(), bInNeedsCPUAccess);

		check( Stride == InVertexBuffer.GetStride() );

		const uint8* InData = InVertexBuffer.Data;
		FMemory::Memcpy( Data, InData, Stride * NumVertices );
	}
}

void FPositionVertexBuffer::Init(const TArray<FVector3f>& InPositions, bool bInNeedsCPUAccess)
{
	NumVertices = InPositions.Num();
	bNeedsCPUAccess = bInNeedsCPUAccess;
	if ( NumVertices )
	{
		AllocateData(bInNeedsCPUAccess);
		check( Stride == InPositions.GetTypeSize() );
		VertexData->ResizeBuffer(NumVertices);
		Data = VertexData->GetDataPointer();
		FMemory::Memcpy( Data, InPositions.GetData(), Stride * NumVertices );
	}
}

void FPositionVertexBuffer::AppendVertices( const FStaticMeshBuildVertex* Vertices, const uint32 NumVerticesToAppend )
{
	if (VertexData == nullptr && NumVerticesToAppend > 0)
	{
		// Allocate the vertex data storage type if the buffer was never allocated before
		AllocateData(bNeedsCPUAccess);
	}

	if( NumVerticesToAppend > 0 )
	{
		check( VertexData != nullptr );
		check( Vertices != nullptr );

		const uint32 FirstDestVertexIndex = NumVertices;
		NumVertices += NumVerticesToAppend;
		VertexData->ResizeBuffer( NumVertices );
		if( NumVertices > 0 )
		{
			Data = VertexData->GetDataPointer();

			// Copy the vertices into the buffer.
			for( uint32 VertexIter = 0; VertexIter < NumVerticesToAppend; ++VertexIter )
			{
				const FStaticMeshBuildVertex& SourceVertex = Vertices[ VertexIter ];

				const uint32 DestVertexIndex = FirstDestVertexIndex + VertexIter;
				VertexPosition( DestVertexIndex ) = SourceVertex.Position;
			}
		}
	}
}

/**
 * Serializer
 *
 * @param	Ar					Archive to serialize with
 * @param	bInNeedsCPUAccess	Whether the elements need to be accessed by the CPU
 */
void FPositionVertexBuffer::Serialize( FArchive& Ar, bool bInNeedsCPUAccess )
{
	bNeedsCPUAccess = bInNeedsCPUAccess;

	SerializeMetaData(Ar);

	if(Ar.IsLoading())
	{
		// Allocate the vertex data storage type.
		AllocateData( bInNeedsCPUAccess );
	}

	if(VertexData != NULL)
	{
		// Serialize the vertex data.
		VertexData->Serialize(Ar);

		// Make a copy of the vertex data pointer.
		Data = NumVertices ? VertexData->GetDataPointer() : nullptr;
	}
}

void FPositionVertexBuffer::SerializeMetaData(FArchive& Ar)
{
	Ar << Stride << NumVertices;
}

void FPositionVertexBuffer::ClearMetaData()
{
	Stride = NumVertices = 0;
}

bool FPositionVertexBuffer::GetAllowCPUAccess() const
{
	return VertexData ? VertexData->GetAllowCPUAccess() : false;
}

/**
* Specialized assignment operator, only used when importing LOD's.  
*/
void FPositionVertexBuffer::operator=(const FPositionVertexBuffer &Other)
{
	//VertexData doesn't need to be allocated here because Build will be called next,
	VertexData = NULL;
}

template <bool bRenderThread>
FBufferRHIRef FPositionVertexBuffer::CreateRHIBuffer_Internal()
{
	return CreateRHIBuffer<bRenderThread>(VertexData, NumVertices, BUF_Static | BUF_ShaderResource, TEXT("FPositionVertexBuffer"));
}

FBufferRHIRef FPositionVertexBuffer::CreateRHIBuffer_RenderThread()
{
	return CreateRHIBuffer_Internal<true>();
}

FBufferRHIRef FPositionVertexBuffer::CreateRHIBuffer_Async()
{
	return CreateRHIBuffer_Internal<false>();
}

/** Copy everything, keeping reference to the same RHI resources. */
void FPositionVertexBuffer::CopyRHIForStreaming(const FPositionVertexBuffer& Other, bool InAllowCPUAccess)
{
	// Copy serialized properties.
	Stride = Other.Stride;
	NumVertices = Other.NumVertices;

	// Handle CPU access.
	if (InAllowCPUAccess)
	{
		bNeedsCPUAccess = Other.bNeedsCPUAccess;
		AllocateData(bNeedsCPUAccess);
	}
	else
	{
		bNeedsCPUAccess = false;
	}

	// Copy resource references.
	VertexBufferRHI = Other.VertexBufferRHI;
	PositionComponentSRV = Other.PositionComponentSRV;
}

void FPositionVertexBuffer::InitRHI()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPositionVertexBuffer::InitRHI);

	const bool bHadVertexData = VertexData != nullptr;
	VertexBufferRHI = CreateRHIBuffer_RenderThread();
	// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
	// Also check to see whether cpu access has been activated on the vertex data
	if (VertexBufferRHI)
	{
		// we have decide to create the SRV based on GMaxRHIShaderPlatform because this is created once and shared between feature levels for editor preview.
		bool bSRV = RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinCacheAvailable(GMaxRHIShaderPlatform);

		// When bAllowCPUAccess is true, the meshes is likely going to be used for Niagara to spawn particles on mesh surface.
		// And it can be the case for CPU *and* GPU access: no differenciation today. That is why we create a SRV in this case.
		// This also avoid setting lots of states on all the members of all the different buffers used by meshes. Follow up: https://jira.it.epicgames.net/browse/UE-69376.
		bSRV |= (VertexData && VertexData->GetAllowCPUAccess());
		if(bSRV)
		{
			// When VertexData is null, this buffer hasn't been streamed in yet. We still need to create a FRHIShaderResourceView which will be
			// cached in a vertex factory uniform buffer later. The nullptr tells the RHI that the SRV doesn't view on anything yet.
			PositionComponentSRV = RHICreateShaderResourceView(FShaderResourceViewInitializer(bHadVertexData ? VertexBufferRHI : nullptr, PF_R32_FLOAT));
		}
	}
}

void FPositionVertexBuffer::ReleaseRHI()
{
	PositionComponentSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

void FPositionVertexBuffer::AllocateData( bool bInNeedsCPUAccess /*= true*/ )
{
	// Clear any old VertexData before allocating.
	CleanUp();

	VertexData = new FPositionVertexData(bInNeedsCPUAccess);
	// Calculate the vertex stride.
	Stride = VertexData->GetStride();
}

void FPositionVertexBuffer::BindPositionVertexBuffer(const FVertexFactory* VertexFactory, FStaticMeshDataType& StaticMeshData) const
{
	StaticMeshData.PositionComponent = FVertexStreamComponent(
		this,
		STRUCT_OFFSET(FPositionVertex, Position),
		GetStride(),
		VET_Float3
	);
	StaticMeshData.PositionComponentSRV = PositionComponentSRV;
}
