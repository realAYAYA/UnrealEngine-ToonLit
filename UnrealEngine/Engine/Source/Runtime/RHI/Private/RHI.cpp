// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHI.cpp: Render Hardware Interface implementation.
=============================================================================*/

#include "RHI.h"
#include "RHITransientResourceAllocator.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "RHIShaderFormatDefinitions.inl"
#include "ProfilingDebugging/CsvProfiler.h"
#include "String/Find.h"
#include "String/LexFromString.h"
#include "String/ParseTokens.h"
#include "Misc/BufferedOutputDevice.h"
#include "Misc/OutputDeviceFile.h"

#if RHI_ENABLE_RESOURCE_INFO
#include "HAL/FileManager.h"
#endif

IMPLEMENT_MODULE(FDefaultModuleImpl, RHI);

/** RHI Logging. */
DEFINE_LOG_CATEGORY(LogRHI);
CSV_DEFINE_CATEGORY(RHI, true);

#if UE_BUILD_SHIPPING
CSV_DEFINE_CATEGORY(DrawCall, false);
#else
CSV_DEFINE_CATEGORY(DrawCall, true);
#endif

// Define counter stats.
DEFINE_STAT(STAT_RHIDrawPrimitiveCalls);
DEFINE_STAT(STAT_RHITriangles);
DEFINE_STAT(STAT_RHILines);

// Define memory stats.
DEFINE_STAT(STAT_RenderTargetMemory2D);
DEFINE_STAT(STAT_RenderTargetMemory3D);
DEFINE_STAT(STAT_RenderTargetMemoryCube);
DEFINE_STAT(STAT_TextureMemory2D);
DEFINE_STAT(STAT_TextureMemory3D);
DEFINE_STAT(STAT_TextureMemoryCube);
DEFINE_STAT(STAT_UniformBufferMemory);
DEFINE_STAT(STAT_IndexBufferMemory);
DEFINE_STAT(STAT_VertexBufferMemory);
DEFINE_STAT(STAT_RTAccelerationStructureMemory);
DEFINE_STAT(STAT_StructuredBufferMemory);
DEFINE_STAT(STAT_PixelBufferMemory);

IMPLEMENT_TYPE_LAYOUT(FRHIUniformBufferLayoutInitializer);
IMPLEMENT_TYPE_LAYOUT(FRHIUniformBufferResource);

#if !defined(RHIRESOURCE_NUM_FRAMES_TO_EXPIRE)
	#define RHIRESOURCE_NUM_FRAMES_TO_EXPIRE 3
#endif

static TAutoConsoleVariable<int32> CVarDisableEngineAndAppRegistration(
	TEXT("r.DisableEngineAndAppRegistration"),
	0,
	TEXT("If true, disables engine and app registration, to disable GPU driver optimizations during debugging and development\n")
	TEXT("Changes will only take effect in new game/editor instances - can't be changed at runtime.\n"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarGraphicsAdapter(
	TEXT("r.GraphicsAdapter"),
	-1,
	TEXT("User request to pick a specific graphics adapter (e.g. when using a integrated graphics card with a discrete one)\n")
	TEXT("For Windows D3D, unless a specific adapter is chosen we reject Microsoft adapters because we don't want the software emulation.\n")
	TEXT("This takes precedence over -prefer{AMD|NVidia|Intel} when the value is >= 0.\n")
	TEXT(" -2: Take the first one that fulfills the criteria\n")
	TEXT(" -1: Favour non integrated because there are usually faster (default)\n")
	TEXT("  0: Adapter #0\n")
	TEXT("  1: Adapter #1, ..."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

template<typename EnumType>
inline FString BuildEnumNameBitList(EnumType Value, const TCHAR*(*GetEnumName)(EnumType))
{
	if (Value == EnumType(0))
	{
		return GetEnumName(Value);
	}

	using T = __underlying_type(EnumType);
	T StateValue = (T)Value;

	FString Name;

	int32 BitIndex = 0;
	while (StateValue)
	{
		if (StateValue & 1)
		{
			if (Name.Len() > 0 && StateValue > 0)
			{
				Name += TEXT("|");
			}

			Name += GetEnumName(EnumType(T(1) << BitIndex));
		}

		BitIndex++;
		StateValue >>= 1;
	}

	return MoveTemp(Name);
}

FString GetRHIAccessName(ERHIAccess Access)
{
		return BuildEnumNameBitList<ERHIAccess>(Access, [](ERHIAccess AccessBit)
		{
			switch (AccessBit)
			{
			default: checkNoEntry(); // fall through
			case ERHIAccess::Unknown:             return TEXT("Unknown");
			case ERHIAccess::CPURead:             return TEXT("CPURead");
			case ERHIAccess::Present:             return TEXT("Present");
			case ERHIAccess::IndirectArgs:        return TEXT("IndirectArgs");
			case ERHIAccess::VertexOrIndexBuffer: return TEXT("VertexOrIndexBuffer");
			case ERHIAccess::SRVCompute:          return TEXT("SRVCompute");
			case ERHIAccess::SRVGraphics:         return TEXT("SRVGraphics");
			case ERHIAccess::CopySrc:             return TEXT("CopySrc");
			case ERHIAccess::ResolveSrc:          return TEXT("ResolveSrc");
			case ERHIAccess::DSVRead:             return TEXT("DSVRead");
			case ERHIAccess::UAVCompute:          return TEXT("UAVCompute");
			case ERHIAccess::UAVGraphics:         return TEXT("UAVGraphics");
			case ERHIAccess::RTV:                 return TEXT("RTV");
			case ERHIAccess::CopyDest:            return TEXT("CopyDest");
			case ERHIAccess::ResolveDst:          return TEXT("ResolveDst");
			case ERHIAccess::DSVWrite:            return TEXT("DSVWrite");
			case ERHIAccess::ShadingRateSource:	  return TEXT("ShadingRateSource");
			case ERHIAccess::BVHRead:             return TEXT("BVHRead");
			case ERHIAccess::BVHWrite:            return TEXT("BVHWrite");
			case ERHIAccess::Discard:             return TEXT("Discard");
			}
		});
}

FString GetResourceTransitionFlagsName(EResourceTransitionFlags Flags)
{
	return BuildEnumNameBitList<EResourceTransitionFlags>(Flags, [](EResourceTransitionFlags Value)
	{
		switch (Value)
		{
		default: checkNoEntry(); // fall through
		case EResourceTransitionFlags::None:                return TEXT("None");
		case EResourceTransitionFlags::MaintainCompression: return TEXT("MaintainCompression");
		case EResourceTransitionFlags::Discard:				return TEXT("Discard");
		case EResourceTransitionFlags::Clear:				return TEXT("Clear");
		}
	});
}

FString GetRHIPipelineName(ERHIPipeline Pipeline)
{
	return BuildEnumNameBitList<ERHIPipeline>(Pipeline, [](ERHIPipeline Value)
	{
		if (Value == ERHIPipeline(0)) { return TEXT("None"); }

		switch (Value)
		{
		default: checkNoEntry(); // fall through
		case ERHIPipeline::Graphics:     return TEXT("Graphics");
		case ERHIPipeline::AsyncCompute: return TEXT("AsyncCompute");
		}
	});
}

FString GetBufferUsageFlagsName(EBufferUsageFlags BufferUsage)
{
	return BuildEnumNameBitList<EBufferUsageFlags>(BufferUsage, [](EBufferUsageFlags BufferUsage)
	{
		switch (BufferUsage)
		{
		default: checkNoEntry(); // fall through
		case BUF_None:					return TEXT("BUF_None");
		case BUF_Static:				return TEXT("BUF_Static");
		case BUF_Dynamic:				return TEXT("BUF_Dynamic");
		case BUF_Volatile:				return TEXT("BUF_Volitile");
		case BUF_UnorderedAccess:		return TEXT("BUF_UnorderedAccess");
		case BUF_ByteAddressBuffer:		return TEXT("BUF_ByteAddressBuffer");
		case BUF_SourceCopy:			return TEXT("BUF_SourceCopy");
		case BUF_StreamOutput:			return TEXT("BUF_StreamOutput");
		case BUF_DrawIndirect:			return TEXT("BUF_DrawIndirect");
		case BUF_ShaderResource:		return TEXT("BUF_ShaderResource");
		case BUF_KeepCPUAccessible:		return TEXT("BUF_KeepCPUAccessible");
		case BUF_FastVRAM:				return TEXT("BUF_FastVRAM");
		case BUF_Shared:				return TEXT("BUF_Shared");
		case BUF_AccelerationStructure:	return TEXT("BUF_AccelerationStructure");
		case BUF_RayTracingScratch:		return TEXT("BUF_RayTracingScratch");
		case BUF_VertexBuffer:			return TEXT("BUF_VertexBuffer");
		case BUF_IndexBuffer:			return TEXT("BUF_IndexBuffer");
		case BUF_StructuredBuffer:		return TEXT("BUF_StructuredBuffer");
		}
	});
}

FString GetTextureCreateFlagsName(ETextureCreateFlags TextureCreateFlags)
{
	return BuildEnumNameBitList<ETextureCreateFlags>(TextureCreateFlags, [](ETextureCreateFlags TextureCreateFlags)
	{
		switch (TextureCreateFlags)
		{
		default: checkNoEntry(); // fall through
		case TexCreate_None:							return TEXT("TexCreate_None");
		case TexCreate_RenderTargetable:				return TEXT("TexCreate_RenderTargetable");
		case TexCreate_ResolveTargetable:				return TEXT("TexCreate_ResolveTargetable");
		case TexCreate_DepthStencilTargetable:			return TEXT("TexCreate_DepthStencilTargetable");
		case TexCreate_ShaderResource:					return TEXT("TexCreate_ShaderResource");
		case TexCreate_SRGB:							return TEXT("TexCreate_SRGB");
		case TexCreate_CPUWritable:						return TEXT("TexCreate_CPUWritable");
		case TexCreate_NoTiling:						return TEXT("TexCreate_NoTiling");
		case TexCreate_VideoDecode:						return TEXT("TexCreate_VideoDecode");
		case TexCreate_Dynamic:							return TEXT("TexCreate_Dynamic");
		case TexCreate_InputAttachmentRead:				return TEXT("TexCreate_InputAttachmentRead");
		case TexCreate_Foveation:						return TEXT("TexCreate_Foveation");
		case TexCreate_3DTiling:						return TEXT("TexCreate_3DTiling");
		case TexCreate_Memoryless:						return TEXT("TexCreate_Memoryless");
		case TexCreate_GenerateMipCapable:				return TEXT("TexCreate_GenerateMipCapable");
		case TexCreate_FastVRAMPartialAlloc:			return TEXT("TexCreate_FastVRAMPartialAlloc");
		case TexCreate_DisableSRVCreation:				return TEXT("TexCreate_DisableSRVCreation");
		case TexCreate_DisableDCC:						return TEXT("TexCreate_DisableDCC");
		case TexCreate_UAV:								return TEXT("TexCreate_UAV");
		case TexCreate_Presentable:						return TEXT("TexCreate_Presentable");
		case TexCreate_CPUReadback:						return TEXT("TexCreate_CPUReadback");
		case TexCreate_OfflineProcessed:				return TEXT("TexCreate_OfflineProcessed");
		case TexCreate_FastVRAM:						return TEXT("TexCreate_FastVRAM");
		case TexCreate_HideInVisualizeTexture:			return TEXT("TexCreate_HideInVisualizeTexture");
		case TexCreate_Virtual:							return TEXT("TexCreate_Virtual");
		case TexCreate_TargetArraySlicesIndependently:	return TEXT("TexCreate_TargetArraySlicesIndependently");
		case TexCreate_Shared:							return TEXT("TexCreate_Shared");
		case TexCreate_NoFastClear:						return TEXT("TexCreate_NoFastClear");
		case TexCreate_DepthStencilResolveTarget:		return TEXT("TexCreate_DepthStencilResolveTarget");
		case TexCreate_Streamable:						return TEXT("TexCreate_Streamable");
		case TexCreate_NoFastClearFinalize:				return TEXT("TexCreate_NoFastClearFinalize");
		case TexCreate_AFRManual:						return TEXT("TexCreate_AFRManual");
		case TexCreate_ReduceMemoryWithTilingMode:		return TEXT("TexCreate_ReduceMemoryWithTilingMode");
		case TexCreate_AtomicCompatible:				return TEXT("TexCreate_AtomicCompatible");
		case TexCreate_External:						return TEXT("TexCreate_External");
		}
	});
}

#if STATS
#include "Stats/StatsData.h"
static void DumpRHIMemory(FOutputDevice& OutputDevice)
{
	TArray<FStatMessage> Stats;
	GetPermanentStats(Stats);

	FName NAME_STATGROUP_RHI(FStatGroup_STATGROUP_RHI::GetGroupName());
	OutputDevice.Logf(TEXT("RHI resource memory (not tracked by our allocator)"));
	int64 TotalMemory = 0;
	for (int32 Index = 0; Index < Stats.Num(); Index++)
	{
		FStatMessage const& Meta = Stats[Index];
		FName LastGroup = Meta.NameAndInfo.GetGroupName();
		if (LastGroup == NAME_STATGROUP_RHI && Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
		{
			OutputDevice.Logf(TEXT("%s"), *FStatsUtils::DebugPrint(Meta));
			TotalMemory += Meta.GetValue_int64();
		}
	}
	OutputDevice.Logf(TEXT("%.3fMB total"), TotalMemory / 1024.f / 1024.f);
}

static FAutoConsoleCommandWithOutputDevice GDumpRHIMemoryCmd(
	TEXT("rhi.DumpMemory"),
	TEXT("Dumps RHI memory stats to the log"),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic(DumpRHIMemory)
	);
#endif

//DO NOT USE THE STATIC FLINEARCOLORS TO INITIALIZE THIS STUFF.  
//Static init order is undefined and you will likely end up with bad values on some platforms.
const FClearValueBinding FClearValueBinding::None(EClearBinding::ENoneBound);
const FClearValueBinding FClearValueBinding::Black(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
const FClearValueBinding FClearValueBinding::BlackMaxAlpha(FLinearColor(0.0f, 0.0f, 0.0f, FLT_MAX));
const FClearValueBinding FClearValueBinding::White(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
const FClearValueBinding FClearValueBinding::Transparent(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
const FClearValueBinding FClearValueBinding::DepthOne(1.0f, 0);
const FClearValueBinding FClearValueBinding::DepthZero(0.0f, 0);
const FClearValueBinding FClearValueBinding::DepthNear((float)ERHIZBuffer::NearPlane, 0);
const FClearValueBinding FClearValueBinding::DepthFar((float)ERHIZBuffer::FarPlane, 0);
const FClearValueBinding FClearValueBinding::Green(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
// Note: this is used as the default normal for DBuffer decals.  It must decode to a value of 0 in DecodeDBufferData.
const FClearValueBinding FClearValueBinding::DefaultNormal8Bit(FLinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f));

std::atomic<TClosableMpscQueue<FRHIResource*>*> FRHIResource::PendingDeletes { new TClosableMpscQueue<FRHIResource*>() };
FHazardPointerCollection FRHIResource::PendingDeletesHPC;
FRHIResource* FRHIResource::CurrentlyDeleting = nullptr;

RHI_API FDrawCallCategoryName* FDrawCallCategoryName::Array[FDrawCallCategoryName::MAX_DRAWCALL_CATEGORY];
RHI_API int32 FDrawCallCategoryName::DisplayCounts[FDrawCallCategoryName::MAX_DRAWCALL_CATEGORY][MAX_NUM_GPUS];
RHI_API int32 FDrawCallCategoryName::NumCategory = 0;

TRefCountPtr<FRHITexture> FRHITextureReference::DefaultTexture;

FString FVertexElement::ToString() const
{
	return FString::Printf(TEXT("<%u %u %u %u %u %u>")
		, uint32(StreamIndex)
		, uint32(Offset)
		, uint32(Type)
		, uint32(AttributeIndex)
		, uint32(Stride)
		, uint32(bUseInstanceIndex)
	);
}

void FVertexElement::FromString(const FString& InSrc)
{
	FromString(FStringView(InSrc));
}

void FVertexElement::FromString(const FStringView& InSrc)
{
	constexpr int32 PartCount = 6;

	TArray<FStringView, TInlineAllocator<PartCount>> Parts;
	UE::String::ParseTokensMultiple(InSrc.TrimStartAndEnd(), {TEXT('\r'), TEXT('\n'), TEXT('\t'), TEXT('<'), TEXT('>'), TEXT(' ')},
		[&Parts](FStringView Part) { if (!Part.IsEmpty()) { Parts.Add(Part); } });

	check(Parts.Num() == PartCount && sizeof(Type) == 1); //not a very robust parser
	const FStringView* PartIt = Parts.GetData();
	LexFromString(StreamIndex, *PartIt++);
	LexFromString(Offset, *PartIt++);
	LexFromString((uint8&)Type, *PartIt++);
	LexFromString(AttributeIndex, *PartIt++);
	LexFromString(Stride, *PartIt++);
	LexFromString(bUseInstanceIndex, *PartIt++);
	check(Parts.GetData() + PartCount == PartIt);
}

uint32 GetTypeHash(const FSamplerStateInitializerRHI& Initializer)
{
	uint32 Hash = GetTypeHash(Initializer.Filter);
	Hash = HashCombine(Hash, GetTypeHash(Initializer.AddressU));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.AddressV));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.AddressW));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.MipBias));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.MinMipLevel));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.MaxMipLevel));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.MaxAnisotropy));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.BorderColor));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.SamplerComparisonFunction));
	return Hash;
}

