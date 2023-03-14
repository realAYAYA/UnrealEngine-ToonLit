// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnifiedBuffer.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "Shader.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"
#include "RenderUtils.h"
#include "RenderGraphUtils.h"

enum class EByteBufferResourceType
{
	Float4_Buffer,
	Float4_StructuredBuffer,
	Uint_Buffer,
	Uint4Aligned_Buffer,
	Float4_Texture,
	Count
};

class FByteBufferShader : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FByteBufferShader, NonVirtual);

	FByteBufferShader() {}
	FByteBufferShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	class ResourceTypeDim : SHADER_PERMUTATION_INT("RESOURCE_TYPE", (int)EByteBufferResourceType::Count);

	using FPermutationDomain = TShaderPermutationDomain<ResourceTypeDim>;

	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		FPermutationDomain PermutationVector( Parameters.PermutationId );

		EByteBufferResourceType ResourceType = (EByteBufferResourceType)PermutationVector.Get<ResourceTypeDim>();

		if (ResourceType == EByteBufferResourceType::Uint_Buffer || ResourceType == EByteBufferResourceType::Uint4Aligned_Buffer)
		{
			return FDataDrivenShaderPlatformInfo::GetSupportsByteBufferComputeShaders(Parameters.Platform);
		}
		else
		{
			return true;
		}
	}

	BEGIN_SHADER_PARAMETER_STRUCT( FParameters, )
		SHADER_PARAMETER(uint32, Value)
		SHADER_PARAMETER(uint32, Size)
		SHADER_PARAMETER(uint32, SrcOffset)
		SHADER_PARAMETER(uint32, DstOffset)
		SHADER_PARAMETER(uint32, Float4sPerLine)
		SHADER_PARAMETER_UAV(RWBuffer<float4>, DstBuffer)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float4>, DstStructuredBuffer)
		SHADER_PARAMETER_UAV(RWByteAddressBuffer, DstByteAddressBuffer)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, DstTexture)
	END_SHADER_PARAMETER_STRUCT()
};

class FMemsetBufferCS : public FByteBufferShader
{
	DECLARE_GLOBAL_SHADER( FMemsetBufferCS );
	SHADER_USE_PARAMETER_STRUCT( FMemsetBufferCS, FByteBufferShader );
};
IMPLEMENT_GLOBAL_SHADER( FMemsetBufferCS, "/Engine/Private/ByteBuffer.usf", "MemsetBufferCS", SF_Compute );

class FMemcpyCS : public FByteBufferShader
{
	DECLARE_GLOBAL_SHADER( FMemcpyCS );
	SHADER_USE_PARAMETER_STRUCT( FMemcpyCS, FByteBufferShader );

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FByteBufferShader::FParameters, Common)
		SHADER_PARAMETER_SRV(Buffer<float4>, SrcBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, SrcStructuredBuffer)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, SrcByteAddressBuffer)
		SHADER_PARAMETER_SRV(Texture2D<float4>, SrcTexture)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER( FMemcpyCS, "/Engine/Private/ByteBuffer.usf", "MemcpyCS", SF_Compute );

class FScatterCopyCS : public FByteBufferShader
{
	DECLARE_GLOBAL_SHADER( FScatterCopyCS );
	SHADER_USE_PARAMETER_STRUCT( FScatterCopyCS, FByteBufferShader );

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FByteBufferShader::FParameters, Common)
		SHADER_PARAMETER(uint32, NumScatters)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, UploadByteAddressBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, UploadStructuredBuffer)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, ScatterByteAddressBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer<uint>, ScatterStructuredBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER( FScatterCopyCS, "/Engine/Private/ByteBuffer.usf", "ScatterCopyCS", SF_Compute );

enum class EResourceType
{
	BUFFER,
	STRUCTURED_BUFFER,
	BYTEBUFFER,
	TEXTURE
};

template<typename ResourceType>
struct ResourceTypeTraits;

template<>
struct ResourceTypeTraits<FRWBuffer>
{
	static const EResourceType Type = EResourceType::BUFFER;
};

template<>
struct ResourceTypeTraits<FRWBufferStructured>
{
	static const EResourceType Type = EResourceType::STRUCTURED_BUFFER;
};

template<>
struct ResourceTypeTraits<FTextureRWBuffer>
{
	static const EResourceType Type = EResourceType::TEXTURE;
};

template<>
struct ResourceTypeTraits<FRWByteAddressBuffer>
{
	static const EResourceType Type = EResourceType::BYTEBUFFER;
};

static uint32 CalculateFloat4sPerLine()
{
	uint16 PrimitivesPerTextureLine = (uint16)FMath::Min((int32)MAX_uint16, (int32)GMaxTextureDimensions) / FScatterUploadBuffer::PrimitiveDataStrideInFloat4s;
	return PrimitivesPerTextureLine * FScatterUploadBuffer::PrimitiveDataStrideInFloat4s;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FRDGByteBufferShader : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FRDGByteBufferShader, NonVirtual);

	FRDGByteBufferShader() {}
	FRDGByteBufferShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

	class ResourceTypeDim : SHADER_PERMUTATION_INT("RESOURCE_TYPE", (int)EByteBufferResourceType::Count);
	using FPermutationDomain = TShaderPermutationDomain<ResourceTypeDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FByteBufferShader::ShouldCompilePermutation(Parameters);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Value)
		SHADER_PARAMETER(uint32, Size)
		SHADER_PARAMETER(uint32, SrcOffset)
		SHADER_PARAMETER(uint32, DstOffset)
		SHADER_PARAMETER(uint32, Float4sPerLine)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, DstBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, DstStructuredBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, DstByteAddressBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DstTexture)
	END_SHADER_PARAMETER_STRUCT()
};

class FRDGMemsetBufferCS : public FRDGByteBufferShader
{
	DECLARE_GLOBAL_SHADER(FRDGMemsetBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FRDGMemsetBufferCS, FRDGByteBufferShader);
};
IMPLEMENT_GLOBAL_SHADER(FRDGMemsetBufferCS, "/Engine/Private/ByteBuffer.usf", "MemsetBufferCS", SF_Compute);

class FRDGMemcpyCS : public FRDGByteBufferShader
{
	DECLARE_GLOBAL_SHADER(FRDGMemcpyCS);
	SHADER_USE_PARAMETER_STRUCT(FRDGMemcpyCS, FRDGByteBufferShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FRDGByteBufferShader::FParameters, Common)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SrcBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, SrcStructuredBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, SrcByteAddressBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, SrcTexture)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FRDGMemcpyCS, "/Engine/Private/ByteBuffer.usf", "MemcpyCS", SF_Compute);

class FRDGScatterCopyCS : public FRDGByteBufferShader
{
	DECLARE_GLOBAL_SHADER(FRDGScatterCopyCS);
	SHADER_USE_PARAMETER_STRUCT(FRDGScatterCopyCS, FRDGByteBufferShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FRDGByteBufferShader::FParameters, Common)
		SHADER_PARAMETER(uint32, NumScatters)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, UploadByteAddressBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, UploadStructuredBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ScatterByteAddressBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ScatterStructuredBuffer)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FRDGScatterCopyCS, "/Engine/Private/ByteBuffer.usf", "ScatterCopyCS", SF_Compute);

EResourceType GetBufferType(FRDGBuffer* Buffer)
{
	const FRDGBufferDesc& Desc = Buffer->Desc;

	if (EnumHasAnyFlags(Desc.Usage, EBufferUsageFlags::ByteAddressBuffer))
	{
		return EResourceType::BYTEBUFFER;
	}
	else if (EnumHasAnyFlags(Desc.Usage, EBufferUsageFlags::StructuredBuffer))
	{
		return EResourceType::STRUCTURED_BUFFER;
	}
	else
	{
		return EResourceType::BUFFER;
	}
}

EResourceType GetResourceType(FRDGViewableResource* Resource)
{
	check(Resource);
	switch (Resource->Type)
	{
	case ERDGViewableResourceType::Texture:
		return EResourceType::TEXTURE;
	case ERDGViewableResourceType::Buffer:
		return GetBufferType(GetAsBuffer(Resource));
	}
	checkNoEntry();
	return EResourceType::BUFFER;
}

void MemsetResource(FRDGBuilder& GraphBuilder, FRDGBuffer* DstResource, const FMemsetResourceParams& Params)
{
	MemsetResource(GraphBuilder, GraphBuilder.CreateUAV(DstResource, ERDGUnorderedAccessViewFlags::SkipBarrier), Params);
}

void MemcpyResource(FRDGBuilder& GraphBuilder, FRDGBuffer* DstResource, FRDGBuffer* SrcResource, const FMemcpyResourceParams& Params)
{
	MemcpyResource(GraphBuilder, GraphBuilder.CreateUAV(DstResource, ERDGUnorderedAccessViewFlags::SkipBarrier), GraphBuilder.CreateSRV(SrcResource), Params);
}

