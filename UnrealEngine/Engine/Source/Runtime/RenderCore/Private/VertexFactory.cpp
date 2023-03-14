// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VertexFactory.cpp: Vertex factory implementation
=============================================================================*/

#include "VertexFactory.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/DebugSerializationFlags.h"
#include "PipelineStateCache.h"
#include "ShaderCompilerCore.h"
#include "RenderUtils.h"

IMPLEMENT_TYPE_LAYOUT(FVertexFactoryShaderParameters);

uint32 FVertexFactoryType::NumVertexFactories = 0;
bool FVertexFactoryType::bInitializedSerializationHistory = false;

static TLinkedList<FVertexFactoryType*>* GVFTypeList = nullptr;

static TArray<FVertexFactoryType*>& GetSortedMaterialVertexFactoryTypes()
{
	static TArray<FVertexFactoryType*> Types;
	return Types;
}

static TMap<FHashedName, FVertexFactoryType*>& GetVFTypeMap()
{
	static TMap<FHashedName, FVertexFactoryType*> VTTypeMap;
	return VTTypeMap;
}

/**
 * @return The global shader factory list.
 */
TLinkedList<FVertexFactoryType*>*& FVertexFactoryType::GetTypeList()
{
	return GVFTypeList;
}

const TArray<FVertexFactoryType*>& FVertexFactoryType::GetSortedMaterialTypes()
{
	return GetSortedMaterialVertexFactoryTypes();
}

/**
 * Finds a FVertexFactoryType by name.
 */
FVertexFactoryType* FVertexFactoryType::GetVFByName(const FHashedName& VFName)
{
	FVertexFactoryType** Type = GetVFTypeMap().Find(VFName);
	return Type ? *Type : nullptr;
}

void FVertexFactoryType::Initialize(const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Cache serialization history for each VF type
		// This history is used to detect when shader serialization changes without a corresponding .usf change
		for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
		{
			FVertexFactoryType* Type = *It;
			GenerateReferencedUniformBuffers(Type->ShaderFilename, Type->Name, ShaderFileToUniformBufferVariables, Type->ReferencedUniformBufferStructsCache);
		}
	}

	bInitializedSerializationHistory = true;
}

void FVertexFactoryType::Uninitialize()
{
	bInitializedSerializationHistory = false;
}

FVertexFactoryType::FVertexFactoryType(
	const TCHAR* InName,
	const TCHAR* InShaderFilename,
	EVertexFactoryFlags InFlags,
	ConstructParametersType InConstructParameters,
	GetParameterTypeLayoutType InGetParameterTypeLayout,
	GetParameterTypeElementShaderBindingsType InGetParameterTypeElementShaderBindings,
	GetPSOPrecacheVertexFetchElementsType InGetPSOPrecacheVertexFetchElements,
	ShouldCacheType InShouldCache,
	ModifyCompilationEnvironmentType InModifyCompilationEnvironment,
	ValidateCompiledResultType InValidateCompiledResult
	):
	Name(InName),
	ShaderFilename(InShaderFilename),
	TypeName(InName),
	HashedName(TypeName),
	Flags(InFlags),
	ConstructParameters(InConstructParameters),
	GetParameterTypeLayout(InGetParameterTypeLayout),
	GetParameterTypeElementShaderBindings(InGetParameterTypeElementShaderBindings),
	GetPSOPrecacheVertexFetchElements(InGetPSOPrecacheVertexFetchElements),
	ShouldCacheRef(InShouldCache),
	ModifyCompilationEnvironmentRef(InModifyCompilationEnvironment),
	ValidateCompiledResultRef(InValidateCompiledResult),
	GlobalListLink(this)
{
	// Make sure the format of the source file path is right.
	check(CheckVirtualShaderFilePath(InShaderFilename));

	checkf(FPaths::GetExtension(InShaderFilename) == TEXT("ush"),
		TEXT("Incorrect virtual shader path extension for vertex factory shader header '%s': Only .ush files should be included."),
		InShaderFilename);

	CachedUniformBufferPlatform = SP_NumPlatforms;

	// This will trigger if an IMPLEMENT_VERTEX_FACTORY_TYPE was in a module not loaded before InitializeShaderTypes
	// Vertex factory types need to be implemented in modules that are loaded before that
	checkf(!bInitializedSerializationHistory, TEXT("VF type was loaded after engine init, use ELoadingPhase::PostConfigInit on your module to cause it to load earlier."));

	// Add this vertex factory type to the global list.
	GlobalListLink.LinkHead(GetTypeList()); 
	GetVFTypeMap().Add(HashedName, this);

	if (IsUsedWithMaterials())
	{
		TArray<FVertexFactoryType*>& SortedTypes = GetSortedMaterialVertexFactoryTypes();
		const int32 SortedIndex = Algo::LowerBoundBy(SortedTypes, HashedName, [](const FVertexFactoryType* InType) { return InType->GetHashedName(); });
		SortedTypes.Insert(this, SortedIndex);
	}

	++NumVertexFactories;
}