bool operator== (const FSamplerStateInitializerRHI& A, const FSamplerStateInitializerRHI& B)
{
	bool bSame = 
		A.Filter == B.Filter &&
		A.AddressU == B.AddressU &&
		A.AddressV == B.AddressV &&
		A.AddressW == B.AddressW &&
		A.MipBias == B.MipBias &&
		A.MinMipLevel == B.MinMipLevel &&
		A.MaxMipLevel == B.MaxMipLevel &&
		A.MaxAnisotropy == B.MaxAnisotropy &&
		A.BorderColor == B.BorderColor &&
		A.SamplerComparisonFunction == B.SamplerComparisonFunction;
	return bSame;
}

uint32 GetTypeHash(const FRasterizerStateInitializerRHI& Initializer)
{
	uint32 Hash = GetTypeHash(Initializer.FillMode);
	Hash = HashCombine(Hash, GetTypeHash(Initializer.CullMode));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.DepthBias));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.SlopeScaleDepthBias));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.DepthClipMode));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.bAllowMSAA));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.bEnableLineAA));
	return Hash;
}
	
bool operator== (const FRasterizerStateInitializerRHI& A, const FRasterizerStateInitializerRHI& B)
{
	bool bSame = 
		A.FillMode == B.FillMode && 
		A.CullMode == B.CullMode && 
		A.DepthBias == B.DepthBias && 
		A.SlopeScaleDepthBias == B.SlopeScaleDepthBias &&
		A.DepthClipMode == B.DepthClipMode &&
		A.bAllowMSAA == B.bAllowMSAA && 
		A.bEnableLineAA == B.bEnableLineAA;
	return bSame;
}

uint32 GetTypeHash(const FDepthStencilStateInitializerRHI& Initializer)
{
	uint32 Hash = GetTypeHash(Initializer.bEnableDepthWrite);
	Hash = HashCombine(Hash, GetTypeHash(Initializer.DepthTest));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.bEnableFrontFaceStencil));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFaceStencilTest));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFaceStencilFailStencilOp));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFaceDepthFailStencilOp));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.FrontFacePassStencilOp));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.bEnableBackFaceStencil));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFaceStencilTest));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFaceStencilFailStencilOp));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFaceDepthFailStencilOp));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.BackFacePassStencilOp));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.StencilReadMask));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.StencilWriteMask));
	return Hash;
}

bool operator== (const FDepthStencilStateInitializerRHI& A, const FDepthStencilStateInitializerRHI& B)
{
	bool bSame = 
		A.bEnableDepthWrite == B.bEnableDepthWrite && 
		A.DepthTest == B.DepthTest && 
		A.bEnableFrontFaceStencil == B.bEnableFrontFaceStencil && 
		A.FrontFaceStencilTest == B.FrontFaceStencilTest && 
		A.FrontFaceStencilFailStencilOp == B.FrontFaceStencilFailStencilOp && 
		A.FrontFaceDepthFailStencilOp == B.FrontFaceDepthFailStencilOp && 
		A.FrontFacePassStencilOp == B.FrontFacePassStencilOp && 
		A.bEnableBackFaceStencil == B.bEnableBackFaceStencil && 
		A.BackFaceStencilTest == B.BackFaceStencilTest && 
		A.BackFaceStencilFailStencilOp == B.BackFaceStencilFailStencilOp && 
		A.BackFaceDepthFailStencilOp == B.BackFaceDepthFailStencilOp && 
		A.BackFacePassStencilOp == B.BackFacePassStencilOp && 
		A.StencilReadMask == B.StencilReadMask && 
		A.StencilWriteMask == B.StencilWriteMask;
	return bSame;
}

FString FDepthStencilStateInitializerRHI::ToString() const
{
	return
		FString::Printf(TEXT("<%u %u ")
			, uint32(!!bEnableDepthWrite)
			, uint32(DepthTest)
		)
		+ FString::Printf(TEXT("%u %u %u %u %u ")
			, uint32(!!bEnableFrontFaceStencil)
			, uint32(FrontFaceStencilTest)
			, uint32(FrontFaceStencilFailStencilOp)
			, uint32(FrontFaceDepthFailStencilOp)
			, uint32(FrontFacePassStencilOp)
		)
		+ FString::Printf(TEXT("%u %u %u %u %u ")
			, uint32(!!bEnableBackFaceStencil)
			, uint32(BackFaceStencilTest)
			, uint32(BackFaceStencilFailStencilOp)
			, uint32(BackFaceDepthFailStencilOp)
			, uint32(BackFacePassStencilOp)
		)
		+ FString::Printf(TEXT("%u %u>")
			, uint32(StencilReadMask)
			, uint32(StencilWriteMask)
		);
}

void FDepthStencilStateInitializerRHI::FromString(const FString& InSrc)
{
	FromString(FStringView(InSrc));
}

void FDepthStencilStateInitializerRHI::FromString(const FStringView& InSrc)
{
	constexpr int32 PartCount = 14;

	TArray<FStringView, TInlineAllocator<PartCount>> Parts;
	UE::String::ParseTokensMultiple(InSrc.TrimStartAndEnd(), {TEXT('\r'), TEXT('\n'), TEXT('\t'), TEXT('<'), TEXT('>'), TEXT(' ')},
		[&Parts](FStringView Part) { if (!Part.IsEmpty()) { Parts.Add(Part); } });

	check(Parts.Num() == PartCount && sizeof(bool) == 1 && sizeof(FrontFaceStencilFailStencilOp) == 1 && sizeof(BackFaceStencilTest) == 1 && sizeof(BackFaceDepthFailStencilOp) == 1); //not a very robust parser

	const FStringView* PartIt = Parts.GetData();

	LexFromString((uint8&)bEnableDepthWrite, *PartIt++);
	LexFromString((uint8&)DepthTest, *PartIt++);

	LexFromString((uint8&)bEnableFrontFaceStencil, *PartIt++);
	LexFromString((uint8&)FrontFaceStencilTest, *PartIt++);
	LexFromString((uint8&)FrontFaceStencilFailStencilOp, *PartIt++);
	LexFromString((uint8&)FrontFaceDepthFailStencilOp, *PartIt++);
	LexFromString((uint8&)FrontFacePassStencilOp, *PartIt++);

	LexFromString((uint8&)bEnableBackFaceStencil, *PartIt++);
	LexFromString((uint8&)BackFaceStencilTest, *PartIt++);
	LexFromString((uint8&)BackFaceStencilFailStencilOp, *PartIt++);
	LexFromString((uint8&)BackFaceDepthFailStencilOp, *PartIt++);
	LexFromString((uint8&)BackFacePassStencilOp, *PartIt++);

	LexFromString(StencilReadMask, *PartIt++);
	LexFromString(StencilWriteMask, *PartIt++);

	check(Parts.GetData() + PartCount == PartIt);
}

FString FBlendStateInitializerRHI::ToString() const
{
	FString Result = TEXT("<");
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		Result += RenderTargets[Index].ToString();
	}
	Result += FString::Printf(TEXT("%d %d>"), uint32(!!bUseIndependentRenderTargetBlendStates), uint32(!!bUseAlphaToCoverage));
	return Result;
}

void FBlendStateInitializerRHI::FromString(const FString& InSrc)
{
	FromString(FStringView(InSrc));
}

void FBlendStateInitializerRHI::FromString(const FStringView& InSrc)
{
	// files written before bUseAlphaToCoverage change (added in CL 13846572) have one less part
	constexpr int32 BackwardCompatiblePartCount = MaxSimultaneousRenderTargets * FRenderTarget::NUM_STRING_FIELDS + 1;
	constexpr int32 PartCount = BackwardCompatiblePartCount + 1;

	TArray<FStringView, TInlineAllocator<PartCount>> Parts;
	UE::String::ParseTokensMultiple(InSrc.TrimStartAndEnd(), {TEXT('\r'), TEXT('\n'), TEXT('\t'), TEXT('<'), TEXT('>'), TEXT(' ')},
		[&Parts](FStringView Part) { if (!Part.IsEmpty()) { Parts.Add(Part); } });

	checkf((Parts.Num() == PartCount || Parts.Num() == BackwardCompatiblePartCount) && sizeof(bool) == 1, 
		TEXT("Expecting %d (or %d, for an older format) parts in the blendstate string, got %d"), PartCount, BackwardCompatiblePartCount, Parts.Num()); //not a very robust parser
	bool bHasAlphaToCoverageField = Parts.Num() == PartCount;

	const FStringView* PartIt = Parts.GetData();
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; Index++)
	{
		RenderTargets[Index].FromString(MakeArrayView(PartIt, FRenderTarget::NUM_STRING_FIELDS));
		PartIt += FRenderTarget::NUM_STRING_FIELDS;
	}
	LexFromString((int8&)bUseIndependentRenderTargetBlendStates, *PartIt++);
	if (bHasAlphaToCoverageField)
	{
		LexFromString((int8&)bUseAlphaToCoverage, *PartIt++);
		check(Parts.GetData() + PartCount == PartIt);
	}
	else
	{
		bUseAlphaToCoverage = false;
		check(Parts.GetData() + BackwardCompatiblePartCount == PartIt);
	}
}

uint32 GetTypeHash(const FBlendStateInitializerRHI& Initializer)
{
	uint32 Hash = GetTypeHash(Initializer.bUseIndependentRenderTargetBlendStates);
	Hash = HashCombine(Hash, Initializer.bUseAlphaToCoverage);
	for (int32 i = 0; i < MaxSimultaneousRenderTargets; ++i)
	{
		Hash = HashCombine(Hash, GetTypeHash(Initializer.RenderTargets[i]));
	}
	
	return Hash;
}

bool operator== (const FBlendStateInitializerRHI& A, const FBlendStateInitializerRHI& B)
{
	bool bSame = A.bUseIndependentRenderTargetBlendStates == B.bUseIndependentRenderTargetBlendStates;
	bSame = bSame && A.bUseAlphaToCoverage == B.bUseAlphaToCoverage;
	for (int32 i = 0; i < MaxSimultaneousRenderTargets && bSame; ++i)
	{
		bSame = bSame && A.RenderTargets[i] == B.RenderTargets[i];
	}
	return bSame;
}


FString FBlendStateInitializerRHI::FRenderTarget::ToString() const
{
	return FString::Printf(TEXT("%u %u %u %u %u %u %u ")
		, uint32(ColorBlendOp)
		, uint32(ColorSrcBlend)
		, uint32(ColorDestBlend)
		, uint32(AlphaBlendOp)
		, uint32(AlphaSrcBlend)
		, uint32(AlphaDestBlend)
		, uint32(ColorWriteMask)
	);
}

void FBlendStateInitializerRHI::FRenderTarget::FromString(const TArray<FString>& Parts, int32 Index)
{
	check(Index + NUM_STRING_FIELDS <= Parts.Num());
	LexFromString((uint8&)ColorBlendOp, *Parts[Index++]);
	LexFromString((uint8&)ColorSrcBlend, *Parts[Index++]);
	LexFromString((uint8&)ColorDestBlend, *Parts[Index++]);
	LexFromString((uint8&)AlphaBlendOp, *Parts[Index++]);
	LexFromString((uint8&)AlphaSrcBlend, *Parts[Index++]);
	LexFromString((uint8&)AlphaDestBlend, *Parts[Index++]);
	LexFromString((uint8&)ColorWriteMask, *Parts[Index++]);
}

void FBlendStateInitializerRHI::FRenderTarget::FromString(TArrayView<const FStringView> Parts)
{
	check(Parts.Num() == NUM_STRING_FIELDS);
	const FStringView* PartIt = Parts.GetData();
	LexFromString((uint8&)ColorBlendOp, *PartIt++);
	LexFromString((uint8&)ColorSrcBlend, *PartIt++);
	LexFromString((uint8&)ColorDestBlend, *PartIt++);
	LexFromString((uint8&)AlphaBlendOp, *PartIt++);
	LexFromString((uint8&)AlphaSrcBlend, *PartIt++);
	LexFromString((uint8&)AlphaDestBlend, *PartIt++);
	LexFromString((uint8&)ColorWriteMask, *PartIt++);
}

uint32 GetTypeHash(const FBlendStateInitializerRHI::FRenderTarget& Initializer)
{
	uint32 Hash = GetTypeHash(Initializer.ColorBlendOp);
	Hash = HashCombine(Hash, GetTypeHash(Initializer.ColorDestBlend));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.ColorSrcBlend));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.AlphaBlendOp));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.AlphaDestBlend));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.AlphaSrcBlend));
	Hash = HashCombine(Hash, GetTypeHash(Initializer.ColorWriteMask));
	return Hash;
}

bool operator==(const FBlendStateInitializerRHI::FRenderTarget& A, const FBlendStateInitializerRHI::FRenderTarget& B)
{
	bool bSame = 
		A.ColorBlendOp == B.ColorBlendOp && 
		A.ColorDestBlend == B.ColorDestBlend && 
		A.ColorSrcBlend == B.ColorSrcBlend && 
		A.AlphaBlendOp == B.AlphaBlendOp && 
		A.AlphaDestBlend == B.AlphaDestBlend && 
		A.AlphaSrcBlend == B.AlphaSrcBlend && 
		A.ColorWriteMask == B.ColorWriteMask;
	return bSame;
}

#if RHI_ENABLE_RESOURCE_INFO

static FCriticalSection GRHIResourceTrackingCriticalSection;
static TSet<FRHIResource*> GRHITrackedResources;
static bool GRHITrackingResources = false;

void FRHIResource::BeginTrackingResource(FRHIResource* InResource)
{
	if (GRHITrackingResources)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/ResourceTracking"));

		FScopeLock Lock(&GRHIResourceTrackingCriticalSection);

		InResource->bBeingTracked = true;

		GRHITrackedResources.Add(InResource);
	}
}

void FRHIResource::EndTrackingResource(FRHIResource* InResource)
{
	if (InResource->bBeingTracked)
	{
		FScopeLock Lock(&GRHIResourceTrackingCriticalSection);
		GRHITrackedResources.Remove(InResource);
		InResource->bBeingTracked = false;
	}
}

void FRHIResource::StartTrackingAllResources()
{
	GRHITrackingResources = true;
}

void FRHIResource::StopTrackingAllResources()
{
	FScopeLock Lock(&GRHIResourceTrackingCriticalSection);
	for (FRHIResource* Resource : GRHITrackedResources)
	{
		if (Resource)
		{
			Resource->bBeingTracked = false;
		}
	}
	GRHITrackedResources.Empty();
	GRHITrackingResources = false;
}

struct FRHIResourceTypeName
{
	ERHIResourceType Type;
	const TCHAR* Name;
};
static const FRHIResourceTypeName GRHIResourceTypeNames[] =
{
#define RHI_RESOURCE_TYPE_DEF(Name) { RRT_##Name, TEXT(#Name) }

	RHI_RESOURCE_TYPE_DEF(None),
	RHI_RESOURCE_TYPE_DEF(SamplerState),
	RHI_RESOURCE_TYPE_DEF(RasterizerState),
	RHI_RESOURCE_TYPE_DEF(DepthStencilState),
	RHI_RESOURCE_TYPE_DEF(BlendState),
	RHI_RESOURCE_TYPE_DEF(VertexDeclaration),
	RHI_RESOURCE_TYPE_DEF(VertexShader),
	RHI_RESOURCE_TYPE_DEF(MeshShader),
	RHI_RESOURCE_TYPE_DEF(AmplificationShader),
	RHI_RESOURCE_TYPE_DEF(PixelShader),
	RHI_RESOURCE_TYPE_DEF(GeometryShader),
	RHI_RESOURCE_TYPE_DEF(RayTracingShader),
	RHI_RESOURCE_TYPE_DEF(ComputeShader),
	RHI_RESOURCE_TYPE_DEF(GraphicsPipelineState),
	RHI_RESOURCE_TYPE_DEF(ComputePipelineState),
	RHI_RESOURCE_TYPE_DEF(RayTracingPipelineState),
	RHI_RESOURCE_TYPE_DEF(BoundShaderState),
	RHI_RESOURCE_TYPE_DEF(UniformBufferLayout),
	RHI_RESOURCE_TYPE_DEF(UniformBuffer),
	RHI_RESOURCE_TYPE_DEF(Buffer),
	RHI_RESOURCE_TYPE_DEF(Texture),
	RHI_RESOURCE_TYPE_DEF(Texture2D),
	RHI_RESOURCE_TYPE_DEF(Texture2DArray),
	RHI_RESOURCE_TYPE_DEF(Texture3D),
	RHI_RESOURCE_TYPE_DEF(TextureCube),
	RHI_RESOURCE_TYPE_DEF(TextureReference),
	RHI_RESOURCE_TYPE_DEF(TimestampCalibrationQuery),
	RHI_RESOURCE_TYPE_DEF(GPUFence),
	RHI_RESOURCE_TYPE_DEF(RenderQuery),
	RHI_RESOURCE_TYPE_DEF(RenderQueryPool),
	RHI_RESOURCE_TYPE_DEF(ComputeFence),
	RHI_RESOURCE_TYPE_DEF(Viewport),
	RHI_RESOURCE_TYPE_DEF(UnorderedAccessView),
	RHI_RESOURCE_TYPE_DEF(ShaderResourceView),
	RHI_RESOURCE_TYPE_DEF(RayTracingAccelerationStructure),
	RHI_RESOURCE_TYPE_DEF(StagingBuffer),
	RHI_RESOURCE_TYPE_DEF(CustomPresent),
	RHI_RESOURCE_TYPE_DEF(ShaderLibrary),
	RHI_RESOURCE_TYPE_DEF(PipelineBinaryLibrary),
};

