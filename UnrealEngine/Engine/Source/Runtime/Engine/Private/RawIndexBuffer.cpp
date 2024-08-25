// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RawIndexBuffer.cpp: Raw index buffer implementation.
=============================================================================*/

#include "RawIndexBuffer.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Interfaces/ITargetPlatform.h"
#include "RenderUtils.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "RHIResourceUpdates.h"

#if WITH_EDITOR
#include "MeshUtilities.h"
template<typename IndexDataType, typename Allocator>
void CacheOptimizeIndexBuffer(TArray<IndexDataType,Allocator>& Indices)
{
	TArray<IndexDataType> TempIndices(Indices);
	IMeshUtilities& MeshUtilities = FModuleManager::LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	MeshUtilities.CacheOptimizeIndexBuffer(TempIndices);
	Indices = TempIndices;
}
#endif // #if WITH_EDITOR

/*-----------------------------------------------------------------------------
FRawIndexBuffer
-----------------------------------------------------------------------------*/

/**
* Orders a triangle list for better vertex cache coherency.
*/
void FRawIndexBuffer::CacheOptimize()
{
#if WITH_EDITOR
	CacheOptimizeIndexBuffer(Indices);
#endif
}

void FRawIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	uint32 Size = Indices.Num() * sizeof(uint16);
	if( Size > 0 )
	{
		// Create the index buffer.
		FRHIResourceCreateInfo CreateInfo(TEXT("FRawIndexBuffer"));
		IndexBufferRHI = RHICmdList.CreateBuffer(Size, BUF_Static | BUF_IndexBuffer, sizeof(uint16), ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);

		// Initialize the buffer.
		void* Buffer = RHICmdList.LockBuffer(IndexBufferRHI, 0, Size, RLM_WriteOnly);
		FMemory::Memcpy(Buffer, Indices.GetData(), Size);
		RHICmdList.UnlockBuffer(IndexBufferRHI);
	}
}

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I)
{
	I.Indices.BulkSerialize( Ar );
	return Ar;
}

/*-----------------------------------------------------------------------------
	FRawIndexBuffer16or32
-----------------------------------------------------------------------------*/

/**
* Orders a triangle list for better vertex cache coherency.
*/
void FRawIndexBuffer16or32::CacheOptimize()
{
#if WITH_EDITOR
	CacheOptimizeIndexBuffer(Indices);
#endif
}

void FRawIndexBuffer16or32::ComputeIndexWidth()
{
	if (GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		const int32 NumIndices = Indices.Num();
		bool bShouldUse32Bit = false;
		int32 i = 0;
		while (!bShouldUse32Bit && i < NumIndices)
		{
			bShouldUse32Bit = Indices[i] > MAX_uint16;
			i++;
		}
	
		b32Bit = bShouldUse32Bit;
	}
	else
	{
		b32Bit = true;
	}
}

void FRawIndexBuffer16or32::InitRHI(FRHICommandListBase& RHICmdList)
{
	const int32 IndexStride = b32Bit ? sizeof(uint32) : sizeof(uint16);
	const int32 NumIndices = Indices.Num();
	const uint32 Size = NumIndices * IndexStride;
	
	if (Size > 0)
	{
		// Create the index buffer.
		FRHIResourceCreateInfo CreateInfo(TEXT("FRawIndexBuffer"));
		IndexBufferRHI = RHICmdList.CreateBuffer(Size, BUF_Static | BUF_IndexBuffer, IndexStride, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);

		// Initialize the buffer.		
		void* Buffer = RHICmdList.LockBuffer(IndexBufferRHI, 0, Size, RLM_WriteOnly);

		if (b32Bit)
		{
			FMemory::Memcpy(Buffer, Indices.GetData(), Size);
		}
		else
		{
			uint16* DestIndices16Bit = (uint16*)Buffer;
			for (int32 i = 0; i < NumIndices; ++i)
			{
				DestIndices16Bit[i] = Indices[i];
			}
		}

		RHICmdList.UnlockBuffer(IndexBufferRHI);
	}

	// Undo/redo can destroy and recreate the render resources for UModels without rebuilding the
	// buffers, so the indices need to be saved when in the editor.
	if (!GIsEditor && !IsRunningCommandlet())
	{
		Indices.Empty();
	}
}

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer16or32& I)
{
	I.Indices.BulkSerialize( Ar );
	return Ar;
}