void MemsetResource(FRDGBuilder& GraphBuilder, FRDGUnorderedAccessView* UAV, const FMemsetResourceParams& Params)
{
	check(UAV);
	FRDGViewableResource* Resource = UAV->GetParent();

	EByteBufferResourceType ResourceTypeEnum = EByteBufferResourceType::Count;

	auto* PassParameters = GraphBuilder.AllocParameters<FRDGMemsetBufferCS::FParameters>();
	PassParameters->Value = Params.Value;
	PassParameters->Size = Params.Count;
	PassParameters->DstOffset = Params.DstOffset;

	// each thread will set 4 floats / uints
	uint32 Divisor = 1;

	switch (GetResourceType(Resource))
	{
	case EResourceType::BYTEBUFFER:
		ResourceTypeEnum = EByteBufferResourceType::Uint_Buffer;
		PassParameters->DstByteAddressBuffer = GetAs<FRDGBufferUAV>(UAV);
		Divisor = 4;
		break;
	case EResourceType::BUFFER:
		ResourceTypeEnum = EByteBufferResourceType::Float4_Buffer;
		PassParameters->DstBuffer = GetAs<FRDGBufferUAV>(UAV);
		break;
	case EResourceType::STRUCTURED_BUFFER:
		ResourceTypeEnum = EByteBufferResourceType::Float4_StructuredBuffer;
		PassParameters->DstStructuredBuffer = GetAs<FRDGBufferUAV>(UAV);
		break;
	case EResourceType::TEXTURE:
		ResourceTypeEnum = EByteBufferResourceType::Float4_Texture;
		PassParameters->DstTexture = GetAs<FRDGTextureUAV>(UAV);
		PassParameters->Float4sPerLine = CalculateFloat4sPerLine();
		break;
	default:
		checkNoEntry();
	}

	FRDGMemsetBufferCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRDGMemsetBufferCS::ResourceTypeDim >((int)ResourceTypeEnum);
	TShaderMapRef<FRDGMemsetBufferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("MemsetResource (%s)", Resource->Name),
		ComputeShader,
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(Params.Count / Divisor, 64u), 1, 1));
}

void MemcpyResource(FRDGBuilder& GraphBuilder, FRDGUnorderedAccessView* UAV, FRDGShaderResourceView* SRV, const FMemcpyResourceParams& Params)
{
	check(UAV && SRV);
	FRDGViewableResource* DstResource = UAV->GetParent();
	FRDGViewableResource* SrcResource = SRV->GetParent();

	const EResourceType ResourceType = GetResourceType(DstResource);
	checkf(ResourceType == GetResourceType(SrcResource), TEXT("Unable to MemcpyResource because the source and destination view types don't match."));

	RDG_EVENT_SCOPE(GraphBuilder, "Memcpy %s -> %s", DstResource->Name, SrcResource->Name);

	// each thread will copy 4 floats / uints
	const uint32 Divisor = ResourceType == EResourceType::BYTEBUFFER ? 4 : 1;

	uint32 NumElementsProcessed = 0;

	while (NumElementsProcessed < Params.Count)
	{
		const uint32 NumWaves = FMath::Max(FMath::Min<uint32>(GRHIMaxDispatchThreadGroupsPerDimension.X, FMath::DivideAndRoundUp(Params.Count / Divisor, 64u)), 1u);
		const uint32 NumElementsPerDispatch = FMath::Min(FMath::Max(NumWaves, 1u) * Divisor * 64, Params.Count - NumElementsProcessed);

		EByteBufferResourceType ResourceTypeEnum = EByteBufferResourceType::Count;

		auto* PassParameters = GraphBuilder.AllocParameters<FRDGMemcpyCS::FParameters>();
		PassParameters->Common.Size = NumElementsPerDispatch;
		PassParameters->Common.SrcOffset = (Params.SrcOffset + NumElementsProcessed);
		PassParameters->Common.DstOffset = (Params.DstOffset + NumElementsProcessed);

		switch (ResourceType)
		{
		case EResourceType::BYTEBUFFER:
			ResourceTypeEnum = EByteBufferResourceType::Uint_Buffer;
			PassParameters->SrcByteAddressBuffer           = GetAs<FRDGBufferSRV>(SRV);
			PassParameters->Common.DstByteAddressBuffer    = GetAs<FRDGBufferUAV>(UAV);
			break;
		case EResourceType::STRUCTURED_BUFFER:
			ResourceTypeEnum = EByteBufferResourceType::Float4_StructuredBuffer;
			PassParameters->SrcStructuredBuffer            = GetAs<FRDGBufferSRV>(SRV);
			PassParameters->Common.DstStructuredBuffer     = GetAs<FRDGBufferUAV>(UAV);
			break;
		case EResourceType::BUFFER:
			ResourceTypeEnum = EByteBufferResourceType::Float4_Buffer;
			PassParameters->SrcBuffer                      = GetAs<FRDGBufferSRV>(SRV);
			PassParameters->Common.DstBuffer               = GetAs<FRDGBufferUAV>(UAV);
			break;
		case EResourceType::TEXTURE:
			ResourceTypeEnum = EByteBufferResourceType::Float4_Texture;
			PassParameters->SrcTexture                     = GetAs<FRDGTextureSRV>(SRV);
			PassParameters->Common.DstTexture              = GetAs<FRDGTextureUAV>(UAV);
			PassParameters->Common.Float4sPerLine          = CalculateFloat4sPerLine();
			break;
		default:
			checkNoEntry();
		}

		FRDGMemcpyCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRDGMemcpyCS::ResourceTypeDim >((int)ResourceTypeEnum);
		TShaderMapRef<FRDGMemcpyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Offset[%d] Count[%d]", NumElementsProcessed, NumElementsPerDispatch),
			ComputeShader,
			PassParameters,
			FIntVector(NumWaves, 1, 1));

		NumElementsProcessed += NumElementsPerDispatch;
	}
}

FRDGBuffer* ResizeBufferIfNeeded(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& ExternalBuffer, const FRDGBufferDesc& BufferDesc, const TCHAR* Name)
{
	FRDGBuffer* InternalBufferNew = nullptr;

	if (!ExternalBuffer)
	{
		InternalBufferNew = GraphBuilder.CreateBuffer(BufferDesc, Name);
		ExternalBuffer = GraphBuilder.ConvertToExternalBuffer(InternalBufferNew);
		return InternalBufferNew;
	}

	FRDGBuffer* InternalBufferOld = GraphBuilder.RegisterExternalBuffer(ExternalBuffer);

	const uint32 BufferSize = BufferDesc.GetSize();
	const uint32 BufferSizeOld = InternalBufferOld->GetSize();

	if (BufferSize != BufferSizeOld)
	{
		InternalBufferNew = GraphBuilder.CreateBuffer(BufferDesc, Name);
		ExternalBuffer = GraphBuilder.ConvertToExternalBuffer(InternalBufferNew);

		// Copy data to new buffer
		FMemcpyResourceParams Params;
		Params.Count = FMath::Min(BufferSize, BufferSizeOld) / BufferDesc.BytesPerElement;
		Params.SrcOffset = 0;
		Params.DstOffset = 0;
		MemcpyResource(GraphBuilder, InternalBufferNew, InternalBufferOld, Params);

		return InternalBufferNew;
	}

	return InternalBufferOld;
}

FRDGBuffer* ResizeBufferIfNeeded(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& ExternalBuffer, EPixelFormat Format, uint32 NumElements, const TCHAR* Name)
{
	const uint32 BytesPerElement = GPixelFormats[Format].BlockBytes;

	return ResizeBufferIfNeeded(GraphBuilder, ExternalBuffer, FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NumElements), Name);
}

FRDGBuffer* ResizeStructuredBufferIfNeeded(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& ExternalBuffer, uint32 NumBytes, const TCHAR* Name)
{
	const uint32 BytesPerElement = 16;

	check((NumBytes & (BytesPerElement - 1)) == 0);

	const uint32 NumElements = NumBytes / BytesPerElement;

	return ResizeBufferIfNeeded(GraphBuilder, ExternalBuffer, FRDGBufferDesc::CreateStructuredDesc(BytesPerElement, NumElements), Name);
}

