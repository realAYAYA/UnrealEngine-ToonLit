// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomResources.h"
#include "EngineUtils.h"
#include "GroomAssetImportData.h"
#include "GroomBuilder.h"
#include "GroomImportOptions.h"
#include "GroomSettings.h"
#include "RenderingThread.h"
#include "Engine/AssetUserData.h"
#include "HairStrandsVertexFactory.h"
#include "Misc/Paths.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/BulkData.h"
#include "UObject/PhysicsObjectVersion.h"
#include "UObject/ReleaseObjectVersion.h"
#include "NiagaraSystem.h"
#include "Async/ParallelFor.h"
#include "RenderGraph.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "GroomBindingBuilder.h"

static int32 GHairStrandsBulkData_ReleaseAfterUse = 0;
static FAutoConsoleVariableRef CVarHairStrandsBulkData_ReleaseAfterUse(TEXT("r.HairStrands.Strands.BulkData.ReleaseAfterUse"), GHairStrandsBulkData_ReleaseAfterUse, TEXT("Release CPU bulk data once hair groom/groom binding asset GPU resources are created. This saves memory"));

static int32 GHairStrandsBulkData_AsyncLoading = -1;
static int32 GHairCardsBulkData_AsyncLoading = -1;

static FAutoConsoleVariableRef CVarHairStrandsBulkData_AsyncLoading(TEXT("r.HairStrands.Strands.BulkData.AsyncLoading"), GHairStrandsBulkData_AsyncLoading, TEXT("Load hair strands data with async loading so that it is not blocking the rendering thread. This value define the MinLOD at which this happen. Default disabled (-1)"));
static FAutoConsoleVariableRef CVarHairCardsBulkData_AsyncLoading(TEXT("r.HairStrands.Cards.BulkData.AsyncLoading"), GHairCardsBulkData_AsyncLoading, TEXT("Load hair cards/meshes data with async loading so that it is not blocking the rendering thread. This value define the MinLOD at which this happen. Default disabled (-1)"));

static int32 GHairStrandsBulkData_Validation = 1;
static FAutoConsoleVariableRef CVarHairStrandsBulkData_Validation(TEXT("r.HairStrands.Strands.BulkData.Validation"), GHairStrandsBulkData_Validation, TEXT("Validate some hair strands data at serialization/loading time."));

static float GHairStrandsDebugVoxel_WorldSize = 0.3f;
static int32 GHairStrandsDebugVoxel_MaxSegmentPerVoxel = 2048;
static FAutoConsoleVariableRef CVarHairStrandsDebugVoxel_WorldSize(TEXT("r.HairStrands.DebugData.VoxelSize"), GHairStrandsDebugVoxel_WorldSize, TEXT("Voxel size use for creating debug data."));
static FAutoConsoleVariableRef CVarHairStrandsDebugVoxel_MaxSegmentPerVoxel(TEXT("r.HairStrands.DebugData.MaxSegmentPerVoxel"), GHairStrandsDebugVoxel_MaxSegmentPerVoxel, TEXT("Max number of segments per Voxel size when creating debug data."));

////////////////////////////////////////////////////////////////////////////////////

EHairResourceLoadingType GetHairResourceLoadingType(EHairGeometryType InGeometryType, int32 InLODIndex)
{
#if !WITH_EDITORONLY_DATA
	switch (InGeometryType)
	{
	case EHairGeometryType::Strands: return InLODIndex <= GHairStrandsBulkData_AsyncLoading ? EHairResourceLoadingType::Async : EHairResourceLoadingType::Sync;
	case EHairGeometryType::Cards:
	case EHairGeometryType::Meshes: return InLODIndex <= GHairCardsBulkData_AsyncLoading ? EHairResourceLoadingType::Async : EHairResourceLoadingType::Sync;
	}
	return EHairResourceLoadingType::Sync;
#else
	return EHairResourceLoadingType::Sync;
#endif
}

enum class EHairResourceUsageType : uint8
{
	Static,
	Dynamic
};

#define HAIRSTRANDS_RESOUCE_NAME(Type, Name) (Type == EHairStrandsResourcesType::Guides  ? TEXT(#Name "(Guides)") : (Type == EHairStrandsResourcesType::Strands ? TEXT(#Name "(Strands)") : TEXT(#Name "(Cards)")))

const TCHAR* ToHairResourceDebugName(const TCHAR* In, FHairResourceName& InDebugNames)
{
#if HAIR_RESOURCE_DEBUG_NAME
	FString& TempDebugName = InDebugNames.Names.AddDefaulted_GetRef();
	TempDebugName = In;
	if (InDebugNames.GroupIndex >= 0)
	{
		TempDebugName += TEXT("_GROUP") + FString::FromInt(InDebugNames.GroupIndex);
	}
	if (InDebugNames.LODIndex >= 0)
	{
		TempDebugName += TEXT("_LOD") + FString::FromInt(InDebugNames.LODIndex);
	}
	TempDebugName += TEXT("_") + InDebugNames.AssetName.ToString();
	return *TempDebugName;
#else
	return In;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////
// FRWBuffer utils 
void UploadDataToBuffer(FReadBuffer& OutBuffer, uint32 DataSizeInBytes, const void* InCpuData)
{
	void* BufferData = RHILockBuffer(OutBuffer.Buffer, 0, DataSizeInBytes, RLM_WriteOnly);
	FMemory::Memcpy(BufferData, InCpuData, DataSizeInBytes);
	RHIUnlockBuffer(OutBuffer.Buffer);
}

void UploadDataToBuffer(FRWBufferStructured& OutBuffer, uint32 DataSizeInBytes, const void* InCpuData)
{
	void* BufferData = RHILockBuffer(OutBuffer.Buffer, 0, DataSizeInBytes, RLM_WriteOnly);
	FMemory::Memcpy(BufferData, InCpuData, DataSizeInBytes);
	RHIUnlockBuffer(OutBuffer.Buffer);
}

template<typename FormatType>
void CreateBuffer(const TArray<typename FormatType::Type>& InData, FRWBuffer& OutBuffer, const TCHAR* DebugName, ERHIAccess InitialAccess = ERHIAccess::SRVMask)
{
	const uint32 DataCount = InData.Num();
	const uint32 DataSizeInBytes = FormatType::SizeInByte*DataCount;

	if (DataSizeInBytes == 0) return;

	OutBuffer.Initialize(FormatType::SizeInByte, DataCount, FormatType::Format, InitialAccess, BUF_Static, DebugName);
	void* BufferData = RHILockBuffer(OutBuffer.Buffer, 0, DataSizeInBytes, RLM_WriteOnly);

	FMemory::Memcpy(BufferData, InData.GetData(), DataSizeInBytes);
	RHIUnlockBuffer(OutBuffer.Buffer);
}

template<typename FormatType>
void CreateBuffer(uint32 InVertexCount, FRWBuffer& OutBuffer, const TCHAR* DebugName)
{
	const uint32 DataCount = InVertexCount;
	const uint32 DataSizeInBytes = FormatType::SizeInByte*DataCount;

	if (DataSizeInBytes == 0) return;

	OutBuffer.Initialize(FormatType::SizeInByte, DataCount, FormatType::Format, ERHIAccess::UAVCompute, BUF_Static, DebugName);
	void* BufferData = RHILockBuffer(OutBuffer.Buffer, 0, DataSizeInBytes, RLM_WriteOnly);
	FMemory::Memset(BufferData, 0, DataSizeInBytes);
	RHIUnlockBuffer(OutBuffer.Buffer);
}

template<typename FormatType>
void CreateBuffer(const TArray<typename FormatType::Type>& InData, FHairCardsVertexBuffer& OutBuffer, const TCHAR* DebugName, ERHIAccess InitialAccess = ERHIAccess::SRVMask)
{
	const uint32 DataCount = InData.Num();
	const uint32 DataSizeInBytes = FormatType::SizeInByte * DataCount;

	if (DataSizeInBytes == 0) return;

	FRHIResourceCreateInfo CreateInfo(DebugName);
	CreateInfo.ResourceArray = nullptr;

	OutBuffer.VertexBufferRHI = RHICreateVertexBuffer(DataSizeInBytes, BUF_Static | BUF_ShaderResource, InitialAccess, CreateInfo);

	void* BufferData = RHILockBuffer(OutBuffer.VertexBufferRHI, 0, DataSizeInBytes, RLM_WriteOnly);
	FMemory::Memcpy(BufferData, InData.GetData(), DataSizeInBytes);
	RHIUnlockBuffer(OutBuffer.VertexBufferRHI);
	OutBuffer.ShaderResourceViewRHI = RHICreateShaderResourceView(OutBuffer.VertexBufferRHI, FormatType::SizeInByte, FormatType::Format);
}

/////////////////////////////////////////////////////////////////////////////////////////
// RDG buffers utils 

static FRDGBufferRef InternalCreateVertexBuffer(
	FRDGBuilder& GraphBuilder,
	const TCHAR* Name,
	const FRDGBufferDesc& Desc,
	const void* InitialData,
	uint64 InitialDataSize,
	ERDGInitialDataFlags InitialDataFlags)
{
	checkf(EnumHasAnyFlags(Desc.Usage, EBufferUsageFlags::VertexBuffer), TEXT("CreateVertexBuffer called with an FRDGBufferDesc underlying type that is not 'VertexBuffer'. Buffer: %s"), Name);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, Name, ERDGBufferFlags::MultiFrame);
	if (InitialData && InitialDataSize)
	{
		GraphBuilder.QueueBufferUpload(Buffer, InitialData, InitialDataSize, InitialDataFlags);
	}
	return Buffer;
}

template<typename FormatType>
void InternalCreateVertexBufferRDG_FromBulkData(FRDGBuilder& GraphBuilder, FByteBulkData& InBulkData, uint32 InDataCount, FRDGExternalBuffer& Out, const TCHAR* DebugName, EHairResourceUsageType UsageType)
{
	const uint32 InDataCount_Check = InBulkData.GetBulkDataSize() / sizeof(typename FormatType::BulkType);
	check(InDataCount_Check == InDataCount);

	const uint32 DataSizeInBytes = FormatType::SizeInByte * InDataCount;
	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(FormatType::SizeInByte, InDataCount);
	if (UsageType != EHairResourceUsageType::Dynamic)
	{
		Desc.Usage = Desc.Usage & (~BUF_UnorderedAccess);
	}

	// Unloading of the bulk data is only supported on cooked build, as we can reload the data from the file/archieve
	#if !WITH_EDITORONLY_DATA
	const bool bReleaseCPUData = GHairStrandsBulkData_ReleaseAfterUse > 0;
	if (bReleaseCPUData)
	{
		InBulkData.SetBulkDataFlags(BULKDATA_SingleUse);
	}
	#endif

	const typename FormatType::BulkType* BulkData = (const typename FormatType::BulkType*)InBulkData.Lock(LOCK_READ_ONLY);
	FRDGBufferRef Buffer = nullptr;
	#if !WITH_EDITORONLY_DATA
	if (bReleaseCPUData)
	{
		checkf(EnumHasAnyFlags(Desc.Usage, EBufferUsageFlags::VertexBuffer), TEXT("CreateVertexBuffer called with an FRDGBufferDesc underlying type that is not 'VertexBuffer'. Buffer: %s"), DebugName);
		Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);
		if (BulkData && DataSizeInBytes)
		{
			GraphBuilder.QueueBufferUpload(Buffer, BulkData, DataSizeInBytes, [&InBulkData](const void* Ptr) { InBulkData.Unlock(); });
		}
	}
	else
	#endif
	{
		Buffer = InternalCreateVertexBuffer(
			GraphBuilder,
			DebugName,
			Desc,
			BulkData,
			DataSizeInBytes,
			ERDGInitialDataFlags::None); // Copy data internally
		InBulkData.Unlock();
	}

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out, FormatType::Format);
}