/*-----------------------------------------------------------------------------
	FRawStaticIndexBuffer
-----------------------------------------------------------------------------*/

FRawStaticIndexBuffer::FRawStaticIndexBuffer(bool InNeedsCPUAccess)
	: IndexStorage(InNeedsCPUAccess)
	, CachedNumIndices(-1)
	, b32Bit(false)
	, bShouldExpandTo32Bit(false)
{
}

void FRawStaticIndexBuffer::SetIndices(const TArray<uint32>& InIndices, EIndexBufferStride::Type DesiredStride)
{
	int32 NumIndices = InIndices.Num();
	bool bShouldUse32Bit = false;

	// Figure out if we should store the indices as 16 or 32 bit.
	if (DesiredStride == EIndexBufferStride::Force32Bit)
	{
		bShouldUse32Bit = true;
	}
	else if (DesiredStride == EIndexBufferStride::AutoDetect)
	{
		int32 i = 0;
		while (!bShouldUse32Bit && i < NumIndices)
		{
			bShouldUse32Bit = InIndices[i] > MAX_uint16;
			i++;
		}
	}

	// Allocate storage for the indices.
	int32 IndexStride = bShouldUse32Bit ? sizeof(uint32) : sizeof(uint16);
	IndexStorage.Empty(IndexStride * NumIndices);
	IndexStorage.AddUninitialized(IndexStride * NumIndices);
	CachedNumIndices = NumIndices;

	// Store them!
	if (bShouldUse32Bit)
	{
		// If the indices are 32 bit we can just do a memcpy.
		check(IndexStorage.Num() == InIndices.Num() * InIndices.GetTypeSize());
		FMemory::Memcpy(IndexStorage.GetData(),InIndices.GetData(),IndexStorage.Num());
		b32Bit = true;
	}
	else
	{
		// Copy element by element demoting 32-bit integers to 16-bit.
		check(IndexStorage.Num() == InIndices.Num() * sizeof(uint16));
		uint16* DestIndices16Bit = (uint16*)IndexStorage.GetData();
		for (int32 i = 0; i < NumIndices; ++i)
		{
			DestIndices16Bit[i] = InIndices[i];
		}
		b32Bit = false;
	}
}

void FRawStaticIndexBuffer::InsertIndices( const uint32 At, const uint32* IndicesToAppend, const uint32 NumIndicesToAppend )
{
	if( NumIndicesToAppend > 0 )
	{
		const uint32 IndexStride = b32Bit ? sizeof( uint32 ) : sizeof( uint16 );

		IndexStorage.InsertUninitialized( At * IndexStride, NumIndicesToAppend * IndexStride );
		CachedNumIndices = IndexStorage.Num() / static_cast<int32>(IndexStride);
		uint8* const DestIndices = &IndexStorage[ At * IndexStride ];

		if( IndicesToAppend )
		{
			if( b32Bit )
			{
				// If the indices are 32 bit we can just do a memcpy.
				FMemory::Memcpy( DestIndices, IndicesToAppend, NumIndicesToAppend * IndexStride );
			}
			else
			{
				// Copy element by element demoting 32-bit integers to 16-bit.
				uint16* DestIndices16Bit = (uint16*)DestIndices;
				for( uint32 Index = 0; Index < NumIndicesToAppend; ++Index )
				{
					DestIndices16Bit[ Index ] = IndicesToAppend[ Index ];
				}
			}
		}
		else
		{
			// If no indices to insert were supplied, just clear the buffer
			FMemory::Memset( DestIndices, 0, NumIndicesToAppend * IndexStride );
		}
	}
}