FRDGBuffer* ResizeStructuredBufferSOAIfNeeded(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& ExternalBuffer, const FResizeResourceSOAParams& Params, const TCHAR* Name)
{
	const uint32 BytesPerElement = 16;
	const uint32 ExternalBufferSize = TryGetSize(ExternalBuffer);

	checkf(Params.NumBytes % BytesPerElement == 0, TEXT("NumBytes (%s) must be a multiple of BytesPerElement (%s)"), Params.NumBytes, BytesPerElement);
	checkf(ExternalBufferSize % BytesPerElement == 0, TEXT("NumBytes (%s) must be a multiple of BytesPerElement (%s)"), ExternalBufferSize, BytesPerElement);

	uint32 NumElements = Params.NumBytes / BytesPerElement;
	uint32 NumElementsOld = ExternalBufferSize / BytesPerElement;

	checkf(NumElements % Params.NumArrays == 0, TEXT("NumElements (%s) must be a multiple of NumArrays (%s)"), NumElements, Params.NumArrays);
	checkf(NumElementsOld % Params.NumArrays == 0, TEXT("NumElements (%s) must be a multiple of NumArrays (%s)"), NumElementsOld, Params.NumArrays);

	const FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(BytesPerElement, NumElements);

	FRDGBuffer* InternalBufferNew = nullptr;

	if (!ExternalBuffer)
	{
		InternalBufferNew = GraphBuilder.CreateBuffer(BufferDesc, Name);
		ExternalBuffer = GraphBuilder.ConvertToExternalBuffer(InternalBufferNew);
		return InternalBufferNew;
	}

	FRDGBuffer* InternalBufferOld = GraphBuilder.RegisterExternalBuffer(ExternalBuffer);

	const uint32 BufferSize = BufferDesc.GetSize();
	const uint32 BufferSizeOld = InternalBufferOld->GetSize();

	if (BufferSize != BufferSizeOld)
	{
		InternalBufferNew = GraphBuilder.CreateBuffer(BufferDesc, Name);
		ExternalBuffer = GraphBuilder.ConvertToExternalBuffer(InternalBufferNew);

		FRDGBufferUAV* NewBufferUAV = GraphBuilder.CreateUAV(InternalBufferNew, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferSRV* OldBufferSRV = GraphBuilder.CreateSRV(InternalBufferOld);

		// Copy data to new buffer
		uint32 OldArraySize = NumElementsOld / Params.NumArrays;
		uint32 NewArraySize = NumElements / Params.NumArrays;

		FMemcpyResourceParams MemcpyParams;
		MemcpyParams.Count = FMath::Min(NewArraySize, OldArraySize);

		for (uint32 Index = 0; Index < Params.NumArrays; Index++)
		{
			MemcpyParams.SrcOffset = Index * OldArraySize;
			MemcpyParams.DstOffset = Index * NewArraySize;
			MemcpyResource(GraphBuilder, NewBufferUAV, OldBufferSRV, MemcpyParams);
		}

		return InternalBufferNew;
	}

	return InternalBufferOld;
}

FRDGBuffer* ResizeByteAddressBufferIfNeeded(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& ExternalBuffer, uint32 NumBytes, const TCHAR* Name)
{
	// Needs to be aligned to 16 bytes to MemcpyResource to work correctly (otherwise it skips last unaligned elements of the buffer during resize)
	check((NumBytes & 15) == 0);

	return ResizeBufferIfNeeded(GraphBuilder, ExternalBuffer, FRDGBufferDesc::CreateByteAddressDesc(NumBytes), Name);
}

void FRDGScatterUploadBuffer::Release()
{
	check(!ScatterData);
	ScatterBuffer = nullptr;
	UploadBuffer = nullptr;
}

uint32 FRDGScatterUploadBuffer::GetNumBytes() const
{
	return TryGetSize(ScatterBuffer) + TryGetSize(UploadBuffer);
}

void FRDGScatterUploadBuffer::Init(FRDGBuilder& GraphBuilder, TArrayView<const uint32> ElementScatterOffsets, uint32 InNumBytesPerElement, bool bInFloat4Buffer, const TCHAR* DebugName)
{
	Init(GraphBuilder, ElementScatterOffsets.Num(), InNumBytesPerElement, bInFloat4Buffer, DebugName);
	FMemory::ParallelMemcpy(ScatterData, ElementScatterOffsets.GetData(), ElementScatterOffsets.Num() * ElementScatterOffsets.GetTypeSize(), EMemcpyCachePolicy::StoreUncached);
	NumScatters = ElementScatterOffsets.Num();
}

void FRDGScatterUploadBuffer::InitPreSized(FRDGBuilder& GraphBuilder, uint32 NumElements, uint32 InNumBytesPerElement, bool bInFloat4Buffer, const TCHAR* DebugName)
{
	Init(GraphBuilder, NumElements, InNumBytesPerElement, bInFloat4Buffer, DebugName);
	NumScatters = NumElements;
}

struct FScatterUploadComputeConfig
{
	uint32 ThreadGroupSize;
	uint32 NumBytesPerThread;
	uint32 NumThreadsPerScatter;
	uint32 NumThreads;
	uint32 NumDispatches;
	uint32 NumLoops;
};

FScatterUploadComputeConfig GetScatterUploadComputeConfig(uint32 NumScatters, uint32 NumBytesPerElement)
{
	constexpr uint32 ThreadGroupSize = 64u;

	FScatterUploadComputeConfig Config;
	Config.ThreadGroupSize = ThreadGroupSize;
	Config.NumBytesPerThread = (NumBytesPerElement & 15) == 0 ? 16 : 4;
	Config.NumThreadsPerScatter = NumBytesPerElement / Config.NumBytesPerThread;
	Config.NumThreads = NumScatters * Config.NumThreadsPerScatter;
	Config.NumDispatches = FMath::DivideAndRoundUp(Config.NumThreads, Config.ThreadGroupSize);
	Config.NumLoops = FMath::DivideAndRoundUp(Config.NumDispatches, (uint32)GMaxComputeDispatchDimension);
	return Config;
}

void FRDGScatterUploadBuffer::Init(FRDGBuilder& GraphBuilder, uint32 NumElements, uint32 InNumBytesPerElement, bool bInFloat4Buffer, const TCHAR* Name)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRDGScatterUploadBuffer::Init);

	NumScatters = 0;
	MaxScatters = NumElements;
	NumBytesPerElement = InNumBytesPerElement;
	bFloat4Buffer = bInFloat4Buffer;

	const EBufferUsageFlags Usage = bInFloat4Buffer ? BUF_None : BUF_ByteAddressBuffer;
	const uint32 TypeSize = bInFloat4Buffer ? 16 : 4;

	const uint32 ScatterNumBytesPerElement = sizeof(uint32);
	const uint32 ScatterBytes = NumElements * ScatterNumBytesPerElement;
	const uint32 ScatterBufferSize = (uint32)FMath::Min((uint64)FMath::RoundUpToPowerOfTwo(ScatterBytes), GetMaxBufferDimension() * sizeof(uint32));
	check(ScatterBufferSize >= ScatterBytes);

	const uint32 UploadNumBytesPerElement = TypeSize;
	const uint32 UploadBytes = NumElements * NumBytesPerElement;
	const uint32 UploadBufferSize = (uint32)FMath::Min((uint64)FMath::RoundUpToPowerOfTwo(UploadBytes), GetMaxBufferDimension() * TypeSize);
	check(UploadBufferSize >= UploadBytes);

	// Recreate buffers is they are already queued into RDG from a previous call.
	if (IsRegistered(GraphBuilder, ScatterBuffer))
	{
		ScatterBuffer = nullptr;
		UploadBuffer = nullptr;
	}

	if (!ScatterBuffer || ScatterBytes > ScatterBuffer->GetSize() || ScatterBufferSize < ScatterBuffer->GetSize() / 2)
	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredUploadDesc(ScatterNumBytesPerElement, ScatterBufferSize / ScatterNumBytesPerElement);
		Desc.Usage |= Usage;

		AllocatePooledBuffer(Desc, ScatterBuffer, Name, ERDGPooledBufferAlignment::None);
	}

	if (!UploadBuffer || UploadBytes > UploadBuffer->GetSize() || UploadBufferSize < UploadBuffer->GetSize() / 2)
	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredUploadDesc(TypeSize, UploadBufferSize / UploadNumBytesPerElement);
		Desc.Usage |= Usage;

		AllocatePooledBuffer(Desc, UploadBuffer, Name, ERDGPooledBufferAlignment::None);
	}

	ScatterData = (uint32*)RHILockBuffer(ScatterBuffer->GetRHI(), 0, ScatterBytes, RLM_WriteOnly);
	UploadData = (uint8*)RHILockBuffer(UploadBuffer->GetRHI(), 0, UploadBytes, RLM_WriteOnly);
}