template<typename FormatType>
void InternalCreateVertexBufferRDG(FRDGBuilder& GraphBuilder, const typename FormatType::Type* InData, uint32 InDataCount, FRDGExternalBuffer& Out, const TCHAR* DebugName, EHairResourceUsageType UsageType, ERDGInitialDataFlags InitialDataFlags)
{
	FRDGBufferRef Buffer = nullptr;

	const uint32 DataSizeInBytes = FormatType::SizeInByte * InDataCount;
	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(FormatType::SizeInByte, InDataCount);
	if (UsageType != EHairResourceUsageType::Dynamic)
	{
		Desc.Usage = Desc.Usage & (~BUF_UnorderedAccess);
	}
	Buffer = InternalCreateVertexBuffer(
		GraphBuilder,
		DebugName,
		Desc,
		InData,
		DataSizeInBytes,
		InitialDataFlags);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out, FormatType::Format);
}

template<typename FormatType>
void InternalCreateVertexBufferRDG(FRDGBuilder& GraphBuilder, const TArray<typename FormatType::Type>& InData, FRDGExternalBuffer& Out, const TCHAR* DebugName, EHairResourceUsageType UsageType, ERDGInitialDataFlags InitialDataFlags= ERDGInitialDataFlags::NoCopy)
{
	FRDGBufferRef Buffer = nullptr;

	const uint32 DataCount = InData.Num();
	const uint32 DataSizeInBytes = FormatType::SizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(FormatType::SizeInByte, InData.Num());
	if (UsageType != EHairResourceUsageType::Dynamic)
	{
		Desc.Usage = Desc.Usage & (~BUF_UnorderedAccess);
	}
	Buffer = InternalCreateVertexBuffer(
		GraphBuilder,
		DebugName,
		Desc,
		InData.GetData(),
		DataSizeInBytes,
		InitialDataFlags);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out, FormatType::Format);
}

template<typename DataType>
void InternalCreateVertexBufferRDG(FRDGBuilder& GraphBuilder, const TArray<DataType>& InData, EPixelFormat Format, FRDGExternalBuffer& Out, const TCHAR* DebugName, EHairResourceUsageType UsageType)
{
	FRDGBufferRef Buffer = nullptr;

	const uint32 DataCount = InData.Num();
	const uint32 DataSizeInBytes = sizeof(DataType) * DataCount;
	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(sizeof(DataType), InData.Num());
	if (UsageType != EHairResourceUsageType::Dynamic)
	{
		Desc.Usage = Desc.Usage & (~BUF_UnorderedAccess);
	}
	Buffer = InternalCreateVertexBuffer(
		GraphBuilder,
		DebugName,
		Desc,
		InData.GetData(),
		DataSizeInBytes,
		ERDGInitialDataFlags::NoCopy);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out, Format);
}

template<typename FormatType>
void InternalCreateVertexBufferRDG(FRDGBuilder& GraphBuilder, uint32 InVertexCount, FRDGExternalBuffer& Out, const TCHAR* DebugName, EHairResourceUsageType Usage)
{
	// Sanity check
	check(Usage == EHairResourceUsageType::Dynamic);

	const uint32 DataCount = InVertexCount;
	const uint32 DataSizeInBytes = FormatType::SizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(FormatType::SizeInByte, InVertexCount);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);

	if (IsFloatFormat(FormatType::Format) || IsUnormFormat(FormatType::Format) || IsSnormFormat(FormatType::Format))
	{
		AddClearUAVFloatPass(GraphBuilder, GraphBuilder.CreateUAV(Buffer, FormatType::Format), 0.0f);
	}
	else
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Buffer, FormatType::Format), 0u);
	}
	
	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out, FormatType::Format);
}

template<typename FormatType>
void InternalCreateStructuredBufferRDG(FRDGBuilder& GraphBuilder, uint32 DataCount, FRDGExternalBuffer& Out, const TCHAR* DebugName, EHairResourceUsageType Usage)
{
	// Sanity check
	check(Usage == EHairResourceUsageType::Dynamic);

	const uint32 DataSizeInBytes = FormatType::SizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(FormatType::SizeInByte, DataCount);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Buffer, FormatType::Format), 0u);
	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out, FormatType::Format);
}


template<typename FormatType>
void InternalCreateStructuredBufferRDG_FromBulkData(FRDGBuilder& GraphBuilder, FByteBulkData& InBulkData, uint32 InDataCount, FRDGExternalBuffer& Out, const TCHAR* DebugName, EHairResourceUsageType UsageType)
{
	const uint32 InSizeInByte = sizeof(typename FormatType::Type);
	const uint32 DataSizeInBytes = InSizeInByte * InDataCount;
	check(InBulkData.GetBulkDataSize() == DataSizeInBytes);

	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(InSizeInByte, InDataCount);
	if (UsageType != EHairResourceUsageType::Dynamic)
	{
		Desc.Usage = Desc.Usage & (~BUF_UnorderedAccess);
	}

	// Unloading of the bulk data is only supported on cooked build, as we can reload the data from the file/archieve
	#if !WITH_EDITORONLY_DATA
	const bool bReleaseCPUData = GHairStrandsBulkData_ReleaseAfterUse > 0;
	if (bReleaseCPUData)
	{
		InBulkData.SetBulkDataFlags(BULKDATA_SingleUse);
	}
	#endif

	const typename FormatType::Type* Data = (const typename FormatType::Type*)InBulkData.Lock(LOCK_READ_ONLY);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);
	if (Data && DataSizeInBytes)
	{
		#if !WITH_EDITORONLY_DATA
		if (bReleaseCPUData)
		{
			GraphBuilder.QueueBufferUpload(Buffer, Data, DataSizeInBytes, [&InBulkData](const void* Ptr) { InBulkData.Unlock(); });
		}
		else
		#endif
		{
			GraphBuilder.QueueBufferUpload(Buffer, Data, DataSizeInBytes, ERDGInitialDataFlags::None);  // Copy data internally
			InBulkData.Unlock();
		}
	}

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out);
}

/////////////////////////////////////////////////////////////////////////////////////////

static UTexture2D* CreateCardTexture(FIntPoint Resolution)
{
	UTexture2D* Out = nullptr;

	// Pass NAME_None as name to ensure an unique name is picked, so GC dont delete the new texture when it wants to delete the old one 
	Out = NewObject<UTexture2D>(GetTransientPackage(), NAME_None /*TEXT("ProceduralFollicleMaskTexture")*/, RF_Transient);
	Out->AddToRoot();
	Out->SetPlatformData(new FTexturePlatformData());
	Out->GetPlatformData()->SizeX = Resolution.X;
	Out->GetPlatformData()->SizeY = Resolution.Y;
	Out->GetPlatformData()->PixelFormat = PF_R32_FLOAT;
	Out->SRGB = false;

	const uint32 MipCount = 1; // FMath::Min(FMath::FloorLog2(Resolution), 5u);// Don't need the full chain
	for (uint32 MipIt = 0; MipIt < MipCount; ++MipIt)
	{
		const uint32 MipResolutionX = Resolution.X >> MipIt;
		const uint32 MipResolutionY = Resolution.Y >> MipIt;
		const uint32 SizeInBytes = sizeof(float) * MipResolutionX * MipResolutionY;

		FTexture2DMipMap* MipMap = new FTexture2DMipMap();
		Out->GetPlatformData()->Mips.Add(MipMap);
		MipMap->SizeX = MipResolutionX;
		MipMap->SizeY = MipResolutionY;
		MipMap->BulkData.Lock(LOCK_READ_WRITE);
		float* MipMemory = (float*)MipMap->BulkData.Realloc(SizeInBytes);
		for (uint32 Y = 0; Y < MipResolutionY; Y++)
			for (uint32 X = 0; X < MipResolutionX; X++)
			{
				MipMemory[X + Y * MipResolutionY] = X / float(MipResolutionX);
			}
		//FMemory::Memzero(MipMemory, SizeInBytes);
		MipMap->BulkData.Unlock();
	}
	Out->UpdateResource();

	return Out;
}

/////////////////////////////////////////////////////////////////////////////////////////
void CreateHairStrandsDebugAttributeBuffer(FRDGBuilder& GraphBuilder, FRDGExternalBuffer* DebugAttributeBuffer, uint32 VertexCount)
{
	if (VertexCount == 0 || !DebugAttributeBuffer)
		return;
	InternalCreateVertexBufferRDG<FHairStrandsAttribute0Format>(GraphBuilder, VertexCount, *DebugAttributeBuffer, TEXT("Hair.Strands_DebugAttributeBuffer"), EHairResourceUsageType::Dynamic);
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairCommonResource::FHairCommonResource(EHairStrandsAllocationType InAllocationType, const FHairResourceName& InResourceName, bool bInUseRenderGraph):
bUseRenderGraph(bInUseRenderGraph),
bIsInitialized(false),
AllocationType(InAllocationType),
ResourceName(InResourceName)
{
}

void FHairCommonResource::InitRHI()
{
	if (bIsInitialized || AllocationType == EHairStrandsAllocationType::Deferred || GUsingNullRHI) { return; }

	check(InternalIsDataLoaded());

	if (bUseRenderGraph)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		FRDGBuilder GraphBuilder(RHICmdList);
		InternalAllocate(GraphBuilder);
		GraphBuilder.Execute();
	}
	else
	{
		InternalAllocate();
	}
	bIsInitialized = true;
}

void FHairCommonResource::ReleaseRHI()
{
	InternalRelease();
	bIsInitialized = false;
}

void FHairCommonResource::Allocate(FRDGBuilder& GraphBuilder, EHairResourceLoadingType LoadingType)
{
	EHairResourceStatus Status = EHairResourceStatus::None;
	Allocate(GraphBuilder, LoadingType, Status);
}