static ERHIResourceType RHIResourceTypeFromString(const FString& InString)
{
	for (const auto& TypeName : GRHIResourceTypeNames)
	{
		if (InString.Equals(TypeName.Name, ESearchCase::IgnoreCase))
		{
			return TypeName.Type;
		}
	}
	return RRT_None;
}

const TCHAR* StringFromRHIResourceType(ERHIResourceType ResourceType)
{
	for (const auto& TypeName : GRHIResourceTypeNames)
	{
		if (TypeName.Type == ResourceType)
		{
			return TypeName.Name;
		}
	}
	return TEXT("<unknown>");
}

enum class EBooleanFilter
{
	No,
	Yes,
	All
};

static EBooleanFilter ParseBooleanFilter(const FString& InText)
{
	if (InText.Equals(TEXT("No"), ESearchCase::IgnoreCase))
	{
		return EBooleanFilter::No;
	}
	if (InText.Equals(TEXT("Yes"), ESearchCase::IgnoreCase))
	{
		return EBooleanFilter::Yes;
	}
	if (InText.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		return EBooleanFilter::All;
	}
	return EBooleanFilter::No;
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDumpRHIResourceCountsCmd(
	TEXT("rhi.DumpResourceCounts"),
	TEXT("Dumps RHI resource counts to the log"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld*, FOutputDevice& OutputDevice)
{
	int32 ResourceCounts[RRT_Num]{};
	int32 TotalResources{};

	FRHIResourceInfo ResourceInfo;

	{
		FScopeLock Lock(&GRHIResourceTrackingCriticalSection);

		TotalResources = GRHITrackedResources.Num();

		for (const FRHIResource* Resource : GRHITrackedResources)
		{
			if (Resource)
			{
				ERHIResourceType ResourceType = Resource->GetType();
				if (ResourceType > 0 && ResourceType < RRT_Num)
				{
					ResourceCounts[ResourceType]++;
				}
			}
		}
	}

	FBufferedOutputDevice BufferedOutput;
	FName CategoryName(TEXT("RHIResources"));

	BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("RHIResource Counts"));

	for (int32 Index = 0; Index < RRT_Num; Index++)
	{
		const int32 CurrentCount = ResourceCounts[Index];
		if (CurrentCount > 0)
		{
			BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("%s: %d"),
				StringFromRHIResourceType((ERHIResourceType)Index),
				CurrentCount);
		}
	}
	BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Total: %d"), TotalResources);

	BufferedOutput.RedirectTo(OutputDevice);
}));

void RHIDumpResourceMemory(const FString& NameFilter, ERHIResourceType TypeFilter, EBooleanFilter TransientFilter, int32 NumberOfResourcesToShow, bool bUseCSVOutput, bool bSummaryOutput, bool bOutputToCSVFile, FBufferedOutputDevice& BufferedOutput)
{	
	FArchive* CSVFile{ nullptr };
	if (bOutputToCSVFile)
	{
		const FString Filename = FString::Printf(TEXT("%srhiDumpResourceMemory-%s.csv"), *FPaths::ProfilingDir(), *FDateTime::Now().ToString());
		CSVFile = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_AllowRead);
	}

	TCHAR ResourceNameBuffer[FName::StringBufferSize];

	auto ShouldIncludeResource = [&](const FRHIResource* Resource, const FRHIResourceInfo& ResourceInfo) -> bool
	{
		if (!NameFilter.IsEmpty())
		{
			if (ResourceInfo.Name.ToString(ResourceNameBuffer) == 0 || UE::String::FindFirst(ResourceNameBuffer, *NameFilter, ESearchCase::IgnoreCase) == INDEX_NONE)
			{
				return false;
			}
		}
		if (TypeFilter != RRT_None)
		{
			if (TypeFilter == RRT_Texture)
			{
				if (ResourceInfo.Type != RRT_Texture2D &&
					ResourceInfo.Type != RRT_Texture2DArray &&
					ResourceInfo.Type != RRT_Texture3D &&
					ResourceInfo.Type != RRT_TextureCube)
				{
					return false;
				}
			}
			else if (TypeFilter != ResourceInfo.Type)
			{
				return false;
			}
		}
		if (TransientFilter != EBooleanFilter::All)
		{
			const bool bAllowedFlag = TransientFilter == EBooleanFilter::Yes ? true : false;
			if (ResourceInfo.IsTransient != bAllowedFlag)
			{
				return false;
			}
		}
		return true;
	};

	struct FLocalResourceEntry
	{
		const FRHIResource* Resource;
		FRHIResourceInfo ResourceInfo;
	};
	TArray<FLocalResourceEntry> Resources;

	FScopeLock Lock(&GRHIResourceTrackingCriticalSection);

	int32 TotalResourcesWithInfo = 0;
	int32 TotalTrackedResources = 0;
	int64 TotalTrackedResourceSize = 0;
	int64 TotalTrackedTransientResourceSize = 0;

	{
		FRHIResourceInfo ResourceInfo;
		TotalTrackedResources = GRHITrackedResources.Num();

		for (const FRHIResource* Resource : GRHITrackedResources)
		{
			if (Resource && Resource->GetResourceInfo(ResourceInfo))
			{
				ResourceInfo.bValid = Resource->IsValid();

				if (ShouldIncludeResource(Resource, ResourceInfo))
				{
					Resources.Emplace(FLocalResourceEntry{ Resource, ResourceInfo });
				}

				TotalResourcesWithInfo++;
				if (ResourceInfo.IsTransient)
				{
					TotalTrackedTransientResourceSize += ResourceInfo.VRamAllocation.AllocationSize;
				}
				else
				{
					TotalTrackedResourceSize += ResourceInfo.VRamAllocation.AllocationSize;
				}
			}
		}
	}

	const int32 NumberOfResourcesBeforeNumberFilter = Resources.Num();
	if (NumberOfResourcesToShow < 0 || NumberOfResourcesToShow > Resources.Num())
	{
		NumberOfResourcesToShow = Resources.Num();
	}

	Resources.Sort([](const FLocalResourceEntry& EntryA, const FLocalResourceEntry& EntryB)
	{
		return EntryA.ResourceInfo.VRamAllocation.AllocationSize > EntryB.ResourceInfo.VRamAllocation.AllocationSize;
	});

	FName CategoryName(TEXT("RHIResources"));

	if (bOutputToCSVFile)
	{
		const TCHAR* Header = TEXT("Name,Type,Size,MarkedForDelete,Transient,Streaming,RenderTarget,UAV,\"Raytracing Acceleration Structure\"\n");
		CSVFile->Serialize(TCHAR_TO_ANSI(Header), FPlatformString::Strlen(Header));
	}
	else if (bUseCSVOutput)
	{
		BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Name,Type,Size,MarkedForDelete,Transient,Streaming,RenderTarget,UAV,\"Raytracing Acceleration Structure\""));
	}
	else
	{
		if (bSummaryOutput == false)
		{
			BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Tracked RHIResources (%d total with info, %d total tracked)"), TotalResourcesWithInfo, TotalTrackedResources);
		}

		if (NumberOfResourcesToShow != NumberOfResourcesBeforeNumberFilter)
		{
			BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Showing %d of %d matched resources"), NumberOfResourcesToShow, NumberOfResourcesBeforeNumberFilter);
		}
	}

	int64 TotalShownResourceSize = 0;

	for (int32 Index = 0; Index < Resources.Num(); Index++)
	{
		if (Index < NumberOfResourcesToShow)
		{
			const FRHIResourceInfo& ResourceInfo = Resources[Index].ResourceInfo;

			ResourceInfo.Name.ToString(ResourceNameBuffer);
			const TCHAR* ResourceType = StringFromRHIResourceType(ResourceInfo.Type);
			const int64 SizeInBytes = ResourceInfo.VRamAllocation.AllocationSize;

			bool bMarkedForDelete = !ResourceInfo.bValid;
			bool bTransient = ResourceInfo.IsTransient;
			bool bStreaming = false;
			bool bRT = false;
			bool bDS = false;
			bool bUAV = false;
			bool bRTAS = false;

			bool bIsTexture = ResourceInfo.Type == RRT_Texture ||
				ResourceInfo.Type == RRT_Texture2D ||
				ResourceInfo.Type == RRT_Texture2DArray ||
				ResourceInfo.Type == RRT_Texture3D ||
				ResourceInfo.Type == RRT_TextureCube;
			if (bIsTexture)
			{
				FRHITexture* Texture = (FRHITexture*)Resources[Index].Resource;
				bRT = EnumHasAnyFlags(Texture->GetFlags(), TexCreate_RenderTargetable);
				bDS = EnumHasAnyFlags(Texture->GetFlags(), TexCreate_DepthStencilTargetable);
				bUAV = EnumHasAnyFlags(Texture->GetFlags(), TexCreate_UAV);
				bStreaming = EnumHasAnyFlags(Texture->GetFlags(), TexCreate_Streamable);
			}
			else if (ResourceInfo.Type == RRT_Buffer)
			{
				FRHIBuffer* Buffer = (FRHIBuffer*)Resources[Index].Resource;
				bUAV = EnumHasAnyFlags((EBufferUsageFlags)Buffer->GetUsage(), BUF_UnorderedAccess);
				bRTAS = EnumHasAnyFlags((EBufferUsageFlags)Buffer->GetUsage(), BUF_AccelerationStructure);
			}

			if (bSummaryOutput == false)
			{
				if (bOutputToCSVFile)
				{					
					const FString Row = FString::Printf(TEXT("%s,%s,%.9f,%s,%s,%s,%s,%s,%s\n"),
						ResourceNameBuffer,
						ResourceType,
						SizeInBytes / double(1 << 20),
						bMarkedForDelete ? TEXT("Yes") : TEXT("No"),
						bTransient ? TEXT("Yes") : TEXT("No"),
						bStreaming ? TEXT("Yes") : TEXT("No"),
						(bRT || bDS) ? TEXT("Yes") : TEXT("No"),
						bUAV ? TEXT("Yes") : TEXT("No"),
						bRTAS ? TEXT("Yes") : TEXT("No"));

					CSVFile->Serialize(TCHAR_TO_ANSI(*Row), Row.Len());
				}
				else if (bUseCSVOutput)
				{
					BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("%s,%s,%.9f,%s,%s,%s,%s,%s,%s"),
						ResourceNameBuffer,
						ResourceType,
						SizeInBytes / double(1 << 20),
						bMarkedForDelete ? TEXT("Yes") : TEXT("No"),
						bTransient ? TEXT("Yes") : TEXT("No"),
						bStreaming ? TEXT("Yes") : TEXT("No"),
						(bRT || bDS) ? TEXT("Yes") : TEXT("No"),
						bUAV ? TEXT("Yes") : TEXT("No"),
						bRTAS ? TEXT("Yes") : TEXT("No"));
				}
				else
				{
					FString ResoureFlags;
					bool bHasFlag = false;
					if (bTransient)
					{
						ResoureFlags += "Transient";
						bHasFlag = true;
					}
					if (bStreaming)
					{
						ResoureFlags += bHasFlag ? "|Streaming" : "Streaming";
						bHasFlag = true;
					}
					if (bRT)
					{
						ResoureFlags += bHasFlag ? "|RT" : "RT";
						bHasFlag = true;
					}
					else if (bDS)
					{
						ResoureFlags += bHasFlag ? "|DS" : "DS";
						bHasFlag = true;
					}
					if (bUAV)
					{
						ResoureFlags += bHasFlag ? "|UAV" : "UAV";
						bHasFlag = true;
					}
					if (bRTAS)
					{
						ResoureFlags += bHasFlag ? "|RTAS" : "RTAS";
						bHasFlag = true;

					}

					BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Name: %s - Type: %s - Size: %.9f MB - Flags: %s"),
						ResourceNameBuffer,
						ResourceType,
						SizeInBytes / double(1 << 20),
						bHasFlag ? *ResoureFlags : TEXT("None"));
				}
			}

			TotalShownResourceSize += SizeInBytes;
		}
	}

	if (bOutputToCSVFile)
	{
		delete CSVFile;
		CSVFile = nullptr;
	}
	else if (!bUseCSVOutput)
	{
		const double TotalNonTransientSizeF = TotalTrackedResourceSize / double(1 << 20);
		const double TotalTransientSizeF = TotalTrackedTransientResourceSize / double(1 << 20);
		const double TotalSizeF = (TotalTrackedResourceSize + TotalTrackedTransientResourceSize) / double(1 << 20);
		const double ShownSizeF = TotalShownResourceSize / double(1 << 20);

		if (NumberOfResourcesToShow != TotalResourcesWithInfo)
		{
			double TotalSizeToUse = 0.0;
			const TCHAR* ExtraText = TEXT("");

			if (TransientFilter == EBooleanFilter::No)
			{
				TotalSizeToUse = TotalNonTransientSizeF;
				ExtraText = TEXT(" non-transient");
			}
			else if (TransientFilter == EBooleanFilter::Yes)
			{
				TotalSizeToUse = TotalTransientSizeF;
				ExtraText = TEXT(" transient");
			}
			else
			{
				TotalSizeToUse = TotalSizeF;
				ExtraText = TEXT("");
			}

			if (TotalSizeToUse == 0.0)
			{
				TotalSizeToUse = TotalSizeF;
			}

			BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Shown %d entries%s. Size: %.2f/%.2f MB (%.2f%% of total%s)"),
				NumberOfResourcesToShow, !NameFilter.IsEmpty() ? *FString::Printf(TEXT(" with name %s"), *NameFilter) : TEXT(""), ShownSizeF, TotalSizeToUse, 100.0 * ShownSizeF / TotalSizeToUse, ExtraText);
		}

		if (bSummaryOutput == false)
		{
			BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Total tracked resource size: %.9f MB"), TotalSizeF);
			if (TotalTrackedTransientResourceSize > 0)
			{
				BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("    Non-Transient: %.9f MB"), TotalNonTransientSizeF);
				BufferedOutput.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("    Transient: %.9f MB"), TotalTransientSizeF);
			}
		}
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDumpRHIResourceMemoryCmd(
	TEXT("rhi.DumpResourceMemory"),
	TEXT("Dumps RHI resource memory stats to the log\n")
	TEXT("Usage: rhi.DumpResourceMemory [<Number To Show>] [all] [summary] [Name=<Filter Text>] [Type=<RHI Resource Type>] [Transient=<no, yes, or all> [csv]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld*, FOutputDevice& OutputDevice)
{
	FString NameFilter;
	ERHIResourceType TypeFilter = RRT_None;
	EBooleanFilter TransientFilter = EBooleanFilter::No;
	int32 NumberOfResourcesToShow = 50;
	bool bUseCSVOutput = false;
	bool bSummaryOutput = false;
	bool bOutputToCSVFile = false;

	for (const FString& Argument : Args)
	{
		if (Argument.Equals(TEXT("all"), ESearchCase::IgnoreCase))
		{
			NumberOfResourcesToShow = -1;
		}
		else if (Argument.Equals(TEXT("-csv"), ESearchCase::IgnoreCase))
		{
			bUseCSVOutput = true;
		}
		else if (Argument.Equals(TEXT("-csvfile"), ESearchCase::IgnoreCase))
		{
			bOutputToCSVFile = true;
		}
		else if (Argument.StartsWith(TEXT("Name="), ESearchCase::IgnoreCase))
		{
			NameFilter = Argument.RightChop(5);
		}
		else if (Argument.StartsWith(TEXT("Type="), ESearchCase::IgnoreCase))
		{
			TypeFilter = RHIResourceTypeFromString(Argument.RightChop(5));
		}
		else if (Argument.StartsWith(TEXT("Transient="), ESearchCase::IgnoreCase))
		{
			TransientFilter = ParseBooleanFilter(Argument.RightChop(10));
		}
		else if (FCString::IsNumeric(*Argument))
		{
			LexFromString(NumberOfResourcesToShow, *Argument);
		}
		else if (Argument.Equals(TEXT("summary"), ESearchCase::IgnoreCase))
		{
			// Respects name, type and transient filters but only reports total sizes.
			// Does not report a list of individual resources.
			bSummaryOutput = true;
			NumberOfResourcesToShow = -1;
		}
		else
		{
			NameFilter = Argument;
		}
	}

	FBufferedOutputDevice BufferedOutput;
	RHIDumpResourceMemory(NameFilter, TypeFilter, TransientFilter, NumberOfResourcesToShow, bUseCSVOutput, bSummaryOutput, bOutputToCSVFile, BufferedOutput);
	BufferedOutput.RedirectTo(OutputDevice);
}));