void FRDGScatterUploadBuffer::ResourceUploadTo(FRDGBuilder& GraphBuilder, FRDGViewableResource* DstResource)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRDGScatterUploadBuffer::ResourceUploadTo);

	RHIUnlockBuffer(ScatterBuffer->GetRHI());
	RHIUnlockBuffer(UploadBuffer->GetRHI());

	ScatterData = nullptr;
	UploadData = nullptr;

	if (NumScatters == 0)
	{
		return;
	}

	const FScatterUploadComputeConfig ComputeConfig = GetScatterUploadComputeConfig(NumScatters, NumBytesPerElement);

	const EResourceType DstResourceType = GetResourceType(DstResource);
	check(bFloat4Buffer != (DstResourceType == EResourceType::BYTEBUFFER));

	EByteBufferResourceType ResourceTypeEnum = EByteBufferResourceType::Count;

	FRDGScatterCopyCS::FParameters Parameters;
	Parameters.Common.Size = ComputeConfig.NumThreadsPerScatter;
	Parameters.NumScatters = NumScatters;

	FRDGBufferSRV* ScatterBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(ScatterBuffer));
	FRDGBufferSRV* UploadBufferSRV  = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(UploadBuffer));

	if (DstResourceType == EResourceType::BYTEBUFFER)
	{
		if (ComputeConfig.NumBytesPerThread == 16)
		{
			ResourceTypeEnum = EByteBufferResourceType::Uint4Aligned_Buffer;
		}
		else
		{
			ResourceTypeEnum = EByteBufferResourceType::Uint_Buffer;
		}
		Parameters.UploadByteAddressBuffer = UploadBufferSRV;
		Parameters.ScatterByteAddressBuffer = ScatterBufferSRV;
		Parameters.Common.DstByteAddressBuffer = GraphBuilder.CreateUAV(GetAsBuffer(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else if (DstResourceType == EResourceType::STRUCTURED_BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_StructuredBuffer;

		Parameters.UploadStructuredBuffer = UploadBufferSRV;
		Parameters.ScatterStructuredBuffer = ScatterBufferSRV;
		Parameters.Common.DstStructuredBuffer = GraphBuilder.CreateUAV(GetAsBuffer(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else if (DstResourceType == EResourceType::BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Buffer;

		Parameters.UploadStructuredBuffer = UploadBufferSRV;
		Parameters.ScatterStructuredBuffer = ScatterBufferSRV;
		Parameters.Common.DstBuffer = GraphBuilder.CreateUAV(GetAsBuffer(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else if (DstResourceType == EResourceType::TEXTURE)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Texture;

		Parameters.UploadStructuredBuffer = UploadBufferSRV;
		Parameters.ScatterStructuredBuffer = ScatterBufferSRV;
		Parameters.Common.DstTexture = GraphBuilder.CreateUAV(GetAsTexture(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);

		Parameters.Common.Float4sPerLine = CalculateFloat4sPerLine();
	}

	FRDGByteBufferShader::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRDGByteBufferShader::ResourceTypeDim>((int)ResourceTypeEnum);
	TShaderMapRef<FRDGScatterCopyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

	for (uint32 LoopIdx = 0; LoopIdx < ComputeConfig.NumLoops; ++LoopIdx)
	{
		Parameters.Common.SrcOffset = LoopIdx * (uint32)GMaxComputeDispatchDimension * ComputeConfig.ThreadGroupSize;

		uint32 LoopNumDispatch = FMath::Min(ComputeConfig.NumDispatches - LoopIdx * (uint32)GMaxComputeDispatchDimension, (uint32)GMaxComputeDispatchDimension);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ScatterUpload[%d] (Resource: %s, Offset: %u, GroupSize: %u)", LoopIdx, DstResource->Name, Parameters.Common.SrcOffset, LoopNumDispatch),
			ComputeShader,
			GraphBuilder.AllocParameters(&Parameters),
			FIntVector(LoopNumDispatch, 1, 1));
	}

	Reset();
}

void FRDGScatterUploadBuffer::Reset()
{
	NumScatters = 0;
	MaxScatters = 0;
	NumBytesPerElement = 0;
}

void FRDGScatterUploader::Lock(FRHICommandListBase& RHICmdList)
{
	check(State == EState::Empty);
	State = EState::Locked;
	ScatterData = (uint32*)RHICmdList.LockBuffer(ScatterBuffer, 0, ScatterBytes, RLM_WriteOnly);
	UploadData = (uint8*)RHICmdList.LockBuffer(UploadBuffer, 0, UploadBytes, RLM_WriteOnly);
}

void FRDGScatterUploader::Unlock(FRHICommandListBase& RHICmdList)
{
	check(State == EState::Locked);
	State = EState::Unlocked;
	RHICmdList.UnlockBuffer(ScatterBuffer);
	RHICmdList.UnlockBuffer(UploadBuffer);
}

FRDGScatterUploader* FRDGAsyncScatterUploadBuffer::Begin(FRDGBuilder& GraphBuilder, FRDGViewableResource* DstResource, uint32 NumElements, uint32 NumBytesPerElement, const TCHAR* Name)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRDGAsyncScatterUploadBuffer::Upload);

	const EResourceType DstResourceType = GetResourceType(DstResource);
	const EBufferUsageFlags Usage = DstResourceType == EResourceType::BYTEBUFFER ? BUF_ByteAddressBuffer : BUF_None;
	const uint32 TypeSize = DstResourceType == EResourceType::BYTEBUFFER ? 4 : 16;

	const uint32 ScatterNumBytesPerElement = sizeof(uint32);
	const uint32 ScatterBytes = NumElements * ScatterNumBytesPerElement;
	const uint32 ScatterBufferSize = (uint32)FMath::Min((uint64)FMath::RoundUpToPowerOfTwo(ScatterBytes), GetMaxBufferDimension() * sizeof(uint32));
	check(ScatterBufferSize >= ScatterBytes);

	const uint32 UploadNumBytesPerElement = TypeSize;
	const uint32 UploadBytes = NumElements * NumBytesPerElement;
	const uint32 UploadBufferSize = (uint32)FMath::Min((uint64)FMath::RoundUpToPowerOfTwo(UploadBytes), GetMaxBufferDimension() * TypeSize);
	check(UploadBufferSize >= UploadBytes);

	// Recreate buffers is they are already queued into RDG from a previous call.
	if (IsRegistered(GraphBuilder, ScatterBuffer))
	{
		ScatterBuffer = nullptr;
		UploadBuffer = nullptr;
	}

	if (!ScatterBuffer || ScatterBytes > ScatterBuffer->GetSize() || ScatterBufferSize < ScatterBuffer->GetSize() / 2)
	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredUploadDesc(ScatterNumBytesPerElement, ScatterBufferSize / ScatterNumBytesPerElement);
		Desc.Usage |= Usage;

		AllocatePooledBuffer(Desc, ScatterBuffer, Name, ERDGPooledBufferAlignment::None);
	}

	if (!UploadBuffer || UploadBytes > UploadBuffer->GetSize() || UploadBufferSize < UploadBuffer->GetSize() / 2)
	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredUploadDesc(TypeSize, UploadBufferSize / UploadNumBytesPerElement);
		Desc.Usage |= Usage;

		AllocatePooledBuffer(Desc, UploadBuffer, Name, ERDGPooledBufferAlignment::None);
	}

	FRDGScatterUploader* Uploader = GraphBuilder.AllocObject<FRDGScatterUploader>();
	Uploader->MaxScatters = NumElements;
	Uploader->NumBytesPerElement = NumBytesPerElement;
	Uploader->DstResource = DstResource;
	Uploader->ScatterBuffer = ScatterBuffer->GetRHI();
	Uploader->UploadBuffer = UploadBuffer->GetRHI();
	Uploader->ScatterBytes = ScatterBytes;
	Uploader->UploadBytes = UploadBytes;
	return Uploader;
}

FRDGScatterUploader* FRDGAsyncScatterUploadBuffer::BeginPreSized(FRDGBuilder& GraphBuilder, FRDGViewableResource* DstResource, uint32 NumElements, uint32 NumBytesPerElement, const TCHAR* Name)
{
	FRDGScatterUploader* Uploader = Begin(GraphBuilder, DstResource, NumElements, NumBytesPerElement, Name);
	Uploader->NumScatters = NumElements;
	Uploader->bNumScattersPreSized = true;
	return Uploader;
}

void FRDGAsyncScatterUploadBuffer::End(FRDGBuilder& GraphBuilder, FRDGScatterUploader* Uploader)
{
	check(Uploader);
	checkf(!FRDGBuilder::IsImmediateMode() || Uploader->State == FRDGScatterUploader::EState::Unlocked, TEXT("In immediate mode, you must fill the uploader prior to calling End."));

	const uint32 NumScatters = Uploader->bNumScattersPreSized ? Uploader->NumScatters : Uploader->MaxScatters;

	const FScatterUploadComputeConfig ComputeConfig = GetScatterUploadComputeConfig(NumScatters, Uploader->NumBytesPerElement);

	FRDGScatterCopyCS::FParameters Parameters;
	Parameters.Common.Size = ComputeConfig.NumThreadsPerScatter;
	Parameters.NumScatters = NumScatters;

	FRDGBufferSRV* ScatterBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(ScatterBuffer));
	FRDGBufferSRV* UploadBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(UploadBuffer));

	FRDGViewableResource* DstResource = Uploader->DstResource;
	const EResourceType DstResourceType = GetResourceType(DstResource);

	EByteBufferResourceType ResourceTypeEnum = EByteBufferResourceType::Count;

	if (DstResourceType == EResourceType::BYTEBUFFER)
	{
		if (ComputeConfig.NumBytesPerThread == 16)
		{
			ResourceTypeEnum = EByteBufferResourceType::Uint4Aligned_Buffer;
		}
		else
		{
			ResourceTypeEnum = EByteBufferResourceType::Uint_Buffer;
		}
		Parameters.UploadByteAddressBuffer = UploadBufferSRV;
		Parameters.ScatterByteAddressBuffer = ScatterBufferSRV;
		Parameters.Common.DstByteAddressBuffer = GraphBuilder.CreateUAV(GetAsBuffer(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else if (DstResourceType == EResourceType::STRUCTURED_BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_StructuredBuffer;

		Parameters.UploadStructuredBuffer = UploadBufferSRV;
		Parameters.ScatterStructuredBuffer = ScatterBufferSRV;
		Parameters.Common.DstStructuredBuffer = GraphBuilder.CreateUAV(GetAsBuffer(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else if (DstResourceType == EResourceType::BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Buffer;

		Parameters.UploadStructuredBuffer = UploadBufferSRV;
		Parameters.ScatterStructuredBuffer = ScatterBufferSRV;
		Parameters.Common.DstBuffer = GraphBuilder.CreateUAV(GetAsBuffer(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else if (DstResourceType == EResourceType::TEXTURE)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Texture;

		Parameters.UploadStructuredBuffer = UploadBufferSRV;
		Parameters.ScatterStructuredBuffer = ScatterBufferSRV;
		Parameters.Common.DstTexture = GraphBuilder.CreateUAV(GetAsTexture(DstResource), ERDGUnorderedAccessViewFlags::SkipBarrier);

		Parameters.Common.Float4sPerLine = CalculateFloat4sPerLine();
	}

	FRDGByteBufferShader::FPermutationDomain PermutationVector;
	PermutationVector.Set<FRDGByteBufferShader::ResourceTypeDim>((int)ResourceTypeEnum);
	TShaderMapRef<FRDGScatterCopyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

	for (uint32 LoopIdx = 0; LoopIdx < ComputeConfig.NumLoops; ++LoopIdx)
	{
		Parameters.Common.SrcOffset = LoopIdx * (uint32)GMaxComputeDispatchDimension * ComputeConfig.ThreadGroupSize;

		FRDGScatterCopyCS::FParameters* PassParameters = GraphBuilder.AllocParameters(&Parameters);

		if (Uploader->bNumScattersPreSized)
		{
			const uint32 LoopNumDispatch = FMath::Min(ComputeConfig.NumDispatches - LoopIdx * (uint32)GMaxComputeDispatchDimension, (uint32)GMaxComputeDispatchDimension);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ScatterUpload[%d] (Resource: %s, Offset: %u, GroupSize: %u)", LoopIdx, DstResource->Name, Parameters.Common.SrcOffset, LoopNumDispatch),
				ComputeShader,
				PassParameters,
				FIntVector(LoopNumDispatch, 1, 1));
		}
		else
		{
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ScatterUpload[%d] (Resource: %s, Offset: %u)", LoopIdx, DstResource->Name, Parameters.Common.SrcOffset),
				ComputeShader,
				PassParameters,
				[PassParameters, LoopIdx, NumBytesPerElement = Uploader->NumBytesPerElement, Uploader]
			{
				const uint32 NumScatters = Uploader->GetFinalNumScatters();
				const FScatterUploadComputeConfig ComputeConfig = GetScatterUploadComputeConfig(NumScatters, NumBytesPerElement);

				if (LoopIdx < ComputeConfig.NumLoops)
				{
					PassParameters->NumScatters = NumScatters;

					return FIntVector(FMath::Min(ComputeConfig.NumDispatches - LoopIdx * (uint32)GMaxComputeDispatchDimension, (uint32)GMaxComputeDispatchDimension), 1, 1);
				}
				else
				{
					return FIntVector::ZeroValue;
				}
			});
		}
	}
}

void FRDGAsyncScatterUploadBuffer::Release()
{
	ScatterBuffer = nullptr;
	UploadBuffer = nullptr;
}

uint32 FRDGAsyncScatterUploadBuffer::GetNumBytes() const
{
	return TryGetSize(ScatterBuffer) + TryGetSize(UploadBuffer);
}

template<typename ResourceType>
void MemsetResource(FRHICommandList& RHICmdList, const ResourceType& DstBuffer, const FMemsetResourceParams& Params)
{
	EByteBufferResourceType ResourceTypeEnum;

	FMemsetBufferCS::FParameters Parameters;
	Parameters.Value = Params.Value;
	Parameters.Size = Params.Count;
	Parameters.DstOffset = Params.DstOffset;

	if (ResourceTypeTraits<ResourceType>::Type == EResourceType::BYTEBUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Uint_Buffer;

		Parameters.DstByteAddressBuffer = DstBuffer.UAV;
	}
	else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Buffer;

		Parameters.DstBuffer = DstBuffer.UAV;
	}
	else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::STRUCTURED_BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_StructuredBuffer;

		Parameters.DstStructuredBuffer = DstBuffer.UAV;
	}
	else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::TEXTURE)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Texture;

		Parameters.DstTexture = DstBuffer.UAV;
		Parameters.Float4sPerLine = CalculateFloat4sPerLine();
	}

	FMemcpyCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FMemcpyCS::ResourceTypeDim >((int)ResourceTypeEnum);

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FMemsetBufferCS>(PermutationVector);

	// each thread will set 4 floats / uints
	const uint32 Divisor = ResourceTypeTraits<ResourceType>::Type == EResourceType::BYTEBUFFER ? 4 : 1;

	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Parameters, FIntVector(FMath::DivideAndRoundUp(Params.Count / Divisor, 64u), 1, 1));
}

template<typename ResourceType>
void MemcpyResource(FRHICommandList& RHICmdList, const ResourceType& DstBuffer, const ResourceType& SrcBuffer, const FMemcpyResourceParams& Params, bool bAlreadyInUAVOverlap)
{
	// each thread will copy 4 floats / uints
	const uint32 Divisor = ResourceTypeTraits<ResourceType>::Type == EResourceType::BYTEBUFFER ? 4 : 1;

	if (!bAlreadyInUAVOverlap)	// TODO: Get rid of this check once BeginUAVOverlap/EndUAVOverlap supports nesting.
		RHICmdList.BeginUAVOverlap(DstBuffer.UAV);

	uint32 NumElementsProcessed = 0;

	while (NumElementsProcessed < Params.Count)
	{
		const uint32 NumWaves = FMath::Max(FMath::Min<uint32>(GRHIMaxDispatchThreadGroupsPerDimension.X, FMath::DivideAndRoundUp(Params.Count / Divisor, 64u)), 1u);
		const uint32 NumElementsPerDispatch = FMath::Min(FMath::Max(NumWaves, 1u) * Divisor * 64, Params.Count - NumElementsProcessed);

		EByteBufferResourceType ResourceTypeEnum;

		FMemcpyCS::FParameters Parameters;
		Parameters.Common.Size = NumElementsPerDispatch;
		Parameters.Common.SrcOffset = (Params.SrcOffset + NumElementsProcessed);
		Parameters.Common.DstOffset = (Params.DstOffset + NumElementsProcessed);

		if (ResourceTypeTraits<ResourceType>::Type == EResourceType::BYTEBUFFER)
		{
			ResourceTypeEnum = EByteBufferResourceType::Uint_Buffer;

			Parameters.SrcByteAddressBuffer = SrcBuffer.SRV;
			Parameters.Common.DstByteAddressBuffer = DstBuffer.UAV;
		}
		else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::STRUCTURED_BUFFER)
		{
			ResourceTypeEnum = EByteBufferResourceType::Float4_StructuredBuffer;

			Parameters.SrcStructuredBuffer = SrcBuffer.SRV;
			Parameters.Common.DstStructuredBuffer = DstBuffer.UAV;
		}
		else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::BUFFER)
		{
			ResourceTypeEnum = EByteBufferResourceType::Float4_Buffer;

			Parameters.SrcBuffer = SrcBuffer.SRV;
			Parameters.Common.DstBuffer = DstBuffer.UAV;
		}
		else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::TEXTURE)
		{
			ResourceTypeEnum = EByteBufferResourceType::Float4_Texture;

			Parameters.SrcTexture = SrcBuffer.SRV;
			Parameters.Common.DstTexture = DstBuffer.UAV;
			Parameters.Common.Float4sPerLine = CalculateFloat4sPerLine();
		}
		else
		{
			check(false);
		}

		FMemcpyCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMemcpyCS::ResourceTypeDim >((int)ResourceTypeEnum);

		auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FMemcpyCS >(PermutationVector);

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Parameters, FIntVector(NumWaves, 1, 1));

		NumElementsProcessed += NumElementsPerDispatch;
	}

	if(!bAlreadyInUAVOverlap)
		RHICmdList.EndUAVOverlap(DstBuffer.UAV);
}