void FHairCommonResource::Allocate(FRDGBuilder& GraphBuilder, EHairResourceLoadingType LoadingType, EHairResourceStatus& Status)
{
	if (bIsInitialized) { Status |= EHairResourceStatus::Valid; return; }

	if (LoadingType == EHairResourceLoadingType::Async && !InternalIsDataLoaded()) { Status |= EHairResourceStatus::Loading; return; }

	check(AllocationType == EHairStrandsAllocationType::Deferred);
	FRenderResource::InitResource(); // Call RenderResource InitResource() so that the resources is marked as initialized
	InternalAllocate(GraphBuilder);
	bIsInitialized = true;
	Status |= EHairResourceStatus::Valid;
}

void FHairCommonResource::AllocateLOD(FRDGBuilder& GraphBuilder, int32 LODIndex, EHairResourceLoadingType LoadingType, EHairResourceStatus& Status)
{
	// When using a async loading, sub-resources allocation requires the common/main resource to be already initialized.
	if (LoadingType == EHairResourceLoadingType::Async && (!InternalIsLODDataLoaded(LODIndex) || !bIsInitialized)) { Status |= EHairResourceStatus::Loading; return; }

	// Sanity check. 
	check(AllocationType == EHairStrandsAllocationType::Deferred);

	InternalAllocateLOD(GraphBuilder, LODIndex);
	Status |= EHairResourceStatus::Valid;
}

void FHairCommonResource::StreamInData()
{
	if (!bIsInitialized)
	{
		InternalIsDataLoaded();
	}
}

void FHairCommonResource::StreamInLODData(int32 LODIndex)
{
	if (!bIsInitialized)
	{
		InternalIsLODDataLoaded(LODIndex);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void FHairCardIndexBuffer::InitRHI()
{
	const uint32 DataSizeInBytes = FHairCardsIndexFormat::SizeInByte * Indices.Num();

	FRHIResourceCreateInfo CreateInfo(TEXT("FHairCardIndexBuffer"));
	IndexBufferRHI = RHICreateBuffer(DataSizeInBytes, BUF_Static | BUF_IndexBuffer, FHairCardsIndexFormat::SizeInByte, ERHIAccess::VertexOrIndexBuffer, CreateInfo);
	void* Buffer = RHILockBuffer(IndexBufferRHI, 0, DataSizeInBytes, RLM_WriteOnly);
	FMemory::Memcpy(Buffer, Indices.GetData(), DataSizeInBytes);
	RHIUnlockBuffer(IndexBufferRHI);
}

FHairCardsRestResource::FHairCardsRestResource(const FHairCardsBulkData& InBulkData, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Immediate, InResourceName, false),
	RestPositionBuffer(),
	RestIndexBuffer(InBulkData.Indices),
	NormalsBuffer(),
	UVsBuffer(),
	MaterialsBuffer(),
	BulkData(InBulkData)
{

}

void FHairCardsRestResource::InternalAllocate()
{
	// These resources are kept as regular (i.e., non-RDG resources) as they need to be bound at the input assembly stage by the Vertex declaraction which requires FVertexBuffer type
	CreateBuffer<FHairCardsPositionFormat>(BulkData.Positions, RestPositionBuffer, ToHairResourceDebugName(TEXT("Hair.CardsRest_PositionBuffer"), ResourceName));
	CreateBuffer<FHairCardsNormalFormat>(BulkData.Normals, NormalsBuffer, ToHairResourceDebugName(TEXT("Hair.CardsRest_NormalBuffer"), ResourceName));
	CreateBuffer<FHairCardsUVFormat>(BulkData.UVs, UVsBuffer, ToHairResourceDebugName(TEXT("Hair.CardsRest_UVBuffer"), ResourceName));
	CreateBuffer<FHairCardsMaterialFormat>(BulkData.Materials, MaterialsBuffer, ToHairResourceDebugName(TEXT("Hair.CardsRest_MaterialBuffer"), ResourceName));

	FSamplerStateRHIRef DefaultSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	DepthSampler = DefaultSampler;
	TangentSampler = DefaultSampler;
	CoverageSampler = DefaultSampler;
	AttributeSampler = DefaultSampler;
	MaterialSampler = DefaultSampler;
}

void FHairCardsRestResource::InternalRelease()
{
}

void FHairCardsRestResource::InitResource()
{
	FRenderResource::InitResource();
	RestIndexBuffer.InitResource();
	RestPositionBuffer.InitResource();
	NormalsBuffer.InitResource();
	UVsBuffer.InitResource();
	MaterialsBuffer.InitResource();
}

void FHairCardsRestResource::ReleaseResource()
{
	FRenderResource::ReleaseResource();
	RestIndexBuffer.ReleaseResource();
	RestPositionBuffer.ReleaseResource();
	NormalsBuffer.ReleaseResource();
	UVsBuffer.ReleaseResource();
	MaterialsBuffer.ReleaseResource();
}

/////////////////////////////////////////////////////////////////////////////////////////
FHairCardsProceduralResource::FHairCardsProceduralResource(const FHairCardsProceduralDatas::FRenderData& InRenderData, const FIntPoint& InAtlasResolution, const FHairCardsVoxel& InVoxel):
	FHairCommonResource(EHairStrandsAllocationType::Immediate, FHairResourceName()),
	CardBoundCount(InRenderData.ClusterBounds.Num()),
	AtlasResolution(InAtlasResolution),
	AtlasRectBuffer(),
	LengthBuffer(),
	CardItToClusterBuffer(),
	ClusterIdToVerticesBuffer(),
	ClusterBoundBuffer(),
	CardsStrandsPositions(),
	CardsStrandsAttributes(),
	RenderData(InRenderData)
{
	CardVoxel = InVoxel;
}

void FHairCardsProceduralResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	InternalCreateVertexBufferRDG<FHairCardsAtlasRectFormat>(GraphBuilder, RenderData.CardsRect, AtlasRectBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_AtlasRectBuffer"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG<FHairCardsDimensionFormat>(GraphBuilder, RenderData.CardsLengths, LengthBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_LengthBuffer"), ResourceName), EHairResourceUsageType::Static);

	InternalCreateVertexBufferRDG<FHairCardsOffsetAndCount>(GraphBuilder, RenderData.CardItToCluster, CardItToClusterBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_CardItToClusterBuffer"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG<FHairCardsOffsetAndCount>(GraphBuilder, RenderData.ClusterIdToVertices, ClusterIdToVerticesBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_ClusterIdToVerticesBuffer"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG<FHairCardsBoundsFormat>(GraphBuilder, RenderData.ClusterBounds, ClusterBoundBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_ClusterBoundBuffer"), ResourceName), EHairResourceUsageType::Static);

	InternalCreateVertexBufferRDG<FHairCardsVoxelDensityFormat>(GraphBuilder, RenderData.VoxelDensity, CardVoxel.DensityBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_VoxelDensityBuffer"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG<FHairCardsVoxelTangentFormat>(GraphBuilder, RenderData.VoxelTangent, CardVoxel.TangentBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_VoxelTangentBuffer"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG<FHairCardsVoxelTangentFormat>(GraphBuilder, RenderData.VoxelNormal, CardVoxel.NormalBuffer, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_VoxelNormalBuffer"), ResourceName), EHairResourceUsageType::Static);

	InternalCreateVertexBufferRDG<FHairCardsStrandsPositionFormat>(GraphBuilder, RenderData.CardsStrandsPositions, CardsStrandsPositions, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_CardsStrandsPositions"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG<FHairCardsStrandsAttributeFormat>(GraphBuilder, RenderData.CardsStrandsAttributes, CardsStrandsAttributes, ToHairResourceDebugName(TEXT("Hair.CardsProcedural_CardsStrandsAttributes"), ResourceName), EHairResourceUsageType::Static);
}

void FHairCardsProceduralResource::InternalRelease()
{
	AtlasRectBuffer.Release();
	LengthBuffer.Release();

	CardItToClusterBuffer.Release();
	ClusterIdToVerticesBuffer.Release();
	ClusterBoundBuffer.Release();
	CardsStrandsPositions.Release();
	CardsStrandsAttributes.Release();

	CardVoxel.DensityBuffer.Release();
	CardVoxel.TangentBuffer.Release();
	CardVoxel.NormalBuffer.Release();
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairCardsDeformedResource::FHairCardsDeformedResource(const FHairCardsBulkData& InBulkData, bool bInInitializedData, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	BulkData(InBulkData), bInitializedData(bInInitializedData)
{}

void FHairCardsDeformedResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	if (bInitializedData)
	{
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.Positions, DeformedPositionBuffer[0], ToHairResourceDebugName(TEXT("Hair.CardsDeformedPosition(Current)"), ResourceName), EHairResourceUsageType::Dynamic);
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.Positions, DeformedPositionBuffer[1], ToHairResourceDebugName(TEXT("Hair.CardsDeformedPosition(Previous)"), ResourceName), EHairResourceUsageType::Dynamic);

		InternalCreateVertexBufferRDG<FHairCardsNormalFormat>(GraphBuilder, BulkData.Normals, DeformedNormalBuffer, ToHairResourceDebugName(TEXT("Hair.CardsDeformedNormal"), ResourceName), EHairResourceUsageType::Dynamic);
	}
	else
	{
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.GetNumVertices(), DeformedPositionBuffer[0], ToHairResourceDebugName(TEXT("Hair.CardsDeformedPosition(Current)"), ResourceName), EHairResourceUsageType::Dynamic);
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.GetNumVertices(), DeformedPositionBuffer[1], ToHairResourceDebugName(TEXT("Hair.CardsDeformedPosition(Previous)"), ResourceName), EHairResourceUsageType::Dynamic);

		InternalCreateVertexBufferRDG<FHairCardsNormalFormat>(GraphBuilder, BulkData.GetNumVertices() * FHairCardsNormalFormat::ComponentCount, DeformedNormalBuffer, ToHairResourceDebugName(TEXT("Hair.CardsDeformedNormal"), ResourceName), EHairResourceUsageType::Dynamic);

		// Manually transit to SRVs, in case of the cards are rendered not visible, but still rendered (in shadows for instance). In such a case, the cards deformation pass is not called, and thus the 
		// buffers are never transit from UAV (clear) to SRV for rasterization. 
		GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, DeformedPositionBuffer[0], ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
		GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, DeformedPositionBuffer[1], ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
	}

	FRDGImportedBuffer CardsDeformedNormalRDGBuffer = Register(GraphBuilder, DeformedNormalBuffer, ERDGImportedBufferFlags::CreateSRV);
	GraphBuilder.UseExternalAccessMode(CardsDeformedNormalRDGBuffer.Buffer, ERHIAccess::SRVMask);
}

void FHairCardsDeformedResource::InternalRelease()
{
	DeformedPositionBuffer[0].Release();
	DeformedPositionBuffer[1].Release();
	DeformedNormalBuffer.Release();
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairMeshesRestResource::FHairMeshesRestResource(const FHairMeshesBulkData& InBulkData, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Immediate, InResourceName, false),
	RestPositionBuffer(),
	IndexBuffer(InBulkData.Indices),
	NormalsBuffer(),
	UVsBuffer(),
	BulkData(InBulkData)
{
	check(BulkData.GetNumVertices() > 0);
	check(IndexBuffer.Indices.Num() > 0);
}

void FHairMeshesRestResource::InternalAllocate()
{
	// These resources are kept as regular (i.e., non-RDG resources) as they need to be bound at the input assembly stage by the Vertex declaraction which requires FVertexBuffer type
	CreateBuffer<FHairCardsPositionFormat>(BulkData.Positions, RestPositionBuffer, ToHairResourceDebugName(TEXT("Hair.MeshesRest_Positions"), ResourceName));
	CreateBuffer<FHairCardsNormalFormat>(BulkData.Normals, NormalsBuffer, ToHairResourceDebugName(TEXT("Hair.MeshesRest_Normals"), ResourceName));
	CreateBuffer<FHairCardsUVFormat>(BulkData.UVs, UVsBuffer, ToHairResourceDebugName(TEXT("Hair.MeshesRest_UVs"), ResourceName));
}

void FHairMeshesRestResource::InternalRelease()
{

}

void FHairMeshesRestResource::InitResource()
{
	FRenderResource::InitResource();
	IndexBuffer.InitResource();
	RestPositionBuffer.InitResource();
	NormalsBuffer.InitResource();
	UVsBuffer.InitResource();
}

void FHairMeshesRestResource::ReleaseResource()
{
	FRenderResource::ReleaseResource();
	IndexBuffer.ReleaseResource();
	RestPositionBuffer.ReleaseResource();
	NormalsBuffer.ReleaseResource();
	UVsBuffer.ReleaseResource();

}

/////////////////////////////////////////////////////////////////////////////////////////

FHairMeshesDeformedResource::FHairMeshesDeformedResource(const FHairMeshesBulkData& InBulkData, bool bInInitializedData, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	BulkData(InBulkData), bInitializedData(bInInitializedData)
{}

void FHairMeshesDeformedResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	if (bInitializedData)
	{
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.Positions, DeformedPositionBuffer[0], ToHairResourceDebugName(TEXT("Hair.MeshesDeformed(Current)"), ResourceName), EHairResourceUsageType::Dynamic);
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.Positions, DeformedPositionBuffer[1], ToHairResourceDebugName(TEXT("Hair.MeshesDeformed(Previous)"), ResourceName), EHairResourceUsageType::Dynamic);
	}
	else
	{
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.GetNumVertices(), DeformedPositionBuffer[0], ToHairResourceDebugName(TEXT("Hair.MeshesDeformed(Current)"), ResourceName), EHairResourceUsageType::Dynamic);
		InternalCreateVertexBufferRDG<FHairCardsPositionFormat>(GraphBuilder, BulkData.GetNumVertices(), DeformedPositionBuffer[1], ToHairResourceDebugName(TEXT("Hair.MeshesDeformed(Previous)"), ResourceName), EHairResourceUsageType::Dynamic);
	}
}