FVertexFactoryType::~FVertexFactoryType()
{
	GlobalListLink.Unlink();
	verify(GetVFTypeMap().Remove(HashedName) == 1);

	if (IsUsedWithMaterials())
	{
		TArray<FVertexFactoryType*>& SortedTypes = GetSortedMaterialVertexFactoryTypes();
		const int32 SortedIndex = Algo::BinarySearchBy(SortedTypes, HashedName, [](const FVertexFactoryType* InType) { return InType->GetHashedName(); });
		check(SortedIndex != INDEX_NONE);
		SortedTypes.RemoveAt(SortedIndex);
	}

	check(NumVertexFactories > 0u);
	--NumVertexFactories;
}

/** Calculates a Hash based on this vertex factory type's source code and includes */
const FSHAHash& FVertexFactoryType::GetSourceHash(EShaderPlatform ShaderPlatform) const
{
	return GetShaderFileHash(GetShaderFilename(), ShaderPlatform);
}

FArchive& operator<<(FArchive& Ar,FVertexFactoryType*& TypeRef)
{
	if(Ar.IsSaving())
	{
		FName TypeName = TypeRef ? FName(TypeRef->GetName()) : NAME_None;
		Ar << TypeName;
	}
	else if(Ar.IsLoading())
	{
		FName TypeName = NAME_None;
		Ar << TypeName;
		TypeRef = FindVertexFactoryType(TypeName);
	}
	return Ar;
}

FVertexFactoryType* FindVertexFactoryType(const FHashedName& TypeName)
{
	return FVertexFactoryType::GetVFByName(TypeName);
}

void FVertexFactory::GetStreams(ERHIFeatureLevel::Type InFeatureLevel, EVertexInputStreamType VertexStreamType, FVertexInputStreamArray& OutVertexStreams) const
{
	check(IsInitialized());
	if (VertexStreamType == EVertexInputStreamType::Default)
	{

		bool bSupportsVertexFetch = SupportsManualVertexFetch(InFeatureLevel);

		for (int32 StreamIndex = 0;StreamIndex < Streams.Num();StreamIndex++)
		{
			const FVertexStream& Stream = Streams[StreamIndex];

			if (!(EnumHasAnyFlags(EVertexStreamUsage::ManualFetch, Stream.VertexStreamUsage) && bSupportsVertexFetch))
			{
				if (!Stream.VertexBuffer)
				{
					OutVertexStreams.Add(FVertexInputStream(StreamIndex, 0, nullptr));
				}
				else
				{
					if (EnumHasAnyFlags(EVertexStreamUsage::Overridden, Stream.VertexStreamUsage) && !Stream.VertexBuffer->IsInitialized())
					{
						OutVertexStreams.Add(FVertexInputStream(StreamIndex, 0, nullptr));
					}
					else
					{
						checkf(Stream.VertexBuffer->IsInitialized(), TEXT("Vertex buffer was not initialized! Stream %u, Stride %u, Name %s"), StreamIndex, Stream.Stride, *Stream.VertexBuffer->GetFriendlyName());
						OutVertexStreams.Add(FVertexInputStream(StreamIndex, Stream.Offset, Stream.VertexBuffer->VertexBufferRHI));
					}
				}
			}
		}
	}
	else if (VertexStreamType == EVertexInputStreamType::PositionOnly)
	{
		// Set the predefined vertex streams.
		for (int32 StreamIndex = 0; StreamIndex < PositionStream.Num(); StreamIndex++)
		{
			const FVertexStream& Stream = PositionStream[StreamIndex];
			check(Stream.VertexBuffer->IsInitialized());
			OutVertexStreams.Add(FVertexInputStream(StreamIndex, Stream.Offset, Stream.VertexBuffer->VertexBufferRHI));
		}
	}
	else if (VertexStreamType == EVertexInputStreamType::PositionAndNormalOnly)
	{
		// Set the predefined vertex streams.
		for (int32 StreamIndex = 0; StreamIndex < PositionAndNormalStream.Num(); StreamIndex++)
		{
			const FVertexStream& Stream = PositionAndNormalStream[StreamIndex];
			check(Stream.VertexBuffer->IsInitialized());
			OutVertexStreams.Add(FVertexInputStream(StreamIndex, Stream.Offset, Stream.VertexBuffer->VertexBufferRHI));
		}
	}
	else
	{
		// NOT_IMPLEMENTED
	}
}