void FRawStaticIndexBuffer::AppendIndices( const uint32* IndicesToAppend, const uint32 NumIndicesToAppend )
{
	InsertIndices( b32Bit ? IndexStorage.Num() / 4 : IndexStorage.Num() / 2, IndicesToAppend, NumIndicesToAppend );
}

void FRawStaticIndexBuffer::RemoveIndicesAt( const uint32 At, const uint32 NumIndicesToRemove )
{
	if( NumIndicesToRemove > 0 )
	{
		const int32 IndexStride = b32Bit ? sizeof( uint32 ) : sizeof( uint16 );
		IndexStorage.RemoveAt( At * IndexStride, NumIndicesToRemove * IndexStride );
		CachedNumIndices = IndexStorage.Num() / IndexStride;
	}
}

void FRawStaticIndexBuffer::GetCopy(TArray<uint32>& OutIndices) const
{
	int32 NumIndices = b32Bit ? (IndexStorage.Num() / 4) : (IndexStorage.Num() / 2);
	OutIndices.Empty(NumIndices);
	OutIndices.AddUninitialized(NumIndices);

	if (b32Bit)
	{
		// If the indices are 32 bit we can just do a memcpy.
		check(IndexStorage.Num() == OutIndices.Num() * OutIndices.GetTypeSize());
		FMemory::Memcpy(OutIndices.GetData(),IndexStorage.GetData(),IndexStorage.Num());
	}
	else
	{
		// Copy element by element promoting 16-bit integers to 32-bit.
		check(IndexStorage.Num() == OutIndices.Num() * sizeof(uint16));
		const uint16* SrcIndices16Bit = (const uint16*)IndexStorage.GetData();
		for (int32 i = 0; i < NumIndices; ++i)
		{
			OutIndices[i] = SrcIndices16Bit[i];
		}
	}
}

void FRawStaticIndexBuffer::ExpandTo32Bit()
{
	if (b32Bit)
		return;

	b32Bit = true;
	bool bAllowCpuAccess = IndexStorage.GetAllowCPUAccess();
	TResourceArray<uint8, INDEXBUFFER_ALIGNMENT> CopyIndex(bAllowCpuAccess);
	CopyIndex.Empty(sizeof(uint32) * CachedNumIndices);
	CopyIndex.AddUninitialized(sizeof(uint32) * CachedNumIndices);

	uint16* SrcIndices16Bit = (uint16*)IndexStorage.GetData();
	uint32* DstIndices32Bit = (uint32*)CopyIndex.GetData();
	for (int32 i = 0; i < CachedNumIndices; ++i)
	{
		DstIndices32Bit[i] = SrcIndices16Bit[i];
	}

	IndexStorage.Empty();
	IndexStorage = MoveTemp(CopyIndex);
}

const uint16* FRawStaticIndexBuffer::AccessStream16() const
{
	if (!b32Bit)
	{
		return reinterpret_cast<const uint16*>(IndexStorage.GetData());
	}
	return nullptr;
}

const uint32* FRawStaticIndexBuffer::AccessStream32() const
{
	if (b32Bit)
	{
		return reinterpret_cast<const uint32*>(IndexStorage.GetData());
	}
	return nullptr;
}

FIndexArrayView FRawStaticIndexBuffer::GetArrayView() const
{
	int32 NumIndices = b32Bit ? (IndexStorage.Num() / 4) : (IndexStorage.Num() / 2);
	return FIndexArrayView(IndexStorage.GetData(),NumIndices,b32Bit);
}