void FHairMeshesDeformedResource::InternalRelease()
{
	DeformedPositionBuffer[0].Release();
	DeformedPositionBuffer[1].Release();
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairStrandsRestResource::FHairStrandsRestResource(FHairStrandsBulkData& InBulkData, EHairStrandsResourcesType InCurveType, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	PositionBuffer(), Attribute0Buffer(), Attribute1Buffer(), MaterialBuffer(), BulkData(InBulkData), CurveType(InCurveType)
{
	// Sanity check
	check(!!(BulkData.Flags & FHairStrandsBulkData::DataFlags_HasData));
}

bool FHairStrandsRestResource::InternalIsDataLoaded()
{
	if (BulkDataRequest.IsNone())
	{
		FBulkDataBatchRequest::FBatchBuilder Batch = FBulkDataBatchRequest::NewBatch(4);
		Batch.Read(BulkData.Positions);
		Batch.Read(BulkData.Attributes0);
		Batch.Read(BulkData.Attributes1);

		if (!!(BulkData.Flags & FHairStrandsBulkData::DataFlags_HasMaterialData))
		{
			Batch.Read(BulkData.Materials);
		}

		Batch.Issue(BulkDataRequest);
	}
	
	return BulkDataRequest.IsCompleted();
}

void FHairStrandsRestResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	BulkDataRequest = FBulkDataBatchRequest();

	const uint32 PointCount = BulkData.PointCount;

	// 1. Lock data, which force the loading data from files (on non-editor build/cooked data). These data are then uploaded to the GPU
	// 2. A local copy is done by the buffer uploader. This copy is discarded once the uploading is done.
	InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsPositionFormat>(GraphBuilder, BulkData.Positions, PointCount, PositionBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRest_PositionBuffer), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsAttribute0Format>(GraphBuilder, BulkData.Attributes0, PointCount, Attribute0Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRest_Attribute0Buffer), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsAttribute1Format>(GraphBuilder, BulkData.Attributes1, PointCount, Attribute1Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRest_Attribute1Buffer), ResourceName), EHairResourceUsageType::Static);

	if (!!(BulkData.Flags & FHairStrandsBulkData::DataFlags_HasMaterialData))
	{
		InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsMaterialFormat>(GraphBuilder, BulkData.Materials, PointCount, MaterialBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRest_MaterialBuffer), ResourceName), EHairResourceUsageType::Static);
	}

	TArray<FVector4f> RestOffset;
	RestOffset.Add((FVector3f)BulkData.GetPositionOffset());// LWC_TODO: precision loss
	InternalCreateVertexBufferRDG<FHairStrandsPositionOffsetFormat>(GraphBuilder, RestOffset, PositionOffsetBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRest_PositionOffsetBuffer), ResourceName), EHairResourceUsageType::Static, ERDGInitialDataFlags::None);
	GraphBuilder.UseExternalAccessMode(Register(GraphBuilder, PositionOffsetBuffer, ERDGImportedBufferFlags::CreateSRV).Buffer, ERHIAccess::SRVMask);
}

void AddHairTangentPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	uint32 VertexStart,
	uint32 VertexCount,
	FHairGroupPublicData* HairGroupPublicData,
	FRDGBufferSRVRef PositionBuffer,
	FRDGImportedBuffer OutTangentBuffer);

FRDGExternalBuffer FHairStrandsRestResource::GetTangentBuffer(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap)
{
	// Lazy allocation and update
	if (TangentBuffer.Buffer == nullptr)
	{
		InternalCreateVertexBufferRDG<FHairStrandsTangentFormat>(GraphBuilder, BulkData.PointCount * FHairStrandsTangentFormat::ComponentCount, TangentBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsRest_TangentBuffer"), ResourceName), EHairResourceUsageType::Dynamic);

		AddHairTangentPass(
			GraphBuilder,
			ShaderMap,
			0,
			BulkData.PointCount,
			nullptr,
			RegisterAsSRV(GraphBuilder, PositionBuffer),
			Register(GraphBuilder, TangentBuffer, ERDGImportedBufferFlags::CreateUAV));
	}

	return TangentBuffer;
}

void FHairStrandsRestResource::InternalRelease()
{
	PositionBuffer.Release();
	PositionOffsetBuffer.Release();
	Attribute0Buffer.Release();
	Attribute1Buffer.Release();
	MaterialBuffer.Release();
	TangentBuffer.Release();
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairStrandsDeformedResource::FHairStrandsDeformedResource(FHairStrandsBulkData& InBulkData, EHairStrandsResourcesType InCurveType, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	BulkData(InBulkData), CurveType(InCurveType)
{
	GetPositionOffset(FHairStrandsDeformedResource::EFrameType::Current)  = BulkData.GetPositionOffset();
	GetPositionOffset(FHairStrandsDeformedResource::EFrameType::Previous) = BulkData.GetPositionOffset();
}

void FHairStrandsDeformedResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	const uint32 PointCount = BulkData.PointCount;

	InternalCreateVertexBufferRDG<FHairStrandsPositionFormat>(GraphBuilder, BulkData.PointCount, DeformedPositionBuffer[0], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsDeformed_DeformedPositionBuffer0), ResourceName), EHairResourceUsageType::Dynamic);
	InternalCreateVertexBufferRDG<FHairStrandsPositionFormat>(GraphBuilder, BulkData.PointCount, DeformedPositionBuffer[1], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsDeformed_DeformedPositionBuffer1), ResourceName), EHairResourceUsageType::Dynamic);
	InternalCreateVertexBufferRDG<FHairStrandsTangentFormat>(GraphBuilder, BulkData.PointCount * FHairStrandsTangentFormat::ComponentCount, TangentBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsDeformed_TangentBuffer), ResourceName), EHairResourceUsageType::Dynamic);

	TArray<FVector4f> DefaultOffsets;
	DefaultOffsets.Add((FVector3f)BulkData.GetPositionOffset()); // LWC_TODO: precision loss
	InternalCreateVertexBufferRDG<FHairStrandsPositionOffsetFormat>(GraphBuilder, DefaultOffsets, DeformedOffsetBuffer[0], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsDeformed_DeformedOffsetBuffer0), ResourceName), EHairResourceUsageType::Dynamic, ERDGInitialDataFlags::None);
	InternalCreateVertexBufferRDG<FHairStrandsPositionOffsetFormat>(GraphBuilder, DefaultOffsets, DeformedOffsetBuffer[1], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsDeformed_DeformedOffsetBuffer1), ResourceName), EHairResourceUsageType::Dynamic, ERDGInitialDataFlags::None);

}

void FHairStrandsDeformedResource::InternalRelease()
{
	DeformedPositionBuffer[0].Release();
	DeformedPositionBuffer[1].Release();
	TangentBuffer.Release();

	DeformedOffsetBuffer[0].Release();
	DeformedOffsetBuffer[1].Release();
}

/////////////////////////////////////////////////////////////////////////////////////////
// Cluster culling data

FHairStrandsClusterCullingData::FHairStrandsClusterCullingData()
{

}

void FHairStrandsClusterCullingData::Reset()
{
	*this = FHairStrandsClusterCullingData();
}

/////////////////////////////////////////////////////////////////////////////////////////
// Cluster culling bulk data
FHairStrandsClusterCullingBulkData::FHairStrandsClusterCullingBulkData()
{

}