void FVertexFactory::OffsetInstanceStreams(uint32 InstanceOffset, EVertexInputStreamType VertexStreamType, FVertexInputStreamArray& VertexStreams) const
{
	const TArrayView<const FVertexStream>& StreamArray = 
		  VertexStreamType == EVertexInputStreamType::PositionOnly ?			MakeArrayView(PositionStream) : 
		( VertexStreamType == EVertexInputStreamType::PositionAndNormalOnly ?	MakeArrayView(PositionAndNormalStream) : 
		/*VertexStreamType == EVertexInputStreamType::Default*/					MakeArrayView(Streams));

	for (int32 StreamIndex = 0; StreamIndex < StreamArray.Num(); StreamIndex++)
	{
		const FVertexStream& Stream = StreamArray[StreamIndex];

		if (EnumHasAnyFlags(EVertexStreamUsage::Instancing, Stream.VertexStreamUsage))
		{
			for (int32 BindingIndex = 0; BindingIndex < VertexStreams.Num(); BindingIndex++)
			{
				if (VertexStreams[BindingIndex].StreamIndex == StreamIndex)
				{
					VertexStreams[BindingIndex].Offset = Stream.Offset + Stream.Stride * InstanceOffset;
				}
			}
		}
	}
}

void FVertexFactory::ReleaseRHI()
{
	Declaration.SafeRelease();
	PositionDeclaration.SafeRelease();
	PositionAndNormalDeclaration.SafeRelease();
	Streams.Empty();
	PositionStream.Empty();
	PositionAndNormalStream.Empty();
}

bool FVertexFactory::AddPrimitiveIdStreamElement(EVertexInputStreamType InputStreamType, FVertexDeclarationElementList& Elements, uint8 AttributeIndex, uint8 AttributeIndex_Mobile)
{
	if (GetType()->SupportsPrimitiveIdStream() && UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel))
	{
		// In the editor add streams for both configurations - mobile and desktop
		if (AttributeIndex_Mobile != 0xff && (GIsEditor || GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1))
		{
			constexpr uint32 BufferStride = FPrimitiveIdDummyBufferMobile::BufferStride;
			// instance transform stream
			Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummyMobile, 0, 16*0, BufferStride, VET_Float4, EVertexStreamUsage::Instancing), AttributeIndex_Mobile+0, InputStreamType));
			Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummyMobile, 0, 16*1, BufferStride, VET_Float4, EVertexStreamUsage::Instancing), AttributeIndex_Mobile+1, InputStreamType));
			Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummyMobile, 0, 16*2, BufferStride, VET_Float4, EVertexStreamUsage::Instancing), AttributeIndex_Mobile+2, InputStreamType));
			Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummyMobile, 0, 16*3, BufferStride, VET_Float4, EVertexStreamUsage::Instancing), AttributeIndex_Mobile+3, InputStreamType));
			Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummyMobile, 0, 16*4, BufferStride, VET_Float4, EVertexStreamUsage::Instancing), AttributeIndex_Mobile+4, InputStreamType));
			SetPrimitiveIdStreamIndex(ERHIFeatureLevel::ES3_1, InputStreamType, Elements.Last().StreamIndex);
		}
		
		if (GIsEditor || GMaxRHIFeatureLevel > ERHIFeatureLevel::ES3_1)
		{
			// When the VF is used for rendering in normal mesh passes, this vertex buffer and offset will be overridden
			Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummy, 0, 0, PrimitiveIdStreamStride, VET_UInt, EVertexStreamUsage::Instancing), AttributeIndex, InputStreamType));
			SetPrimitiveIdStreamIndex(GMaxRHIFeatureLevel, InputStreamType, Elements.Last().StreamIndex);
		}
		return true;
	}

	return false;
}