FBufferRHIRef FRawStaticIndexBuffer::CreateRHIBuffer(FRHICommandListBase& RHICmdList)
{
	const uint32 IndexStride = b32Bit ? sizeof(uint32) : sizeof(uint16);
	const uint32 SizeInBytes = IndexStorage.Num();

	if (GetNumIndices() > 0)
	{
		// Systems that generate data for GPUSkinPassThrough use index buffer as SRV.
		bool bSRV = RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinPassThroughSupported(GMaxRHIShaderPlatform);

		// When bAllowCPUAccess is true, the meshes is likely going to be used for Niagara to spawn particles on mesh surface.
		// And it can be the case for CPU *and* GPU access: no differenciation today. That is why we create a SRV in this case.
		// This also avoid setting lots of states on all the members of all the different buffers used by meshes. Follow up: https://jira.it.epicgames.net/browse/UE-69376.
		bSRV |= IndexStorage.GetAllowCPUAccess();

		const EBufferUsageFlags BufferFlags = EBufferUsageFlags::Static | (bSRV ? EBufferUsageFlags::ShaderResource : EBufferUsageFlags::None);

		const static FLazyName ClassName32(TEXT("FRawStaticIndexBuffer32"));
		const static FLazyName ClassName16(TEXT("FRawStaticIndexBuffer16"));

		// Create the index buffer.
		FRHIResourceCreateInfo CreateInfo(Is32Bit() ? TEXT("FRawStaticIndexBuffer32") : TEXT("FRawStaticIndexBuffer16"), &IndexStorage);
		CreateInfo.ClassName = Is32Bit() ? ClassName32 : ClassName16;
		CreateInfo.OwnerName = GetOwnerName();
		CreateInfo.bWithoutNativeResource = !SizeInBytes;

		FBufferRHIRef Buffer = RHICmdList.CreateBuffer(SizeInBytes, BufferFlags | EBufferUsageFlags::IndexBuffer, IndexStride, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		Buffer->SetOwnerName(GetOwnerName());
		return Buffer;
	}
	return nullptr;
}

FBufferRHIRef FRawStaticIndexBuffer::CreateRHIBuffer_RenderThread()
{
	return CreateRHIBuffer(FRHICommandListExecutor::GetImmediateCommandList());
}

FBufferRHIRef FRawStaticIndexBuffer::CreateRHIBuffer_Async()
{
	FRHIAsyncCommandList CommandList;
	return CreateRHIBuffer(*CommandList);
}

void FRawStaticIndexBuffer::InitRHIForStreaming(FRHIBuffer* IntermediateBuffer, FRHIResourceUpdateBatcher& Batcher)
{
	if (IndexBufferRHI && IntermediateBuffer)
	{
		Batcher.QueueUpdateRequest(IndexBufferRHI, IntermediateBuffer);
	}
}

void FRawStaticIndexBuffer::ReleaseRHIForStreaming(FRHIResourceUpdateBatcher& Batcher)
{
	if (IndexBufferRHI)
	{
		Batcher.QueueUpdateRequest(IndexBufferRHI, nullptr);
	}
}

void FRawStaticIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRawStaticIndexBuffer::InitRHI);
	IndexBufferRHI = CreateRHIBuffer(RHICmdList);
}

void FRawStaticIndexBuffer::Serialize(FArchive& Ar, bool bNeedsCPUAccess)
{
	IndexStorage.SetAllowCPUAccess(bNeedsCPUAccess);

	if (Ar.UEVer() < VER_UE4_SUPPORT_32BIT_STATIC_MESH_INDICES)
	{
		TResourceArray<uint16,INDEXBUFFER_ALIGNMENT> LegacyIndices;

		b32Bit = false;
		LegacyIndices.BulkSerialize(Ar);
		int32 NumIndices = LegacyIndices.Num();
		int32 IndexStride = sizeof(uint16);
		IndexStorage.Empty(NumIndices * IndexStride);
		IndexStorage.AddUninitialized(NumIndices * IndexStride);
		FMemory::Memcpy(IndexStorage.GetData(),LegacyIndices.GetData(),IndexStorage.Num());
		CachedNumIndices = IndexStorage.Num() / (b32Bit ? 4 : 2);
	}
	else
	{
		Ar << b32Bit;
		IndexStorage.BulkSerialize(Ar);
		CachedNumIndices = IndexStorage.Num() / (b32Bit ? 4 : 2);

		if (Ar.IsCooking() && (CachedNumIndices > 0) && !b32Bit)
		{
			bShouldExpandTo32Bit = Ar.CookingTarget()->ShouldExpandTo32Bit((const uint16*)IndexStorage.GetData(), CachedNumIndices);
		}
		else
		{
			bShouldExpandTo32Bit = false;
		}

		Ar << bShouldExpandTo32Bit;

		if (Ar.IsLoading() && (CachedNumIndices > 0) && bShouldExpandTo32Bit && FPlatformMisc::Expand16BitIndicesTo32BitOnLoad())
		{
			ExpandTo32Bit();
		}
	}
}