void FHairStrandsClusterCullingBulkData::Reset()
{
	ClusterCount = 0;
	ClusterLODCount = 0;
	VertexCount = 0;
	VertexLODCount = 0;

	LODVisibility.Empty();;
	CPULODScreenSize.Empty();

	ClusterLODInfos.RemoveBulkData();
	VertexToClusterIds.RemoveBulkData();
	ClusterVertexIds.RemoveBulkData();
	PackedClusterInfos.RemoveBulkData();

	// Reset the bulk byte buffer to ensure the (serialize) data size is reset to 0
	ClusterLODInfos 	= FByteBulkData();
	VertexToClusterIds 	= FByteBulkData();
	ClusterVertexIds 	= FByteBulkData();
	PackedClusterInfos 	= FByteBulkData();
}

void FHairStrandsClusterCullingBulkData::Serialize(FArchive& Ar, UObject* Owner)
{
	Ar << ClusterCount;
	Ar << ClusterLODCount;
	Ar << VertexCount;
	Ar << VertexLODCount;
	Ar << LODVisibility;
	Ar << CPULODScreenSize;

	const int32 ChunkIndex = 0;
	bool bAttemptFileMapping = false;

	if (Ar.IsSaving())
	{
		const uint32 BulkFlags = BULKDATA_Force_NOT_InlinePayload;
		ClusterLODInfos.SetBulkDataFlags(BulkFlags);
		VertexToClusterIds.SetBulkDataFlags(BulkFlags);
		ClusterVertexIds.SetBulkDataFlags(BulkFlags);
		PackedClusterInfos.SetBulkDataFlags(BulkFlags);
	}

	if (ClusterLODCount)
	{
		ClusterLODInfos.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	}

	if (VertexCount)
	{
		VertexToClusterIds.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	}

	if (VertexLODCount)
	{
		ClusterVertexIds.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	}

	if (ClusterCount)
	{
		PackedClusterInfos.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	}

	if (GHairStrandsBulkData_Validation > 0 && Ar.IsSaving())
	{
		Validate(true);
	}
}

void FHairStrandsClusterCullingBulkData::Validate(bool bIsSaving)
{
	if (ClusterCount == 0)
		return;

	const FHairClusterInfo::Packed* Datas = (const FHairClusterInfo::Packed*)PackedClusterInfos.Lock(LOCK_READ_ONLY);
	
	// Simple heuristic to check if the data are valid
	const uint32 MaxCount = FMath::Min(ClusterCount, 128u);
	bool bIsValid = true;
	for (uint32 It = 0; It < MaxCount; ++It)
	{
		bIsValid = bIsValid && Datas[It].LODCount <= 8;
		if (!bIsValid) break;
	}
	if (!bIsValid)
	{
		FString Name = ClusterLODInfos.GetDebugName();
		UE_LOG(LogHairStrands, Error, TEXT("[Groom/DDC] Strands - Invalid ClusterCullingBulkData when %s bulk data - %s"), bIsSaving ? TEXT("Saving") : TEXT("Loading"), *Name);
	}

	PackedClusterInfos.Unlock();
}

/////////////////////////////////////////////////////////////////////////////////////////
// Cluster culling resources
FHairStrandsClusterCullingResource::FHairStrandsClusterCullingResource(FHairStrandsClusterCullingBulkData& InBulkData, const FHairResourceName& InResourceName):
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	BulkData(InBulkData)
{

}

bool FHairStrandsClusterCullingResource::InternalIsDataLoaded()
{
	if (BulkDataRequest.IsNone())
	{
		FBulkDataBatchRequest::NewBatch(4)
			.Read(BulkData.PackedClusterInfos)
			.Read(BulkData.ClusterLODInfos)
			.Read(BulkData.VertexToClusterIds)
			.Read(BulkData.ClusterVertexIds)
			.Issue(BulkDataRequest);
	}

	return BulkDataRequest.IsCompleted();
}

void FHairStrandsClusterCullingResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	BulkDataRequest = FBulkDataBatchRequest();

	if (GHairStrandsBulkData_Validation > 0)
	{
		BulkData.Validate(false);
	}

	InternalCreateStructuredBufferRDG_FromBulkData<FHairClusterInfoFormat>(GraphBuilder, BulkData.PackedClusterInfos, BulkData.ClusterCount, ClusterInfoBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsClusterCulling_ClusterInfoBuffer"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateStructuredBufferRDG_FromBulkData<FHairClusterLODInfoFormat>(GraphBuilder, BulkData.ClusterLODInfos, BulkData.ClusterLODCount, ClusterLODInfoBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsClusterCulling_ClusterLODInfoBuffer"), ResourceName), EHairResourceUsageType::Static);

	InternalCreateVertexBufferRDG_FromBulkData<FHairClusterIndexFormat>(GraphBuilder, BulkData.VertexToClusterIds, BulkData.VertexCount, VertexToClusterIdBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsClusterCulling_VertexToClusterIds"), ResourceName), EHairResourceUsageType::Static);
	InternalCreateVertexBufferRDG_FromBulkData<FHairClusterIndexFormat>(GraphBuilder, BulkData.ClusterVertexIds, BulkData.VertexLODCount, ClusterVertexIdBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsClusterCulling_ClusterVertexIds"), ResourceName), EHairResourceUsageType::Static);
}

void FHairStrandsClusterCullingResource::InternalRelease()
{
	ClusterInfoBuffer.Release();
	ClusterLODInfoBuffer.Release();
	ClusterVertexIdBuffer.Release();
	VertexToClusterIdBuffer.Release();
	BulkDataRequest = FBulkDataBatchRequest();
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairStrandsRestRootResource::FHairStrandsRestRootResource(FHairStrandsRootBulkData& InBulkData, EHairStrandsResourcesType InCurveType, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	BulkData(InBulkData), CurveType(InCurveType)
{
	PopulateFromRootData();
}

void FHairStrandsRestRootResource::PopulateFromRootData()
{
	uint32 LODIndex = 0;
	LODs.Reserve(BulkData.MeshProjectionLODs.Num());
	for (const FHairStrandsRootBulkData::FMeshProjectionLOD& MeshProjectionLOD : BulkData.MeshProjectionLODs)
	{
		FLOD& LOD = LODs.AddDefaulted_GetRef();

		LOD.LODIndex = MeshProjectionLOD.LODIndex;
		LOD.Status = FLOD::EStatus::Invalid;
		LOD.SampleCount = MeshProjectionLOD.SampleCount;
	}
	LODRequests.SetNum(BulkData.MeshProjectionLODs.Num());
}

bool FHairStrandsRestRootResource::InternalIsDataLoaded()
{
	if (BulkData.PointCount == 0)
	{
		return true;
	}

	if (BulkDataRequest.IsNone())
	{
		FBulkDataBatchRequest::NewBatch(1)
			.Read(BulkData.VertexToCurveIndexBuffer)
			.Issue(BulkDataRequest);
	}

	return BulkDataRequest.IsCompleted();
}

bool FHairStrandsRestRootResource::InternalIsLODDataLoaded(int32 LODIndex)
{
	bool bIsLoading = false;
	
	check(LODs.Num() == BulkData.MeshProjectionLODs.Num());
	if (LODIndex >= 0 && LODIndex < LODs.Num())
	{
		FBulkDataBatchRequest& LODRequest = LODRequests[LODIndex];
		if (LODRequest.IsNone())
		{
			FBulkDataBatchRequest::FBatchBuilder Batch = FBulkDataBatchRequest::NewBatch(9);
			FHairStrandsRootBulkData::FMeshProjectionLOD& CPUData = BulkData.MeshProjectionLODs[LODIndex];
			const bool bHasValidCPUData = CPUData.RootBarycentricBuffer.GetBulkDataSize() > 0;
			if (bHasValidCPUData)
			{
				Batch.Read(CPUData.RootBarycentricBuffer);
				Batch.Read(CPUData.RootToUniqueTriangleIndexBuffer);
				Batch.Read(CPUData.UniqueTriangleIndexBuffer);

				Batch.Read(CPUData.RestUniqueTrianglePosition0Buffer);
				Batch.Read(CPUData.RestUniqueTrianglePosition1Buffer);
				Batch.Read(CPUData.RestUniqueTrianglePosition2Buffer);
			}
			
			const bool bHasValidCPUWeights = CPUData.MeshSampleIndicesBuffer.GetBulkDataSize() > 0;
			if (bHasValidCPUWeights)
			{
				Batch.Read(CPUData.MeshInterpolationWeightsBuffer);
				Batch.Read(CPUData.MeshSampleIndicesBuffer);
				Batch.Read(CPUData.RestSamplePositionsBuffer);
			}
			
			if (bHasValidCPUData || bHasValidCPUWeights)
			{
				Batch.Issue(LODRequest);
			}
		}

		bIsLoading = LODRequest.IsCompleted() == false;
	}

	return !bIsLoading;
}

void FHairStrandsRestRootResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	// Once empty, the MeshProjectionLODsneeds to be repopulate as it might be re-initialized. 
	// E.g., when a resource is updated, it is first released, then re-init. 
	if (LODs.IsEmpty())
	{
		PopulateFromRootData();
	}

	if (BulkData.PointCount > 0)
	{
		InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsIndexFormat>(GraphBuilder, BulkData.VertexToCurveIndexBuffer, BulkData.PointCount, VertexToCurveIndexBuffer, HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_VertexToCurveIndexBuffer), EHairResourceUsageType::Static);
	}
}

void FHairStrandsRestRootResource::InternalAllocateLOD(FRDGBuilder& GraphBuilder, int32 LODIndex)
{
	// Sanity check ti insure that the 'common' part of FHairStrandsRestRootResource is already initialized
	check(bIsInitialized);
	check(BulkData.PointCount > 0);
	check(LODs.Num() == BulkData.MeshProjectionLODs.Num());
	if (LODIndex >= 0 && LODIndex < LODs.Num())
	{
		FLOD& GPUData = LODs[LODIndex];
		const bool bIsLODInitialized = GPUData.Status == FLOD::EStatus::Completed || GPUData.Status == FLOD::EStatus::Initialized;
		if (bIsLODInitialized)
		{
			return;
		}
		
		LODRequests[LODIndex] = FBulkDataBatchRequest();

		FHairStrandsRootBulkData::FMeshProjectionLOD& CPUData = BulkData.MeshProjectionLODs[LODIndex];
		const bool bHasValidCPUData = CPUData.RootBarycentricBuffer.GetBulkDataSize() > 0;
		if (bHasValidCPUData)
		{
			GPUData.Status = FLOD::EStatus::Completed;
			
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsRootBarycentricFormat>(GraphBuilder, CPUData.RootBarycentricBuffer, BulkData.RootCount, GPUData.RootBarycentricBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RootTriangleBarycentricBuffer), ResourceName), EHairResourceUsageType::Static);
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsRootToUniqueTriangleIndexFormat>(GraphBuilder, CPUData.RootToUniqueTriangleIndexBuffer, BulkData.RootCount, GPUData.RootToUniqueTriangleIndexBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RootToUniqueTriangleIndexBuffer), ResourceName), EHairResourceUsageType::Static);
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsUniqueTriangleIndexFormat>(GraphBuilder, CPUData.UniqueTriangleIndexBuffer, CPUData.UniqueTriangleCount, GPUData.UniqueTriangleIndexBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_UniqueTriangleIndexBuffer), ResourceName), EHairResourceUsageType::Static);

			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.RestUniqueTrianglePosition0Buffer, CPUData.UniqueTriangleCount, GPUData.RestUniqueTrianglePosition0Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestUniqueTrianglePosition0Buffer), ResourceName), EHairResourceUsageType::Static);
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.RestUniqueTrianglePosition1Buffer, CPUData.UniqueTriangleCount, GPUData.RestUniqueTrianglePosition1Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestUniqueTrianglePosition1Buffer), ResourceName), EHairResourceUsageType::Static);
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.RestUniqueTrianglePosition2Buffer, CPUData.UniqueTriangleCount, GPUData.RestUniqueTrianglePosition2Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestUniqueTrianglePosition2Buffer), ResourceName), EHairResourceUsageType::Static);
		}
		else
		{
			GPUData.Status = FLOD::EStatus::Initialized;
			
			InternalCreateVertexBufferRDG<FHairStrandsRootBarycentricFormat>(GraphBuilder, BulkData.RootCount, GPUData.RootBarycentricBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RootBarycentricBuffer), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsRootToUniqueTriangleIndexFormat>(GraphBuilder, BulkData.RootCount, GPUData.RootToUniqueTriangleIndexBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RootToUniqueTriangleIndexBuffer), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsUniqueTriangleIndexFormat>(GraphBuilder, CPUData.UniqueTriangleCount, GPUData.UniqueTriangleIndexBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_UniqueTriangleIndexBuffer), ResourceName), EHairResourceUsageType::Dynamic);
			
			// Create buffers. Initialization will be done by render passes
			InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.UniqueTriangleCount, GPUData.RestUniqueTrianglePosition0Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestUniqueTrianglePosition0Buffer), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.UniqueTriangleCount, GPUData.RestUniqueTrianglePosition1Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestUniqueTrianglePosition1Buffer), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.UniqueTriangleCount, GPUData.RestUniqueTrianglePosition2Buffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestUniqueTrianglePosition2Buffer), ResourceName), EHairResourceUsageType::Dynamic);
		}

		GPUData.SampleCount = CPUData.SampleCount;
		const bool bHasValidCPUWeights = CPUData.MeshSampleIndicesBuffer.GetBulkDataSize() > 0;
		if (bHasValidCPUWeights)
		{
			const uint32 InteroplationWeightCount = CPUData.MeshInterpolationWeightsBuffer.GetBulkDataSize() / sizeof(FHairStrandsWeightFormat::Type);
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsWeightFormat>(GraphBuilder, CPUData.MeshInterpolationWeightsBuffer, InteroplationWeightCount, GPUData.MeshInterpolationWeightsBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_MeshInterpolationWeightsBuffer), ResourceName), EHairResourceUsageType::Static);
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsIndexFormat>(GraphBuilder, CPUData.MeshSampleIndicesBuffer, CPUData.SampleCount, GPUData.MeshSampleIndicesBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_MeshSampleIndicesBuffer), ResourceName), EHairResourceUsageType::Static);
			InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.RestSamplePositionsBuffer, CPUData.SampleCount, GPUData.RestSamplePositionsBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestSamplePositionsBuffer), ResourceName), EHairResourceUsageType::Static);
		}
		else
		{
			// TODO: do not allocate these resources, since they won't be used
			InternalCreateVertexBufferRDG<FHairStrandsWeightFormat>(GraphBuilder, (CPUData.SampleCount+4) * (CPUData.SampleCount+4), GPUData.MeshInterpolationWeightsBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_MeshInterpolationWeightsBuffer), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsIndexFormat>(GraphBuilder, CPUData.SampleCount, GPUData.MeshSampleIndicesBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_MeshSampleIndicesBuffer), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, CPUData.SampleCount, GPUData.RestSamplePositionsBuffer, ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRestRoot_RestSamplePositionsBuffer), ResourceName), EHairResourceUsageType::Dynamic);
		}
	}
}