void RHIDumpResourceMemoryToCSV()
{
	FString NameFilter;
	ERHIResourceType TypeFilter = RRT_None;
	EBooleanFilter TransientFilter = EBooleanFilter::No;
	int32 NumberOfResourcesToShow = -1;
	bool bUseCSVOutput = false;
	bool bSummaryOutput = false;
	bool bOutputToCSVFile = true;

	FBufferedOutputDevice BufferedOutput;
	RHIDumpResourceMemory(NameFilter, TypeFilter, TransientFilter, NumberOfResourcesToShow, bUseCSVOutput, bSummaryOutput, bOutputToCSVFile, BufferedOutput);
}

#endif // RHI_ENABLE_RESOURCE_INFO

bool FRHIResource::Bypass()
{
	return GRHICommandList.Bypass();
}

DECLARE_CYCLE_STAT(TEXT("Delete Resources"), STAT_DeleteResources, STATGROUP_RHICMDLIST);

int32 FRHIResource::FlushPendingDeletes(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_DeleteResources);

	check(IsInRenderingThread());

	TArray<FRHIResource*> DeletedResources;
	TClosableMpscQueue<FRHIResource*>* PendingDeletesPtr = PendingDeletes.exchange(new TClosableMpscQueue<FRHIResource*>());
	PendingDeletesPtr->Close([&DeletedResources](FRHIResource* Resource)
	{	
		DeletedResources.Push(Resource);
	});
	PendingDeletesHPC.Delete(PendingDeletesPtr);

	const int32 NumDeletes = DeletedResources.Num();

	RHICmdList.EnqueueLambda([DeletedResources = MoveTemp(DeletedResources)](FRHICommandListImmediate& RHICmdList) mutable
	{
		if (GDynamicRHI)
		{
			GDynamicRHI->RHIPerFrameRHIFlushComplete();
		}

		for (FRHIResource* Resource : DeletedResources)
		{
			if (Resource->AtomicFlags.Deleteing())
			{
				FRHIResource::CurrentlyDeleting = Resource;
				delete Resource;
			}
		}
	});

	return NumDeletes;
}

static_assert(ERHIZBuffer::FarPlane != ERHIZBuffer::NearPlane, "Near and Far planes must be different!");
static_assert((int32)ERHIZBuffer::NearPlane == 0 || (int32)ERHIZBuffer::NearPlane == 1, "Invalid Values for Near Plane, can only be 0 or 1!");
static_assert((int32)ERHIZBuffer::FarPlane == 0 || (int32)ERHIZBuffer::FarPlane == 1, "Invalid Values for Far Plane, can only be 0 or 1");


/**
 * RHI configuration settings.
 */

static TAutoConsoleVariable<int32> ResourceTableCachingCvar(
	TEXT("rhi.ResourceTableCaching"),
	1,
	TEXT("If 1, the RHI will cache resource table contents within a frame. Otherwise resource tables are rebuilt for every draw call.")
	);