template<>
RENDERCORE_API bool ResizeResourceIfNeeded<FTextureRWBuffer>(FRHICommandList& RHICmdList, FTextureRWBuffer& Texture, uint32 NumBytes, const TCHAR* DebugName)
{
	check((NumBytes & 15) == 0);
	uint32 Float4sPerLine = CalculateFloat4sPerLine();
	uint32 BytesPerLine = Float4sPerLine * 16;

	EPixelFormat BufferFormat = PF_A32B32G32R32F;
	uint32 BytesPerElement = GPixelFormats[BufferFormat].BlockBytes;

	uint32 NumLines = (NumBytes + BytesPerLine - 1) / BytesPerLine;

	if (Texture.NumBytes == 0)
	{
		Texture.Initialize2D(DebugName, BytesPerElement, Float4sPerLine, NumLines, PF_A32B32G32R32F, TexCreate_RenderTargetable | TexCreate_UAV);
		return true;
	}
	else if ((NumLines * Float4sPerLine * BytesPerElement) != Texture.NumBytes)
	{
		FTextureRWBuffer NewTexture;
		NewTexture.Initialize2D(DebugName, BytesPerElement, Float4sPerLine, NumLines, PF_A32B32G32R32F, TexCreate_RenderTargetable | TexCreate_UAV);

		FMemcpyResourceParams Params;
		Params.Count = NumBytes / BytesPerElement;
		Params.SrcOffset = 0;
		Params.DstOffset = 0;
		MemcpyResource(RHICmdList, NewTexture, Texture, Params);
		Texture = NewTexture;
		return true;
	}

	return false;
}