void FRawStaticIndexBuffer::SerializeMetaData(FArchive& Ar)
{
	Ar << CachedNumIndices << b32Bit;
}

void FRawStaticIndexBuffer::ClearMetaData()
{
	CachedNumIndices = -1;
}

void FRawStaticIndexBuffer::Discard()
{
    IndexStorage.SetAllowCPUAccess(false);
    IndexStorage.Discard();
}

bool FRawStaticIndexBuffer16or32Interface::IsSRVNeeded(bool bAllowCPUAccess) const
{
	// Systems that generate data for GPUSkinPassThrough use index buffer as SRV.
	bool bSRV = RHISupportsManualVertexFetch(GMaxRHIShaderPlatform) || IsGPUSkinPassThroughSupported(GMaxRHIShaderPlatform);
	// When bAllowCPUAccess is true, the meshes is likely going to be used for Niagara to spawn particles on mesh surface.
	// And it can be the case for CPU *and* GPU access: no differenciation today. That is why we create a SRV in this case.
	// This also avoid setting lots of states on all the members of all the different buffers used by meshes. Follow up: https://jira.it.epicgames.net/browse/UE-69376.
	bSRV |= bAllowCPUAccess;
	return bSRV;
}

void FRawStaticIndexBuffer16or32Interface::InitRHIForStreaming(FRHIBuffer* IntermediateBuffer, size_t IndexSize, FRHIResourceUpdateBatcher& Batcher)
{
	if (IndexBufferRHI && IntermediateBuffer)
	{
		Batcher.QueueUpdateRequest(IndexBufferRHI, IntermediateBuffer);
	}
}

void FRawStaticIndexBuffer16or32Interface::ReleaseRHIForStreaming(FRHIResourceUpdateBatcher& Batcher)
{
	if (IndexBufferRHI)
	{
		Batcher.QueueUpdateRequest(IndexBufferRHI, nullptr);
	}
}

FBufferRHIRef FRawStaticIndexBuffer16or32Interface::CreateRHIIndexBufferInternal(
	FRHICommandListBase& RHICmdList,
	const TCHAR* InDebugName,
	const FName& InOwnerName,
	int32 IndexCount,
	size_t IndexSize,
	FResourceArrayInterface* ResourceArray,
	bool bNeedSRV
)
{
	// Create the index buffer.
	FRHIResourceCreateInfo CreateInfo(InDebugName, ResourceArray);
	CreateInfo.ClassName = InDebugName;
	CreateInfo.OwnerName = InOwnerName;
	EBufferUsageFlags Flags = EBufferUsageFlags::Static;

	if (bNeedSRV)
	{
		// BUF_ShaderResource is needed for SkinCache RecomputeSkinTangents
		Flags |= EBufferUsageFlags::ShaderResource;
	}

	const uint32 Size = IndexCount * IndexSize;
	CreateInfo.bWithoutNativeResource = !Size;

	FBufferRHIRef Buffer = RHICmdList.CreateBuffer(Size, Flags | EBufferUsageFlags::IndexBuffer, IndexSize, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
	Buffer->SetOwnerName(InOwnerName);
	return Buffer;
}

/*-----------------------------------------------------------------------------
FRawStaticIndexBuffer16or32
-----------------------------------------------------------------------------*/

/**
* Orders a triangle list for better vertex cache coherency.
*/
template <typename INDEX_TYPE>
void FRawStaticIndexBuffer16or32<INDEX_TYPE>::CacheOptimize()
{
#if WITH_EDITOR
	CacheOptimizeIndexBuffer(Indices);
	CachedNumIndices = Indices.Num();
#endif
}