void FHairStrandsRestRootResource::InternalRelease()
{
	VertexToCurveIndexBuffer.Release();
	
	for (FLOD& GPUData : LODs)
	{
		GPUData.Status = FLOD::EStatus::Invalid;
		GPUData.RootBarycentricBuffer.Release();
		GPUData.RootToUniqueTriangleIndexBuffer.Release();
		GPUData.UniqueTriangleIndexBuffer.Release();
		GPUData.RestUniqueTrianglePosition0Buffer.Release();
		GPUData.RestUniqueTrianglePosition1Buffer.Release();
		GPUData.RestUniqueTrianglePosition2Buffer.Release();
		GPUData.SampleCount = 0;
		GPUData.MeshInterpolationWeightsBuffer.Release();
		GPUData.MeshSampleIndicesBuffer.Release();
		GPUData.RestSamplePositionsBuffer.Release();
	}
	LODs.Empty();
	LODRequests.Empty();
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairStrandsDeformedRootResource::FHairStrandsDeformedRootResource(EHairStrandsResourcesType InCurveType, const FHairResourceName& InResourceName):
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	CurveType(InCurveType)
{

}

FHairStrandsDeformedRootResource::FHairStrandsDeformedRootResource(const FHairStrandsRestRootResource* InRestResources, EHairStrandsResourcesType InCurveType, const FHairResourceName& InResourceName):
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	CurveType(InCurveType)
{
	check(InRestResources);
	uint32 LODIndex = 0;
	RootCount = InRestResources->BulkData.RootCount;
	LODs.Reserve(InRestResources->LODs.Num());
	for (const FHairStrandsRestRootResource::FLOD& InLOD : InRestResources->LODs)
	{
		FLOD& LOD = LODs.AddDefaulted_GetRef();

		LOD.Status = FLOD::EStatus::Invalid;
		LOD.LODIndex = InLOD.LODIndex;
		LOD.SampleCount = InLOD.SampleCount;
	}
}

void FHairStrandsDeformedRootResource::InternalAllocateLOD(FRDGBuilder& GraphBuilder, int32 LODIndex)
{
	if (RootCount > 0 && LODIndex >= 0 && LODIndex < LODs.Num())
	{
		FLOD& LOD = LODs[LODIndex];
		if (LOD.Status == FLOD::EStatus::Invalid)
		{		
			LOD.Status = FLOD::EStatus::Initialized;
			if (LOD.SampleCount > 0)
			{
				InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, LOD.SampleCount, LOD.DeformedSamplePositionsBuffer[0], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedSamplePositionsBuffer0), ResourceName), EHairResourceUsageType::Dynamic);
				InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, LOD.SampleCount + 4, LOD.MeshSampleWeightsBuffer[0], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_MeshSampleWeightsBuffer0), ResourceName), EHairResourceUsageType::Dynamic);

				// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
				if (IsHairStrandContinuousDecimationReorderingEnabled())
				{
					InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, LOD.SampleCount, LOD.DeformedSamplePositionsBuffer[1], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedSamplePositionsBuffer1), ResourceName), EHairResourceUsageType::Dynamic);
					InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, LOD.SampleCount + 4, LOD.MeshSampleWeightsBuffer[1], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_MeshSampleWeightsBuffer1), ResourceName), EHairResourceUsageType::Dynamic);
				}
			}

			InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, RootCount, LOD.DeformedUniqueTrianglePosition0Buffer[0], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedUniqueTrianglePosition0Buffer0), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, RootCount, LOD.DeformedUniqueTrianglePosition1Buffer[0], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedUniqueTrianglePosition1Buffer0), ResourceName), EHairResourceUsageType::Dynamic);
			InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, RootCount, LOD.DeformedUniqueTrianglePosition2Buffer[0], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedUniqueTrianglePosition2Buffer0), ResourceName), EHairResourceUsageType::Dynamic);

			// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
			if (IsHairStrandContinuousDecimationReorderingEnabled())
			{
				InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, RootCount, LOD.DeformedUniqueTrianglePosition0Buffer[1], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedUniqueTrianglePosition0Buffer1), ResourceName), EHairResourceUsageType::Dynamic);
				InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, RootCount, LOD.DeformedUniqueTrianglePosition1Buffer[1], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedUniqueTrianglePosition1Buffer1), ResourceName), EHairResourceUsageType::Dynamic);
				InternalCreateVertexBufferRDG<FHairStrandsMeshTrianglePositionFormat>(GraphBuilder, RootCount, LOD.DeformedUniqueTrianglePosition2Buffer[1], ToHairResourceDebugName(HAIRSTRANDS_RESOUCE_NAME(CurveType, Hair.StrandsRootDeformed_DeformedUniqueTrianglePosition2Buffer1), ResourceName), EHairResourceUsageType::Dynamic);
			}
		}
	}
}

void FHairStrandsDeformedRootResource::InternalRelease()
{
	for (FLOD& GPUData : LODs)
	{
		GPUData.Status = FLOD::EStatus::Invalid;
		GPUData.DeformedUniqueTrianglePosition0Buffer[0].Release();
		GPUData.DeformedUniqueTrianglePosition1Buffer[0].Release();
		GPUData.DeformedUniqueTrianglePosition2Buffer[0].Release();
		GPUData.DeformedSamplePositionsBuffer[0].Release();
		GPUData.MeshSampleWeightsBuffer[0].Release();
	
		// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
		if (IsHairStrandContinuousDecimationReorderingEnabled())
		{
			GPUData.DeformedUniqueTrianglePosition0Buffer[1].Release();
			GPUData.DeformedUniqueTrianglePosition1Buffer[1].Release();
			GPUData.DeformedUniqueTrianglePosition2Buffer[1].Release();
			GPUData.DeformedSamplePositionsBuffer[1].Release();
			GPUData.MeshSampleWeightsBuffer[1].Release();
		}
	}
	LODs.Empty();
}

/////////////////////////////////////////////////////////////////////////////////////////
// Root bulk data
FHairStrandsRootBulkData::FHairStrandsRootBulkData()
{

}

bool FHairStrandsRootBulkData::HasProjectionData() const
{
	bool bIsValid = MeshProjectionLODs.Num() > 0;
	bIsValid = RootCount > 0;
	for (const FMeshProjectionLOD& LOD : MeshProjectionLODs)
	{
		bIsValid = bIsValid && LOD.LODIndex > 0;
		if (!bIsValid) break;
	}
	return bIsValid;
}