template<>
RENDERCORE_API bool ResizeResourceIfNeeded<FRWBufferStructured>(FRHICommandList& RHICmdList, FRWBufferStructured& Buffer, uint32 NumBytes, const TCHAR* DebugName)
{
	const uint32 BytesPerElement = 16;

	check((NumBytes & (BytesPerElement - 1)) == 0);

	uint32 NumElements = NumBytes / BytesPerElement;

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, BytesPerElement, NumElements);
		return true;
	}
	else if (NumBytes != Buffer.NumBytes)
	{
		FRWBufferStructured NewBuffer;
		NewBuffer.Initialize(DebugName, BytesPerElement, NumElements);

		RHICmdList.Transition(FRHITransitionInfo(Buffer.UAV, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(NewBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		// Copy data to new buffer
		FMemcpyResourceParams Params;
		Params.Count = FMath::Min(NumBytes, Buffer.NumBytes) / BytesPerElement;
		Params.SrcOffset = 0;
		Params.DstOffset = 0;
		MemcpyResource(RHICmdList, NewBuffer, Buffer, Params);

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

template<>
RENDERCORE_API bool ResizeResourceIfNeeded<FRWByteAddressBuffer>(FRHICommandList& RHICmdList, FRWByteAddressBuffer& Buffer, uint32 NumBytes, const TCHAR* DebugName)
{
	const uint32 BytesPerElement = 4;

	// Needs to be aligned to 16 bytes to MemcpyResource to work correctly (otherwise it skips last unaligned elements of the buffer during resize)
	check((NumBytes & 15) == 0);

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, NumBytes);
		return true;
	}
	else if (NumBytes != Buffer.NumBytes)
	{
		FRWByteAddressBuffer NewBuffer;
		NewBuffer.Initialize(DebugName, NumBytes);

		RHICmdList.Transition({
			FRHITransitionInfo(Buffer.UAV, ERHIAccess::Unknown, ERHIAccess::SRVCompute),
			FRHITransitionInfo(NewBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute)
		});

		// Copy data to new buffer
		FMemcpyResourceParams Params;
		Params.Count = FMath::Min(NumBytes, Buffer.NumBytes) / BytesPerElement;
		Params.SrcOffset = 0;
		Params.DstOffset = 0;
		MemcpyResource(RHICmdList, NewBuffer, Buffer, Params);

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

RENDERCORE_API bool ResizeResourceIfNeeded(FRHICommandList& RHICmdList, FRWBuffer& Buffer, EPixelFormat Format, uint32 NumElements, const TCHAR* DebugName)
{
	const uint32 BytesPerElement = GPixelFormats[Format].BlockBytes;
	const uint32 NumBytes = BytesPerElement * NumElements;

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, BytesPerElement, NumElements, Format);
		return true;
	}
	else if (NumBytes != Buffer.NumBytes)
	{
		FRWBuffer NewBuffer;
		NewBuffer.Initialize(DebugName, BytesPerElement, NumElements, Format);

		RHICmdList.Transition(FRHITransitionInfo(Buffer.UAV, ERHIAccess::Unknown, ERHIAccess::SRVCompute));
		RHICmdList.Transition(FRHITransitionInfo(NewBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));

		// Copy data to new buffer
		FMemcpyResourceParams MemcpyParams;
		MemcpyParams.Count = FMath::Min(NumBytes, Buffer.NumBytes) / BytesPerElement;
		MemcpyParams.SrcOffset = 0;
		MemcpyParams.DstOffset = 0;
		MemcpyResource(RHICmdList, NewBuffer, Buffer, MemcpyParams);

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

template<>
RENDERCORE_API bool ResizeResourceSOAIfNeeded<FRWBufferStructured>(FRHICommandList& RHICmdList, FRWBufferStructured& Buffer, const FResizeResourceSOAParams& Params, const TCHAR* DebugName)
{
	const uint32 BytesPerElement = 16;

	checkf(Params.NumBytes % BytesPerElement == 0, TEXT("NumBytes (%s) must be a multiple of BytesPerElement (%s)"), Params.NumBytes, BytesPerElement);
	checkf(Buffer.NumBytes % BytesPerElement == 0, TEXT("NumBytes (%s) must be a multiple of BytesPerElement (%s)"), Buffer.NumBytes, BytesPerElement);

	uint32 NumElements = Params.NumBytes / BytesPerElement;
	uint32 NumElementsOld = Buffer.NumBytes / BytesPerElement;

	checkf(NumElements % Params.NumArrays == 0, TEXT("NumElements (%s) must be a multiple of NumArrays (%s)"), NumElements, Params.NumArrays);
	checkf(NumElementsOld % Params.NumArrays == 0, TEXT("NumElements (%s) must be a multiple of NumArrays (%s)"), NumElementsOld, Params.NumArrays);

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, BytesPerElement, NumElements);
		return true;
	}
	else if (Params.NumBytes != Buffer.NumBytes)
	{
		FRWBufferStructured NewBuffer;
		NewBuffer.Initialize(DebugName, BytesPerElement, NumElements);

		RHICmdList.Transition({
			FRHITransitionInfo(Buffer.UAV, ERHIAccess::Unknown, ERHIAccess::SRVCompute),
			FRHITransitionInfo(NewBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute)
		});

		// Copy data to new buffer
		uint32 OldArraySize = NumElementsOld / Params.NumArrays;
		uint32 NewArraySize = NumElements / Params.NumArrays;

		RHICmdList.BeginUAVOverlap(NewBuffer.UAV);

		FMemcpyResourceParams MemcpyParams;
		MemcpyParams.Count = FMath::Min(NewArraySize, OldArraySize);

		for( uint32 i = 0; i < Params.NumArrays; i++ )
		{
			MemcpyParams.SrcOffset = i * OldArraySize;
			MemcpyParams.DstOffset = i * NewArraySize;
			MemcpyResource( RHICmdList, NewBuffer, Buffer, MemcpyParams, true );
		}

		RHICmdList.EndUAVOverlap(NewBuffer.UAV);

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

RENDERCORE_API bool ResizeResourceSOAIfNeeded(FRDGBuilder& GraphBuilder, FRWBufferStructured& Buffer, const FResizeResourceSOAParams& Params, const TCHAR* DebugName)
{
	const uint32 BytesPerElement = 16;

	checkf(Params.NumBytes % BytesPerElement == 0, TEXT("NumBytes (%s) must be a multiple of BytesPerElement (%s)"), Params.NumBytes, BytesPerElement);
	checkf(Buffer.NumBytes % BytesPerElement == 0, TEXT("NumBytes (%s) must be a multiple of BytesPerElement (%s)"), Buffer.NumBytes, BytesPerElement);

	uint32 NumElements = Params.NumBytes / BytesPerElement;
	uint32 NumElementsOld = Buffer.NumBytes / BytesPerElement;

	checkf(NumElements % Params.NumArrays == 0, TEXT("NumElements (%s) must be a multiple of NumArrays (%s)"), NumElements, Params.NumArrays);
	checkf(NumElementsOld % Params.NumArrays == 0, TEXT("NumElements (%s) must be a multiple of NumArrays (%s)"), NumElementsOld, Params.NumArrays);

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, BytesPerElement, NumElements);
		return true;
	}
	else if (Params.NumBytes != Buffer.NumBytes)
	{
		FRWBufferStructured NewBuffer;
		FRWBufferStructured OldBuffer = Buffer;
		NewBuffer.Initialize(DebugName, BytesPerElement, NumElements);

		AddPass(GraphBuilder, RDG_EVENT_NAME("ResizeResourceSOAIfNeeded"), 
			[OldBuffer, NewBuffer, NumElements, NumElementsOld, Params](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.Transition({
				FRHITransitionInfo(OldBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::SRVCompute),
				FRHITransitionInfo(NewBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute)
			});

			// Copy data to new buffer
			uint32 OldArraySize = NumElementsOld / Params.NumArrays;
			uint32 NewArraySize = NumElements / Params.NumArrays;

			RHICmdList.BeginUAVOverlap(NewBuffer.UAV);

			FMemcpyResourceParams MemcpyParams;
			MemcpyParams.Count = FMath::Min(NewArraySize, OldArraySize);

			for (uint32 i = 0; i < Params.NumArrays; i++)
			{
				MemcpyParams.SrcOffset = i * OldArraySize;
				MemcpyParams.DstOffset = i * NewArraySize;
				MemcpyResource(RHICmdList, NewBuffer, OldBuffer, MemcpyParams, true);
			}
			RHICmdList.EndUAVOverlap(NewBuffer.UAV);
		});

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

template <typename FBufferType>
void AddCopyBufferPass(FRDGBuilder& GraphBuilder, const FBufferType &NewBuffer, const FBufferType &OldBuffer, uint32 ElementSize)
{
	AddPass(GraphBuilder, RDG_EVENT_NAME("ResizeResourceIfNeeded-Copy"), 
		[OldBuffer, NewBuffer, ElementSize](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.Transition({
			FRHITransitionInfo(OldBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::SRVCompute),
			FRHITransitionInfo(NewBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute)
		});

		// Copy data to new buffer
		FMemcpyResourceParams MemcpyParams;
		MemcpyParams.Count = FMath::Min(NewBuffer.NumBytes, OldBuffer.NumBytes) / ElementSize;
		MemcpyParams.SrcOffset = 0;
		MemcpyParams.DstOffset = 0;

		MemcpyResource(RHICmdList, NewBuffer, OldBuffer, MemcpyParams);
	});
}

RENDERCORE_API bool ResizeResourceIfNeeded(FRDGBuilder& GraphBuilder, FRWBufferStructured& Buffer, uint32 NumBytes, const TCHAR* DebugName)
{
	const uint32 BytesPerElement = 16;

	checkf((NumBytes % BytesPerElement) == 0, TEXT("NumBytes (%s) must be a multiple of BytesPerElement (%s)"), NumBytes, BytesPerElement);

	uint32 NumElements = NumBytes / BytesPerElement;

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, BytesPerElement, NumElements);
		return true;
	}
	else if (NumBytes != Buffer.NumBytes)
	{
		FRWBufferStructured NewBuffer;
		NewBuffer.Initialize(DebugName, BytesPerElement, NumElements);

		AddCopyBufferPass(GraphBuilder, NewBuffer, Buffer, BytesPerElement);

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

RENDERCORE_API bool ResizeResourceIfNeeded(FRDGBuilder& GraphBuilder, FRWByteAddressBuffer& Buffer, uint32 NumBytes, const TCHAR* DebugName)
{
	// Needs to be aligned to 16 bytes to MemcpyResource to work correctly (otherwise it skips last unaligned elements of the buffer during resize)
	check((NumBytes & 15) == 0);

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, NumBytes);
		return true;
	}
	else if (NumBytes != Buffer.NumBytes)
	{
		FRWByteAddressBuffer NewBuffer;
		NewBuffer.Initialize(DebugName, NumBytes);

		AddCopyBufferPass(GraphBuilder, NewBuffer, Buffer, 4);

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

RENDERCORE_API bool ResizeResourceIfNeeded(FRDGBuilder& GraphBuilder, FRWBuffer& Buffer, EPixelFormat Format, uint32 NumElements, const TCHAR* DebugName)
{
	const uint32 BytesPerElement = GPixelFormats[Format].BlockBytes;
	const uint32 NumBytes = BytesPerElement * NumElements;

	if (Buffer.NumBytes == 0)
	{
		Buffer.Initialize(DebugName, BytesPerElement, NumElements, Format);
		return true;
	}
	else if (NumBytes != Buffer.NumBytes)
	{
		FRWBuffer NewBuffer;
		NewBuffer.Initialize(DebugName, BytesPerElement, NumElements, Format);

		AddCopyBufferPass(GraphBuilder, NewBuffer, Buffer, BytesPerElement);

		Buffer = NewBuffer;
		return true;
	}

	return false;
}

void FScatterUploadBuffer::Init( uint32 NumElements, uint32 InNumBytesPerElement, bool bInFloat4Buffer, const TCHAR* DebugName )
{
	NumScatters = 0;
	MaxScatters = NumElements;
	NumBytesPerElement = InNumBytesPerElement;
	bFloat4Buffer = bInFloat4Buffer;

	const EBufferUsageFlags Usage = bInFloat4Buffer ? BUF_None : BUF_ByteAddressBuffer;
	const uint32 TypeSize = bInFloat4Buffer ? 16 : 4;

	uint32 ScatterBytes = NumElements * sizeof( uint32 );
	uint32 ScatterBufferSize = FMath::RoundUpToPowerOfTwo( ScatterBytes );
	
	uint32 UploadBytes = NumElements * NumBytesPerElement;
	uint32 UploadBufferSize = FMath::RoundUpToPowerOfTwo( UploadBytes );

	if (bUploadViaCreate)
	{
		if (ScatterBytes > ScatterDataSize || ScatterBufferSize < ScatterDataSize / 2)
		{
			FMemory::Free(ScatterData);
			ScatterData = (uint32*)FMemory::Malloc(ScatterBufferSize);
			ScatterDataSize = ScatterBufferSize;
		}

		if (UploadBytes > UploadDataSize || UploadBufferSize < UploadDataSize / 2)
		{
			FMemory::Free(UploadData);
			UploadData = (uint8*)FMemory::Malloc(UploadBufferSize);
			UploadDataSize = UploadBufferSize;
		}
	}
	else
	{
		check(ScatterData == nullptr);
		check(UploadData == nullptr);

		if (ScatterBytes > ScatterBuffer.NumBytes || ScatterBufferSize < ScatterBuffer.NumBytes / 2)
		{
			// Resize Scatter Buffer
			ScatterBuffer.Release();
			ScatterBuffer.NumBytes = ScatterBufferSize;

			FRHIResourceCreateInfo CreateInfo(DebugName);
			ScatterBuffer.Buffer = RHICreateStructuredBuffer(sizeof(uint32), ScatterBuffer.NumBytes, BUF_ShaderResource | BUF_Volatile | Usage, CreateInfo);
			ScatterBuffer.SRV = RHICreateShaderResourceView(ScatterBuffer.Buffer);
		}

		if (UploadBytes > UploadBuffer.NumBytes || UploadBufferSize < UploadBuffer.NumBytes / 2)
		{
			// Resize Upload Buffer
			UploadBuffer.Release();
			UploadBuffer.NumBytes = UploadBufferSize;

			FRHIResourceCreateInfo CreateInfo(DebugName);
			UploadBuffer.Buffer = RHICreateStructuredBuffer(TypeSize, UploadBuffer.NumBytes, BUF_ShaderResource | BUF_Volatile | Usage, CreateInfo);
			UploadBuffer.SRV = RHICreateShaderResourceView(UploadBuffer.Buffer);
		}

		ScatterData = (uint32*)RHILockBuffer(ScatterBuffer.Buffer, 0, ScatterBytes, RLM_WriteOnly);
		UploadData = (uint8*)RHILockBuffer(UploadBuffer.Buffer, 0, UploadBytes, RLM_WriteOnly);
	}
}

void FScatterUploadBuffer::Init(TArrayView<const uint32> ElementScatterOffsets, uint32 InNumBytesPerElement, bool bInFloat4Buffer, const TCHAR* DebugName)
{
	Init(ElementScatterOffsets.Num(), InNumBytesPerElement, bInFloat4Buffer, DebugName);
	FMemory::ParallelMemcpy(ScatterData, ElementScatterOffsets.GetData(), ElementScatterOffsets.Num() * ElementScatterOffsets.GetTypeSize(), EMemcpyCachePolicy::StoreUncached);
	NumScatters = ElementScatterOffsets.Num();
}

void FScatterUploadBuffer::InitPreSized(uint32 NumElements, uint32 InNumBytesPerElement, bool bInFloat4Buffer, const TCHAR* DebugName)
{
	Init(NumElements, InNumBytesPerElement, bInFloat4Buffer, DebugName);
	NumScatters = NumElements;
}

// Helper type used to initialize the buffer data on creation
struct FScatterUploadBufferResourceArray : public FResourceArrayInterface
{
	const void* const DataPtr;
	const int32 DataSize;

	FScatterUploadBufferResourceArray(void* InDataPtr, int32 InDataSize)
		: DataPtr(InDataPtr)
		, DataSize(InDataSize)
	{
	}

	const void* GetResourceData() const override { return DataPtr; }
	uint32 GetResourceDataSize() const override { return DataSize; }

	// Not necessary for our purposes
	void Discard() override { }
	bool IsStatic() const override { return false; }
	bool GetAllowCPUAccess() const override { return true; }
	void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }
};

template<typename ResourceType>
void FScatterUploadBuffer::ResourceUploadTo(FRHICommandList& RHICmdList, const ResourceType& DstBuffer, bool bFlush)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FScatterUploadBuffer::ResourceUploadTo);

	if (bUploadViaCreate)
	{
		ScatterBuffer.Release();
		UploadBuffer.Release();

		ScatterBuffer.NumBytes = ScatterDataSize;
		UploadBuffer.NumBytes = UploadDataSize;

		const uint32 TypeSize = bFloat4Buffer ? 16 : 4;
		const EBufferUsageFlags Usage = bFloat4Buffer ? BUF_None : BUF_ByteAddressBuffer;

		{
			FScatterUploadBufferResourceArray ScatterResourceArray(ScatterData, ScatterDataSize);
			FRHIResourceCreateInfo CreateInfo(TEXT("ScatterResourceArray"), &ScatterResourceArray);
			ScatterBuffer.Buffer = RHICreateStructuredBuffer(sizeof(uint32), ScatterDataSize, BUF_ShaderResource | BUF_Volatile | Usage, CreateInfo);
			ScatterBuffer.SRV = RHICreateShaderResourceView(ScatterBuffer.Buffer);
		}
		{
			FScatterUploadBufferResourceArray UploadResourceArray(UploadData, UploadDataSize);
			FRHIResourceCreateInfo CreateInfo(TEXT("ScatterUploadBuffer"), &UploadResourceArray);
			UploadBuffer.Buffer = RHICreateStructuredBuffer(TypeSize, UploadDataSize, BUF_ShaderResource | BUF_Volatile | Usage, CreateInfo);
			UploadBuffer.SRV = RHICreateShaderResourceView(UploadBuffer.Buffer);
		}
	}
	else
	{
		RHIUnlockBuffer(ScatterBuffer.Buffer);
		RHIUnlockBuffer(UploadBuffer.Buffer);

		ScatterData = nullptr;
		UploadData = nullptr;
	}

	if (NumScatters == 0)
		return;

	constexpr uint32 ThreadGroupSize = 64u;
	uint32 NumBytesPerThread = (NumBytesPerElement & 15) == 0 ? 16 : 4;
	uint32 NumThreadsPerScatter = NumBytesPerElement / NumBytesPerThread;
	uint32 NumThreads = NumScatters * NumThreadsPerScatter;
	uint32 NumDispatches = FMath::DivideAndRoundUp(NumThreads, ThreadGroupSize);
	uint32 NumLoops = FMath::DivideAndRoundUp(NumDispatches, (uint32)GMaxComputeDispatchDimension);

	EByteBufferResourceType ResourceTypeEnum;

	FScatterCopyCS::FParameters Parameters;
	Parameters.Common.Size = NumThreadsPerScatter;
	Parameters.NumScatters = NumScatters;

	check(bFloat4Buffer || ResourceTypeTraits<ResourceType>::Type == EResourceType::BYTEBUFFER);

	if (ResourceTypeTraits<ResourceType>::Type == EResourceType::BYTEBUFFER)
	{
		if (NumBytesPerThread == 16)
		{
			ResourceTypeEnum = EByteBufferResourceType::Uint4Aligned_Buffer;
		}
		else
		{
			ResourceTypeEnum = EByteBufferResourceType::Uint_Buffer;
		}
		Parameters.UploadByteAddressBuffer = UploadBuffer.SRV;
		Parameters.ScatterByteAddressBuffer = ScatterBuffer.SRV;
		Parameters.Common.DstByteAddressBuffer = DstBuffer.UAV;
	}
	else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::STRUCTURED_BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_StructuredBuffer;

		Parameters.UploadStructuredBuffer = UploadBuffer.SRV;
		Parameters.ScatterStructuredBuffer = ScatterBuffer.SRV;
		Parameters.Common.DstStructuredBuffer = DstBuffer.UAV;
	}
	else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::BUFFER)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Buffer;

		Parameters.UploadStructuredBuffer = UploadBuffer.SRV;
		Parameters.ScatterStructuredBuffer = ScatterBuffer.SRV;
		Parameters.Common.DstBuffer = DstBuffer.UAV;
	}
	else if (ResourceTypeTraits<ResourceType>::Type == EResourceType::TEXTURE)
	{
		ResourceTypeEnum = EByteBufferResourceType::Float4_Texture;

		Parameters.UploadStructuredBuffer = UploadBuffer.SRV;
		Parameters.ScatterStructuredBuffer = ScatterBuffer.SRV;
		Parameters.Common.DstTexture = DstBuffer.UAV;

		Parameters.Common.Float4sPerLine = CalculateFloat4sPerLine();
	}

	FByteBufferShader::FPermutationDomain PermutationVector;
	PermutationVector.Set<FByteBufferShader::ResourceTypeDim>((int)ResourceTypeEnum);

	auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FScatterCopyCS>(PermutationVector);

	RHICmdList.BeginUAVOverlap(DstBuffer.UAV);

	for (uint32 LoopIdx = 0; LoopIdx < NumLoops; ++LoopIdx)
	{
		Parameters.Common.SrcOffset = LoopIdx * (uint32)GMaxComputeDispatchDimension * ThreadGroupSize;

		uint32 LoopNumDispatch = FMath::Min(NumDispatches - LoopIdx * (uint32)GMaxComputeDispatchDimension, (uint32)GMaxComputeDispatchDimension);

		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Parameters, FIntVector(LoopNumDispatch, 1, 1));
	}

	RHICmdList.EndUAVOverlap(DstBuffer.UAV);

	// We need to unbind shader SRVs in this case, because scatter upload buffers are sometimes used more than once in a
	// frame, and this can cause rendering bugs on D3D12, where the driver fails to update the bound SRV with new data.
	UnsetShaderSRVs(RHICmdList, ComputeShader, ComputeShader.GetComputeShader());

	if (bFlush)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}
}