static TAutoConsoleVariable<int32> GSaveScreenshotAfterProfilingGPUCVar(
	TEXT("r.ProfileGPU.Screenshot"),
	1,
	TEXT("Whether a screenshot should be taken when profiling the GPU. 0:off, 1:on (default)"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> GShowProfilerAfterProfilingGPUCVar(
	TEXT("r.ProfileGPU.ShowUI"),
	1,
	TEXT("Whether the user interface profiler should be displayed after profiling the GPU.\n")
	TEXT("The results will always go to the log/console\n")
	TEXT("0:off, 1:on (default)"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<float> GGPUHitchThresholdCVar(
	TEXT("RHI.GPUHitchThreshold"),
	100.0f,
	TEXT("Threshold for detecting hitches on the GPU (in milliseconds).")
	);
static TAutoConsoleVariable<int32> GCVarRHIRenderPass(
	TEXT("r.RHIRenderPasses"),
	0,
	TEXT(""),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarGPUCrashDebugging(
	TEXT("r.GPUCrashDebugging"),
	0,
	TEXT("Enable vendor specific GPU crash analysis tools"),
	ECVF_ReadOnly
	);

static TAutoConsoleVariable<int32> CVarGPUCrashDump(
	TEXT("r.GPUCrashDump"),
	0,
	TEXT("Enable vendor specific GPU crash dumps"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarGPUCrashOnOutOfMemory(
	TEXT("r.GPUCrashOnOutOfMemory"),
	0,
	TEXT("Enable crash reporting on GPU OOM"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarGPUCrashDebuggingAftermathMarkers(
	TEXT("r.GPUCrashDebugging.Aftermath.Markers"),
	0,
	TEXT("Enable draw event markers in Aftermath dumps"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarGPUCrashDebuggingAftermathCallstack(
	TEXT("r.GPUCrashDebugging.Aftermath.Callstack"),
	0,
	TEXT("Enable callstack capture in Aftermath dumps"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarGPUCrashDebuggingAftermathResourceTracking(
	TEXT("r.GPUCrashDebugging.Aftermath.ResourceTracking"),
	0,
	TEXT("Enable resource tracking for Aftermath dumps"),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<int32> CVarGPUCrashDebuggingAftermathTrackAll(
	TEXT("r.GPUCrashDebugging.Aftermath.TrackAll"),
	1,
	TEXT("Enable maximum tracking for Aftermath dumps"),
	ECVF_ReadOnly
);

static FAutoConsoleVariableRef CVarEnableVariableRateShading(
	TEXT("r.VRS.Enable"),
	GRHIVariableRateShadingEnabled,
	TEXT("Toggle to enable Variable Rate Shading."),
	ECVF_RenderThreadSafe);

static FAutoConsoleVariableRef CVarEnableAttachmentVariableRateShading(
	TEXT("r.VRS.EnableImage"),
	GRHIAttachmentVariableRateShadingEnabled,
	TEXT("Toggle to enable image-based Variable Rate Shading."),
	ECVF_RenderThreadSafe);


int32 GRHIBindlessResourceConfiguration = 0;
static FAutoConsoleVariableRef CVarEnableBindlessResources(
	TEXT("rhi.Bindless.Resources"),
	GRHIBindlessResourceConfiguration,
	TEXT("Set to 1 to enable for all shader types. Set to 2 to restrict to Raytracing shaders."),
	ECVF_ReadOnly
);

int32 GRHIBindlessSamplerConfiguration = 0;
static FAutoConsoleVariableRef CVarEnableBindlessSamplers(
	TEXT("rhi.Bindless.Samplers"),
	GRHIBindlessSamplerConfiguration,
	TEXT("Set to 1 to enable for all shader types. Set to 2 to restrict to Raytracing shaders."),
	ECVF_ReadOnly
);

static ERHIBindlessConfiguration DetermineBindlessConfiguration(EShaderPlatform Platform, int32 BindlessConfigSetting)
{
	if (!RHISupportsBindless(Platform) || BindlessConfigSetting == 0)
	{
		return ERHIBindlessConfiguration::Disabled;
	}

	return BindlessConfigSetting == 2 ? ERHIBindlessConfiguration::RayTracingShaders : ERHIBindlessConfiguration::AllShaders;
}

ERHIBindlessConfiguration RHIGetBindlessResourcesConfiguration(EShaderPlatform Platform)
{
	return DetermineBindlessConfiguration(Platform, GRHIBindlessResourceConfiguration);
}

ERHIBindlessConfiguration RHIGetBindlessSamplersConfiguration(EShaderPlatform Platform)
{
	return DetermineBindlessConfiguration(Platform, GRHIBindlessSamplerConfiguration);
}

namespace RHIConfig
{
	bool ShouldSaveScreenshotAfterProfilingGPU()
	{
		return GSaveScreenshotAfterProfilingGPUCVar.GetValueOnAnyThread() != 0;
	}

	bool ShouldShowProfilerAfterProfilingGPU()
	{
		return GShowProfilerAfterProfilingGPUCVar.GetValueOnAnyThread() != 0;
	}

	float GetGPUHitchThreshold()
	{
		return GGPUHitchThresholdCVar.GetValueOnAnyThread() * 0.001f;
	}
}

/**
 * RHI globals.
 */

bool GIsRHIInitialized = false;
int32 GRHIPersistentThreadGroupCount = 0;
int32 GMaxTextureMipCount = MAX_TEXTURE_MIP_COUNT;
bool GSupportsQuadBufferStereo = false;
FString GRHIAdapterName;
FString GRHIAdapterInternalDriverVersion;
FString GRHIAdapterUserDriverVersion;
FString GRHIAdapterDriverDate;
bool GRHIAdapterDriverOnDenyList = false;
uint32 GRHIVendorId = 0;
uint32 GRHIDeviceId = 0;
uint32 GRHIDeviceRevision = 0;
bool GRHIDeviceIsAMDPreGCNArchitecture = false;
bool GSupportsRenderDepthTargetableShaderResources = true;
TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_G8(true);
TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_FloatRGBA(true);
bool GSupportsShaderFramebufferFetch = false;
bool GSupportsShaderDepthStencilFetch = false;
bool GSupportsShaderMRTFramebufferFetch = false;
bool GSupportsPixelLocalStorage = false;
bool GSupportsTimestampRenderQueries = false;
bool GRHISupportsGPUTimestampBubblesRemoval = false;
bool GRHISupportsFrameCyclesBubblesRemoval = false;
bool GHardwareHiddenSurfaceRemoval = false;
bool GRHISupportsAsyncTextureCreation = false;
bool GRHISupportsQuadTopology = false;
bool GRHISupportsRectTopology = false;
bool GRHISupportsPrimitiveShaders = false;
bool GRHISupportsAtomicUInt64 = false;
bool GRHISupportsDX12AtomicUInt64 = false;
bool GRHISupportsPipelineStateSortKey = false;
bool GRHISupportsResummarizeHTile = false;
bool GRHISupportsExplicitHTile = false;
bool GRHISupportsExplicitFMask = false;
bool GRHISupportsDepthUAV = false;
bool GSupportsParallelRenderingTasksWithSeparateRHIThread = true;
bool GRHIThreadNeedsKicking = false;
int32 GRHIMaximumReccommendedOustandingOcclusionQueries = MAX_int32;
bool GRHISupportsExactOcclusionQueries = true;
bool GSupportsVolumeTextureRendering = true;
bool GSupportsSeparateRenderTargetBlendState = false;
bool GRHINeedsUnatlasedCSMDepthsWorkaround = false;
bool GSupportsTexture3D = true;
bool GSupportsMobileMultiView = false;
bool GSupportsImageExternal = false;
bool GRHISupportsDrawIndirect = true;
bool GRHISupportsMultithreading = false;
bool GRHISupportsUpdateFromBufferTexture = false;
bool GSupportsWideMRT = true;
bool GRHINeedsExtraDeletionLatency = false;
bool GRHIForceNoDeletionLatencyForStreamingTextures = false;
TRHIGlobal<int32> GMaxComputeDispatchDimension((1 << 16) - 1);
bool GRHILazyShaderCodeLoading = false;
bool GRHISupportsLazyShaderCodeLoading = false;
TRHIGlobal<int32> GMaxShadowDepthBufferSizeX(2048);
TRHIGlobal<int32> GMaxShadowDepthBufferSizeY(2048);
TRHIGlobal<int32> GMaxTextureDimensions(2048);
TRHIGlobal<int64> GMaxBufferDimensions(1<<27);
TRHIGlobal<int64> GRHIMaxConstantBufferByteSize(1<<27);
TRHIGlobal<int64> GMaxComputeSharedMemory(1<<15);
TRHIGlobal<int32> GMaxVolumeTextureDimensions(2048);
TRHIGlobal<int32> GMaxCubeTextureDimensions(2048);
TRHIGlobal<int32> GMaxWorkGroupInvocations(1024);
bool GRHISupportsRawViewsForAnyBuffer = false;
bool GRHISupportsRWTextureBuffers = true;
bool GRHISupportsVRS = false;
bool GRHISupportsLateVRSUpdate = false;
int32 GMaxTextureArrayLayers = 256;
int32 GMaxTextureSamplers = 16;
bool GUsingNullRHI = false;
int32 GDrawUPVertexCheckCount = MAX_int32;
int32 GDrawUPIndexCheckCount = MAX_int32;
bool GTriggerGPUProfile = false;
FString GGPUTraceFileName;
bool GRHISupportsTextureStreaming = false;
bool GSupportsDepthBoundsTest = false;
bool GSupportsEfficientAsyncCompute = false;
bool GRHISupportsBaseVertexIndex = true;
bool GRHISupportsFirstInstance = false;
bool GRHISupportsDynamicResolution = false;
bool GRHISupportsRayTracing = false;
bool GRHISupportsRayTracingPSOAdditions = false;
bool GRHISupportsRayTracingDispatchIndirect = false;
bool GRHISupportsRayTracingAsyncBuildAccelerationStructure = false;
bool GRHISupportsRayTracingAMDHitToken = false;
bool GRHISupportsInlineRayTracing = false;
bool GRHISupportsRayTracingShaders = false;
uint32 GRHIRayTracingAccelerationStructureAlignment = 0;
uint32 GRHIRayTracingScratchBufferAlignment = 0;
uint32 GRHIRayTracingShaderTableAlignment = 0;
uint32 GRHIRayTracingInstanceDescriptorSize = 0;
bool GRHISupportsWaveOperations = false;
int32 GRHIMinimumWaveSize = 0;
int32 GRHIMaximumWaveSize = 0;
bool GRHISupportsRHIThread = false;
bool GRHISupportsRHIOnTaskThread = false;
bool GRHISupportsParallelRHIExecute = false;
bool GSupportsParallelOcclusionQueries = false;
bool GSupportsTransientResourceAliasing = false;
bool GRHIRequiresRenderTargetForPixelShaderUAVs = false;
bool GRHISupportsUAVFormatAliasing = false;
bool GRHISupportsDirectGPUMemoryLock = false;
bool GRHISupportsMultithreadedShaderCreation = true;
bool GRHISupportsMultithreadedResources = false;

bool GRHISupportsMSAADepthSampleAccess = false;

bool GRHISupportsBackBufferWithCustomDepthStencil = true;

bool GRHIIsHDREnabled = false;
bool GRHISupportsHDROutput = false;

bool GRHIVariableRateShadingEnabled = true;
bool GRHIAttachmentVariableRateShadingEnabled = true;
bool GRHISupportsPipelineVariableRateShading = false;
bool GRHISupportsLargerVariableRateShadingSizes = false;
bool GRHISupportsAttachmentVariableRateShading = false;
bool GRHISupportsComplexVariableRateShadingCombinerOps = false;
bool GRHISupportsVariableRateShadingAttachmentArrayTextures = false;
int32 GRHIVariableRateShadingImageTileMaxWidth = 0;
int32 GRHIVariableRateShadingImageTileMaxHeight = 0;
int32 GRHIVariableRateShadingImageTileMinWidth = 0;
int32 GRHIVariableRateShadingImageTileMinHeight = 0;
EVRSImageDataType GRHIVariableRateShadingImageDataType = VRSImage_NotSupported;
EPixelFormat GRHIVariableRateShadingImageFormat = PF_Unknown;
bool GRHISupportsLateVariableRateShadingUpdate = false;

EPixelFormat GRHIHDRDisplayOutputFormat = PF_FloatRGBA;

FIntVector GRHIMaxDispatchThreadGroupsPerDimension(0, 0, 0);

uint64 GRHIPresentCounter = 1;

bool GRHISupportsArrayIndexFromAnyShader = false;
bool GRHISupportsStencilRefFromPixelShader = false;
bool GRHISupportsConservativeRasterization = false;

bool GRHISupportsPipelineFileCache = false;

bool GRHIDeviceIsIntegrated = false;

/** Whether we are profiling GPU hitches. */
bool GTriggerGPUHitchProfile = false;

bool GRHISupportsPixelShaderUAVs = true;

bool GRHISupportsMeshShadersTier0 = false;
bool GRHISupportsMeshShadersTier1 = false;

bool GRHISupportsShaderTimestamp = false;

bool GRHISupportsEfficientUploadOnResourceCreation = false;

bool GRHISupportsAsyncPipelinePrecompile = true;

bool GRHISupportsMapWriteNoOverwrite = false;

bool GRHISupportsSeparateDepthStencilCopyAccess = true;

bool GRHISupportsBindless = false;

FVertexElementTypeSupportInfo GVertexElementTypeSupport;

RHI_API int32 volatile GCurrentTextureMemorySize = 0;
RHI_API int32 volatile GCurrentRendertargetMemorySize = 0;
RHI_API int64 GTexturePoolSize = 0 * 1024 * 1024;
RHI_API int32 GPoolSizeVRAMPercentage = 0;

RHI_API uint64 GDemotedLocalMemorySize = 0;

RHI_API EShaderPlatform GShaderPlatformForFeatureLevel[ERHIFeatureLevel::Num] = {SP_NumPlatforms,SP_NumPlatforms,SP_NumPlatforms,SP_NumPlatforms,SP_NumPlatforms};

// simple stats about draw calls. GNum is the previous frame and 
// GCurrent is the current frame.
// GCurrentNumDrawCallsRHIPtr points to the drawcall counter to increment
RHI_API int32 GCurrentNumDrawCallsRHI[MAX_NUM_GPUS] = {};
RHI_API int32 GNumDrawCallsRHI[MAX_NUM_GPUS] = {};
thread_local FRHIDrawCallsStatPtr GCurrentNumDrawCallsRHIPtr = &GCurrentNumDrawCallsRHI;
RHI_API int32 GCurrentNumPrimitivesDrawnRHI[MAX_NUM_GPUS] = {};
RHI_API int32 GNumPrimitivesDrawnRHI[MAX_NUM_GPUS] = {};

RHI_API uint64 GRHITransitionPrivateData_SizeInBytes = 0;
RHI_API uint64 GRHITransitionPrivateData_AlignInBytes = 0;

// By default, read only states and UAV states are allowed to participate in state merging.
ERHIAccess GRHIMergeableAccessMask = ERHIAccess::ReadOnlyMask | ERHIAccess::UAVMask;

// By default, only exclusively read only accesses are allowed.
ERHIAccess GRHIMultiPipelineMergeableAccessMask = ERHIAccess::ReadOnlyExclusiveMask;

/** Called once per frame only from within an RHI. */
void RHIPrivateBeginFrame()
{
	for (int32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; GPUIndex++)
	{
		GNumDrawCallsRHI[GPUIndex] = GCurrentNumDrawCallsRHI[GPUIndex];
	}
	
#if CSV_PROFILER
	// Only copy the display counters every so many frames to keep things more stable.
	const int32 FramesUntilDisplayCopy = 30;
	static int32 FrameCount = 0;
	bool bCopyDisplayFrames = false;
	++FrameCount;
	if (FrameCount >= FramesUntilDisplayCopy)
	{
		bCopyDisplayFrames = true;
		FrameCount = 0;
	}

	for (int32 Index=0; Index<FDrawCallCategoryName::NumCategory; ++Index)
	{
		FDrawCallCategoryName* CategoryName = FDrawCallCategoryName::Array[Index];
		for (int32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; GPUIndex++)
		{
			if (bCopyDisplayFrames)
			{
				FDrawCallCategoryName::DisplayCounts[Index][GPUIndex] = CategoryName->Counters[GPUIndex];
			}
			GNumDrawCallsRHI[GPUIndex] += CategoryName->Counters[GPUIndex];
		}
		// Multi-GPU support : CSV stats do not support MGPU yet
		FCsvProfiler::RecordCustomStat(CategoryName->Name, CSV_CATEGORY_INDEX(DrawCall), CategoryName->Counters[0], ECsvCustomStatOp::Set);
		for (int32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; GPUIndex++)
		{
			CategoryName->Counters[GPUIndex] = 0;
		}
	}
#endif

	for (int32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; GPUIndex++)
	{
		GNumPrimitivesDrawnRHI[GPUIndex] = GCurrentNumPrimitivesDrawnRHI[GPUIndex];
	}
	// Multi-GPU support : CSV stats do not support MGPU yet
	CSV_CUSTOM_STAT(RHI, DrawCalls, GNumDrawCallsRHI[0], ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(RHI, PrimitivesDrawn, GNumPrimitivesDrawnRHI[0], ECsvCustomStatOp::Set);
	for (int32 GPUIndex = 0; GPUIndex < MAX_NUM_GPUS; GPUIndex++)
	{
		GCurrentNumDrawCallsRHI[GPUIndex] = GCurrentNumPrimitivesDrawnRHI[GPUIndex] = 0;
	}
}

/** Whether to initialize 3D textures using a bulk data (or through a mip update if false). */
RHI_API bool GUseTexture3DBulkDataRHI = false;

//
// The current shader platform.
//

RHI_API EShaderPlatform GMaxRHIShaderPlatform = SP_PCD3D_SM5;

/** The maximum feature level supported on this machine */
RHI_API ERHIFeatureLevel::Type GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;

FName FeatureLevelNames[] = 
{
	FName(TEXT("ES2")),
	FName(TEXT("ES3_1")),
	FName(TEXT("SM4_REMOVED")),
	FName(TEXT("SM5")),
	FName(TEXT("SM6")),
};

static_assert(UE_ARRAY_COUNT(FeatureLevelNames) == ERHIFeatureLevel::Num, "Missing entry from feature level names.");

RHI_API bool GetFeatureLevelFromName(FName Name, ERHIFeatureLevel::Type& OutFeatureLevel)
{
	for (int32 NameIndex = 0; NameIndex < UE_ARRAY_COUNT(FeatureLevelNames); NameIndex++)
	{
		if (FeatureLevelNames[NameIndex] == Name)
		{
			OutFeatureLevel = (ERHIFeatureLevel::Type)NameIndex;
			return true;
		}
	}

	OutFeatureLevel = ERHIFeatureLevel::Num;
	return false;
}

RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FString& OutName)
{
	check(InFeatureLevel < UE_ARRAY_COUNT(FeatureLevelNames));
	if (InFeatureLevel < UE_ARRAY_COUNT(FeatureLevelNames))
	{
		FeatureLevelNames[(int32)InFeatureLevel].ToString(OutName);
	}
	else
	{
		OutName = TEXT("InvalidFeatureLevel");
	}	
}

static FName InvalidFeatureLevelName(TEXT("InvalidFeatureLevel"));
RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FName& OutName)
{
	check(InFeatureLevel < UE_ARRAY_COUNT(FeatureLevelNames));
	if (InFeatureLevel < UE_ARRAY_COUNT(FeatureLevelNames))
	{
		OutName = FeatureLevelNames[(int32)InFeatureLevel];
	}
	else
	{
		
		OutName = InvalidFeatureLevelName;
	}
}

FName ShadingPathNames[] =
{
	FName(TEXT("Deferred")),
	FName(TEXT("Forward")),
	FName(TEXT("Mobile")),
};

static_assert(UE_ARRAY_COUNT(ShadingPathNames) == ERHIShadingPath::Num, "Missing entry from shading path names.");

RHI_API bool GetShadingPathFromName(FName Name, ERHIShadingPath::Type& OutShadingPath)
{
	for (int32 NameIndex = 0; NameIndex < UE_ARRAY_COUNT(ShadingPathNames); NameIndex++)
	{
		if (ShadingPathNames[NameIndex] == Name)
		{
			OutShadingPath = (ERHIShadingPath::Type)NameIndex;
			return true;
		}
	}

	OutShadingPath = ERHIShadingPath::Num;
	return false;
}

RHI_API void GetShadingPathName(ERHIShadingPath::Type InShadingPath, FString& OutName)
{
	check(InShadingPath < UE_ARRAY_COUNT(ShadingPathNames));
	if (InShadingPath < UE_ARRAY_COUNT(ShadingPathNames))
	{
		ShadingPathNames[(int32)InShadingPath].ToString(OutName);
	}
	else
	{
		OutName = TEXT("InvalidShadingPath");
	}
}

static FName InvalidShadingPathName(TEXT("InvalidShadingPath"));
RHI_API void GetShadingPathName(ERHIShadingPath::Type InShadingPath, FName& OutName)
{
	check(InShadingPath < UE_ARRAY_COUNT(ShadingPathNames));
	if (InShadingPath < UE_ARRAY_COUNT(ShadingPathNames))
	{
		OutName = ShadingPathNames[(int32)InShadingPath];
	}
	else
	{

		OutName = InvalidShadingPathName;
	}
}

static FName NAME_PLATFORM_WINDOWS(TEXT("Windows"));
static FName NAME_PLATFORM_ANDROID(TEXT("Android"));
static FName NAME_PLATFORM_IOS(TEXT("IOS"));
static FName NAME_PLATFORM_MAC(TEXT("Mac"));
static FName NAME_PLATFORM_TVOS(TEXT("TVOS"));

// @todo platplug: This is still here, only being used now by UMaterialShaderQualitySettings::GetOrCreatePlatformSettings
// since I have moved the other uses to FindTargetPlatformWithSupport
// But I'd like to delete it anyway!
FName ShaderPlatformToPlatformName(EShaderPlatform Platform)
{
	switch (Platform)
	{
	case SP_PCD3D_SM6:
	case SP_PCD3D_SM5:
	case SP_PCD3D_ES3_1:
	case SP_OPENGL_PCES3_1:
	case SP_VULKAN_PCES3_1:
	case SP_VULKAN_SM5:
	case SP_D3D_ES3_1_HOLOLENS:
		return NAME_PLATFORM_WINDOWS;
	case SP_VULKAN_ES3_1_ANDROID:
	case SP_VULKAN_SM5_ANDROID:
	case SP_OPENGL_ES3_1_ANDROID:
		return NAME_PLATFORM_ANDROID;
	case SP_METAL:
	case SP_METAL_MRT:
		return NAME_PLATFORM_IOS;
	case SP_METAL_SM5:
	case SP_METAL_MACES3_1:
	case SP_METAL_MRT_MAC:
		return NAME_PLATFORM_MAC;
	case SP_METAL_TVOS:
	case SP_METAL_MRT_TVOS:
		return NAME_PLATFORM_TVOS;


	default:
		if (FStaticShaderPlatformNames::IsStaticPlatform(Platform))
		{
			return FStaticShaderPlatformNames::Get().GetPlatformName(Platform);
		}
		else
		{
			return NAME_None;
		}
	}
}

FName LegacyShaderPlatformToShaderFormat(EShaderPlatform Platform)
{
	return  FDataDrivenShaderPlatformInfo::GetShaderFormat(Platform);
}

EShaderPlatform ShaderFormatToLegacyShaderPlatform(FName ShaderFormat)
{
	return ShaderFormatNameToShaderPlatform(ShaderFormat);
}

RHI_API bool IsRHIDeviceAMD()
{
	check(GRHIVendorId != 0);
	// AMD's drivers tested on July 11 2013 have hitching problems with async resource streaming, setting single threaded for now until fixed.
	return GRHIVendorId == 0x1002;
}

RHI_API bool IsRHIDeviceIntel()
{
	check(GRHIVendorId != 0);
	return GRHIVendorId == 0x8086;
}

RHI_API bool IsRHIDeviceNVIDIA()
{
	check(GRHIVendorId != 0);
	// NVIDIA GPUs are discrete and use DedicatedVideoMemory only.
	return GRHIVendorId == 0x10DE;
}

RHI_API const TCHAR* RHIVendorIdToString()
{
	return RHIVendorIdToString((EGpuVendorId)GRHIVendorId);
}

RHI_API const TCHAR* RHIVendorIdToString(EGpuVendorId VendorId)
{
	switch (VendorId)
	{
	case EGpuVendorId::Amd:			return TEXT("AMD");
	case EGpuVendorId::ImgTec:		return TEXT("ImgTec");
	case EGpuVendorId::Nvidia:		return TEXT("NVIDIA");
	case EGpuVendorId::Arm:			return TEXT("ARM");
	case EGpuVendorId::Broadcom:	return TEXT("Broadcom");
	case EGpuVendorId::Qualcomm:	return TEXT("Qualcomm");
	case EGpuVendorId::Apple:		return TEXT("Apple");
	case EGpuVendorId::Intel:		return TEXT("Intel");
	case EGpuVendorId::Vivante:		return TEXT("Vivante");
	case EGpuVendorId::VeriSilicon:	return TEXT("VeriSilicon");
	case EGpuVendorId::Kazan:		return TEXT("Kazan");
	case EGpuVendorId::Codeplay:	return TEXT("Codeplay");
	case EGpuVendorId::Mesa:		return TEXT("Mesa");
	case EGpuVendorId::NotQueried:	return TEXT("Not Queried");
	default:
		break;
	}

	return TEXT("Unknown");
}

RHI_API uint32 RHIGetMetalShaderLanguageVersion(const FStaticShaderPlatform Platform)
{
    if (IsMetalPlatform(Platform))
	{
		if (IsPCPlatform(Platform))
		{
            static int32 MacMetalShaderLanguageVersion = -1;
            if (MacMetalShaderLanguageVersion == -1)
            {
                if (!GConfig->GetInt(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("MetalLanguageVersion"), MacMetalShaderLanguageVersion, GEngineIni))
                {
                    MacMetalShaderLanguageVersion = 0; // 0 means default EMacMetalShaderStandard::MacMetalSLStandard_Minimum
                }
            }
            return MacMetalShaderLanguageVersion;
		}
		else
		{
            static int32 IOSMetalShaderLanguageVersion = -1;
            if (IOSMetalShaderLanguageVersion == -1)
            {
                if (!GConfig->GetInt(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("MetalLanguageVersion"), IOSMetalShaderLanguageVersion, GEngineIni))
                {
                	IOSMetalShaderLanguageVersion = 0;  // 0 means default EIOSMetalShaderStandard::IOSMetalSLStandard_Minimum
                }
            }
            return IOSMetalShaderLanguageVersion;
        }
	}
	return 0;
}

static ERHIFeatureLevel::Type GRHIMobilePreviewFeatureLevel = ERHIFeatureLevel::Num;
RHI_API void RHISetMobilePreviewFeatureLevel(ERHIFeatureLevel::Type MobilePreviewFeatureLevel)
{
	check(GRHIMobilePreviewFeatureLevel == ERHIFeatureLevel::Num);
	check(!GIsEditor);
	GRHIMobilePreviewFeatureLevel = MobilePreviewFeatureLevel;
}

bool RHIGetPreviewFeatureLevel(ERHIFeatureLevel::Type& PreviewFeatureLevelOUT)
{
	static bool bForceFeatureLevelES3_1 = !GIsEditor && (FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1")));

	if (bForceFeatureLevelES3_1)
	{
		PreviewFeatureLevelOUT = ERHIFeatureLevel::ES3_1;
	}
	else if (!GIsEditor && GRHIMobilePreviewFeatureLevel != ERHIFeatureLevel::Num)
	{
		PreviewFeatureLevelOUT = GRHIMobilePreviewFeatureLevel;
	}
	else
	{
		return false;
	}
	return true;
}

 RHI_API EPixelFormat RHIPreferredPixelFormatHint(EPixelFormat PreferredPixelFormat)
{
	if (GDynamicRHI)
	{
		return GDynamicRHI->RHIPreferredPixelFormatHint(PreferredPixelFormat);
	}
	return PreferredPixelFormat;
}

RHI_API int32 RHIGetPreferredClearUAVRectPSResourceType(const FStaticShaderPlatform Platform)
{
	if (IsMetalPlatform(Platform))
	{
		static constexpr uint32 METAL_TEXTUREBUFFER_SHADER_LANGUAGE_VERSION = 4;
		if (METAL_TEXTUREBUFFER_SHADER_LANGUAGE_VERSION <= RHIGetMetalShaderLanguageVersion(Platform))
		{
			return 0; // BUFFER
		}
	}
	return 1; // TEXTURE_2D
}

void FRHIRenderPassInfo::ConvertToRenderTargetsInfo(FRHISetRenderTargetsInfo& OutRTInfo) const
{
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		if (!ColorRenderTargets[Index].RenderTarget)
		{
			break;
		}

		OutRTInfo.ColorRenderTarget[Index].Texture = ColorRenderTargets[Index].RenderTarget;
		ERenderTargetLoadAction LoadAction = GetLoadAction(ColorRenderTargets[Index].Action);
		OutRTInfo.ColorRenderTarget[Index].LoadAction = LoadAction;
		OutRTInfo.ColorRenderTarget[Index].StoreAction = GetStoreAction(ColorRenderTargets[Index].Action);
		OutRTInfo.ColorRenderTarget[Index].ArraySliceIndex = ColorRenderTargets[Index].ArraySlice;
		OutRTInfo.ColorRenderTarget[Index].MipIndex = ColorRenderTargets[Index].MipIndex;
		++OutRTInfo.NumColorRenderTargets;

		OutRTInfo.bClearColor |= (LoadAction == ERenderTargetLoadAction::EClear);

		ensure(!OutRTInfo.bHasResolveAttachments || ColorRenderTargets[Index].ResolveTarget);
		if (ColorRenderTargets[Index].ResolveTarget)
		{
			OutRTInfo.bHasResolveAttachments = true;
			OutRTInfo.ColorResolveRenderTarget[Index] = OutRTInfo.ColorRenderTarget[Index];
			OutRTInfo.ColorResolveRenderTarget[Index].Texture = ColorRenderTargets[Index].ResolveTarget;
		}
	}

	ERenderTargetActions DepthActions = GetDepthActions(DepthStencilRenderTarget.Action);
	ERenderTargetActions StencilActions = GetStencilActions(DepthStencilRenderTarget.Action);
	ERenderTargetLoadAction DepthLoadAction = GetLoadAction(DepthActions);
	ERenderTargetStoreAction DepthStoreAction = GetStoreAction(DepthActions);
	ERenderTargetLoadAction StencilLoadAction = GetLoadAction(StencilActions);
	ERenderTargetStoreAction StencilStoreAction = GetStoreAction(StencilActions);

	OutRTInfo.DepthStencilRenderTarget = FRHIDepthRenderTargetView(DepthStencilRenderTarget.DepthStencilTarget,
		DepthLoadAction,
		GetStoreAction(DepthActions),
		StencilLoadAction,
		GetStoreAction(StencilActions),
		DepthStencilRenderTarget.ExclusiveDepthStencil);
	OutRTInfo.bClearDepth = (DepthLoadAction == ERenderTargetLoadAction::EClear);
	OutRTInfo.bClearStencil = (StencilLoadAction == ERenderTargetLoadAction::EClear);

	OutRTInfo.ShadingRateTexture = ShadingRateTexture;
	OutRTInfo.ShadingRateTextureCombiner = ShadingRateTextureCombiner;
	OutRTInfo.MultiViewCount = MultiViewCount;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void FRHIRenderPassInfo::Validate() const
{
	int32 NumSamples = -1;	// -1 means nothing found yet
	int32 ColorIndex = 0;
	for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
	{
		const FColorEntry& Entry = ColorRenderTargets[ColorIndex];
		if (Entry.RenderTarget)
		{
			// Ensure NumSamples matches amongst all color RTs
			if (NumSamples == -1)
			{
				NumSamples = Entry.RenderTarget->GetNumSamples();
			}
			else
			{
				ensure(Entry.RenderTarget->GetNumSamples() == NumSamples);
			}

			ERenderTargetStoreAction Store = GetStoreAction(Entry.Action);
			// Don't try to resolve a non-msaa
			ensure(Store != ERenderTargetStoreAction::EMultisampleResolve || Entry.RenderTarget->GetNumSamples() > 1);
			// Don't resolve to null
			ensure(Store != ERenderTargetStoreAction::EMultisampleResolve || Entry.ResolveTarget);

			if (Entry.ResolveTarget)
			{
				//ensure(Store == ERenderTargetStoreAction::EMultisampleResolve);
			}
		}
		else
		{
			break;
		}
	}

	int32 NumColorRenderTargets = ColorIndex;
	for (; ColorIndex < MaxSimultaneousRenderTargets; ++ColorIndex)
	{
		// Gap in the sequence of valid render targets (ie RT0, null, RT2, ...)
		ensureMsgf(!ColorRenderTargets[ColorIndex].RenderTarget, TEXT("Missing color render target on slot %d"), ColorIndex - 1);
	}

	if (DepthStencilRenderTarget.DepthStencilTarget)
	{
		// Ensure NumSamples matches with color RT
		if (NumSamples != -1)
		{
			ensure(DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() == NumSamples);
		}
		ERenderTargetStoreAction DepthStore = GetStoreAction(GetDepthActions(DepthStencilRenderTarget.Action));
		ERenderTargetStoreAction StencilStore = GetStoreAction(GetStencilActions(DepthStencilRenderTarget.Action));
		bool bIsMSAAResolve = (DepthStore == ERenderTargetStoreAction::EMultisampleResolve) || (StencilStore == ERenderTargetStoreAction::EMultisampleResolve);
		// Don't try to resolve a non-msaa
		ensure(!bIsMSAAResolve || DepthStencilRenderTarget.DepthStencilTarget->GetNumSamples() > 1);
		// Don't resolve to null
		//ensure(DepthStencilRenderTarget.ResolveTarget || DepthStore != ERenderTargetStoreAction::EStore);

		// Don't write to depth if read-only
		//ensure(DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite() || DepthStore != ERenderTargetStoreAction::EStore);
		// This is not true for stencil. VK and Metal specify that the DontCare store action MAY leave the attachment in an undefined state.
		/*ensure(DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite() || StencilStore != ERenderTargetStoreAction::EStore);*/

		// If we have a depthstencil target we MUST Store it or it will be undefined after rendering.
		if (DepthStencilRenderTarget.DepthStencilTarget->GetFormat() != PF_D24)
		{
			// If this is DepthStencil we must store it out unless we are absolutely sure it will never be used again.
			// it is valid to use a depthbuffer for performance and not need the results later.
			//ensure(StencilStore == ERenderTargetStoreAction::EStore);
		}

		if (DepthStencilRenderTarget.ExclusiveDepthStencil.IsDepthWrite())
		{
			// this check is incorrect for mobile, depth/stencil is intermediate and we don't want to store it to main memory
			//ensure(DepthStore == ERenderTargetStoreAction::EStore);
		}

		if (DepthStencilRenderTarget.ExclusiveDepthStencil.IsStencilWrite())
		{
			// this check is incorrect for mobile, depth/stencil is intermediate and we don't want to store it to main memory
			//ensure(StencilStore == ERenderTargetStoreAction::EStore);
		}
		
		if (SubpassHint == ESubpassHint::DepthReadSubpass)
		{
			// for depth read sub-pass
			// 1. render pass must have depth target
			// 2. depth target must support InputAttachement
			ensure(EnumHasAnyFlags(DepthStencilRenderTarget.DepthStencilTarget->GetFlags(), TexCreate_InputAttachmentRead));
		}
	}
	else
	{
		ensure(DepthStencilRenderTarget.Action == EDepthStencilTargetActions::DontLoad_DontStore);
		ensure(DepthStencilRenderTarget.ExclusiveDepthStencil == FExclusiveDepthStencil::DepthNop_StencilNop);
		ensure(SubpassHint != ESubpassHint::DepthReadSubpass);
	}
}
#endif

#define ValidateResourceDesc(expr, format, ...) \
	if (bFatal) \
	{ \
		checkf(expr, format, ##__VA_ARGS__); \
	} \
	else if (!(expr)) \
	{ \
		return false;\
	} \

// static
bool FRHITextureDesc::Validate(const FRHITextureCreateInfo& Desc, const TCHAR* Name, bool bFatal)
{
	// Validate texture's pixel format.
	{
		ValidateResourceDesc(Desc.Format != PF_Unknown, TEXT("Illegal to create texture %s with an invalid pixel format."), Name);
		ValidateResourceDesc(Desc.Format < PF_MAX, TEXT("Illegal to create texture %s with an invalid pixel format."), Name);
		ValidateResourceDesc(GPixelFormats[Desc.Format].Supported,
			TEXT("Failed to create texture %s with pixel format %s because it is not supported."),
			Name,
			GPixelFormats[Desc.Format].Name);
	}

	// Validate texture's extent.
	{
		int32 MaxDimension = (Desc.Dimension == ETextureDimension::TextureCube || Desc.Dimension == ETextureDimension::TextureCubeArray) ? GMaxCubeTextureDimensions : GMaxTextureDimensions;

		ValidateResourceDesc(Desc.Extent.X > 0, TEXT("Texture %s's Extent.X=%d is invalid."), Name, Desc.Extent.X);
		ValidateResourceDesc(Desc.Extent.X <= MaxDimension, TEXT("Texture %s's Extent.X=%d is too large."), Name, Desc.Extent.X);

		ValidateResourceDesc(Desc.Extent.Y > 0, TEXT("Texture %s's Extent.Y=%d is invalid."), Name, Desc.Extent.Y);
		ValidateResourceDesc(Desc.Extent.Y <= MaxDimension, TEXT("Texture %s's Extent.Y=%d is too large."), Name, Desc.Extent.Y);
	}

	// Validate texture's depth
	if (Desc.Dimension == ETextureDimension::Texture3D)
	{
		ValidateResourceDesc(Desc.Depth > 0, TEXT("Texture %s's Depth=%d is invalid."), Name, int32(Desc.Depth));
		ValidateResourceDesc(Desc.Depth <= GMaxTextureDimensions, TEXT("Texture %s's Extent.Depth=%d is too large."), Name, Desc.Depth);
	}
	else
	{
		ValidateResourceDesc(Desc.Depth == 1, TEXT("Texture %s's Depth=%d is invalid for Dimension=%s."), Name, int32(Desc.Depth), GetTextureDimensionString(Desc.Dimension));
	}

	// Validate texture's array size
	if (Desc.Dimension == ETextureDimension::Texture2DArray || Desc.Dimension == ETextureDimension::TextureCubeArray)
	{
		ValidateResourceDesc(Desc.ArraySize > 0, TEXT("Texture %s's ArraySize=%d is invalid."), Name, Desc.ArraySize);
		ValidateResourceDesc(Desc.ArraySize <= GMaxTextureArrayLayers, TEXT("Texture %s's Extent.ArraySize=%d is too large."), Name, int32(Desc.ArraySize));
	}
	else
	{
		ValidateResourceDesc(Desc.ArraySize == 1, TEXT("Texture %s's ArraySize=%d is invalid for Dimension=%s."), Name, Desc.ArraySize,  GetTextureDimensionString(Desc.Dimension));
	}

	// Validate texture's samples count.
	if (Desc.Dimension == ETextureDimension::Texture2D || Desc.Dimension == ETextureDimension::Texture2DArray)
	{
		ValidateResourceDesc(Desc.NumSamples > 0, TEXT("Texture %s's NumSamples=%d is invalid."), Name, Desc.NumSamples);
	}
	else
	{
		ValidateResourceDesc(Desc.NumSamples == 1, TEXT("Texture %s's NumSamples=%d is invalid for Dimension=%s."), Name, Desc.NumSamples, GetTextureDimensionString(Desc.Dimension));
	}

	// Validate texture's mips.
	if (Desc.IsMultisample())
	{
		ValidateResourceDesc(Desc.NumMips == 1, TEXT("MSAA Texture %s's can only have one mip."), Name);
	}
	else
	{
		ValidateResourceDesc(Desc.NumMips > 0, TEXT("Texture %s's NumMips=%d is invalid."), Name, Desc.NumMips);
		ValidateResourceDesc(Desc.NumMips <= GMaxTextureMipCount, TEXT("Texture %s's NumMips=%d is too large."), Name, Desc.NumMips);
	}

	return true;
}

// static
bool FRHITextureSRVCreateInfo::Validate(const FRHITextureDesc& TextureDesc, const FRHITextureSRVCreateInfo& TextureSRVDesc, const TCHAR* TextureName, bool bFatal)
{
	if (TextureName == nullptr)
	{
		TextureName = TEXT("UnnamedTexture");
	}

	ValidateResourceDesc(TextureDesc.Flags & TexCreate_ShaderResource,
		TEXT("Attempted to create SRV from texture %s which was not created with TexCreate_ShaderResource"),
		TextureName);

	// Validate the pixel format if overridden by the SRV's descriptor.
	if (TextureSRVDesc.Format == PF_X24_G8)
	{
		// PF_X24_G8 is a bit of mess in the RHI, used to read the stencil, but have varying BlockBytes.
		ValidateResourceDesc(TextureDesc.Format == PF_DepthStencil,
			TEXT("PF_X24_G8 is only to read stencil from a PF_DepthStencil texture"));
	}
	else if (TextureSRVDesc.Format != PF_Unknown)
	{
		ValidateResourceDesc(TextureSRVDesc.Format < PF_MAX,
			TEXT("Illegal to create SRV for texture %s with invalid FPooledRenderTargetDesc::Format."),
			TextureName);
		ValidateResourceDesc(GPixelFormats[TextureSRVDesc.Format].Supported,
			TEXT("Failed to create SRV for texture %s with pixel format %s because it is not supported."),
			TextureName, GPixelFormats[TextureSRVDesc.Format].Name);

		EPixelFormat ResourcePixelFormat = TextureDesc.Format;

		ValidateResourceDesc(
			GPixelFormats[TextureSRVDesc.Format].BlockBytes == GPixelFormats[ResourcePixelFormat].BlockBytes &&
			GPixelFormats[TextureSRVDesc.Format].BlockSizeX == GPixelFormats[ResourcePixelFormat].BlockSizeX &&
			GPixelFormats[TextureSRVDesc.Format].BlockSizeY == GPixelFormats[ResourcePixelFormat].BlockSizeY &&
			GPixelFormats[TextureSRVDesc.Format].BlockSizeZ == GPixelFormats[ResourcePixelFormat].BlockSizeZ,
			TEXT("Failed to create SRV for texture %s with pixel format %s because it does not match the byte size of the texture's pixel format %s."),
			TextureName, GPixelFormats[TextureSRVDesc.Format].Name, GPixelFormats[ResourcePixelFormat].Name);
	}

	ValidateResourceDesc((TextureSRVDesc.MipLevel + TextureSRVDesc.NumMipLevels) <= TextureDesc.NumMips,
		TEXT("Failed to create SRV at mips %d-%d: the texture %s has only %d mip levels."),
		TextureSRVDesc.MipLevel, (TextureSRVDesc.MipLevel + TextureSRVDesc.NumMipLevels), TextureName, TextureDesc.NumMips);

	// Validate the array sloces
	if (TextureDesc.IsTextureArray())
	{
		ValidateResourceDesc((TextureSRVDesc.FirstArraySlice + TextureSRVDesc.NumArraySlices) <= TextureDesc.ArraySize,
			TEXT("Failed to create SRV at array slices %d-%d: the texture array %s has only %d slices."),
			TextureSRVDesc.FirstArraySlice,
			(TextureSRVDesc.FirstArraySlice + TextureSRVDesc.NumArraySlices),
			TextureName,
			TextureDesc.ArraySize);
	}
	else
	{
		ValidateResourceDesc(TextureSRVDesc.FirstArraySlice == 0,
			TEXT("Failed to create SRV with FirstArraySlice=%d: the texture %s is not a texture array."),
			TextureSRVDesc.FirstArraySlice, TextureName);
		ValidateResourceDesc(TextureSRVDesc.NumArraySlices == 0,
			TEXT("Failed to create SRV with NumArraySlices=%d: the texture %s is not a texture array."),
			TextureSRVDesc.NumArraySlices, TextureName);
	}

	ValidateResourceDesc(TextureSRVDesc.MetaData != ERHITextureMetaDataAccess::FMask || GRHISupportsExplicitFMask,
		TEXT("Failed to create FMask SRV for texture %s because the current RHI doesn't support it. Be sure to gate the call with GRHISupportsExplicitFMask."),
		TextureName);

	ValidateResourceDesc(TextureSRVDesc.MetaData != ERHITextureMetaDataAccess::HTile || GRHISupportsExplicitHTile,
		TEXT("Failed to create HTile SRV for texture %s because the current RHI doesn't support it. Be sure to gate the call with GRHISupportsExplicitHTile."),
		TextureName);

	return true;
}

#undef ValidateResourceDesc

uint64 FRHITextureDesc::CalcMemorySizeEstimate(uint32 FirstMipIndex, uint32 LastMipIndex) const
{
#if DO_CHECK
	Validate(*this, TEXT("CalcMemorySizeEstimate"), /* bFatal = */true);
#endif
	check(FirstMipIndex < NumMips && FirstMipIndex <= LastMipIndex && LastMipIndex < NumMips);

	uint64 MemorySize = 0;
	for (uint32 MipIndex = FirstMipIndex; MipIndex <= LastMipIndex; ++MipIndex)
	{
		FIntVector MipSizeInBlocks = FIntVector(
			FMath::DivideAndRoundUp(FMath::Max(Extent.X >> MipIndex, 1), GPixelFormats[Format].BlockSizeX),
			FMath::DivideAndRoundUp(FMath::Max(Extent.Y >> MipIndex, 1), GPixelFormats[Format].BlockSizeY),
			FMath::DivideAndRoundUp(FMath::Max(Depth    >> MipIndex, 1), GPixelFormats[Format].BlockSizeZ)
		);

		uint32 NumBlocksInMip = MipSizeInBlocks.X * MipSizeInBlocks.Y * MipSizeInBlocks.Z;
		MemorySize += NumBlocksInMip * GPixelFormats[Format].BlockBytes;
	}

	MemorySize *= ArraySize;
	MemorySize *= NumSamples;

	if (IsTextureCube())
	{
		MemorySize *= 6;
	}

	return MemorySize;
}

static FRHIPanicEvent RHIPanicEvent;
FRHIPanicEvent& RHIGetPanicDelegate()
{
	return RHIPanicEvent;
}

#include "Misc/DataDrivenPlatformInfoRegistry.h"

FString LexToString(EShaderPlatform Platform, bool bError)
{
	switch (Platform)
	{
	case SP_PCD3D_SM6: return TEXT("PCD3D_SM6");
	case SP_PCD3D_SM5: return TEXT("PCD3D_SM5");
	case SP_PCD3D_ES3_1: return TEXT("PCD3D_ES3_1");
	case SP_OPENGL_PCES3_1: return TEXT("OPENGL_PCES3_1");
	case SP_OPENGL_ES3_1_ANDROID: return TEXT("OPENGL_ES3_1_ANDROID");
	case SP_METAL: return TEXT("METAL");
	case SP_METAL_MRT: return TEXT("METAL_MRT");
	case SP_METAL_TVOS: return TEXT("METAL_TVOS");
	case SP_METAL_MRT_TVOS: return TEXT("METAL_MRT_TVOS");
	case SP_METAL_MRT_MAC: return TEXT("METAL_MRT_MAC");
	case SP_METAL_SM5: return TEXT("METAL_SM5");
	case SP_METAL_MACES3_1: return TEXT("METAL_MACES3_1");
	case SP_VULKAN_ES3_1_ANDROID: return TEXT("VULKAN_ES3_1_ANDROID");
	case SP_VULKAN_PCES3_1: return TEXT("VULKAN_PCES3_1");
	case SP_VULKAN_SM5: return TEXT("VULKAN_SM5");
	case SP_VULKAN_SM5_ANDROID: return TEXT("VULKAN_SM5_ANDROID");
	case SP_D3D_ES3_1_HOLOLENS: return TEXT("D3D_ES3_1_HOLOLENS");
	case SP_CUSTOM_PLATFORM_FIRST: return TEXT("SP_CUSTOM_PLATFORM_FIRST");
	case SP_CUSTOM_PLATFORM_LAST: return TEXT("SP_CUSTOM_PLATFORM_LAST");

	default:
		if (FStaticShaderPlatformNames::IsStaticPlatform(Platform))
		{
			return FStaticShaderPlatformNames::Get().GetShaderPlatform(Platform).ToString();
		}
		else if (Platform > SP_CUSTOM_PLATFORM_FIRST && Platform < SP_CUSTOM_PLATFORM_LAST)
		{
			return FString::Printf(TEXT("SP_CUSTOM_PLATFORM_%d"), Platform - SP_CUSTOM_PLATFORM_FIRST);
		}
		else
		{
			checkf(!bError, TEXT("Unknown or removed EShaderPlatform %d!"), (int32)Platform);
			return TEXT("");
		}
	}
}

FString LexToString(EShaderPlatform Platform)
{
	bool bError = true;
	return LexToString(Platform, bError);
}

void LexFromString(EShaderPlatform& Value, const TCHAR* String)
{
	Value = EShaderPlatform::SP_NumPlatforms;

	for (int i = 0; i < (int)EShaderPlatform::SP_NumPlatforms; ++i)
	{
		if (LexToString((EShaderPlatform)i, false).Equals(String, ESearchCase::IgnoreCase))
		{
			Value = (EShaderPlatform)i;
			return;
		}
	}
}

FString LexToString(ERHIFeatureLevel::Type Level)
{
	switch (Level)
	{
		case ERHIFeatureLevel::ES2_REMOVED:
			return TEXT("ES2_REMOVED");
		case ERHIFeatureLevel::ES3_1:
			return TEXT("ES3_1");
		case ERHIFeatureLevel::SM4_REMOVED:
			return TEXT("SM4_REMOVED");
		case ERHIFeatureLevel::SM5:
			return TEXT("SM5");
		case ERHIFeatureLevel::SM6:
			return TEXT("SM6");
		default:
			break;
	}
	return TEXT("UnknownFeatureLevel");
}

const TCHAR* LexToString(ERHIDescriptorHeapType InHeapType)
{
	switch (InHeapType)
	{
	default: checkNoEntry();                   return TEXT("UnknownDescriptorHeapType");
	case ERHIDescriptorHeapType::Standard:     return TEXT("Standard");
	case ERHIDescriptorHeapType::RenderTarget: return TEXT("RenderTarget");
	case ERHIDescriptorHeapType::DepthStencil: return TEXT("DepthStencil");
	case ERHIDescriptorHeapType::Sampler:      return TEXT("Sampler");
	}
}

const FName LANGUAGE_D3D("D3D");
const FName LANGUAGE_Metal("Metal");
const FName LANGUAGE_OpenGL("OpenGL");
const FName LANGUAGE_Vulkan("Vulkan");
const FName LANGUAGE_Sony("Sony");
const FName LANGUAGE_Nintendo("Nintendo");

RHI_API FGenericDataDrivenShaderPlatformInfo FGenericDataDrivenShaderPlatformInfo::Infos[SP_NumPlatforms];

// Gets a string from a section, or empty string if it didn't exist
static inline FString GetSectionString(const FConfigSection& Section, FName Key)
{
	return Section.FindRef(Key).GetValue();
}

// Gets a bool from a section.  It returns the original value if the setting does not exist
static inline bool GetSectionBool(const FConfigSection& Section, FName Key, bool OriginalValue)
{
	const FConfigValue* ConfigValue = Section.Find(Key);
	if (ConfigValue != nullptr)
	{
		return FCString::ToBool(*ConfigValue->GetValue());
	}
	else
	{
		return OriginalValue;
	}
}

// Gets an integer from a section.  It returns the original value if the setting does not exist
static inline uint32 GetSectionUint(const FConfigSection& Section, FName Key, uint32 OriginalValue)
{
	const FConfigValue* ConfigValue = Section.Find(Key);
	if (ConfigValue != nullptr)
	{
		return (uint32)FCString::Atoi(*ConfigValue->GetValue());
	}
	else
	{
		return OriginalValue;
	}
}

// Gets an integer from a section.  It returns the original value if the setting does not exist
static inline uint32 GetSectionFeatureSupport(const FConfigSection& Section, FName Key, uint32 OriginalValue)
{
	const FConfigValue* ConfigValue = Section.Find(Key);
	if (ConfigValue != nullptr)
	{
		FString Value = ConfigValue->GetValue();
		if (Value == TEXT("Unsupported"))
		{
			return uint32(ERHIFeatureSupport::Unsupported);
		}
		if (Value == TEXT("RuntimeDependent"))
		{
			return uint32(ERHIFeatureSupport::RuntimeDependent);
		}
		else if (Value == TEXT("RuntimeGuaranteed"))
		{
			return uint32(ERHIFeatureSupport::RuntimeGuaranteed);
		}
		else
		{
			checkf(false, TEXT("Unknown ERHIFeatureSupport value \"%s\" for %s"), *Value, *Key.ToString());
		}
	}

	return OriginalValue;
}

FRHIViewableResource* GetViewableResource(const FRHITransitionInfo& Info)
{
	switch (Info.Type)
	{
	case FRHITransitionInfo::EType::Buffer:
	case FRHITransitionInfo::EType::Texture:
		return Info.ViewableResource;

	case FRHITransitionInfo::EType::UAV:
		return Info.UAV ? Info.UAV->GetParentResource() : nullptr;
	}

	return nullptr;
}

void FGenericDataDrivenShaderPlatformInfo::SetDefaultValues()
{
	MaxFeatureLevel = ERHIFeatureLevel::Num;
	bSupportsMSAA = true;
	bSupportsDOFHybridScattering = true;
	bSupportsHZBOcclusion = true;
	bSupportsWaterIndirectDraw = true;
	bSupportsAsyncPipelineCompilation = true;
	bSupportsManualVertexFetch = true;
	bSupportsVolumeTextureAtomics = true;
	PreviewShaderPlatformParent = EShaderPlatform::SP_NumPlatforms;
}

void FGenericDataDrivenShaderPlatformInfo::ParseDataDrivenShaderInfo(const FConfigSection& Section, FGenericDataDrivenShaderPlatformInfo& Info)
{
	Info.Language = *GetSectionString(Section, "Language");
	Info.ShaderFormat = *GetSectionString(Section, "ShaderFormat");
	checkf(!Info.ShaderFormat.IsNone(), TEXT("Missing ShaderFormat for ShaderPlatform %s  ShaderFormat %s"), *Info.Name.ToString(), *Info.ShaderFormat.ToString());

	GetFeatureLevelFromName(*GetSectionString(Section, "MaxFeatureLevel"), Info.MaxFeatureLevel);

	Info.ShaderPropertiesHash = 0;
	FString ShaderPropertiesString = Info.Name.GetPlainNameString();

#define ADD_TO_PROPERTIES_STRING(SettingName, SettingValue) \
	ShaderPropertiesString += TEXT(#SettingName); \
	ShaderPropertiesString += FString::Printf(TEXT("_%d"), SettingValue);

#define GET_SECTION_BOOL_HELPER(SettingName)	\
	Info.SettingName = GetSectionBool(Section, #SettingName, Info.SettingName);	\
	ADD_TO_PROPERTIES_STRING(SettingName, Info.SettingName)

#define GET_SECTION_INT_HELPER(SettingName)	\
	Info.SettingName = GetSectionUint(Section, #SettingName, Info.SettingName); \
	ADD_TO_PROPERTIES_STRING(SettingName, Info.SettingName)

#define GET_SECTION_SUPPORT_HELPER(SettingName)	\
	Info.SettingName = GetSectionFeatureSupport(Section, #SettingName, Info.SettingName); \
	ADD_TO_PROPERTIES_STRING(SettingName, Info.SettingName)

	GET_SECTION_BOOL_HELPER(bIsMobile);
	GET_SECTION_BOOL_HELPER(bIsMetalMRT);
	GET_SECTION_BOOL_HELPER(bIsPC);
	GET_SECTION_BOOL_HELPER(bIsConsole);
	GET_SECTION_BOOL_HELPER(bIsAndroidOpenGLES);
	GET_SECTION_BOOL_HELPER(bSupportsDebugViewShaders);
	GET_SECTION_BOOL_HELPER(bSupportsMobileMultiView);
	GET_SECTION_BOOL_HELPER(bSupportsArrayTextureCompression);
	GET_SECTION_BOOL_HELPER(bSupportsDistanceFields);
	GET_SECTION_BOOL_HELPER(bSupportsDiaphragmDOF);
	GET_SECTION_BOOL_HELPER(bSupportsRGBColorBuffer);
	GET_SECTION_BOOL_HELPER(bSupportsCapsuleShadows);
	GET_SECTION_BOOL_HELPER(bSupportsPercentageCloserShadows);
	GET_SECTION_BOOL_HELPER(bSupportsVolumetricFog);
	GET_SECTION_BOOL_HELPER(bSupportsIndexBufferUAVs);
	GET_SECTION_BOOL_HELPER(bSupportsInstancedStereo);
	GET_SECTION_SUPPORT_HELPER(SupportsMultiViewport);
	GET_SECTION_BOOL_HELPER(bSupportsMSAA);
	GET_SECTION_BOOL_HELPER(bSupports4ComponentUAVReadWrite);
	GET_SECTION_BOOL_HELPER(bSupportsRenderTargetWriteMask);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracing);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingShaders);
	GET_SECTION_BOOL_HELPER(bSupportsInlineRayTracing);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingCallableShaders);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingProceduralPrimitive);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingTraversalStatistics);
	GET_SECTION_BOOL_HELPER(bSupportsRayTracingIndirectInstanceData);
	GET_SECTION_BOOL_HELPER(bSupportsPathTracing);
	GET_SECTION_BOOL_HELPER(bSupportsHighEndRayTracingReflections);
	GET_SECTION_BOOL_HELPER(bSupportsByteBufferComputeShaders);
	GET_SECTION_BOOL_HELPER(bSupportsGPUScene);
	GET_SECTION_BOOL_HELPER(bSupportsPrimitiveShaders);
	GET_SECTION_BOOL_HELPER(bSupportsUInt64ImageAtomics);
	GET_SECTION_BOOL_HELPER(bRequiresVendorExtensionsForAtomics);
	GET_SECTION_BOOL_HELPER(bSupportsNanite);
	GET_SECTION_BOOL_HELPER(bSupportsLumenGI);
	GET_SECTION_BOOL_HELPER(bSupportsSSDIndirect);
	GET_SECTION_BOOL_HELPER(bSupportsTemporalHistoryUpscale);
	GET_SECTION_BOOL_HELPER(bSupportsRTIndexFromVS);
	GET_SECTION_BOOL_HELPER(bSupportsIntrinsicWaveOnce);
	GET_SECTION_BOOL_HELPER(bSupportsConservativeRasterization);
	GET_SECTION_SUPPORT_HELPER(bSupportsWaveOperations);
	GET_SECTION_INT_HELPER(MinimumWaveSize);
	GET_SECTION_INT_HELPER(MaximumWaveSize);
	GET_SECTION_BOOL_HELPER(bRequiresExplicit128bitRT);
	GET_SECTION_BOOL_HELPER(bSupportsGen5TemporalAA);
	GET_SECTION_BOOL_HELPER(bTargetsTiledGPU);
	GET_SECTION_BOOL_HELPER(bNeedsOfflineCompiler);
	GET_SECTION_BOOL_HELPER(bSupportsComputeFramework);
	GET_SECTION_BOOL_HELPER(bSupportsAnisotropicMaterials);
	GET_SECTION_BOOL_HELPER(bSupportsDualSourceBlending);
	GET_SECTION_BOOL_HELPER(bRequiresGeneratePrevTransformBuffer);
	GET_SECTION_BOOL_HELPER(bRequiresRenderTargetDuringRaster);
	GET_SECTION_BOOL_HELPER(bRequiresDisableForwardLocalLights);
	GET_SECTION_BOOL_HELPER(bCompileSignalProcessingPipeline);
	GET_SECTION_BOOL_HELPER(bSupportsMeshShadersTier0);
	GET_SECTION_BOOL_HELPER(bSupportsMeshShadersTier1);
	GET_SECTION_INT_HELPER(MaxMeshShaderThreadGroupSize);
	GET_SECTION_BOOL_HELPER(bSupportsPerPixelDBufferMask);
	GET_SECTION_BOOL_HELPER(bIsHlslcc);
	GET_SECTION_BOOL_HELPER(bSupportsDxc);
	GET_SECTION_BOOL_HELPER(bSupportsVariableRateShading);
	GET_SECTION_BOOL_HELPER(bIsSPIRV);
	GET_SECTION_INT_HELPER(NumberOfComputeThreads);

	GET_SECTION_BOOL_HELPER(bWaterUsesSimpleForwardShading);
	GET_SECTION_BOOL_HELPER(bSupportsHairStrandGeometry);
	GET_SECTION_BOOL_HELPER(bSupportsDOFHybridScattering);
	GET_SECTION_BOOL_HELPER(bNeedsExtraMobileFrames);
	GET_SECTION_BOOL_HELPER(bSupportsHZBOcclusion);
	GET_SECTION_BOOL_HELPER(bSupportsWaterIndirectDraw);
	GET_SECTION_BOOL_HELPER(bSupportsAsyncPipelineCompilation);
	GET_SECTION_BOOL_HELPER(bSupportsManualVertexFetch);
	GET_SECTION_BOOL_HELPER(bRequiresReverseCullingOnMobile);
	GET_SECTION_BOOL_HELPER(bOverrideFMaterial_NeedsGBufferEnabled);
	GET_SECTION_BOOL_HELPER(bSupportsMobileDistanceField);
	GET_SECTION_BOOL_HELPER(bSupportsFFTBloom);
	GET_SECTION_BOOL_HELPER(bSupportsVertexShaderLayer);
	GET_SECTION_BOOL_HELPER(bSupportsBindless);
	GET_SECTION_BOOL_HELPER(bSupportsVolumeTextureAtomics);
	GET_SECTION_BOOL_HELPER(bSupportsROV);
	GET_SECTION_BOOL_HELPER(bSupportsOIT);
	GET_SECTION_SUPPORT_HELPER(bSupportsRealTypes);
	GET_SECTION_INT_HELPER(EnablesHLSL2021ByDefault);
	GET_SECTION_BOOL_HELPER(bSupportsSceneDataCompressedTransforms);
	GET_SECTION_BOOL_HELPER(bSupportsSwapchainUAVs);
#undef GET_SECTION_BOOL_HELPER
#undef GET_SECTION_INT_HELPER
#undef GET_SECTION_SUPPORT_HELPER
#undef ADD_TO_PROPERTIES_STRING

	Info.ShaderPropertiesHash = GetTypeHash(ShaderPropertiesString);
#if WITH_EDITOR
	FTextStringHelper::ReadFromBuffer(*GetSectionString(Section, FName("FriendlyName")), Info.FriendlyName);
#endif
}

void FGenericDataDrivenShaderPlatformInfo::Initialize()
{
	static bool bInitialized = false;
	if (bInitialized)
	{
		return;
	}

	// look for the standard DataDriven ini files
	int32 NumDDInfoFiles = FDataDrivenPlatformInfoRegistry::GetNumDataDrivenIniFiles();
	int32 CustomShaderPlatform = EShaderPlatform::SP_CUSTOM_PLATFORM_FIRST;

	struct PlatformInfoAndPlatformEnum
	{
		FGenericDataDrivenShaderPlatformInfo Info;
		EShaderPlatform ShaderPlatform;
	};

	for (int32 Index = 0; Index < NumDDInfoFiles; Index++)
	{
		FConfigFile IniFile;
		FString PlatformName;

		FDataDrivenPlatformInfoRegistry::LoadDataDrivenIniFile(Index, IniFile, PlatformName);

		// now walk over the file, looking for ShaderPlatformInfo sections
		for (auto Section : IniFile)
		{
			if (Section.Key.StartsWith(TEXT("ShaderPlatform ")))
			{
				const FString& SectionName = Section.Key;

				EShaderPlatform ShaderPlatform;
				// get enum value for the string name
				LexFromString(ShaderPlatform, *SectionName.Mid(15));
				if (ShaderPlatform == EShaderPlatform::SP_NumPlatforms)
				{
#if DDPI_HAS_EXTENDED_PLATFORMINFO_DATA
					const bool bIsEnabled = FDataDrivenPlatformInfoRegistry::GetPlatformInfo(PlatformName).bEnabledForUse;
#else
					const bool bIsEnabled = true;
#endif
					UE_CLOG(bIsEnabled, LogRHI, Warning, TEXT("Found an unknown shader platform %s in a DataDriven ini file"), *SectionName.Mid(15));
					continue;
				}
				
				// at this point, we can start pulling information out
				Infos[ShaderPlatform].Name = *SectionName.Mid(15);
				ParseDataDrivenShaderInfo(Section.Value, Infos[ShaderPlatform]);	
				Infos[ShaderPlatform].bContainsValidPlatformInfo = true;

#if WITH_EDITOR
				for (const FPreviewPlatformMenuItem& Item : FDataDrivenPlatformInfoRegistry::GetAllPreviewPlatformMenuItems())
				{
					FName PreviewPlatformName = *(Infos[ShaderPlatform].Name).ToString();
					if (Item.ShaderPlatformToPreview == PreviewPlatformName)
					{
						EShaderPlatform PreviewShaderPlatform = EShaderPlatform(CustomShaderPlatform++);
						ParseDataDrivenShaderInfo(Section.Value, Infos[PreviewShaderPlatform]);
						if (!Item.OptionalFriendlyNameOverride.IsEmpty())
						{
							Infos[PreviewShaderPlatform].FriendlyName = Item.OptionalFriendlyNameOverride;
						}
						Infos[PreviewShaderPlatform].Name = Item.PreviewShaderPlatformName;
						Infos[PreviewShaderPlatform].PreviewShaderPlatformParent = ShaderPlatform;
						Infos[PreviewShaderPlatform].bIsPreviewPlatform = true;
						Infos[PreviewShaderPlatform].bContainsValidPlatformInfo = true;
					}
				}
#endif
			}
		}
	}
	bInitialized = true;
}

void FGenericDataDrivenShaderPlatformInfo::UpdatePreviewPlatforms()
{
	for (int i = 0; i < EShaderPlatform::SP_NumPlatforms; ++i)
	{
		EShaderPlatform ShaderPlatform = EShaderPlatform(i);
		if (IsValid(ShaderPlatform))
		{
			ERHIFeatureLevel::Type PreviewSPMaxFeatureLevel = Infos[ShaderPlatform].MaxFeatureLevel;
			EShaderPlatform EditorSPForPreviewMaxFeatureLevel = GShaderPlatformForFeatureLevel[PreviewSPMaxFeatureLevel];
			if (Infos[ShaderPlatform].bIsPreviewPlatform && EditorSPForPreviewMaxFeatureLevel < EShaderPlatform::SP_NumPlatforms)
			{
				Infos[ShaderPlatform].ShaderFormat = Infos[EditorSPForPreviewMaxFeatureLevel].ShaderFormat;
				Infos[ShaderPlatform].Language = Infos[EditorSPForPreviewMaxFeatureLevel].Language;
				Infos[ShaderPlatform].bIsHlslcc = Infos[EditorSPForPreviewMaxFeatureLevel].bIsHlslcc;
				Infos[ShaderPlatform].bSupportsDxc = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsDxc;
				Infos[ShaderPlatform].bSupportsGPUScene &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsGPUScene;
				Infos[ShaderPlatform].bIsPC = true;
				Infos[ShaderPlatform].bIsConsole = false;
				Infos[ShaderPlatform].bSupportsSceneDataCompressedTransforms = Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsSceneDataCompressedTransforms;
				Infos[ShaderPlatform].bSupportsNanite &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsNanite;
				Infos[ShaderPlatform].bSupportsLumenGI &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsLumenGI;
				Infos[ShaderPlatform].bSupportsUInt64ImageAtomics &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsUInt64ImageAtomics;
				Infos[ShaderPlatform].bSupportsGen5TemporalAA &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsGen5TemporalAA;
				Infos[ShaderPlatform].bSupportsMobileMultiView &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsMobileMultiView;
				Infos[ShaderPlatform].bSupportsInstancedStereo &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsInstancedStereo;
				Infos[ShaderPlatform].bSupportsManualVertexFetch &= Infos[EditorSPForPreviewMaxFeatureLevel].bSupportsManualVertexFetch;
				Infos[ShaderPlatform].bSupportsRenderTargetWriteMask = false;
				Infos[ShaderPlatform].bSupportsIntrinsicWaveOnce = false;
				Infos[ShaderPlatform].bSupportsDOFHybridScattering = false;
				Infos[ShaderPlatform].bSupports4ComponentUAVReadWrite = false;
				Infos[ShaderPlatform].bContainsValidPlatformInfo = true;
			}
		}
	}
}

#if WITH_EDITOR
FText FGenericDataDrivenShaderPlatformInfo::GetFriendlyName(const FStaticShaderPlatform Platform)
{
	if (IsRunningCommandlet() || GUsingNullRHI)
	{
		return FText();
	}
	check(IsValid(Platform));
	return Infos[Platform].FriendlyName;
}
#endif

const EShaderPlatform FGenericDataDrivenShaderPlatformInfo::GetShaderPlatformFromName(const FName ShaderPlatformName)
{
	for (int32 i = 0; i < SP_NumPlatforms; ++i)
	{
		const EShaderPlatform Platform = static_cast<EShaderPlatform>(i);
		if (!Infos[Platform].bContainsValidPlatformInfo)
		{
			continue;
		}

		if (Infos[Platform].Name == ShaderPlatformName)
		{
			return Platform;
		}
	}
	return SP_NumPlatforms;
}

//
//	MSAA sample offsets.
//
// https://docs.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_standard_multisample_quality_levels
FVector2f GRHIDefaultMSAASampleOffsets[1 + 2 + 4 + 8 + 16] = {
	// MSAA x1
	FVector2f(+0.0f / 8.0f, +0.0f / 8.0f),

	// MSAA x2
	FVector2f(+4.0f / 8.0f, +4.0f / 8.0f),
	FVector2f(-4.0f / 8.0f, -4.0f / 8.0f),

	// MSAA x4
	FVector2f(-2.0f / 8.0f, -6.0f / 8.0f),
	FVector2f(+6.0f / 8.0f, -2.0f / 8.0f),
	FVector2f(-6.0f / 8.0f, +2.0f / 8.0f),
	FVector2f(+2.0f / 8.0f, +6.0f / 8.0f),

	// MSAA x8
	FVector2f(+1.0f / 8.0f, -3.0f / 8.0f),
	FVector2f(-1.0f / 8.0f, +3.0f / 8.0f),
	FVector2f(+5.0f / 8.0f, +1.0f / 8.0f),
	FVector2f(-3.0f / 8.0f, -5.0f / 8.0f),
	FVector2f(-5.0f / 8.0f, +5.0f / 8.0f),
	FVector2f(-7.0f / 8.0f, -1.0f / 8.0f),
	FVector2f(+3.0f / 8.0f, +7.0f / 8.0f),
	FVector2f(+7.0f / 8.0f, -7.0f / 8.0f),

	// MSAA x16
	FVector2f(+1.0f / 8.0f, +1.0f / 8.0f),
	FVector2f(-1.0f / 8.0f, -3.0f / 8.0f),
	FVector2f(-3.0f / 8.0f, +2.0f / 8.0f),
	FVector2f(+4.0f / 8.0f, -1.0f / 8.0f),
	FVector2f(-5.0f / 8.0f, -2.0f / 8.0f),
	FVector2f(+2.0f / 8.0f, +5.0f / 8.0f),
	FVector2f(+5.0f / 8.0f, +3.0f / 8.0f),
	FVector2f(+3.0f / 8.0f, -5.0f / 8.0f),
	FVector2f(-2.0f / 8.0f, +6.0f / 8.0f),
	FVector2f(+0.0f / 8.0f, -7.0f / 8.0f),
	FVector2f(-4.0f / 8.0f, -6.0f / 8.0f),
	FVector2f(-6.0f / 8.0f, +4.0f / 8.0f),
	FVector2f(-8.0f / 8.0f, +0.0f / 8.0f),
	FVector2f(+7.0f / 8.0f, -4.0f / 8.0f),
	FVector2f(+6.0f / 8.0f, +7.0f / 8.0f),
	FVector2f(-7.0f / 8.0f, -8.0f / 8.0f),
};

int32 CalculateMSAASampleArrayIndex(int32 NumSamples, int32 SampleIndex)
{
	check(NumSamples > 0 && NumSamples <= 16);
	check(FMath::IsPowerOfTwo(NumSamples));
	check(SampleIndex < NumSamples);

	return NumSamples - 1 + SampleIndex;
}

void RHIInitDefaultPixelFormatCapabilities()
{
	for (FPixelFormatInfo& Info : GPixelFormats)
	{
		if (Info.Supported)
		{
			const EPixelFormat PixelFormat = Info.UnrealFormat;
			if (IsBlockCompressedFormat(PixelFormat))
			{
				// Block compressed formats should have limited capabilities
				EnumAddFlags(Info.Capabilities, EPixelFormatCapabilities::AnyTexture | EPixelFormatCapabilities::TextureMipmaps | EPixelFormatCapabilities::TextureLoad | EPixelFormatCapabilities::TextureSample | EPixelFormatCapabilities::TextureGather);
			}
			else
			{
				EnumAddFlags(Info.Capabilities, EPixelFormatCapabilities::AllTextureFlags | EPixelFormatCapabilities::AllBufferFlags | EPixelFormatCapabilities::UAV);
				if (!IsDepthOrStencilFormat(PixelFormat))
				{
					EnumRemoveFlags(Info.Capabilities, EPixelFormatCapabilities::DepthStencil);
				}
			}
		}
	}
}

//
//	CalculateImageBytes
//

SIZE_T CalculateImageBytes(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format)
{
	if ( Format == PF_A1 )
	{		
		// The number of bytes needed to store all 1 bit pixels in a line is the width of the image divided by the number of bits in a byte
		uint32 BytesPerLine = (SizeX + 7) / 8;
		// The number of actual bytes in a 1 bit image is the bytes per line of pixels times the number of lines
		return sizeof(uint8) * BytesPerLine * SizeY;
	}
	else if( SizeZ > 0 )
	{
		return GPixelFormats[Format].Get3DImageSizeInBytes(SizeX, SizeY, SizeZ);
	}
	else
	{
		return GPixelFormats[Format].Get2DImageSizeInBytes(SizeX, SizeY);
	}
}

FRHIShaderResourceView* FRHITextureViewCache::GetOrCreateSRV(FRHITexture* Texture, const FRHITextureSRVCreateInfo& SRVCreateInfo)
{
	for (const auto& KeyValue : SRVs)
	{
		if (KeyValue.Key == SRVCreateInfo)
		{
			return KeyValue.Value.GetReference();
		}
	}

	FShaderResourceViewRHIRef RHIShaderResourceView;

	if (SRVCreateInfo.MetaData != ERHITextureMetaDataAccess::None)
	{
		FRHITexture2D* Texture2D = Texture->GetTexture2D();
		check(Texture2D);

		switch (SRVCreateInfo.MetaData)
		{
		case ERHITextureMetaDataAccess::HTile:
			check(GRHISupportsExplicitHTile);
			RHIShaderResourceView = RHICreateShaderResourceViewHTile(Texture2D);
			break;

		case ERHITextureMetaDataAccess::FMask:
			RHIShaderResourceView = RHICreateShaderResourceViewFMask(Texture2D);
			break;

		case ERHITextureMetaDataAccess::CMask:
			RHIShaderResourceView = RHICreateShaderResourceViewWriteMask(Texture2D);
			break;
		}
	}

	if (!RHIShaderResourceView)
	{
		RHIShaderResourceView = RHICreateShaderResourceView(Texture, SRVCreateInfo);
	}

	check(RHIShaderResourceView);
	FRHIShaderResourceView* View = RHIShaderResourceView.GetReference();
	SRVs.Emplace(SRVCreateInfo, MoveTemp(RHIShaderResourceView));
	return View;
}

FRHIUnorderedAccessView* FRHITextureViewCache::GetOrCreateUAV(FRHITexture* Texture, const FRHITextureUAVCreateInfo& UAVCreateInfo)
{
	for (const auto& KeyValue : UAVs)
	{
		if (KeyValue.Key == UAVCreateInfo)
		{
			return KeyValue.Value.GetReference();
		}
	}

	FUnorderedAccessViewRHIRef RHIUnorderedAccessView;

	if (UAVCreateInfo.MetaData != ERHITextureMetaDataAccess::None)
	{
		FRHITexture2D* Texture2D = Texture->GetTexture2D();
		check(Texture2D);

		switch (UAVCreateInfo.MetaData)
		{
		case ERHITextureMetaDataAccess::HTile:
			check(GRHISupportsExplicitHTile);
			RHIUnorderedAccessView = RHICreateUnorderedAccessViewHTile(Texture2D);
			break;

		case ERHITextureMetaDataAccess::Stencil:
			RHIUnorderedAccessView = RHICreateUnorderedAccessViewStencil(Texture2D, UAVCreateInfo.MipLevel);
			break;
		}
	}

	if (!RHIUnorderedAccessView)
	{
		if (UAVCreateInfo.Format != PF_Unknown)
		{
			RHIUnorderedAccessView = RHICreateUnorderedAccessView(Texture, UAVCreateInfo.MipLevel, UAVCreateInfo.Format, UAVCreateInfo.FirstArraySlice, UAVCreateInfo.NumArraySlices);
		}
		else
		{
			RHIUnorderedAccessView = RHICreateUnorderedAccessView(Texture, UAVCreateInfo.MipLevel, UAVCreateInfo.FirstArraySlice, UAVCreateInfo.NumArraySlices);
		}
	}

	check(RHIUnorderedAccessView);
	FRHIUnorderedAccessView* View = RHIUnorderedAccessView.GetReference();
	UAVs.Emplace(UAVCreateInfo, MoveTemp(RHIUnorderedAccessView));
	return View;
}

FRHIShaderResourceView* FRHIBufferViewCache::GetOrCreateSRV(FRHIBuffer* Buffer, const FRHIBufferSRVCreateInfo& SRVCreateInfo)
{
	for (const auto& KeyValue : SRVs)
	{
		if (KeyValue.Key == SRVCreateInfo)
		{
			return KeyValue.Value.GetReference();
		}
	}

	FShaderResourceViewRHIRef RHIShaderResourceView;

	if (SRVCreateInfo.Format != PF_Unknown)
	{
		RHIShaderResourceView = RHICreateShaderResourceView(Buffer, SRVCreateInfo.BytesPerElement, SRVCreateInfo.Format);
	}
	else
	{
		RHIShaderResourceView = RHICreateShaderResourceView(Buffer);
	}

	FRHIShaderResourceView* View = RHIShaderResourceView.GetReference();
	SRVs.Emplace(SRVCreateInfo, MoveTemp(RHIShaderResourceView));
	return View;
}

FRHIUnorderedAccessView* FRHIBufferViewCache::GetOrCreateUAV(FRHIBuffer* Buffer, const FRHIBufferUAVCreateInfo& UAVCreateInfo)
{
	for (const auto& KeyValue : UAVs)
	{
		if (KeyValue.Key == UAVCreateInfo)
		{
			return KeyValue.Value.GetReference();
		}
	}

	FUnorderedAccessViewRHIRef RHIUnorderedAccessView;

	if (UAVCreateInfo.Format != PF_Unknown)
	{
		RHIUnorderedAccessView = RHICreateUnorderedAccessView(Buffer, UAVCreateInfo.Format);
	}
	else
	{
		RHIUnorderedAccessView = RHICreateUnorderedAccessView(Buffer, UAVCreateInfo.bSupportsAtomicCounter, UAVCreateInfo.bSupportsAppendBuffer);
	}

	FRHIUnorderedAccessView* View = RHIUnorderedAccessView.GetReference();
	UAVs.Emplace(UAVCreateInfo, MoveTemp(RHIUnorderedAccessView));
	return View;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void FRHITextureViewCache::SetDebugName(const TCHAR* DebugName)
{
	for (const auto& KeyValue : UAVs)
	{
		RHIBindDebugLabelName(KeyValue.Value, DebugName);
	}
}

void FRHIBufferViewCache::SetDebugName(const TCHAR* DebugName)
{
	for (const auto& KeyValue : UAVs)
	{
		RHIBindDebugLabelName(KeyValue.Value, DebugName);
	}
}

#endif

void FRHITransientTexture::Acquire(const TCHAR* InName, uint32 InPassIndex, uint64 InAcquireCycle)
{
	FRHITransientResource::Acquire(InName, InPassIndex, InAcquireCycle);
	ViewCache.SetDebugName(InName);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	RHIBindDebugLabelName(GetRHI(), InName);
#endif
}

void FRHITransientBuffer::Acquire(const TCHAR* InName, uint32 InPassIndex, uint64 InAcquireCycle)
{
	FRHITransientResource::Acquire(InName, InPassIndex, InAcquireCycle);
	ViewCache.SetDebugName(InName);

	// TODO: Add method to rename a buffer.
}

FDebugName::FDebugName()
	: Name()
	, Number(NAME_NO_NUMBER_INTERNAL)
{
}

FDebugName::FDebugName(FName InName)
	: Name(InName)
	, Number(NAME_NO_NUMBER_INTERNAL)
{
}

FDebugName::FDebugName(FName InName, int32 InNumber)
	: Name(InName)
	, Number(InNumber)
{
}

FDebugName::FDebugName(FMemoryImageName InName, int32 InNumber)
	: Name(InName)
	, Number(InNumber)
{
}

FDebugName& FDebugName::operator=(FName Other)
{
	Name = FMemoryImageName(Other);
	Number = NAME_NO_NUMBER_INTERNAL;
	return *this;
}

FString FDebugName::ToString() const
{
	FString Out;
	FName(Name).AppendString(Out);
	if (Number != NAME_NO_NUMBER_INTERNAL)
	{
		Out.Appendf(TEXT("_%u"), Number);
	}
	return Out;
}

void FDebugName::AppendString(FStringBuilderBase& Builder) const
{
	FName(Name).AppendString(Builder);
	if (Number != NAME_NO_NUMBER_INTERNAL)
	{
		Builder << '_' << Number;
	}
}

RHI_API void RHISetCurrentNumDrawCallPtr(FRHIDrawCallsStatPtr InNumDrawCallsRHIPtr)
{
	GCurrentNumDrawCallsRHIPtr = InNumDrawCallsRHIPtr;
}

RHI_API void RHIIncCurrentNumDrawCallPtr(uint32 GPUIndex)
{
	FPlatformAtomics::InterlockedIncrement(&(*GCurrentNumDrawCallsRHIPtr)[GPUIndex]);
}