FVertexElement FVertexFactory::AccessStreamComponent(const FVertexStreamComponent& Component, uint8 AttributeIndex)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.Offset = Component.StreamOffset;
	VertexStream.VertexStreamUsage = Component.VertexStreamUsage;

	return FVertexElement((uint8)Streams.AddUnique(VertexStream),Component.Offset,Component.Type,AttributeIndex,VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
}

FVertexElement FVertexFactory::AccessStreamComponent(const FVertexStreamComponent& Component,uint8 AttributeIndex, EVertexInputStreamType InputStreamType)
{
	FVertexStream VertexStream;
	VertexStream.VertexBuffer = Component.VertexBuffer;
	VertexStream.Stride = Component.Stride;
	VertexStream.Offset = Component.StreamOffset;
	VertexStream.VertexStreamUsage = Component.VertexStreamUsage;

	if (InputStreamType == EVertexInputStreamType::PositionOnly)
		return FVertexElement((uint8)PositionStream.AddUnique(VertexStream), Component.Offset, Component.Type, AttributeIndex, VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
	else if (InputStreamType == EVertexInputStreamType::PositionAndNormalOnly)
		return FVertexElement((uint8)PositionAndNormalStream.AddUnique(VertexStream), Component.Offset, Component.Type, AttributeIndex, VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
	else /* (InputStreamType == EVertexInputStreamType::Default) */
		return FVertexElement((uint8)Streams.AddUnique(VertexStream), Component.Offset, Component.Type, AttributeIndex, VertexStream.Stride, EnumHasAnyFlags(EVertexStreamUsage::Instancing, VertexStream.VertexStreamUsage));
}

void FVertexFactory::InitDeclaration(const FVertexDeclarationElementList& Elements, EVertexInputStreamType StreamType)
{
	
	if (StreamType == EVertexInputStreamType::PositionOnly)
	{
		PositionDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	else if (StreamType == EVertexInputStreamType::PositionAndNormalOnly)
	{
		PositionAndNormalDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	else // (StreamType == EVertexInputStreamType::Default)
	{
		// Create the vertex declaration for rendering the factory normally.
		Declaration = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
}

void FPrimitiveIdDummyBuffer::InitRHI() 
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo(TEXT("FPrimitiveIdDummyBuffer"));

	VertexBufferRHI = RHICreateBuffer(sizeof(uint32), BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
	uint32* Vertices = (uint32*)RHILockBuffer(VertexBufferRHI, 0, sizeof(uint32), RLM_WriteOnly);
	Vertices[0] = 0;
	RHIUnlockBuffer(VertexBufferRHI);
	VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(uint32), PF_R32_UINT);
}

TGlobalResource<FPrimitiveIdDummyBuffer> GPrimitiveIdDummy;

void FPrimitiveIdDummyBufferMobile::InitRHI() 
{
	// create a static vertex buffer
	FRHIResourceCreateInfo CreateInfo(TEXT("FPrimitiveIdDummyBufferMobile"));

	VertexBufferRHI = RHICreateBuffer(BufferStride, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
	FVector4f* Vertices = (FVector4f*)RHILockBuffer(VertexBufferRHI, 0, BufferStride, RLM_WriteOnly);
	Vertices[0] = FVector4f(0, 0, 0, 0);
	Vertices[1] = FVector4f(1, 0, 0, 0);
	Vertices[2] = FVector4f(0, 1, 0, 0);
	Vertices[3] = FVector4f(0, 0, 1, 0);
	Vertices[4] = FVector4f(0, 0, 0, 0);
	RHIUnlockBuffer(VertexBufferRHI);
	VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(FVector4f), PF_A32B32G32R32F);
}

TGlobalResource<FPrimitiveIdDummyBufferMobile> GPrimitiveIdDummyMobile;