const TArray<uint32>& FHairStrandsRootBulkData::GetValidSectionIndices(int32 LODIndex) const
{
	check(LODIndex >= 0 && LODIndex < MeshProjectionLODs.Num());
	return MeshProjectionLODs[LODIndex].UniqueSectionIndices;
}

static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairStrandsRootBulkData::FMeshProjectionLOD& LOD)
{
	const int32 ChunkIndex = 0;
	bool bAttemptFileMapping = false;

	if (Ar.IsSaving())
	{
		const uint32 BulkFlags = BULKDATA_Force_NOT_InlinePayload;
		LOD.RootBarycentricBuffer.SetBulkDataFlags(BulkFlags);
		LOD.RootToUniqueTriangleIndexBuffer.SetBulkDataFlags(BulkFlags);
		LOD.UniqueTriangleIndexBuffer.SetBulkDataFlags(BulkFlags);
		LOD.RestUniqueTrianglePosition0Buffer.SetBulkDataFlags(BulkFlags);
		LOD.RestUniqueTrianglePosition1Buffer.SetBulkDataFlags(BulkFlags);
		LOD.RestUniqueTrianglePosition2Buffer.SetBulkDataFlags(BulkFlags);

		LOD.MeshInterpolationWeightsBuffer.SetBulkDataFlags(BulkFlags);
		LOD.MeshSampleIndicesBuffer.SetBulkDataFlags(BulkFlags);
		LOD.RestSamplePositionsBuffer.SetBulkDataFlags(BulkFlags);
	}

	Ar << LOD.LODIndex;
	Ar << LOD.UniqueTriangleCount;
	LOD.RootBarycentricBuffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	LOD.RootToUniqueTriangleIndexBuffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	LOD.UniqueTriangleIndexBuffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	LOD.RestUniqueTrianglePosition0Buffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	LOD.RestUniqueTrianglePosition1Buffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	LOD.RestUniqueTrianglePosition2Buffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);

	Ar << LOD.SampleCount;
	if (LOD.SampleCount)
	{
		LOD.MeshInterpolationWeightsBuffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		LOD.MeshSampleIndicesBuffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		LOD.RestSamplePositionsBuffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
	}
		LOD.UniqueSectionIndices.BulkSerialize(Ar);
}

static void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairStrandsRootBulkData::FMeshProjectionLOD>& LODs)
{
	uint32 LODCount = LODs.Num();
	Ar << LODCount;
	if (Ar.IsLoading())
	{
		LODs.SetNum(LODCount);
	}
	for (uint32 LODIt = 0; LODIt < LODCount; ++LODIt)
	{
		InternalSerialize(Ar, Owner, LODs[LODIt]);
	}
}

void FHairStrandsRootBulkData::Serialize(FArchive& Ar, UObject* Owner)
{
	const int32 ChunkIndex = 0;
	bool bAttemptFileMapping = false;

	{
		const uint32 BulkFlags = BULKDATA_Force_NOT_InlinePayload;
		VertexToCurveIndexBuffer.SetBulkDataFlags(BulkFlags);
	}

	if (!Ar.IsObjectReferenceCollector())
	{
		Ar << RootCount;
		Ar << PointCount;
		VertexToCurveIndexBuffer.Serialize(Ar, Owner, ChunkIndex, bAttemptFileMapping);
		InternalSerialize(Ar, Owner, MeshProjectionLODs);
	}
}

void FHairStrandsRootBulkData::Reset()
{
	RootCount = 0;
	VertexToCurveIndexBuffer.RemoveBulkData();
	VertexToCurveIndexBuffer = FByteBulkData();
	for (FMeshProjectionLOD& LOD : MeshProjectionLODs)
	{
		LOD.RootBarycentricBuffer.RemoveBulkData();
		LOD.RootToUniqueTriangleIndexBuffer.RemoveBulkData();
		LOD.UniqueTriangleIndexBuffer.RemoveBulkData();
		LOD.RestUniqueTrianglePosition0Buffer.RemoveBulkData();
		LOD.RestUniqueTrianglePosition1Buffer.RemoveBulkData();
		LOD.RestUniqueTrianglePosition2Buffer.RemoveBulkData();
		LOD.MeshInterpolationWeightsBuffer.RemoveBulkData();
		LOD.MeshSampleIndicesBuffer.RemoveBulkData();
		LOD.RestSamplePositionsBuffer.RemoveBulkData();

		// Reset the bulk byte buffer to ensure the (serialize) data size is reset to 0
		LOD.RootBarycentricBuffer 			 = FByteBulkData();
		LOD.RootToUniqueTriangleIndexBuffer  = FByteBulkData();
		LOD.UniqueTriangleIndexBuffer		 = FByteBulkData();
		LOD.RestUniqueTrianglePosition0Buffer= FByteBulkData();
		LOD.RestUniqueTrianglePosition1Buffer= FByteBulkData();
		LOD.RestUniqueTrianglePosition2Buffer= FByteBulkData();
		LOD.MeshInterpolationWeightsBuffer 	 = FByteBulkData();
		LOD.MeshSampleIndicesBuffer 		 = FByteBulkData();
		LOD.RestSamplePositionsBuffer 		 = FByteBulkData();

		LOD.UniqueSectionIndices.Empty();
	}
	MeshProjectionLODs.Empty();
}

/////////////////////////////////////////////////////////////////////////////////////////
// Root data
FHairStrandsRootData::FHairStrandsRootData()
{

}

bool FHairStrandsRootData::HasProjectionData() const
{
	bool bIsValid = MeshProjectionLODs.Num() > 0;
	for (const FMeshProjectionLOD& LOD : MeshProjectionLODs)
	{
		const bool bHasValidCPUData = LOD.RootBarycentricBuffer.Num() > 0;
		if (bHasValidCPUData)
		{
			bIsValid = bIsValid && LOD.RootBarycentricBuffer.Num() > 0;
			bIsValid = bIsValid && LOD.RootToUniqueTriangleIndexBuffer.Num() > 0;
			bIsValid = bIsValid && LOD.UniqueTriangleIndexBuffer.Num() > 0;
			bIsValid = bIsValid && LOD.RestUniqueTrianglePosition0Buffer.Num() > 0;
			bIsValid = bIsValid && LOD.RestUniqueTrianglePosition1Buffer.Num() > 0;
			bIsValid = bIsValid && LOD.RestUniqueTrianglePosition2Buffer.Num() > 0;

			if (!bIsValid) break;
		}
	}

	return bIsValid;
}

void FHairStrandsRootData::Reset()
{
	RootCount = 0;
	VertexToCurveIndexBuffer.Empty();
	MeshProjectionLODs.Empty();
}

/////////////////////////////////////////////////////////////////////////////////////////