template RENDERCORE_API void MemsetResource<FRWBufferStructured>(FRHICommandList& RHICmdList, const FRWBufferStructured& DstBuffer, const FMemsetResourceParams& Params);
template RENDERCORE_API void MemsetResource<FRWByteAddressBuffer>(FRHICommandList& RHICmdList, const FRWByteAddressBuffer& DstBuffer, const FMemsetResourceParams& Params);

template RENDERCORE_API void MemcpyResource<FTextureRWBuffer>(FRHICommandList& RHICmdList, const FTextureRWBuffer& DstBuffer, const FTextureRWBuffer& SrcBuffer, const FMemcpyResourceParams& Params, bool bAlreadyInUAVOverlap);
template RENDERCORE_API void MemcpyResource<FRWBuffer>(FRHICommandList& RHICmdList, const FRWBuffer& DstBuffer, const FRWBuffer& SrcBuffer, const FMemcpyResourceParams& Params, bool bAlreadyInUAVOverlap);
template RENDERCORE_API void MemcpyResource<FRWBufferStructured>(FRHICommandList& RHICmdList, const FRWBufferStructured& DstBuffer, const FRWBufferStructured& SrcBuffer, const FMemcpyResourceParams& Params, bool bAlreadyInUAVOverlap);
template RENDERCORE_API void MemcpyResource<FRWByteAddressBuffer>(FRHICommandList& RHICmdList, const FRWByteAddressBuffer& DstBuffer, const FRWByteAddressBuffer& SrcBuffer, const FMemcpyResourceParams& Params, bool bAlreadyInUAVOverlap);

template RENDERCORE_API void FScatterUploadBuffer::ResourceUploadTo<FTextureRWBuffer>(FRHICommandList& RHICmdList, const FTextureRWBuffer& DstBuffer, bool bFlush);
template RENDERCORE_API void FScatterUploadBuffer::ResourceUploadTo<FRWBuffer>(FRHICommandList& RHICmdList, const FRWBuffer& DstBuffer, bool bFlush);
template RENDERCORE_API void FScatterUploadBuffer::ResourceUploadTo<FRWBufferStructured>(FRHICommandList& RHICmdList, const FRWBufferStructured& DstBuffer, bool bFlush);
template RENDERCORE_API void FScatterUploadBuffer::ResourceUploadTo<FRWByteAddressBuffer>(FRHICommandList& RHICmdList, const FRWByteAddressBuffer& DstBuffer, bool bFlush);