FHairStrandsInterpolationResource::FHairStrandsInterpolationResource(FHairStrandsInterpolationBulkData& InBulkData, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	InterpolationBuffer(), Interpolation0Buffer(), Interpolation1Buffer(), BulkData(InBulkData)
{
	// Sanity check
	check(!!(BulkData.Flags & FHairStrandsInterpolationBulkData::DataFlags_HasData));
}

bool FHairStrandsInterpolationResource::InternalIsDataLoaded()
{
	if (BulkDataRequest.IsNone())
	{
		FBulkDataBatchRequest::FBatchBuilder Batch = FBulkDataBatchRequest::NewBatch(4);
		const bool bUseSingleGuide = !!(BulkData.Flags & FHairStrandsInterpolationBulkData::DataFlags_HasSingleGuideData);
		if (bUseSingleGuide)
		{
			Batch.Read(BulkData.Interpolation);
		}
		else
		{
			Batch.Read(BulkData.Interpolation0);
			Batch.Read(BulkData.Interpolation1);
		}
		Batch.Read(BulkData.SimRootPointIndex);

		Batch.Issue(BulkDataRequest);
	}

	return BulkDataRequest.IsCompleted();
}

void FHairStrandsInterpolationResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	BulkDataRequest = FBulkDataBatchRequest();

	const bool bUseSingleGuide = !!(BulkData.Flags & FHairStrandsInterpolationBulkData::DataFlags_HasSingleGuideData);
	if (bUseSingleGuide)
	{
		InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsInterpolationFormat>(GraphBuilder, BulkData.Interpolation, BulkData.PointCount, InterpolationBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsInterpolation_InterpolationBuffer"), ResourceName), EHairResourceUsageType::Static);
	}
	else
	{
		InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsInterpolation0Format>(GraphBuilder, BulkData.Interpolation0, BulkData.PointCount, Interpolation0Buffer, ToHairResourceDebugName(TEXT("Hair.StrandsInterpolation_Interpolation0Buffer"), ResourceName), EHairResourceUsageType::Static);
		InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsInterpolation1Format>(GraphBuilder, BulkData.Interpolation1, BulkData.PointCount, Interpolation1Buffer, ToHairResourceDebugName(TEXT("Hair.StrandsInterpolation_Interpolation1Buffer"), ResourceName), EHairResourceUsageType::Static);
	}
	InternalCreateVertexBufferRDG_FromBulkData<FHairStrandsRootIndexFormat>(GraphBuilder, BulkData.SimRootPointIndex, BulkData.SimPointCount, SimRootPointIndexBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsInterpolation_SimRootPointIndex"), ResourceName), EHairResourceUsageType::Static);
}

void FHairStrandsInterpolationResource::InternalRelease()
{
	InterpolationBuffer.Release();
	Interpolation0Buffer.Release();
	Interpolation1Buffer.Release();
	SimRootPointIndexBuffer.Release();
	BulkDataRequest = FBulkDataBatchRequest();
}

/////////////////////////////////////////////////////////////////////////////////////////
void FHairCardsInterpolationDatas::SetNum(const uint32 NumPoints)
{
	PointsSimCurvesIndex.SetNum(NumPoints);
	PointsSimCurvesVertexIndex.SetNum(NumPoints);
	PointsSimCurvesVertexLerp.SetNum(NumPoints);
}

void FHairCardsInterpolationDatas::Reset()
{
	PointsSimCurvesIndex.Empty();
	PointsSimCurvesVertexIndex.Empty();
	PointsSimCurvesVertexLerp.Empty();
}

void FHairCardsInterpolationBulkData::Serialize(FArchive& Ar)
{
	Ar << Interpolation;
}

FHairCardsInterpolationResource::FHairCardsInterpolationResource(FHairCardsInterpolationBulkData& InBulkData, const FHairResourceName& InResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, InResourceName),
	InterpolationBuffer(), BulkData(InBulkData)
{
}

void FHairCardsInterpolationResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	InternalCreateVertexBufferRDG<FHairCardsInterpolationFormat>(GraphBuilder, BulkData.Interpolation, InterpolationBuffer, ToHairResourceDebugName(TEXT("Hair.CardsInterpolation_InterpolationBuffer"), ResourceName), EHairResourceUsageType::Static);
}

void FHairCardsInterpolationResource::InternalRelease()
{
	InterpolationBuffer.Release();
}

/////////////////////////////////////////////////////////////////////////////////////////

#if RHI_RAYTRACING
// RT geometry for strands is built as a 4 sided cylinder
//   each vertex of the curve becomes 4 points
//   each curve segment turns into 2*4=8 triangles (3 indices for each)
// there is some waste due to the degenerate triangles emitted from the end points of each curve
// total memory usage is: 4*float4 + 8*uint3 = 40 bytes per vertex
// The previous implementation used a "cross" layout without an index buffer
// which used 6*float4 = 48 bytes per vertex
// NOTE: the vertex buffer is a float4 because it is registered as a UAV for the compute shader to work
// TODO: use a plain float vertex buffer with 3x the entries instead to save memory? (float3 UAVs are not allowed)
bool GetSupportHairStrandsProceduralPrimitive(EShaderPlatform InShaderPlatform);
FHairStrandsRaytracingResource::FHairStrandsRaytracingResource(const FHairStrandsBulkData& InData, const FHairResourceName& ResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, ResourceName), bOwnBuffers(true)
{
	bProceduralPrimitive = GetSupportHairStrandsProceduralPrimitive(GMaxRHIShaderPlatform);
	if (bProceduralPrimitive)
	{
		// only allocate space for primitive AABBs
		VertexCount = InData.GetNumPoints() * 2 * STRANDS_PROCEDURAL_INTERSECTOR_MAX_SPLITS;
	}
	else
	{
		VertexCount = InData.GetNumPoints() * 4;
		IndexCount = InData.GetNumPoints() * 8 * 3;
	}
}

FHairStrandsRaytracingResource::FHairStrandsRaytracingResource(const FHairCardsBulkData& InData, const FHairResourceName& ResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, ResourceName),
	VertexCount(InData.GetNumVertices()), bOwnBuffers(false)
{}

FHairStrandsRaytracingResource::FHairStrandsRaytracingResource(const FHairMeshesBulkData& InData, const FHairResourceName& ResourceName) :
	FHairCommonResource(EHairStrandsAllocationType::Deferred, ResourceName),
	VertexCount(InData.GetNumVertices()), bOwnBuffers(false)
{}

void FHairStrandsRaytracingResource::InternalAllocate(FRDGBuilder& GraphBuilder)
{
	if (bOwnBuffers)
	{
		InternalCreateVertexBufferRDG<FHairStrandsRaytracingFormat>(GraphBuilder, VertexCount, PositionBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsRaytracing_PositionBuffer"), ResourceName), EHairResourceUsageType::Dynamic);
		InternalCreateStructuredBufferRDG<FHairStrandsIndexFormat>(GraphBuilder, IndexCount, IndexBuffer, ToHairResourceDebugName(TEXT("Hair.StrandsRaytracing_IndexBuffer"), ResourceName), EHairResourceUsageType::Dynamic);
	}
}

void FHairStrandsRaytracingResource::InternalRelease()
{
	PositionBuffer.Release();
	IndexBuffer.Release();
	RayTracingGeometry.ReleaseResource();
	bIsRTGeometryInitialized = false;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug data

static uint32 ToLinearCoord(const FIntVector& T, const FIntVector& Resolution)
{
	// Morton instead for better locality?
	return T.X + T.Y * Resolution.X + T.Z * Resolution.X * Resolution.Y;
}

static FIntVector ToCoord(const FVector3f& T, const FIntVector& Resolution, const FVector3f& MinBound, const float VoxelSize)
{
	const FVector3f C = (T - MinBound) / VoxelSize;
	return FIntVector(
		FMath::Clamp(FMath::FloorToInt(C.X), 0, Resolution.X - 1),
		FMath::Clamp(FMath::FloorToInt(C.Y), 0, Resolution.Y - 1),
		FMath::Clamp(FMath::FloorToInt(C.Z), 0, Resolution.Z - 1));
}

void CreateHairStrandsDebugDatas(
	const FHairStrandsDatas& InData,
	FHairStrandsDebugDatas& Out)
{
	const FVector3f BoundSize = FVector3f(InData.BoundingBox.Max) - FVector3f(InData.BoundingBox.Min);
	Out.VoxelDescription.VoxelSize = FMath::Clamp(GHairStrandsDebugVoxel_WorldSize, 0.1f, 10.f);
	Out.VoxelDescription.VoxelResolution = FIntVector(FMath::CeilToInt(BoundSize.X / Out.VoxelDescription.VoxelSize), FMath::CeilToInt(BoundSize.Y / Out.VoxelDescription.VoxelSize), FMath::CeilToInt(BoundSize.Z / Out.VoxelDescription.VoxelSize));
	Out.VoxelDescription.VoxelMinBound = FVector3f(InData.BoundingBox.Min);
	Out.VoxelDescription.VoxelMaxBound = FVector3f(Out.VoxelDescription.VoxelResolution) * Out.VoxelDescription.VoxelSize + FVector3f(InData.BoundingBox.Min);
	Out.VoxelOffsetAndCount.Init(FHairStrandsDebugDatas::FOffsetAndCount(), Out.VoxelDescription.VoxelResolution.X * Out.VoxelDescription.VoxelResolution.Y * Out.VoxelDescription.VoxelResolution.Z);
	Out.VoxelDescription.MaxSegmentPerVoxel = 0;

	uint32 AllocationCount = 0;
	TArray<TArray<FHairStrandsDebugDatas::FVoxel>> TempVoxelData;

	const uint32 MaxNumberOfSegmentPerVoxel = uint32(FMath::Clamp(GHairStrandsDebugVoxel_MaxSegmentPerVoxel, 16, 64000));

	// Fill in voxel (TODO: make it parallel)
	const uint32 CurveCount = InData.StrandsCurves.Num();
	for (uint32 CurveIndex = 0; CurveIndex < CurveCount; ++CurveIndex)
	{
		const uint32 PointOffset = InData.StrandsCurves.CurvesOffset[CurveIndex];
		const uint32 PointCount = InData.StrandsCurves.CurvesCount[CurveIndex];

		for (uint32 PointIndex = 0; PointIndex < PointCount - 1; ++PointIndex)
		{
			const uint32 Index0 = PointOffset + PointIndex;
			const uint32 Index1 = PointOffset + PointIndex + 1;
			const FVector3f& P0 = InData.StrandsPoints.PointsPosition[Index0];
			const FVector3f& P1 = InData.StrandsPoints.PointsPosition[Index1];
			const FVector3f Segment = P1 - P0;

			const float Length = Segment.Size();
			const uint32 StepCount = FMath::CeilToInt(Length / (0.25f * Out.VoxelDescription.VoxelSize));
			uint32 PrevLinearCoord = ~0;
			for (uint32 StepIt = 0; StepIt < StepCount + 1; ++StepIt)
			{
				const FVector3f P = P0 + Segment * StepIt / float(StepCount);
				const FIntVector Coord = ToCoord(P, Out.VoxelDescription.VoxelResolution, Out.VoxelDescription.VoxelMinBound, Out.VoxelDescription.VoxelSize);
				const uint32 LinearCoord = ToLinearCoord(Coord, Out.VoxelDescription.VoxelResolution);
				if (LinearCoord != PrevLinearCoord)
				{
					if (Out.VoxelOffsetAndCount[LinearCoord].Count == 0)
					{
						Out.VoxelOffsetAndCount[LinearCoord].Offset = TempVoxelData.Num();
						TempVoxelData.Add(TArray<FHairStrandsDebugDatas::FVoxel>());
					}

					if (Out.VoxelOffsetAndCount[LinearCoord].Count+1 < MaxNumberOfSegmentPerVoxel)
					{
						const uint32 Offset = Out.VoxelOffsetAndCount[LinearCoord].Offset;
						Out.VoxelOffsetAndCount[LinearCoord].Count += 1;
						TempVoxelData[Offset].Add({Index0, Index1});
					}

					Out.VoxelDescription.MaxSegmentPerVoxel = FMath::Max(Out.VoxelDescription.MaxSegmentPerVoxel, Out.VoxelOffsetAndCount[LinearCoord].Count);

					PrevLinearCoord = LinearCoord;

					++AllocationCount;
				}
			}
		}
	}

	check(Out.VoxelDescription.MaxSegmentPerVoxel < MaxNumberOfSegmentPerVoxel);
	Out.VoxelData.Reserve(AllocationCount);

	for (int32 Index = 0, Count = Out.VoxelOffsetAndCount.Num(); Index < Count; ++Index)
	{
		if (Out.VoxelOffsetAndCount[Index].Count > 0)
		{
			const uint32 ArrayIndex = Out.VoxelOffsetAndCount[Index].Offset;
			const uint32 NewOffset = Out.VoxelData.Num();
			Out.VoxelOffsetAndCount[Index].Offset = NewOffset;

			for (FHairStrandsDebugDatas::FVoxel& Voxel : TempVoxelData[ArrayIndex])
			{
				Voxel.Index1 = NewOffset;
				Out.VoxelData.Add(Voxel);
			}
		}
		else
		{
			Out.VoxelOffsetAndCount[Index].Offset = 0;
		}

		// Sanity check
		//check(Out.VoxelOffsetAndCount[Index].Offset + Out.VoxelOffsetAndCount[Index].Count == Out.VoxelData.Num());
	}

	check(Out.VoxelData.Num()>0);
}

void CreateHairStrandsDebugResources(FRDGBuilder& GraphBuilder, const FHairStrandsDebugDatas* In, FHairStrandsDebugDatas::FResources* Out)
{
	check(In);
	check(Out);

	Out->VoxelDescription = In->VoxelDescription;

	FRDGBufferRef VoxelOffsetAndCount = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("HairStrandsDebug_VoxelOffsetAndCount"),
		sizeof(FHairStrandsDebugDatas::FOffsetAndCount),
		In->VoxelOffsetAndCount.Num(),
		In->VoxelOffsetAndCount.GetData(),
		sizeof(FHairStrandsDebugDatas::FOffsetAndCount) * In->VoxelOffsetAndCount.Num());

	FRDGBufferRef VoxelData = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("HairStrandsDebug_VoxelData"),
		sizeof(FHairStrandsDebugDatas::FVoxel),
		In->VoxelData.Num(),
		In->VoxelData.GetData(),
		sizeof(FHairStrandsDebugDatas::FVoxel) * In->VoxelData.Num());

	Out->VoxelOffsetAndCount = ConvertToExternalAccessBuffer(GraphBuilder, VoxelOffsetAndCount);
	Out->VoxelData = ConvertToExternalAccessBuffer(GraphBuilder, VoxelData);
}
