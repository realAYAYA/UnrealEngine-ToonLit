// Copyright Epic Games, Inc. All Rights Reserved.
// .

#include "VulkanShaderFormat.h"

#include "hlslcc.h"
#include "RHIShaderFormatDefinitions.inl"
#include "ShaderCompilerCommon.h"
#include "ShaderCompilerDefinitions.h"
#include "ShaderParameterParser.h"
#include "ShaderPreprocessTypes.h"
#include "SpirvReflectCommon.h"
#include "VulkanCommon.h"

#include "VulkanThirdParty.h"
#include "VulkanBackend.h"
#include "VulkanShaderResources.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

inline bool IsVulkanShaderFormat(FName ShaderFormat)
{
	return ShaderFormat == NAME_VULKAN_ES3_1_ANDROID
		|| ShaderFormat == NAME_VULKAN_ES3_1
		|| ShaderFormat == NAME_VULKAN_SM5
		|| ShaderFormat == NAME_VULKAN_SM6
		|| ShaderFormat == NAME_VULKAN_SM5_ANDROID;
}

inline bool IsAndroidShaderFormat(FName ShaderFormat)
{
	return ShaderFormat == NAME_VULKAN_ES3_1_ANDROID
		|| ShaderFormat == NAME_VULKAN_SM5_ANDROID;
}

inline bool SupportsOfflineCompiler(FName ShaderFormat)
{
	return ShaderFormat == NAME_VULKAN_ES3_1_ANDROID
		|| ShaderFormat == NAME_VULKAN_ES3_1
		|| ShaderFormat == NAME_VULKAN_SM5_ANDROID;
}

enum class EVulkanShaderVersion
{
	ES3_1,
	ES3_1_ANDROID,
	SM5,
	SM5_ANDROID,
	SM6,
	Invalid,
};

inline EVulkanShaderVersion FormatToVersion(FName Format)
{
	if (Format == NAME_VULKAN_ES3_1)
	{
		return EVulkanShaderVersion::ES3_1;
	}
	else if (Format == NAME_VULKAN_ES3_1_ANDROID)
	{
		return EVulkanShaderVersion::ES3_1_ANDROID;
	}
	else if (Format == NAME_VULKAN_SM5_ANDROID)
	{
		return EVulkanShaderVersion::SM5_ANDROID;
	}
	else if (Format == NAME_VULKAN_SM5)
	{
		return EVulkanShaderVersion::SM5;
	}
	else if (Format == NAME_VULKAN_SM6)
	{
		return EVulkanShaderVersion::SM6;
	}
	else
	{
		FString FormatStr = Format.ToString();
		checkf(0, TEXT("Invalid shader format passed to Vulkan shader compiler: %s"), *FormatStr);
		return EVulkanShaderVersion::Invalid;
	}
}

inline CrossCompiler::FShaderConductorOptions::ETargetEnvironment GetMinimumTargetEnvironment(const FShaderCompilerInput& Input)
{
	const EVulkanShaderVersion ShaderVersion = FormatToVersion(Input.ShaderFormat);
	if (ShaderVersion == EVulkanShaderVersion::SM6)
	{
		return CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_3;
	}
	else if (Input.IsRayTracingShader() || Input.Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing))
	{
		return CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_2;
	}
	else
	{
		return CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_1;
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogVulkanShaderCompiler, Log, All); 

static bool Match(const ANSICHAR* &Str, ANSICHAR Char)
{
	if (*Str == Char)
	{
		++Str;
		return true;
	}

	return false;
}

template <typename T>
uint32 ParseNumber(const T* Str, bool bEmptyIsZero = false)
{
	check(Str);

	uint32 Num = 0;

	int32 Len = 0;
	// Find terminating character
	for(int32 Index=0; Index<128; Index++)
	{
		if(Str[Index] == 0)
		{
			Len = Index;
			break;
		}
	}

	if (Len == 0)
	{
		if (bEmptyIsZero)
		{
			return 0;
		}
		else
		{
			check(0);
		}
	}

	// Find offset to integer type
	int32 Offset = -1;
	for(int32 Index=0; Index<Len; Index++)
	{
		if (*(Str + Index) >= '0' && *(Str + Index) <= '9')
		{
			Offset = Index;
			break;
		}
	}

	// Check if we found a number
	check(Offset >= 0);

	Str += Offset;

	while (*(Str) && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}

	return Num;
}

static bool ContainsBinding(const FVulkanBindingTable& BindingTable, const FString& Name)
{
	for (const FVulkanBindingTable::FBinding& Binding : BindingTable.GetBindings())
	{
		if (Binding.Name == Name)
		{
			return true;
		}
	}

	return false;
}

static void GetResourceEntryFromUBMember(const FShaderResourceTableMap& ResourceTableMap, const FString& UBName, uint16 ResourceIndex, FUniformResourceEntry& OutEntry)
{
	for (const FUniformResourceEntry& Entry : ResourceTableMap.Resources)
	{
		if (Entry.GetUniformBufferName() == UBName && Entry.ResourceIndex == ResourceIndex)
		{
			OutEntry = Entry;
			return;
		}
	}

	check(0);
}


struct FPatchType
{
	int32	HeaderGlobalIndex;
	uint16	CombinedAliasIndex;
};


static const FString kBindlessCBPrefix = TEXT("__BindlessCB");
static const FString kBindlessHeapSuffix = TEXT("_Heap");
static FString GetBindlessUBNameFromHeap(const FString& HeapName)
{
	check(HeapName.StartsWith(kBindlessCBPrefix));
	check(HeapName.EndsWith(kBindlessHeapSuffix));

	int32 NameStart = HeapName.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromStart, kBindlessCBPrefix.Len() + 1);
	check(NameStart != INDEX_NONE);
	NameStart++;
	return HeapName.Mid(NameStart, HeapName.Len() - NameStart - kBindlessHeapSuffix.Len());
}


// A collection of states and data that is locked in at the top level call and doesn't change throughout the compilation process
struct FVulkanShaderCompilerInternalState
{
	FVulkanShaderCompilerInternalState(const FShaderCompilerInput& InInput, const FShaderParameterParser* InParameterParser)
		: Input(InInput)
		, ParameterParser(InParameterParser)
		, Version(FormatToVersion(Input.ShaderFormat))
		, MinimumTargetEnvironment(GetMinimumTargetEnvironment(InInput))
		, bStripReflect(InInput.IsRayTracingShader() || (IsAndroidShaderFormat(Input.ShaderFormat) && InInput.Environment.GetCompileArgument(TEXT("STRIP_REFLECT_ANDROID"), true)))
		, bUseBindlessUniformBuffer(InInput.IsRayTracingShader() && ((EShaderFrequency)InInput.Target.Frequency != SF_RayGen))
		, bIsRayHitGroupShader(InInput.IsRayTracingShader() && ((EShaderFrequency)InInput.Target.Frequency == SF_RayHitGroup))
		, bSupportsBindless(InInput.Environment.CompilerFlags.Contains(CFLAG_BindlessResources) || InInput.Environment.CompilerFlags.Contains(CFLAG_BindlessSamplers))
		, bDebugDump(InInput.DumpDebugInfoEnabled())
	{
		if (bIsRayHitGroupShader)
		{
			UE::ShaderCompilerCommon::ParseRayTracingEntryPoint(Input.EntryPointName, ClosestHitEntry, AnyHitEntry, IntersectionEntry);
			checkf(!ClosestHitEntry.IsEmpty(), TEXT("All hit groups must contain at least a closest hit shader module"));
		}
	}

	const FShaderCompilerInput& Input;
	const FShaderParameterParser* ParameterParser;

	const EVulkanShaderVersion Version;
	const CrossCompiler::FShaderConductorOptions::ETargetEnvironment MinimumTargetEnvironment;

	const bool bStripReflect;
	const bool bUseBindlessUniformBuffer;
	const bool bIsRayHitGroupShader;

	const bool bSupportsBindless;
	const bool bDebugDump;

	// Ray tracing specific states
	enum class EHitGroupShaderType
	{
		None,
		ClosestHit,
		AnyHit,
		Intersection
	};
	EHitGroupShaderType HitGroupShaderType = EHitGroupShaderType::None;
	FString ClosestHitEntry;
	FString AnyHitEntry;
	FString IntersectionEntry;

	TArray<FString> AllBindlessUBs;

	// Forwarded calls for convenience
	inline EShaderFrequency GetShaderFrequency() const
	{
		return static_cast<EShaderFrequency>(Input.Target.Frequency);
	}
	inline const FString& GetEntryPointName() const
	{
		if (bIsRayHitGroupShader)
		{
			switch (HitGroupShaderType)
			{
			case EHitGroupShaderType::AnyHit: 
				return AnyHitEntry;
			case EHitGroupShaderType::Intersection:
				return IntersectionEntry;
			case EHitGroupShaderType::ClosestHit:
				return ClosestHitEntry;

			case EHitGroupShaderType::None:
				[[fallthrough]];
			default:
				return Input.EntryPointName;
			};
		}
		else
		{
			return Input.EntryPointName;
		}
	}
	inline bool IsRayTracingShader() const
	{
		return Input.IsRayTracingShader();
	}
	inline bool UseRootParametersStructure() const
	{
		// Only supported for RayGen currently
		return (GetShaderFrequency() == SF_RayGen) && (Input.RootParametersStructure != nullptr);
	}
	inline bool IsSM6() const
	{
		return (Version == EVulkanShaderVersion::SM6);
	}
	inline bool IsSM5() const
	{
		return (Version == EVulkanShaderVersion::SM5) || (Version == EVulkanShaderVersion::SM5_ANDROID);
	}
	inline bool IsMobileES31() const
	{
		return (Version == EVulkanShaderVersion::ES3_1 || Version == EVulkanShaderVersion::ES3_1_ANDROID);
	}
	inline EHlslShaderFrequency GetHlslShaderFrequency() const
	{
		const EHlslShaderFrequency FrequencyTable[] =
		{
			HSF_VertexShader,
			HSF_InvalidFrequency,
			HSF_InvalidFrequency,
			HSF_PixelShader,
			(IsSM5() || IsSM6()) ? HSF_GeometryShader : HSF_InvalidFrequency,
			HSF_ComputeShader,
			(IsSM5() || IsSM6()) ? HSF_RayGen : HSF_InvalidFrequency,
			(IsSM5() || IsSM6()) ? HSF_RayMiss : HSF_InvalidFrequency,
			(IsSM5() || IsSM6()) ? HSF_RayHitGroup : HSF_InvalidFrequency,
			(IsSM5() || IsSM6()) ? HSF_RayCallable : HSF_InvalidFrequency,
		};
		return FrequencyTable[Input.Target.Frequency];
	}
	inline FString GetDebugName() const
	{
		return Input.DumpDebugInfoPath.Right(Input.DumpDebugInfoPath.Len() - Input.DumpDebugInfoRootPath.Len());
	}
	inline bool HasMultipleEntryPoints() const
	{
		return !ClosestHitEntry.IsEmpty() && (!AnyHitEntry.IsEmpty() || !IntersectionEntry.IsEmpty());
	}
	inline FString GetSPVExtension() const
	{
		switch (HitGroupShaderType)
		{
		case EHitGroupShaderType::AnyHit:
			return TEXT("anyhit.spv");
		case EHitGroupShaderType::Intersection:
			return TEXT("intersection.spv");
		case EHitGroupShaderType::ClosestHit:
			return TEXT("closesthit.spv");
		case EHitGroupShaderType::None: 
			[[fallthrough]];
		default:
			return TEXT("spv");
		};
	}
};

// Data structures that will get serialized into ShaderCompilerOutput
struct VulkanShaderCompilerSerializedOutput
{
	VulkanShaderCompilerSerializedOutput()
		: Header(FVulkanShaderHeader::EZero)
	{
	}

	FVulkanShaderHeader Header;
	FShaderResourceTable ShaderResourceTable;
	FVulkanSpirv Spirv;

	TSet<FString> UsedBindlessUB;
};


static int32 AddGlobal( const TArray<VkDescriptorType>& DescriptorTypes,
						const FString& ParameterName,
						uint16 BindingIndex,
						VulkanShaderCompilerSerializedOutput& SerializedOutput, 
						const TArray<FString>& GlobalNames
)
{
	const int32 HeaderGlobalIndex = GlobalNames.Find(ParameterName);
	check(HeaderGlobalIndex != INDEX_NONE);
	check(GlobalNames[HeaderGlobalIndex] == ParameterName);

	FVulkanShaderHeader::FGlobalInfo& GlobalInfo = SerializedOutput.Header.Globals[HeaderGlobalIndex];
	const FVulkanSpirv::FEntry* Entry = SerializedOutput.Spirv.GetEntry(ParameterName);
	if (Entry)
	{
		if (Entry->Binding == -1)
		{
			// Texel buffers get put into a uniform block
			Entry = SerializedOutput.Spirv.GetEntry(ParameterName + TEXT("_BUFFER"));
			check(Entry);
			check(Entry->Binding != -1);
		}
	}
	else
	{
		Entry = SerializedOutput.Spirv.GetEntryByBindingIndex(BindingIndex);
		check(Entry);
		check(Entry->Binding != -1);
		if (!Entry->Name.EndsWith(TEXT("_BUFFER")))
		{
			checkf(false, TEXT("CombinedSamplers should not be used anymore"))
		}
	}

	const VkDescriptorType DescriptorType = DescriptorTypes[Entry->Binding];

	GlobalInfo.OriginalBindingIndex = Entry->Binding;
	SerializedOutput.Header.GlobalSpirvInfos[HeaderGlobalIndex] = FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex);

	const int32 GlobalDescriptorTypeIndex = SerializedOutput.Header.GlobalDescriptorTypes.Add(DescriptorTypeToBinding(DescriptorType));
	GlobalInfo.TypeIndex = GlobalDescriptorTypeIndex;
	GlobalInfo.CombinedSamplerStateAliasIndex = UINT16_MAX;

#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	GlobalInfo.DebugName = ParameterName;
#endif

	return HeaderGlobalIndex;
}

static void AddUBResources( const FString& UBName,
							const FShaderResourceTableMap& ResourceTableMap,
							uint32 BufferIndex,
							const TArray<uint32>& BindingArray,
							const TArray<VkDescriptorType>& DescriptorTypes,
							FVulkanShaderHeader::FUniformBufferInfo& OutUBInfo,
							VulkanShaderCompilerSerializedOutput& SerializedOutput,
							TArray<FString>& GlobalNames)
{
	if (BindingArray.Num() > 0)
	{
		uint32 BufferOffset = BindingArray[BufferIndex];
		if (BufferOffset > 0)
		{
			// Extract all resources related to the current BufferIndex
			const uint32* ResourceInfos = &BindingArray[BufferOffset];
			uint32 ResourceInfo = *ResourceInfos++;
			do
			{
				// Verify that we have correct buffer index
				check(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

				// Extract binding index from ResourceInfo
				const uint32 BindingIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

				// Extract index of the resource stored in the resource table from ResourceInfo
				const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

				FUniformResourceEntry ResourceTableEntry;
				GetResourceEntryFromUBMember(ResourceTableMap, UBName, ResourceIndex, ResourceTableEntry);

				FVulkanShaderHeader::FUBResourceInfo& UBResourceInfo = OutUBInfo.ResourceEntries.AddZeroed_GetRef();;

				const int32 HeaderGlobalIndex = AddGlobal(DescriptorTypes, ResourceTableEntry.UniformBufferMemberName, BindingIndex, SerializedOutput, GlobalNames);
				UBResourceInfo.SourceUBResourceIndex = ResourceIndex;
				UBResourceInfo.OriginalBindingIndex = BindingIndex;
				UBResourceInfo.GlobalIndex = HeaderGlobalIndex;
				UBResourceInfo.UBBaseType = (EUniformBufferBaseType)ResourceTableEntry.Type;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
				UBResourceInfo.DebugName = ResourceTableEntry.UniformBufferMemberName;
#endif
				// Iterate to next info
				ResourceInfo = *ResourceInfos++;
			}
			while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
		}
	}
}

static void AddUniformBuffer(
	const FShaderCompilerResourceTable& ShaderResourceTable,
	const TArray<VkDescriptorType>& DescriptorTypes,
	const FShaderCompilerInput& ShaderInput,
	const FString& UBName,
	uint16 BindingIndex,
	FShaderParameterMap& InOutParameterMap,
	VulkanShaderCompilerSerializedOutput& SerializedOutput,
	TArray<FString>& GlobalNames
)
{
	FVulkanShaderHeader& OutHeader = SerializedOutput.Header;

	const int32 HeaderUBIndex = OutHeader.UniformBuffers.AddZeroed();
	FVulkanShaderHeader::FUniformBufferInfo& UBInfo = OutHeader.UniformBuffers[HeaderUBIndex];

	const FUniformBufferEntry* UniformBufferEntry = ShaderInput.Environment.UniformBufferMap.Find(UBName);
	if (UniformBufferEntry)
	{
		UBInfo.LayoutHash = UniformBufferEntry->LayoutHash;
	}
	else if ((UBName == FShaderParametersMetadata::kRootUniformBufferBindingName) && ShaderInput.RootParametersStructure)
	{
		UBInfo.LayoutHash = ShaderInput.RootParametersStructure->GetLayoutHash();
	}
	else
	{
		UBInfo.LayoutHash = 0;
	}

#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	UBInfo.DebugName = UBName;
#endif

	const FVulkanSpirv::FEntry* Entry = SerializedOutput.Spirv.GetEntry(UBName);
	if (Entry)
	{
		UBInfo.bOnlyHasResources = false;
		UBInfo.ConstantDataOriginalBindingIndex = BindingIndex;
		
		int32 SpirvInfoIndex = OutHeader.UniformBufferSpirvInfos.Add(FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex));
		check(SpirvInfoIndex == HeaderUBIndex);
	}
	else
	{
		UBInfo.bOnlyHasResources = true;
		UBInfo.ConstantDataOriginalBindingIndex = UINT16_MAX;

		int32 SpirvInfoIndex = OutHeader.UniformBufferSpirvInfos.Add(FVulkanShaderHeader::FSpirvInfo());
		check(SpirvInfoIndex == HeaderUBIndex);
	}

	// Add used resources...
	if (ShaderResourceTable.ResourceTableBits & (1 << BindingIndex))
	{
		// Make sure to process in the same order as when gathering names below
		AddUBResources(UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, ShaderResourceTable.TextureMap, DescriptorTypes, UBInfo, SerializedOutput, GlobalNames);
		AddUBResources(UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, ShaderResourceTable.SamplerMap, DescriptorTypes, UBInfo, SerializedOutput, GlobalNames);
		AddUBResources(UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, ShaderResourceTable.ShaderResourceViewMap, DescriptorTypes, UBInfo, SerializedOutput, GlobalNames);
		AddUBResources(UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, ShaderResourceTable.UnorderedAccessViewMap, DescriptorTypes, UBInfo, SerializedOutput, GlobalNames);
	}
	else
	{
		// If we're using real uniform buffers we have to have resources at least
		checkf(!UBInfo.bOnlyHasResources, TEXT("UBName = %s, BindingIndex = %d"), *UBName, (int32)BindingIndex);
	}

	// Currently we don't support mismatched uniform buffer layouts/cbuffers with resources!
	check(UniformBufferEntry || UBInfo.ResourceEntries.Num() == 0);

	InOutParameterMap.RemoveParameterAllocation(*UBName);
	InOutParameterMap.AddParameterAllocation(*UBName, HeaderUBIndex, (uint16)FVulkanShaderHeader::UniformBuffer, 1, EShaderParameterType::UniformBuffer);
}

static int32 DoAddGlobal(const FString& Name, FVulkanShaderHeader& OutHeader, TArray<FString>& OutGlobalNames)
{
	check(!OutGlobalNames.Contains(Name));
	const int32 NameIndex = OutGlobalNames.Add(Name);
	const int32 GlobalIndex = OutHeader.Globals.AddDefaulted();
	check(NameIndex == GlobalIndex);
	const int32 GlobalSpirvIndex = OutHeader.GlobalSpirvInfos.AddDefaulted();
	check(GlobalSpirvIndex == GlobalIndex);
	return GlobalIndex;
}

static void PrepareUBResourceEntryGlobals(const TArray<uint32>& BindingArray, const FShaderResourceTableMap& ResourceTableMap,
	int32 BufferIndex, const FString& UBName, TArray<FString>& OutGlobalNames, FVulkanShaderHeader& OutHeader)
{
	if (BindingArray.Num() > 0)
	{
		uint32 BufferOffset = BindingArray[BufferIndex];
		if (BufferOffset > 0)
		{
			// Extract all resources related to the current BufferIndex
			const uint32* ResourceInfos = &BindingArray[BufferOffset];
			uint32 ResourceInfo = *ResourceInfos++;
			do
			{
				// Verify that we have correct buffer index
				check(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);

				// Extract index of the resource stored in the resource table from ResourceInfo
				const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

				FUniformResourceEntry ResourceTableEntry;
				GetResourceEntryFromUBMember(ResourceTableMap, UBName, ResourceIndex, ResourceTableEntry);

				DoAddGlobal(ResourceTableEntry.UniformBufferMemberName, OutHeader, OutGlobalNames);

				// Iterate to next info
				ResourceInfo = *ResourceInfos++;
			}
			while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
		}
	}
}


static void PrepareGlobals(const FVulkanBindingTable& BindingTable, const FSpirvReflectBindings& SpirvReflectBindings, const FShaderCompilerResourceTable& SRT, const TMap<FString, FVulkanShaderHeader::EType>& EntryTypes, const FShaderCompilerInput& ShaderInput, const TArray<FString>& ParameterNames, FShaderParameterMap& ParameterMap, TArray<FString>& OutGlobalNames, FVulkanShaderHeader& OutHeader)
{
	auto IsSamplerState = [&SpirvReflectBindings](const FString& ParameterName)
	{
		for (SpvReflectDescriptorBinding* DescriptorBinding : SpirvReflectBindings.Samplers)
		{
			if (ParameterName == DescriptorBinding->name)
			{
				return true;
			}
		}
		return false;
	};

	// First pass, gather names for all the Globals that are NOT Samplers
	{
		auto AddGlobalNamesForUB = [&](const FString& ParameterName)
		{
			TOptional<FParameterAllocation> ParameterAllocation = ParameterMap.FindParameterAllocation(*ParameterName);
			checkf(ParameterAllocation.IsSet(), TEXT("PrepareGlobals failed to find resource ParameterName=%s"), *ParameterName);

			// Add used resources...
			if (SRT.ResourceTableBits & (1 << ParameterAllocation->BufferIndex))
			{
				PrepareUBResourceEntryGlobals(SRT.TextureMap, ShaderInput.Environment.ResourceTableMap, ParameterAllocation->BufferIndex, ParameterName, OutGlobalNames, OutHeader);
				PrepareUBResourceEntryGlobals(SRT.ShaderResourceViewMap, ShaderInput.Environment.ResourceTableMap, ParameterAllocation->BufferIndex, ParameterName, OutGlobalNames, OutHeader);
				PrepareUBResourceEntryGlobals(SRT.UnorderedAccessViewMap, ShaderInput.Environment.ResourceTableMap, ParameterAllocation->BufferIndex, ParameterName, OutGlobalNames, OutHeader);
			}
		};

		for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
		{
			const FString& ParameterName = ParameterNames[ParameterIndex];
			const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
			if (FoundType)
			{
				switch (*FoundType)
				{
				case FVulkanShaderHeader::Global:
					if (!IsSamplerState(ParameterName))
					{
						DoAddGlobal(ParameterName, OutHeader, OutGlobalNames);
					}
					break;
				case FVulkanShaderHeader::UniformBuffer:
					AddGlobalNamesForUB(ParameterName);
					break;
				case FVulkanShaderHeader::PackedGlobal:
					// Ignore
					break;
				default:
					check(0);
					break;
				}
			}
			else
			{
				AddGlobalNamesForUB(ParameterName);
			}
		}
	}

	// Second pass, add all samplers
	{
		auto AddGlobalNamesForUB = [&](const FString& ParameterName)
		{
			TOptional<FParameterAllocation> ParameterAllocation = ParameterMap.FindParameterAllocation(*ParameterName);
			checkf(ParameterAllocation.IsSet(), TEXT("PrepareGlobals failed to find sampler ParameterName=%s"), *ParameterName);

			// Add used resources...
			if (SRT.ResourceTableBits & (1 << ParameterAllocation->BufferIndex))
			{
				PrepareUBResourceEntryGlobals(SRT.SamplerMap, ShaderInput.Environment.ResourceTableMap, ParameterAllocation->BufferIndex, ParameterName, OutGlobalNames, OutHeader);
			}
		};

		for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
		{
			const FString& ParameterName = ParameterNames[ParameterIndex];
			const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
			if (FoundType)
			{
				switch (*FoundType)
				{
				case FVulkanShaderHeader::Global:
					if (IsSamplerState(ParameterName))
					{
						DoAddGlobal(ParameterName, OutHeader, OutGlobalNames);
					}
					break;
				case FVulkanShaderHeader::UniformBuffer:
					AddGlobalNamesForUB(ParameterName);
					break;
				case FVulkanShaderHeader::PackedGlobal:
					break;
				default:
					check(0);
					break;
				}
			}
			else
			{
				AddGlobalNamesForUB(ParameterName);
			}
		}
	}

	// Now input attachments
	if (BindingTable.InputAttachmentsMask != 0)
	{
		uint32 InputAttachmentsMask = BindingTable.InputAttachmentsMask;
		for (int32 Index = 0; InputAttachmentsMask != 0; ++Index, InputAttachmentsMask>>= 1)
		{
			if (InputAttachmentsMask & 1)
			{
				DoAddGlobal(VULKAN_SUBPASS_FETCH_VAR_W[Index], OutHeader, OutGlobalNames);
			}
		}
	}
}

static void ConvertToHeader(
	FShaderCompilerResourceTable& ShaderResourceTable,
	const FVulkanBindingTable& BindingTable,
	const TArray<VkDescriptorType>& DescriptorTypes,
	const TMap<FString, FVulkanShaderHeader::EType>& EntryTypes,
	const FShaderCompilerInput& ShaderInput,
	const FSpirvReflectBindings& SpirvReflectBindings,
	FShaderParameterMap& InOutParameterMap,
	VulkanShaderCompilerSerializedOutput& SerializedOutput
)
{
	FVulkanShaderHeader& OutHeader = SerializedOutput.Header;

	// Names that match the Header.Globals array
	TArray<FString> GlobalNames;

	TArray<FString> ParameterNames;
	InOutParameterMap.GetAllParameterNames(ParameterNames);

	PrepareGlobals(BindingTable, SpirvReflectBindings, ShaderResourceTable, EntryTypes, ShaderInput, ParameterNames, InOutParameterMap, GlobalNames, OutHeader);

	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		const FString& ParameterName = *ParameterNames[ParameterIndex];
		const bool bFoundParam = InOutParameterMap.FindParameterAllocation(*ParameterName, BufferIndex, BaseIndex, Size);
		check(bFoundParam);

		const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
		if (FoundType)
		{
			switch (*FoundType)
			{
			case FVulkanShaderHeader::Global:
				{
					const int32 HeaderGlobalIndex = AddGlobal(DescriptorTypes, ParameterName, BaseIndex, SerializedOutput, GlobalNames);

					const FParameterAllocation* ParameterAllocation = InOutParameterMap.GetParameterMap().Find(*ParameterName);
					check(ParameterAllocation);
					const EShaderParameterType ParamType = ParameterAllocation->Type;

					InOutParameterMap.RemoveParameterAllocation(*ParameterName);
					InOutParameterMap.AddParameterAllocation(*ParameterName, (uint16)FVulkanShaderHeader::Global, HeaderGlobalIndex, Size, ParamType);
				}
				break;
			case FVulkanShaderHeader::PackedGlobal:
				{
					FVulkanShaderHeader::FPackedGlobalInfo& PackedGlobalInfo = OutHeader.PackedGlobals.AddZeroed_GetRef();
					PackedGlobalInfo.PackedTypeIndex = CrossCompiler::EPackedTypeIndex::HighP;
					PackedGlobalInfo.PackedUBIndex = BufferIndex;
					checkf(Size > 0, TEXT("Assertion failed for shader parameter: %s"), *ParameterName);
					PackedGlobalInfo.ConstantDataSizeInFloats = Size / sizeof(float);
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
					PackedGlobalInfo.DebugName = ParameterName;
#endif
					// Keep the original parameter info from InOutParameterMap as it's a shortcut into the packed global array!
				}
				break;
			case FVulkanShaderHeader::UniformBuffer:
				AddUniformBuffer(ShaderResourceTable, DescriptorTypes, ShaderInput, ParameterName, BufferIndex, InOutParameterMap, SerializedOutput, GlobalNames);
				break;
			default:
				check(0);
				break;
			}
		}
		else
		{
			// Not found means it's a new resource-only UniformBuffer
			AddUniformBuffer(ShaderResourceTable, DescriptorTypes, ShaderInput, ParameterName, BufferIndex, InOutParameterMap, SerializedOutput, GlobalNames);
		}
	}

	// Finally check for subpass/input attachments
	if (BindingTable.InputAttachmentsMask != 0)
	{
		const static FVulkanShaderHeader::EAttachmentType AttachmentTypes[] = 
		{
			FVulkanShaderHeader::EAttachmentType::Depth,
			FVulkanShaderHeader::EAttachmentType::Color0,
			FVulkanShaderHeader::EAttachmentType::Color1,
			FVulkanShaderHeader::EAttachmentType::Color2,
			FVulkanShaderHeader::EAttachmentType::Color3,
			FVulkanShaderHeader::EAttachmentType::Color4,
			FVulkanShaderHeader::EAttachmentType::Color5,
			FVulkanShaderHeader::EAttachmentType::Color6,
			FVulkanShaderHeader::EAttachmentType::Color7
		};

		uint32 InputAttachmentsMask = BindingTable.InputAttachmentsMask;
		for (int32 Index = 0; InputAttachmentsMask != 0; ++Index, InputAttachmentsMask>>=1)
		{
			if ((InputAttachmentsMask & 1) == 0)
			{
				continue;
			}

			const FString AttachmentName(VULKAN_SUBPASS_FETCH_VAR_W[Index]);
			const VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			const FVulkanShaderHeader::EAttachmentType AttachmentType = AttachmentTypes[Index];
			{
				const int32 HeaderGlobalIndex = GlobalNames.Find(AttachmentName);
				check(HeaderGlobalIndex != INDEX_NONE);
				check(GlobalNames[HeaderGlobalIndex] == AttachmentName);
				FVulkanShaderHeader::FGlobalInfo& GlobalInfo = OutHeader.Globals[HeaderGlobalIndex];
				const FVulkanSpirv::FEntry* Entry = SerializedOutput.Spirv.GetEntry(AttachmentName);
				check(Entry);
				check(Entry->Binding != -1);

				GlobalInfo.OriginalBindingIndex = Entry->Binding;
				OutHeader.GlobalSpirvInfos[HeaderGlobalIndex] = FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex);
				const int32 GlobalDescriptorTypeIndex = OutHeader.GlobalDescriptorTypes.Add(DescriptorTypeToBinding(DescriptorType));
				GlobalInfo.TypeIndex = GlobalDescriptorTypeIndex;
				GlobalInfo.CombinedSamplerStateAliasIndex = UINT16_MAX;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
				GlobalInfo.DebugName = AttachmentName;
#endif
				FVulkanShaderHeader::FInputAttachment& AttachmentInfo = OutHeader.InputAttachments.AddZeroed_GetRef();
				AttachmentInfo.GlobalIndex = HeaderGlobalIndex;
				AttachmentInfo.Type = AttachmentType;
			}
		}
	}
}


// Fills the SRT using final values kept in the FVulkanShaderHeader.
// NOTE: Uses GlobalIndex so it can be consumed directly at runtime.
// NOTE: Keep in sync with BuildResourceTableMapping.
static FShaderResourceTable BuildSRTFromHeader(const FVulkanShaderHeader& NEWHeader)
{
	FShaderResourceTable ShaderResourceTable;

	TArray<uint32> TextureMap;
	TArray<uint32> ShaderResourceViewMap;
	TArray<uint32> SamplerMap;
	TArray<uint32> UnorderedAccessViewMap;

	for (int32 UBIndex = 0; UBIndex < NEWHeader.UniformBuffers.Num(); ++UBIndex)
	{
		const FVulkanShaderHeader::FUniformBufferInfo& UBHeader = NEWHeader.UniformBuffers[UBIndex];

		ShaderResourceTable.ResourceTableLayoutHashes.Emplace(UBHeader.LayoutHash);
		if (UBHeader.ResourceEntries.Num() > 0)
		{
			ShaderResourceTable.ResourceTableBits |= 1 << UBIndex;

			for (const FVulkanShaderHeader::FUBResourceInfo& UBRes : UBHeader.ResourceEntries)
			{
				uint32 ResourceMap = FRHIResourceTableEntry::Create(UBIndex, UBRes.SourceUBResourceIndex, UBRes.GlobalIndex);
				switch (UBRes.UBBaseType)
				{
				case UBMT_TEXTURE:
				case UBMT_RDG_TEXTURE:
					TextureMap.Add(ResourceMap);
					break;
				case UBMT_SAMPLER:
					SamplerMap.Add(ResourceMap);
					break;
				case UBMT_SRV:
				case UBMT_RDG_TEXTURE_SRV:
				case UBMT_RDG_BUFFER_SRV:
					ShaderResourceViewMap.Add(ResourceMap);
					break;
				case UBMT_UAV:
				case UBMT_RDG_TEXTURE_UAV:
				case UBMT_RDG_BUFFER_UAV:
					UnorderedAccessViewMap.Add(ResourceMap);
					break;
				default:
					check(false);
				}
			}
		}
	}

	const int32 MaxBoundResourceTable = NEWHeader.UniformBuffers.Num();
	BuildResourceTableTokenStream(TextureMap, MaxBoundResourceTable, ShaderResourceTable.TextureMap);
	BuildResourceTableTokenStream(ShaderResourceViewMap, MaxBoundResourceTable, ShaderResourceTable.ShaderResourceViewMap);
	BuildResourceTableTokenStream(SamplerMap, MaxBoundResourceTable, ShaderResourceTable.SamplerMap);
	BuildResourceTableTokenStream(UnorderedAccessViewMap, MaxBoundResourceTable, ShaderResourceTable.UnorderedAccessViewMap);

	return ShaderResourceTable;
}



static void BuildShaderOutput(
	VulkanShaderCompilerSerializedOutput& SerializedOutput,
	FShaderCompilerOutput&		ShaderOutput,
	const FVulkanShaderCompilerInternalState& InternalState,
	const FSpirvReflectBindings& SpirvReflectBindings,
	TMap<FString, FVulkanShaderHeader::EType>& EntryTypes,
	const FVulkanBindingTable&	BindingTable,
	const FString&				DebugName,
	uint32						PackedGlobalArraySize,
	TBitArray<>&				UsedUniformBufferSlots
)
{
	const FShaderCompilerInput& ShaderInput = InternalState.Input;
	const EShaderFrequency Frequency = InternalState.GetShaderFrequency();

	FVulkanShaderHeader& NEWHeader = SerializedOutput.Header;

	NEWHeader.SpirvCRC = SerializedOutput.Spirv.CRC;
	NEWHeader.RayTracingPayloadType = ShaderInput.Environment.GetCompileArgument(TEXT("RT_PAYLOAD_TYPE"), 0u);
	NEWHeader.RayTracingPayloadSize = ShaderInput.Environment.GetCompileArgument(TEXT("RT_PAYLOAD_MAX_SIZE"), 0u);
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	NEWHeader.DebugName = DebugName;
#endif

	// :todo-jn: Hash entire SPIRV for now, could eventually be removed since we use ShaderKeys
	FSHA1::HashBuffer(SerializedOutput.Spirv.Data.GetData(), SerializedOutput.Spirv.GetByteSize(), (uint8*)&NEWHeader.SourceHash);


	// Flattens the array dimensions of the interface variable (aka shader attribute), e.g. from float4[2][3] -> float4[6]
	auto FlattenAttributeArrayDimension = [](const SpvReflectInterfaceVariable& Attribute, uint32 FirstArrayDim = 0)
	{
		uint32 FlattenedArrayDim = 1;
		for (uint32 ArrayDimIndex = FirstArrayDim; ArrayDimIndex < Attribute.array.dims_count; ++ArrayDimIndex)
		{
			FlattenedArrayDim *= Attribute.array.dims[ArrayDimIndex];
		}
		return FlattenedArrayDim;
	};


	// Only process input attributes for vertex shaders.
	if (Frequency == SF_Vertex)
	{
		static const FString AttributePrefix = TEXT("ATTRIBUTE");

		for (const SpvReflectInterfaceVariable* Attribute : SpirvReflectBindings.InputAttributes)
		{
			if (CrossCompiler::FShaderConductorContext::IsIntermediateSpirvOutputVariable(Attribute->name))
			{
				continue;
			}

			if (!Attribute->semantic)
			{
				continue;
			}

			const FString InputAttrName(ANSI_TO_TCHAR(Attribute->semantic));
			if (InputAttrName.StartsWith(AttributePrefix))
			{
				const uint32 AttributeIndex = ParseNumber(*InputAttrName + AttributePrefix.Len(), /*bEmptyIsZero:*/ true);
				const uint32 FlattenedArrayDim = FlattenAttributeArrayDimension(*Attribute);
				for (uint32 Index = 0; Index < FlattenedArrayDim; ++Index)
				{
					const uint32 BitIndex = (AttributeIndex + Index);
					NEWHeader.InOutMask |= (1u << BitIndex);
				}
			}
		}
	}

	// Only process output attributes for pixel shaders.
	if (Frequency == SF_Pixel)
	{
		static const FString TargetPrefix = "SV_Target";

		for (const SpvReflectInterfaceVariable* Attribute : SpirvReflectBindings.OutputAttributes)
		{
			// Only depth writes for pixel shaders must be tracked.
			if (Attribute->built_in == SpvBuiltInFragDepth)
			{
				const uint32 BitIndex = (CrossCompiler::FShaderBindingInOutMask::DepthStencilMaskIndex);
				NEWHeader.InOutMask |= (1u << BitIndex);
			}
			else
			{
				// Only targets for pixel shaders must be tracked.
				const FString OutputAttrName(ANSI_TO_TCHAR(Attribute->semantic));
				if (OutputAttrName.StartsWith(TargetPrefix))
				{
					const uint32 TargetIndex = ParseNumber(*OutputAttrName + TargetPrefix.Len(), /*bEmptyIsZero:*/ true);

					const uint32 FlattenedArrayDim = FlattenAttributeArrayDimension(*Attribute);
					for (uint32 Index = 0; Index < FlattenedArrayDim; ++Index)
					{
						const uint32 BitIndex = (TargetIndex + Index);
						NEWHeader.InOutMask |= (1u << BitIndex);
					}
				}
			}
		}
	}
	
	const int32 StageOffset = InternalState.bSupportsBindless ? (ShaderStage::GetStageForFrequency(Frequency) * VulkanBindless::MaxUniformBuffersPerStage) : 0;

	TArray<VkDescriptorType> DescriptorTypes;
	const TArray<FVulkanBindingTable::FBinding>& HlslccBindings = BindingTable.GetBindings();
	for (int32 Index = 0; Index < HlslccBindings.Num(); ++Index)
	{
		const FVulkanBindingTable::FBinding& Binding = HlslccBindings[Index];
		DescriptorTypes.Add(BindingToDescriptorType(Binding.Type));
	}

	//#todo-rco: When using regular UBs, also set UsedUniformBufferSlots[] = 1

	TArray<FString> OriginalParameters;
	ShaderOutput.ParameterMap.GetAllParameterNames(OriginalParameters);

	// Build the SRT for this shader.
	FShaderCompilerResourceTable ShaderResourceTable;
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		if (!BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.UniformBufferMap, UsedUniformBufferSlots, ShaderOutput.ParameterMap, /*MaxBoundResourceTable, */GenericSRT))
		{
			ShaderOutput.Errors.Add(TEXT("Internal error on BuildResourceTableMapping."));
			return;
		}

		// Copy over the bits indicating which resource tables are active.
		ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;
		ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, ShaderResourceTable.TextureMap, true);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, ShaderResourceTable.ShaderResourceViewMap, true);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, ShaderResourceTable.SamplerMap, true);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, ShaderResourceTable.UnorderedAccessViewMap, true);
	}

	TArray<FString> NewParameters;
	ShaderOutput.ParameterMap.GetAllParameterNames(NewParameters);

	// Mark all used uniform buffer indices; however some are empty (eg GBuffers) so gather those as NewParameters
	uint16 NumParams = 0;
	for (int32 Index = NewParameters.Num() - 1; Index >= 0; --Index)
	{
		uint16 OutIndex, OutBase, OutSize;
		const bool bFound = ShaderOutput.ParameterMap.FindParameterAllocation(*NewParameters[Index], OutIndex, OutBase, OutSize);
		ensure(bFound);
		NumParams = FMath::Max((uint16)(OutIndex + 1), NumParams);
		if (OriginalParameters.Contains(NewParameters[Index]))
		{
			NewParameters.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}

	if (PackedGlobalArraySize > 0)
	{
		FVulkanShaderHeader::FPackedUBInfo& PackedUB = NEWHeader.PackedUBs.AddZeroed_GetRef();
		PackedUB.OriginalBindingIndex = StageOffset;
		PackedUB.PackedTypeIndex = CrossCompiler::EPackedTypeIndex::HighP;
		PackedUB.SizeInBytes = Align(PackedGlobalArraySize, 16u);

		const FVulkanSpirv::FEntry* Entry = SerializedOutput.Spirv.GetEntryByBindingIndex(StageOffset);
		check(Entry);
		PackedUB.SPIRVDescriptorSetOffset = Entry->WordDescriptorSetIndex;
		PackedUB.SPIRVBindingIndexOffset = Entry->WordBindingIndex;
	}

	ConvertToHeader(ShaderResourceTable, BindingTable, DescriptorTypes, EntryTypes, ShaderInput, SpirvReflectBindings, ShaderOutput.ParameterMap, SerializedOutput);
	check(NEWHeader.EmulatedUBsCopyInfo.Num() == 0);

	if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
	{
		NEWHeader.DebugName = ShaderInput.GenerateShaderName();
	}

	// Build the SRT for this shader from the NEWHeader
	SerializedOutput.ShaderResourceTable = BuildSRTFromHeader(NEWHeader);

	ShaderOutput.bSucceeded = true;

	// guard disassembly of SPIRV code on bExtractShaderSource setting since presumably this isn't that cheap.
	// this roughly will maintain existing behaviour, except the debug usf will be this version of the code 
	// instead of the output of  preprocessing if this setting is enabled (which is probably fine since this is only
	// ever set in editor)
	if (ShaderInput.ExtraSettings.bExtractShaderSource)
	{
		TArray<ANSICHAR> AssemblyText;
		if (CrossCompiler::FShaderConductorContext::Disassemble(CrossCompiler::EShaderConductorIR::Spirv, SerializedOutput.Spirv.GetByteData(), SerializedOutput.Spirv.GetByteSize(), AssemblyText))
		{
			ShaderOutput.ModifiedShaderSource = FString(AssemblyText.GetData());
		}
	}
	if (ShaderInput.ExtraSettings.OfflineCompilerPath.Len() > 0)
	{
		if (SupportsOfflineCompiler(ShaderInput.ShaderFormat))
		{
			CompileOfflineMali(ShaderInput, ShaderOutput, (const ANSICHAR*)SerializedOutput.Spirv.GetByteData(), SerializedOutput.Spirv.GetByteSize(), true, SerializedOutput.Spirv.EntryPointName);
		}
	}

	// Ray generation shaders rely on a different binding model that aren't compatible with global uniform buffers.
	if (!InternalState.IsRayTracingShader())
	{
		CullGlobalUniformBuffers(ShaderInput.Environment.UniformBufferMap, ShaderOutput.ParameterMap);
	}
}


#if PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX

static void GatherSpirvReflectionBindings(
	spv_reflect::ShaderModule&	Reflection,
	FSpirvReflectBindings&		OutBindings,
	TSet<FString>&				OutBindlessUB,
	const FVulkanShaderCompilerInternalState& InternalState)
{
	// Change descriptor set numbers
	TArray<SpvReflectDescriptorSet*> DescriptorSets;
	uint32 NumDescriptorSets = 0;

	// If bindless is supported, then offset the descriptor set to fit the bindless heaps at the beginning
	const EShaderFrequency ShaderFrequency = InternalState.GetShaderFrequency();
	const uint32 StageIndex = (uint32)ShaderStage::GetStageForFrequency(ShaderFrequency);
	const uint32 DescSetNo = InternalState.bSupportsBindless ? VulkanBindless::NumBindlessSets + StageIndex : StageIndex;

	SpvReflectResult SpvResult = Reflection.EnumerateDescriptorSets(&NumDescriptorSets, nullptr);
	check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);
	if (NumDescriptorSets > 0)
	{
		DescriptorSets.SetNum(NumDescriptorSets);
		SpvResult = Reflection.EnumerateDescriptorSets(&NumDescriptorSets, DescriptorSets.GetData());
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		for (const SpvReflectDescriptorSet* DescSet : DescriptorSets)
		{
			Reflection.ChangeDescriptorSetNumber(DescSet, DescSetNo);
		}
	}

	OutBindings.GatherInputAttributes(Reflection);
	OutBindings.GatherOutputAttributes(Reflection);
	OutBindings.GatherDescriptorBindings(Reflection);

	// Storage buffers always occupy a UAV binding slot, so move all SBufferSRVs into the SBufferUAVs array
	OutBindings.SBufferUAVs.Append(OutBindings.SBufferSRVs);
	OutBindings.SBufferSRVs.Empty();

	// Change indices of input attributes by their name suffix. Only in the vertex shader stage, "ATTRIBUTE" semantics have a special meaning for shader attributes.
	if (ShaderFrequency == SF_Vertex)
	{
		OutBindings.AssignInputAttributeLocationsBySemanticIndex(Reflection, CrossCompiler::FShaderConductorContext::GetIdentifierTable().InputAttribute);
	}

	// Patch resource heaps descriptor set numbers
	if (InternalState.bSupportsBindless)
	{
		// Move the bindless heap to its dedicated descriptor set and remove it from our regular binding arrays
		auto MoveBindlessHeaps = [&](TArray<SpvReflectDescriptorBinding*>& BindingArray, const TCHAR* HeapPrefix, uint32 BinldessDescSetNo)
		{
			for (int32 Index = BindingArray.Num() - 1; Index >= 0; --Index)
			{
				const SpvReflectDescriptorBinding* pBinding = BindingArray[Index];
				const FString BindingName(ANSI_TO_TCHAR(pBinding->name));
				if (BindingName.StartsWith(HeapPrefix))
				{
					const uint32 Binding = 0;  // single bindless heap per descriptor set
					Reflection.ChangeDescriptorBindingNumbers(pBinding, Binding, BinldessDescSetNo);
					BindingArray.RemoveAtSwap(Index);
				}
			}
		};

		// Remove sampler heaps from binding arrays
		MoveBindlessHeaps(OutBindings.Samplers, FShaderParameterParser::kBindlessSamplerArrayPrefix, VulkanBindless::BindlessSamplerSet);

		// Remove resource heaps from binding arrays
		MoveBindlessHeaps(OutBindings.SBufferUAVs, FShaderParameterParser::kBindlessUAVArrayPrefix, VulkanBindless::BindlessStorageBufferSet);
		MoveBindlessHeaps(OutBindings.SBufferUAVs, FShaderParameterParser::kBindlessSRVArrayPrefix, VulkanBindless::BindlessStorageBufferSet);  // try with both prefixes, they were merged earlier
		MoveBindlessHeaps(OutBindings.TextureSRVs, FShaderParameterParser::kBindlessSRVArrayPrefix, VulkanBindless::BindlessSampledImageSet);
		MoveBindlessHeaps(OutBindings.TextureUAVs, FShaderParameterParser::kBindlessUAVArrayPrefix, VulkanBindless::BindlessStorageImageSet);
		MoveBindlessHeaps(OutBindings.TextureUAVs, FShaderParameterParser::kBindlessSRVArrayPrefix, VulkanBindless::BindlessStorageImageSet);  // try with both prefixes, R64 SRV textures are read as storage images
		MoveBindlessHeaps(OutBindings.TBufferSRVs, FShaderParameterParser::kBindlessSRVArrayPrefix, VulkanBindless::BindlessUniformTexelBufferSet);
		MoveBindlessHeaps(OutBindings.TBufferUAVs, FShaderParameterParser::kBindlessUAVArrayPrefix, VulkanBindless::BindlessStorageTexelBufferSet);
		MoveBindlessHeaps(OutBindings.AccelerationStructures, FShaderParameterParser::kBindlessSRVArrayPrefix, VulkanBindless::BindlessAccelerationStructureSet);

		// Move uniform buffers to the correct set
		{
			const uint32 BindingOffset = (StageIndex * VulkanBindless::MaxUniformBuffersPerStage);
			for (int32 Index = OutBindings.UniformBuffers.Num() - 1; Index >= 0; --Index)
			{
				const SpvReflectDescriptorBinding* pBinding = OutBindings.UniformBuffers[Index];
				const FString BindingName(ANSI_TO_TCHAR(pBinding->name));
				if (BindingName.StartsWith(kBindlessCBPrefix))
				{
					check(InternalState.bUseBindlessUniformBuffer);
					Reflection.ChangeDescriptorBindingNumbers(pBinding, 0, VulkanBindless::BindlessUniformBufferSet);
					const FString BindlessUBName = GetBindlessUBNameFromHeap(BindingName);
					checkf(InternalState.AllBindlessUBs.Contains(BindlessUBName), TEXT("Bindless Uniform Buffer was found in SPIRV but not tracked in internal state"));
					OutBindlessUB.Add(BindlessUBName);
					OutBindings.UniformBuffers.RemoveAtSwap(Index);
				}
				else
				{
					Reflection.ChangeDescriptorBindingNumbers(pBinding, BindingOffset + pBinding->binding, VulkanBindless::BindlessSingleUseUniformBufferSet);
				}
			}
		}
	}
}

static uint32 CalculateSpirvInstructionCount(FVulkanSpirv& Spirv)
{
	// Count instructions inside functions
	bool bInsideFunction = false;
	uint32 ApproxInstructionCount = 0;
	for (FSpirvConstIterator Iter = Spirv.cbegin(); Iter != Spirv.cend(); ++Iter)
	{
		switch (Iter.Opcode())
		{

		case SpvOpFunction:
		{
			check(!bInsideFunction);
			bInsideFunction = true;
		}
		break;

		case SpvOpFunctionEnd:
		{
			check(bInsideFunction);
			bInsideFunction = false;
		}
		break;

		case SpvOpLabel:
		case SpvOpAccessChain:
		case SpvOpSelectionMerge:
		case SpvOpCompositeConstruct:
		case SpvOpCompositeInsert:
		case SpvOpCompositeExtract:
			// Skip a few ops that show up often but don't result in much work on their own
			break;

		default:
		{
			if (bInsideFunction)
			{
				++ApproxInstructionCount;
			}
		}
		break;

		}
	}
	check(!bInsideFunction);

	return ApproxInstructionCount;
}

static bool BuildShaderOutputFromSpirv(
	CrossCompiler::FShaderConductorContext&	CompilerContext,
	const FVulkanShaderCompilerInternalState& InternalState,
	VulkanShaderCompilerSerializedOutput&   SerializedOutput,
	FShaderCompilerOutput&					Output,
	FVulkanBindingTable&					BindingTable
)
{
	// Reflect SPIR-V module with SPIRV-Reflect library
	const size_t SpirvDataSize = SerializedOutput.Spirv.GetByteSize();
	spv_reflect::ShaderModule Reflection(SpirvDataSize, SerializedOutput.Spirv.GetByteData(), SPV_REFLECT_RETURN_FLAG_SAMPLER_IMAGE_USAGE);
	check(Reflection.GetResult() == SPV_REFLECT_RESULT_SUCCESS);

	// Ray tracing shaders are not being rewritten to remove unreferenced entry points due to a bug in dxc.
	// An issue prevents multiple entrypoints in the same spirv module, so limit ourselves to one entrypoint at a time
	// Change final entry point name in SPIR-V module
	{
		checkf(Reflection.GetEntryPointCount() == 1, TEXT("Too many entry points in SPIR-V module: Expected 1, but got %d"), Reflection.GetEntryPointCount());
		const SpvReflectResult Result = Reflection.ChangeEntryPointName(0, "main_00000000_00000000");
		check(Result == SPV_REFLECT_RESULT_SUCCESS);
	}

	FSpirvReflectBindings Bindings;
	GatherSpirvReflectionBindings(Reflection, Bindings, SerializedOutput.UsedBindlessUB, InternalState);

	// Build binding table
	TMap<const SpvReflectDescriptorBinding*, int32> BindingToIndexMap;

	const FString UBOGlobalsNameSpv(ANSI_TO_TCHAR(CrossCompiler::FShaderConductorContext::GetIdentifierTable().GlobalsUniformBuffer));
	const FString UBORootParamNameSpv(FShaderParametersMetadata::kRootUniformBufferBindingName);

	auto RegisterBindings = [&BindingTable, &BindingToIndexMap, &UBOGlobalsNameSpv, &UBORootParamNameSpv]
							(TArray<SpvReflectDescriptorBinding*>& Bindings, const char* BlockName, EVulkanBindingType::EType BindingType)
	{
		for (const SpvReflectDescriptorBinding* Binding : Bindings)
		{
			const bool bIsGlobalOrRootBuffer = ((UBOGlobalsNameSpv == Binding->name) || (UBORootParamNameSpv == Binding->name));
			if (((BindingType == EVulkanBindingType::PackedUniformBuffer) && !bIsGlobalOrRootBuffer) ||
				((BindingType == EVulkanBindingType::UniformBuffer) && bIsGlobalOrRootBuffer))
			{
				continue;
			}

			const int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, BlockName, BindingType);
			BindingToIndexMap.Add(Binding, BindingIndex);

			if (BindingType == EVulkanBindingType::InputAttachment)
			{
				BindingTable.InputAttachmentsMask |= (1u << Binding->input_attachment_index);
			}
		}
	};

	RegisterBindings(Bindings.UniformBuffers, "h", EVulkanBindingType::PackedUniformBuffer);
	RegisterBindings(Bindings.UniformBuffers, "u", EVulkanBindingType::UniformBuffer);
	RegisterBindings(Bindings.InputAttachments, "a", EVulkanBindingType::InputAttachment);

	RegisterBindings(Bindings.TBufferUAVs, "u", EVulkanBindingType::StorageTexelBuffer);
	RegisterBindings(Bindings.SBufferUAVs, "u", EVulkanBindingType::StorageBuffer);
	RegisterBindings(Bindings.TextureUAVs, "u", EVulkanBindingType::StorageImage);

	RegisterBindings(Bindings.TBufferSRVs, "s", EVulkanBindingType::UniformTexelBuffer);
	checkf(Bindings.SBufferSRVs.IsEmpty(), TEXT("GatherSpirvReflectionBindings should have dumped all SBufferSRVs into SBufferUAVs."));
	RegisterBindings(Bindings.TextureSRVs, "s", EVulkanBindingType::Image);

	RegisterBindings(Bindings.Samplers, "z", EVulkanBindingType::Sampler);
	RegisterBindings(Bindings.AccelerationStructures, "r", EVulkanBindingType::AccelerationStructure);

	// Sort binding table
	BindingTable.SortBindings();

	uint32 PackedGlobalArraySize = 0;
	TBitArray<> UsedUniformBufferSlots;
	const int32 MaxNumBits = VulkanBindless::MaxUniformBuffersPerStage * SF_NumFrequencies;
	UsedUniformBufferSlots.Init(false, MaxNumBits);

	TMap<FString, FVulkanShaderHeader::EType> EntryTypes;

	// Final descriptor binding numbers for all other resource types
	{
		const int32 StageOffset = InternalState.bSupportsBindless ? (ShaderStage::GetStageForFrequency(InternalState.GetShaderFrequency()) * VulkanBindless::MaxUniformBuffersPerStage) : 0;
		const uint32_t DescSetNumber = InternalState.bSupportsBindless ? (uint32_t)VulkanBindless::BindlessSingleUseUniformBufferSet : (uint32_t)SPV_REFLECT_SET_NUMBER_DONT_CHANGE;

		auto AddReflectionInfos = [&](TArray<SpvReflectDescriptorBinding*>& BindingArray, EVulkanBindingType::EType BindingType, int32 BindingOffset)
		{
			for (const SpvReflectDescriptorBinding* Binding : BindingArray)
			{
				checkf(!InternalState.bSupportsBindless || (BindingType == EVulkanBindingType::UniformBuffer) || (BindingType == EVulkanBindingType::PackedUniformBuffer),
					TEXT("Bindless shaders should only have uniform buffers."));

				const FString ResourceName(ANSI_TO_TCHAR(Binding->name));

				const bool bIsGlobalOrRootBuffer = ((UBOGlobalsNameSpv == ResourceName) || (UBORootParamNameSpv == ResourceName));
				if (((BindingType == EVulkanBindingType::PackedUniformBuffer) && !bIsGlobalOrRootBuffer) ||
					((BindingType == EVulkanBindingType::UniformBuffer) && bIsGlobalOrRootBuffer))
				{
					continue;
				}

				const int32 BindingIndex = BindingTable.GetRealBindingIndex(BindingToIndexMap[Binding]) + StageOffset;

				const SpvReflectResult SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex, DescSetNumber);
				check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

				const int32 ReflectionSlot = SerializedOutput.Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
				check(InternalState.ParameterParser);
				const FShaderParameterParser::FParsedShaderParameter* ParsedParam = InternalState.ParameterParser->FindParameterInfosUnsafe(ResourceName);

				auto AddShaderValidationType = [] (uint32_t VulkanBindingIndex, const FShaderParameterParser::FParsedShaderParameter* ParsedParam, FShaderCompilerOutput& Output) {
					/*if (ParsedParam)
					{
						if (IsResourceBindingTypeSRV(ParsedParam->ParsedTypeDecl))
						{
							AddShaderValidationSRVType(VulkanBindingIndex, ParsedParam->ParsedTypeDecl, Output);
						}
						else
						{
							AddShaderValidationUAVType(VulkanBindingIndex, ParsedParam->ParsedTypeDecl, Output);
						}
					}*/
				};

				switch (BindingType)
				{
				case EVulkanBindingType::StorageTexelBuffer:
				case EVulkanBindingType::StorageBuffer:
				case EVulkanBindingType::StorageImage:
					HandleReflectedShaderUAV(ResourceName, BindingOffset, ReflectionSlot, 1, Output);
					EntryTypes.Add(ResourceName, FVulkanShaderHeader::Global);

					AddShaderValidationType(BindingOffset, ParsedParam, Output);
					break;

				case EVulkanBindingType::Image:
					// todo-jn: Could verify that we have the samplers...
					//for (uint32 UsageIndex = 0; UsageIndex < Binding->usage_binding_count; ++UsageIndex)
					//{
					//	const SpvReflectDescriptorBinding* AssociatedResource = Binding->usage_bindings[UsageIndex];
					//	AssociatedResourceNames[UsageIndex] = ANSI_TO_TCHAR(AssociatedResource->name);
					//}
					[[fallthrough]];

				case EVulkanBindingType::UniformTexelBuffer:
					HandleReflectedShaderResource(ResourceName, BindingOffset, ReflectionSlot, 1, Output);
					EntryTypes.Add(ResourceName, FVulkanShaderHeader::Global);

					AddShaderValidationType(BindingOffset, ParsedParam, Output);
					break;

				case EVulkanBindingType::Sampler:
					HandleReflectedShaderSampler(ResourceName, ReflectionSlot, Output);
					//HandleReflectedShaderSampler(ResourceName, BindingOffset, ReflectionSlot, 1, Output);
					EntryTypes.Add(ResourceName, FVulkanShaderHeader::Global);
					break;

				case EVulkanBindingType::AccelerationStructure:
					HandleReflectedShaderResource(ResourceName, BindingOffset, ReflectionSlot, 1, Output);
					EntryTypes.Add(ResourceName, FVulkanShaderHeader::Global);

					AddShaderValidationType(BindingOffset, ParsedParam, Output);
					break;

				case EVulkanBindingType::InputAttachment:
					// Do Nothing
					break;

				case EVulkanBindingType::PackedUniformBuffer:
					{
						// Use the given global ResourceName instead of patching it to _Globals_h
						check(!UsedUniformBufferSlots[ReflectionSlot]);
						UsedUniformBufferSlots[ReflectionSlot] = true;

						if (InternalState.UseRootParametersStructure())
						{
							check(ReflectionSlot == FShaderParametersMetadata::kRootCBufferBindingIndex);
							HandleReflectedUniformBuffer(ResourceName, ReflectionSlot, Output);
							EntryTypes.Add(ResourceName, FVulkanShaderHeader::UniformBuffer);
						}

						// Register all uniform buffer members of Globals as loose data
						for (uint32 MemberIndex = 0; MemberIndex < Binding->block.member_count; ++MemberIndex)
						{
							const SpvReflectBlockVariable& Member = Binding->block.members[MemberIndex];

							FString MemberName(ANSI_TO_TCHAR(Member.name));
							FStringView AdjustedMemberName(MemberName);

							const EShaderParameterType BindlessParameterType = FShaderParameterParser::ParseAndRemoveBindlessParameterPrefix(AdjustedMemberName);

							// Add all members of global ub, and only bindless samplers/resources for root param
							if (!InternalState.UseRootParametersStructure() || BindlessParameterType != EShaderParameterType::LooseData)
							{
								HandleReflectedGlobalConstantBufferMember(
									MemberName,
									BindingOffset,
									Member.absolute_offset,
									Member.size,
									Output
								);

								EntryTypes.Add(FString(AdjustedMemberName), FVulkanShaderHeader::PackedGlobal);
							}

							PackedGlobalArraySize = FMath::Max<uint32>((Member.absolute_offset + Member.size), PackedGlobalArraySize);
						}
					}
					break;

				case EVulkanBindingType::UniformBuffer:
					{
						check(!UsedUniformBufferSlots[ReflectionSlot]);
						UsedUniformBufferSlots[ReflectionSlot] = true;
						HandleReflectedUniformBuffer(ResourceName, ReflectionSlot, Output);
						EntryTypes.Add(ResourceName, FVulkanShaderHeader::UniformBuffer);

						AddShaderValidationUBSize(BindingOffset, Binding->block.padded_size, Output);
					}
					break;

				default:
					check(false);
					break;
				};

				BindingOffset++;
			}
			return BindingOffset;
		};

		// Process Globals first (PackedUniformBuffer) and then regular UBs
		const int32 GlobalUBCount = AddReflectionInfos(Bindings.UniformBuffers, EVulkanBindingType::PackedUniformBuffer, 0);
		int32 UBOBindings = AddReflectionInfos(Bindings.UniformBuffers, EVulkanBindingType::UniformBuffer, 0) + GlobalUBCount;

		AddReflectionInfos(Bindings.InputAttachments, EVulkanBindingType::InputAttachment, 0);

		int32 UAVBindings = 0;
		UAVBindings = AddReflectionInfos(Bindings.TBufferUAVs, EVulkanBindingType::StorageTexelBuffer, UAVBindings);
		UAVBindings = AddReflectionInfos(Bindings.SBufferUAVs, EVulkanBindingType::StorageBuffer, UAVBindings);
		UAVBindings = AddReflectionInfos(Bindings.TextureUAVs, EVulkanBindingType::StorageImage, UAVBindings);

		int32 SRVBindings = 0;
		SRVBindings = AddReflectionInfos(Bindings.TBufferSRVs, EVulkanBindingType::UniformTexelBuffer, SRVBindings);
		checkf(Bindings.SBufferSRVs.IsEmpty(), TEXT("GatherSpirvReflectionBindings should have dumped all SBufferSRVs into SBufferUAVs."));
		SRVBindings = AddReflectionInfos(Bindings.TextureSRVs, EVulkanBindingType::Image, SRVBindings);

		Output.NumTextureSamplers = AddReflectionInfos(Bindings.Samplers, EVulkanBindingType::Sampler, 0);
		AddReflectionInfos(Bindings.AccelerationStructures, EVulkanBindingType::AccelerationStructure, 0);
	}

	Output.Target = InternalState.Input.Target;

	// Overwrite updated SPIRV code
	SerializedOutput.Spirv.Data = TArray<uint32>(Reflection.GetCode(), Reflection.GetCodeSize() / 4);

	// We have to strip out most debug instructions (except OpName) for Vulkan mobile
	if (InternalState.bStripReflect)
	{
		const char* OptArgs[] = { "--strip-reflect", "-O"};
		if (!CompilerContext.OptimizeSpirv(SerializedOutput.Spirv.Data, OptArgs, UE_ARRAY_COUNT(OptArgs)))
		{
			UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Failed to strip debug instructions from SPIR-V module"));
			return false;
		}
	}

	// For Android run an additional pass to patch spirv to be compatible across drivers
	if (IsAndroidShaderFormat(InternalState.Input.ShaderFormat))
	{
		const char* OptArgs[] = { "--android-driver-patch" };
		if (!CompilerContext.OptimizeSpirv(SerializedOutput.Spirv.Data, OptArgs, UE_ARRAY_COUNT(OptArgs)))
		{
			UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Failed to apply driver patches for Android"));
			return false;
		}
	}

	PatchSpirvReflectionEntries(SerializedOutput.Spirv);

	// :todo-jn: We don't store the CRC of each member of the hit group, leave the entrypoint untouched on the extra modules
	if (InternalState.HasMultipleEntryPoints() && (InternalState.HitGroupShaderType != FVulkanShaderCompilerInternalState::EHitGroupShaderType::ClosestHit))
	{
		SerializedOutput.Spirv.EntryPointName = "main_00000000_00000000";
	}
	else
	{
		SerializedOutput.Spirv.EntryPointName = PatchSpirvEntryPointWithCRC(SerializedOutput.Spirv, SerializedOutput.Spirv.CRC);
	}

	Output.NumInstructions = CalculateSpirvInstructionCount(SerializedOutput.Spirv);

	BuildShaderOutput(
		SerializedOutput,
		Output,
		InternalState,
		Bindings,
		EntryTypes,
		BindingTable,
		InternalState.GetDebugName(),
		PackedGlobalArraySize,
		UsedUniformBufferSlots
	);

	if (InternalState.bDebugDump)
	{
		FString SPVExt(InternalState.GetSPVExtension());
		FString SPVASMExt(SPVExt + TEXT("asm"));

		// Write meta data to debug output file and write SPIR-V dump in binary and text form
		DumpDebugShaderBinary(InternalState.Input, SerializedOutput.Spirv.GetByteData(), SerializedOutput.Spirv.GetByteSize(), SPVExt);
		DumpDebugShaderDisassembledSpirv(InternalState.Input, SerializedOutput.Spirv.GetByteData(), SerializedOutput.Spirv.GetByteSize(), SPVASMExt);
	}

	return true;
}

// Replaces OpImageFetch with OpImageRead for 64bit samplers
static void Patch64bitSamplers(FVulkanSpirv& Spirv)
{
	uint32_t ULongSampledTypeId = 0;
	uint32_t LongSampledTypeId = 0;

	TArray<uint32_t, TInlineAllocator<2>> ImageTypeIDs;
	TArray<uint32_t, TInlineAllocator<2>> LoadedIDs;


	// Count instructions inside functions
	for (FSpirvIterator Iter = Spirv.begin(); Iter != Spirv.end(); ++Iter)
	{
		switch (Iter.Opcode())
		{

		case SpvOpTypeInt:
		{
			// Operands:
			// 1 - Result Id
			// 2 - Width specifies how many bits wide the type is
			// 3 - Signedness: 0 indicates unsigned

			const uint32_t IntWidth = Iter.Operand(2);
			if (IntWidth == 64)
			{
				const uint32_t IntSignedness = Iter.Operand(3);
				if (IntSignedness == 1)
				{
					check(LongSampledTypeId == 0);
					LongSampledTypeId = Iter.Operand(1);
				}
				else
				{
					check(ULongSampledTypeId == 0);
					ULongSampledTypeId = Iter.Operand(1);
				}
			}
		}
		break;

		case SpvOpTypeImage:
		{
			// Operands:
			// 1 - Result Id
			// 2 - Sampled Type is the type of the components that result from sampling or reading from this image type
			// 3 - Dim is the image dimensionality (Dim).
			// 4 - Depth : 0 indicates not a depth image, 1 indicates a depth image, 2 means no indication as to whether this is a depth or non-depth image
			// 5 - Arrayed : 0 indicates non-arrayed content, 1 indicates arrayed content
			// 6 - MS : 0 indicates single-sampled content, 1 indicates multisampled content
			// 7 - Sampled : 0 indicates this is only known at run time, not at compile time, 1 indicates used with sampler, 2 indicates used without a sampler (a storage image)
			// 8 - Image Format

			if ((Iter.Operand(7) == 1) && (Iter.Operand(6) == 0) && (Iter.Operand(5) == 0))
			{
				// Patch the node info and the SPIRV
				const uint32_t SampledTypeId = Iter.Operand(2);
				const uint32_t WithoutSampler = 2;
				if (SampledTypeId == LongSampledTypeId)
				{
					uint32* CurrentOpPtr = *Iter;
					CurrentOpPtr[7] = WithoutSampler;
					CurrentOpPtr[8] = (uint32_t)SpvImageFormatR64i;
					ImageTypeIDs.Add(Iter.Operand(1));
				}
				else if (SampledTypeId == ULongSampledTypeId)
				{
					uint32* CurrentOpPtr = *Iter;
					CurrentOpPtr[7] = WithoutSampler;
					CurrentOpPtr[8] = (uint32_t)SpvImageFormatR64ui;
					ImageTypeIDs.Add(Iter.Operand(1));
				}
			}
		}
		break;

		case SpvOpLoad:
		{
			// Operands:
			// 1 - Result Type Id
			// 2 - Result Id
			// 3 - Pointer

			// Find loaded images of this type
			if (ImageTypeIDs.Find(Iter.Operand(1)) != INDEX_NONE)
			{
				LoadedIDs.Add(Iter.Operand(2));
			}
		}
		break;

		case SpvOpImageFetch:
		{
			// Operands:
			// 1 - Result Type Id
			// 2 - Result Id
			// 3 - Image Id
			// 4 - Coordinate
			// 5 - Image Operands

			// If this is one of the modified images, patch the node and the SPIRV.
			if (LoadedIDs.Find(Iter.Operand(3)) != INDEX_NONE)
			{
				const uint32_t OldWordCount = Iter.WordCount();
				const uint32_t NewWordCount = 5;
				check(OldWordCount >= NewWordCount);
				const uint32_t EncodedOpImageRead = (NewWordCount << 16) | ((uint32_t)SpvOpImageRead & 0xFFFF);
				uint32* CurrentOpPtr = *Iter;
				(*CurrentOpPtr) = EncodedOpImageRead;

				// Remove unsupported image operands (mostly force LOD 0)
				const uint32_t NopWordCount = 1;
				const uint32_t EncodedOpNop = (NopWordCount << 16) | ((uint32_t)SpvOpNop & 0xFFFF);
				for (uint32_t ImageOperandIndex = NewWordCount; ImageOperandIndex < OldWordCount; ++ImageOperandIndex)
				{
					CurrentOpPtr[ImageOperandIndex] = EncodedOpNop;
				}
			}
		}
		break;

		default:
		break;
		}
	}
}

static void VulkanCreateDXCCompileBatchFiles(
	const CrossCompiler::FShaderConductorContext& CompilerContext,
	const FVulkanShaderCompilerInternalState& InternalState,
	const CrossCompiler::FShaderConductorOptions& Options)
{
	const FString USFFilename = InternalState.Input.GetSourceFilename();
	const FString SPVFilename = FPaths::GetBaseFilename(USFFilename) + TEXT(".DXC.spv");
	const FString GLSLFilename = FPaths::GetBaseFilename(USFFilename) + TEXT(".SPV.glsl");

	FString DxcPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());

	DxcPath = FPaths::Combine(DxcPath, TEXT("Binaries/ThirdParty/ShaderConductor/Win64"));
	FPaths::MakePlatformFilename(DxcPath);

	FString DxcFilename = FPaths::Combine(DxcPath, TEXT("dxc.exe"));
	FPaths::MakePlatformFilename(DxcFilename);

	// CompileDXC.bat
	{
		const FString DxcArguments = CompilerContext.GenerateDxcArguments(Options);

		FString BatchFileContents =  FString::Printf(
			TEXT(
				"@ECHO OFF\n"
				"SET DXC=\"%s\"\n"
				"SET SPIRVCROSS=\"spirv-cross.exe\"\n"
				"IF NOT EXIST %%DXC%% (\n"
				"\tECHO Couldn't find dxc.exe under \"%s\"\n"
				"\tGOTO :END\n"
				")\n"
				"ECHO Compiling with DXC...\n"
				"%%DXC%% %s -Fo %s %s\n"
				"WHERE %%SPIRVCROSS%%\n"
				"IF %%ERRORLEVEL%% NEQ 0 (\n"
				"\tECHO spirv-cross.exe not found in Path environment variable, please build it from source https://github.com/KhronosGroup/SPIRV-Cross\n"
				"\tGOTO :END\n"
				")\n"
				"ECHO Translating SPIRV back to glsl...\n"
				"%%SPIRVCROSS%% --vulkan-semantics --output %s %s\n"
				":END\n"
				"PAUSE\n"
			),
			*DxcFilename,
			*DxcPath,
			*DxcArguments,
			*SPVFilename,
			*USFFilename,
			*GLSLFilename,
			*SPVFilename
		);

		FFileHelper::SaveStringToFile(BatchFileContents, *(InternalState.Input.DumpDebugInfoPath / TEXT("CompileDXC.bat")));
	}
}

// Quick and dirty way to get the location of the entrypoint in the source
// NOTE: Preprocessed shaders have mcros resolves and comments removed, it makes this easier...
static FString ParseEntrypointDecl(FShaderSource::FViewType PreprocessedShader, FStringView Entrypoint)
{
	FShaderSource::FStringType EntrypointConverted(Entrypoint);
	auto SkipWhitespace = [&](int32& Index)
	{
		while (FChar::IsWhitespace(PreprocessedShader[Index]))
		{
			++Index;
		}
	};

	auto EraseDebugLines = [](FString& EntryPointDecl)
	{
		int32 HashIndex;
		while (EntryPointDecl.FindChar(TEXT('#'), HashIndex))
		{
			while ((HashIndex < EntryPointDecl.Len()) && (!FChar::IsLinebreak(EntryPointDecl[HashIndex])))
			{
				EntryPointDecl[HashIndex] = TEXT(' ');
				++HashIndex;
			}
		}
	};

	FString EntryPointDecl;

	// Go through all the case sensitive matches in the source
	int32 EntrypointIndex = PreprocessedShader.Find(EntrypointConverted);
	check(EntrypointIndex != INDEX_NONE);
	while (EntrypointIndex != INDEX_NONE)
	{
		// This should be the beginning of a new word
		if ((EntrypointIndex == 0) || !FChar::IsWhitespace(PreprocessedShader[EntrypointIndex - 1]))
		{
			EntrypointIndex = PreprocessedShader.Find(EntrypointConverted, EntrypointIndex + 1);
			continue;
		}

		// The next thing after the entrypoint should its parameters
		// White space is allowed, so skip any that is found

		int32 ParamsStart = EntrypointIndex + Entrypoint.Len();
		SkipWhitespace(ParamsStart);
		if (PreprocessedShader[ParamsStart] != '(')
		{
			EntrypointIndex = PreprocessedShader.Find(EntrypointConverted, ParamsStart);
			continue;
		}

		int32 ParamsEnd = PreprocessedShader.Find(SHADER_SOURCE_LITERAL(")"), ParamsStart + 1);
		check(ParamsEnd != INDEX_NONE);
		if (ParamsEnd == INDEX_NONE)
		{
			// Suspicious
			EntrypointIndex = PreprocessedShader.Find(EntrypointConverted, ParamsStart);
			continue;
		}

		// Make sure to grab everything up to the function content

		int32 DeclEnd = ParamsEnd + 1;
		while (PreprocessedShader[DeclEnd] != '{' && (PreprocessedShader[DeclEnd] != ';'))
		{
			++DeclEnd;
		}
		if (PreprocessedShader[DeclEnd] != '{')
		{
			EntrypointIndex = PreprocessedShader.Find(EntrypointConverted, DeclEnd);
			continue;
		}

		// Now back up to pick up the return value, the attributes and everything else that can come with it, like "[numthreads(1,1,1)]"

		int32 DeclBegin = EntrypointIndex - 1;
		while ( (DeclBegin > 0) && (PreprocessedShader[DeclBegin] != ';') && (PreprocessedShader[DeclBegin] != '}'))
		{
			--DeclBegin;
		}
		++DeclBegin;

		EntryPointDecl = FString(DeclEnd - DeclBegin, &PreprocessedShader[DeclBegin]);
		EraseDebugLines(EntryPointDecl);
		EntryPointDecl.TrimStartAndEndInline();
		break;
	}

	return EntryPointDecl;
}

uint8 ParseWaveSize(
	const FVulkanShaderCompilerInternalState& InternalState,
	FShaderSource::FViewType PreprocessedShader
	)
{
	uint8 WaveSize = 0;
	if (!InternalState.IsRayTracingShader())
	{
		const FString EntrypointDecl = ParseEntrypointDecl(PreprocessedShader, InternalState.GetEntryPointName());

		const FString WaveSizeMacro(TEXT("VULKAN_WAVESIZE("));
		int32 WaveSizeIndex = EntrypointDecl.Find(*WaveSizeMacro, ESearchCase::CaseSensitive);
		while (WaveSizeIndex != INDEX_NONE)
		{
			const int32 StartNumber = WaveSizeIndex + WaveSizeMacro.Len();
			const int32 EndNumber = EntrypointDecl.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartNumber);
			check(EndNumber != INDEX_NONE);

			FString WaveSizeValue(EndNumber - StartNumber, &EntrypointDecl[StartNumber]);
			WaveSizeValue.RemoveSpacesInline();
			if (WaveSizeValue != TEXT("N"))  // skip the macro decl
			{
				float FloatResult = 0.0;
				if (FMath::Eval(WaveSizeValue, FloatResult))
				{
					checkf((FloatResult >= 0.0f) && (FloatResult < (float)MAX_uint8), TEXT("Specified wave size is too large for 8bit uint!"));
					WaveSize = static_cast<uint8>(FloatResult);

				}
				else
				{
					check(WaveSizeValue.IsNumeric());
					const int32 ConvertedWaveSize = FCString::Atoi(*WaveSizeValue);
					checkf((ConvertedWaveSize > 0) && (ConvertedWaveSize < MAX_uint8), TEXT("Specified wave size is too large for 8bit uint!"));
					WaveSize = (uint8)ConvertedWaveSize;
				}
				break;
			}

			WaveSizeIndex = EntrypointDecl.Find(*WaveSizeMacro, ESearchCase::CaseSensitive, ESearchDir::FromStart, EndNumber);
		}
	}

	// Take note of preferred wave size flag if none was specified in HLSL
	if ((WaveSize == 0) && InternalState.Input.Environment.CompilerFlags.Contains(CFLAG_Wave32))
	{
		WaveSize = 32;
	}

	return WaveSize;
}

static bool CompileWithShaderConductor(
	const FVulkanShaderCompilerInternalState& InternalState,
	FShaderSource::FViewType PreprocessedShader,
	VulkanShaderCompilerSerializedOutput& SerializedOutput,
	FShaderCompilerOutput&	Output
)
{
	const FShaderCompilerInput& Input = InternalState.Input;

	FVulkanBindingTable BindingTable(InternalState.GetHlslShaderFrequency());
	CrossCompiler::FShaderConductorContext CompilerContext;

	// Inject additional macro definitions to circumvent missing features: external textures
	PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
	FShaderCompilerDefinitions AdditionalDefines;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Load shader source into compiler context
	CompilerContext.LoadSource(PreprocessedShader, Input.VirtualSourceFilePath, InternalState.GetEntryPointName(), InternalState.GetShaderFrequency(), &AdditionalDefines);

	// Initialize compilation options for ShaderConductor
	CrossCompiler::FShaderConductorOptions Options;
	Options.TargetEnvironment = InternalState.MinimumTargetEnvironment;

	// VK_EXT_scalar_block_layout is required by raytracing and by Nanite (so expect it to be present in SM6/Vulkan_1_3)
	Options.bDisableScalarBlockLayout = !(InternalState.IsRayTracingShader() || InternalState.IsSM6());

	if (Input.Environment.CompilerFlags.Contains(CFLAG_AllowRealTypes))
	{
		Options.bEnable16bitTypes = true;
	}

	// Enable HLSL 2021 if specified
	if (Input.Environment.CompilerFlags.Contains(CFLAG_HLSL2021))
	{
		Options.HlslVersion = 2021;
	}

	if (InternalState.bDebugDump)
	{
		VulkanCreateDXCCompileBatchFiles(CompilerContext, InternalState, Options);
	}

	// Before the shader rewritter removes all traces of it, pull any WAVESIZE directives from the shader source
	SerializedOutput.Header.WaveSize = ParseWaveSize(InternalState, PreprocessedShader);

	const bool bRewriteHlslSource = !InternalState.IsRayTracingShader();
	if (bRewriteHlslSource)
	{
		// Rewrite HLSL source code to remove unused global resources and variables
		FString RewrittenHlslSource;

		Options.bRemoveUnusedGlobals = true;
		if (!CompilerContext.RewriteHlsl(Options, (InternalState.bDebugDump ? &RewrittenHlslSource : nullptr)))
		{
			CompilerContext.FlushErrors(Output.Errors);
			return false;
		}
		Options.bRemoveUnusedGlobals = false;

		if (InternalState.bDebugDump)
		{
			DumpDebugShaderText(Input, RewrittenHlslSource, TEXT("rewritten.hlsl"));
		}
	}

	// Compile HLSL source to SPIR-V binary
	if (!CompilerContext.CompileHlslToSpirv(Options, SerializedOutput.Spirv.Data))
	{
		CompilerContext.FlushErrors(Output.Errors);
		return false;
	}

	// If this shader samples R64 image formats, they need to get converted to STORAGE_IMAGE
	// todo-jnmo: Scope this with a CFLAG if it affects compilation times 
	Patch64bitSamplers(SerializedOutput.Spirv);

	// Build shader output and binding table
	Output.bSucceeded = BuildShaderOutputFromSpirv(CompilerContext, InternalState, SerializedOutput, Output, BindingTable);

	// Flush warnings
	CompilerContext.FlushErrors(Output.Errors);
	return true;
}

#endif // PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX



static void RemoveUnusedBindlessHeaps(FString& PreprocessedShaderSource, const TCHAR* HeapType)
{
	const FString HeapIdentifier = TEXT("VULKAN_") + FString(HeapType) + TEXT("_HEAP(");

	int32 SearchIndex = PreprocessedShaderSource.Find(HeapIdentifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, -1);
	while (SearchIndex != INDEX_NONE)
	{
		const int32 TypeNameStartIndex = SearchIndex + HeapIdentifier.Len();
		const int32 TypeNameEndIndex = PreprocessedShaderSource.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, TypeNameStartIndex);
		FString TypeName(TypeNameEndIndex - TypeNameStartIndex, &PreprocessedShaderSource[TypeNameStartIndex]);
		TypeName.TrimStartAndEndInline();

		// Make sure it's one of our automatically generated types
		if (TypeName.StartsWith(TEXT("SafeType")))
		{
			// Ugly and fast way to make sure ity's a heap declaration (this should catch the generated ones at least)
			if ((PreprocessedShaderSource[TypeNameEndIndex + 1] == '[') &&
				(PreprocessedShaderSource[TypeNameEndIndex + 2] == ']') &&
				(PreprocessedShaderSource[TypeNameEndIndex + 3] == ';'))
			{
				int32 FirstUseIndex = PreprocessedShaderSource.Find(TypeName, ESearchCase::CaseSensitive, ESearchDir::FromStart, TypeNameEndIndex + 4);
				while (FirstUseIndex != INDEX_NONE)
				{
					const int32 NextChar = FirstUseIndex + TypeName.Len();
					if (FChar::IsWhitespace(PreprocessedShaderSource[NextChar]) || PreprocessedShaderSource[NextChar] == ')')
					{
						break;
					}
					FirstUseIndex = PreprocessedShaderSource.Find(TypeName, ESearchCase::CaseSensitive, ESearchDir::FromStart, NextChar);
				}

				if (FirstUseIndex == INDEX_NONE)
				{
					const int32 DeclarationBeginIndex = PreprocessedShaderSource.Find(TypeName, ESearchCase::CaseSensitive, ESearchDir::FromEnd, SearchIndex);
					if ((DeclarationBeginIndex != INDEX_NONE) && ((SearchIndex - DeclarationBeginIndex) < (TypeName.Len() + 4)))
					{
						for (int32 Idx = DeclarationBeginIndex; Idx <= (TypeNameEndIndex + 3); Idx++)
						{
							PreprocessedShaderSource[Idx] = ' ';
						}
					}
				}
			}
		}

		SearchIndex = PreprocessedShaderSource.Find(HeapIdentifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, TypeNameEndIndex + 4);
	}
}

void ModifyVulkanCompilerInput(FShaderCompilerInput& Input)
{
	FVulkanShaderCompilerInternalState InternalState(Input, nullptr);
	Input.Environment.SetDefine(TEXT("COMPILER_HLSLCC"), 1);
	Input.Environment.SetDefine(TEXT("COMPILER_VULKAN"), 1);
	if (InternalState.IsMobileES31())
	{
		Input.Environment.SetDefine(TEXT("ES3_1_PROFILE"), 1);
		Input.Environment.SetDefine(TEXT("VULKAN_PROFILE"), 1);
	}
	else if (InternalState.IsSM6())
	{
		Input.Environment.SetDefine(TEXT("VULKAN_PROFILE_SM6"), 1);
	}
	else if (InternalState.IsSM5())
	{
		Input.Environment.SetDefine(TEXT("VULKAN_PROFILE_SM5"), 1);
	}
	Input.Environment.SetDefine(TEXT("row_major"), TEXT(""));

	Input.Environment.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
	Input.Environment.SetDefine(TEXT("COMPILER_SUPPORTS_DUAL_SOURCE_BLENDING_SLOT_DECORATION"), (uint32)1);
	Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_ROV"), 0); // Disabled until DXC->SPRIV ROV support is implemented

	if (Input.Environment.FullPrecisionInPS || (IsValidRef(Input.SharedEnvironment) && Input.SharedEnvironment->FullPrecisionInPS))
	{
		Input.Environment.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing))
	{
		Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_INLINE_RAY_TRACING"), 1);
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_AllowRealTypes))
	{
		Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_REAL_TYPES"), 1);
	}

	// We have ETargetEnvironment::Vulkan_1_1 by default as a min spec now
	{
		Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_SM6_0_WAVE_OPERATIONS"), 1);
		Input.Environment.SetDefine(TEXT("VULKAN_SUPPORTS_SUBGROUP_SIZE_CONTROL"), 1);
	}

	Input.Environment.SetDefine(TEXT("VULKAN_BINDLESS_SRV_ARRAY_PREFIX"), FShaderParameterParser::kBindlessSRVArrayPrefix);
	Input.Environment.SetDefine(TEXT("VULKAN_BINDLESS_UAV_ARRAY_PREFIX"), FShaderParameterParser::kBindlessUAVArrayPrefix);
	Input.Environment.SetDefine(TEXT("VULKAN_BINDLESS_SAMPLER_ARRAY_PREFIX"), FShaderParameterParser::kBindlessSamplerArrayPrefix);
	Input.Environment.SetDefine(TEXT("VULKAN_MAX_BINDLESS_UNIFORM_BUFFERS_PER_STAGE"), VulkanBindless::MaxUniformBuffersPerStage);

	if (IsAndroidShaderFormat(Input.ShaderFormat))
	{
		// On most Android devices uint64_t is unsupported so we emulate as 2 uint32_t's 
		Input.Environment.SetDefine(TEXT("EMULATE_VKDEVICEADRESS"), 1);
	}

	if (Input.IsRayTracingShader())
	{
		// Name of the structure in raytracing shader records in VulkanCommon.usf
		Input.RequiredSymbols.Add(TEXT("HitGroupSystemRootConstants"));
	}
}

// :todo-jn: TEMPORARY EXPERIMENT - will eventually move into preprocessing step
static TArray<FString> ConvertUBToBindless(FString& PreprocessedShaderSource)
{
	// Fill a map so we pull our bindless sampler/resource indices from the right struct
	// :todo-jn: Do we not have the layout somewhere instead of calculating offsets?  there must be a better way...
	auto GenerateNewDecl = [](const int32 CBIndex, const FString& Members, const FString& CBName)
	{
		const FString PrefixedCBName = FString::Printf(TEXT("%s%d_%s"), *kBindlessCBPrefix, CBIndex, *CBName);
		const FString BindlessCBType = PrefixedCBName + TEXT("_Type");
		const FString BindlessCBHeapName = PrefixedCBName + kBindlessHeapSuffix;
		const FString PaddingName = FString::Printf(TEXT("%s_Padding"), *CBName);

		FString CBDecl;
		CBDecl.Reserve(Members.Len() * 3);  // start somewhere approx less bad

		// Declare the struct
		CBDecl += TEXT("struct ") + BindlessCBType + TEXT(" \n{\n") + Members + TEXT("\n};\n");

		// Declare the safetype and bindless array for this cb
		CBDecl += FString::Printf(TEXT("ConstantBuffer<%s> %s[];\n"), *BindlessCBType, *BindlessCBHeapName);

		// Now bring in the CB
		CBDecl += FString::Printf(TEXT("static const %s %s = %s[VulkanHitGroupSystemParameters.BindlessUniformBuffers[%d]];\n"),
			*BindlessCBType, *PrefixedCBName, *BindlessCBHeapName, CBIndex);

		// Now create a global scope var for each value (as the cbuffer would provide) to patch in seemlessly with the rest of the code
		uint32 MemberOffset = 0;
		const TCHAR* MemberSearchPtr = *Members;
		const uint32 LastMemberSemicolonIndex = Members.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, -1);
		check(LastMemberSemicolonIndex != INDEX_NONE);
		const TCHAR* LastMemberSemicolon = &Members[LastMemberSemicolonIndex];

		do
		{
			const TCHAR* MemberTypeStartPtr = nullptr;
			const TCHAR* MemberTypeEndPtr = nullptr;
			ParseHLSLTypeName(MemberSearchPtr, MemberTypeStartPtr, MemberTypeEndPtr);
			const FString MemberTypeName(MemberTypeEndPtr - MemberTypeStartPtr, MemberTypeStartPtr);

			FString MemberName;
			MemberSearchPtr = ParseHLSLSymbolName(MemberTypeEndPtr, MemberName);
			check(MemberName.Len() > 0);

			if (MemberName.StartsWith(PaddingName))
			{
				while (*MemberSearchPtr && *MemberSearchPtr != ';')
				{
					MemberSearchPtr++;
				}
			}
			else
			{
				// Skip over trailing tokens and pick up arrays
				FString ArrayDecl;
				while (*MemberSearchPtr && *MemberSearchPtr != ';')
				{
					if (*MemberSearchPtr == '[')
					{
						ArrayDecl.AppendChar(*MemberSearchPtr);

						MemberSearchPtr++;
						while (*MemberSearchPtr && *MemberSearchPtr != ']')
						{
							ArrayDecl.AppendChar(*MemberSearchPtr);
							MemberSearchPtr++;
						}

						ArrayDecl.AppendChar(*MemberSearchPtr);
					}

					MemberSearchPtr++;
				}

				CBDecl += FString::Printf(TEXT("static const %s %s%s = %s.%s;\n"), *MemberTypeName, *MemberName, *ArrayDecl, *PrefixedCBName, *MemberName);
			}

			MemberSearchPtr++;

		} while (MemberSearchPtr < LastMemberSemicolon);

		return CBDecl;
	};

	// replace "cbuffer" decl with a struct filled from bindless constant buffer
	TArray<FString> BindlessUBs;
	{
		const FString UniformBufferDeclIdentifier = TEXT("cbuffer");

		int32 SearchIndex = PreprocessedShaderSource.Find(UniformBufferDeclIdentifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, -1);
		while (SearchIndex != INDEX_NONE)
		{
			FString StructName;
			const TCHAR* StructNameEndPtr = ParseHLSLSymbolName(&PreprocessedShaderSource[SearchIndex + UniformBufferDeclIdentifier.Len()], StructName);
			check(StructName.Len() > 0);

			const int32 CBIndex = BindlessUBs.Add(StructName);
			check(CBIndex < 16);

			const TCHAR* OpeningBracePtr = FCString::Strstr(&PreprocessedShaderSource[SearchIndex + UniformBufferDeclIdentifier.Len()], TEXT("{"));
			check(OpeningBracePtr);
			const TCHAR* ClosingBracePtr = FindMatchingClosingBrace(OpeningBracePtr + 1);
			check(ClosingBracePtr);
			const int32 ClosingBraceIndex = ClosingBracePtr - (*PreprocessedShaderSource);

			const FString Members(ClosingBracePtr - OpeningBracePtr - 1, OpeningBracePtr + 1);
			const FString NewDecl = GenerateNewDecl(CBIndex, Members, StructName);

			const int32 OldDeclLen = ClosingBraceIndex - SearchIndex + 1;
			PreprocessedShaderSource.RemoveAt(SearchIndex, OldDeclLen, EAllowShrinking::No);
			PreprocessedShaderSource.InsertAt(SearchIndex, NewDecl);

			SearchIndex = PreprocessedShaderSource.Find(UniformBufferDeclIdentifier, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchIndex + NewDecl.Len());
		}
	}
	return BindlessUBs;
}


static void UpdateBindlessUBs(const FVulkanShaderCompilerInternalState& InternalState, VulkanShaderCompilerSerializedOutput& SerializedOutput, FShaderCompilerOutput& Output)
{
	auto GetLayoutHash = [&InternalState](const FString& UBName)
	{
		uint32 LayoutHash = 0;
		const FUniformBufferEntry* UniformBufferEntry = InternalState.Input.Environment.UniformBufferMap.Find(UBName);
		if (UniformBufferEntry)
		{
			LayoutHash = UniformBufferEntry->LayoutHash;
		}
		else if ((UBName == FShaderParametersMetadata::kRootUniformBufferBindingName) && InternalState.Input.RootParametersStructure)
		{
			LayoutHash = InternalState.Input.RootParametersStructure->GetLayoutHash();
		}
		else
		{
			LayoutHash = 0;
		}
		return LayoutHash;
	};

	SerializedOutput.Header.UniformBuffers.Empty();
	for (int32 CBIndex = 0; CBIndex < InternalState.AllBindlessUBs.Num(); CBIndex++)
	{
		const FString& CBName = InternalState.AllBindlessUBs[CBIndex];

		// It's possible SPIRV compilation has optimized out a buffer from every shader in the group
		if (SerializedOutput.UsedBindlessUB.Contains(CBName))
		{
			FVulkanShaderHeader::FUniformBufferInfo& UBInfo = SerializedOutput.Header.UniformBuffers.AddZeroed_GetRef();
			UBInfo.LayoutHash = GetLayoutHash(CBName);
			UBInfo.ConstantDataOriginalBindingIndex = CBIndex;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
			UBInfo.DebugName = CBName;
#endif

			const int32 UBIndex = SerializedOutput.Header.UniformBuffers.Num() - 1;
			Output.ParameterMap.AddParameterAllocation(*CBName, UBIndex, (uint16)FVulkanShaderHeader::UniformBuffer, 1, EShaderParameterType::UniformBuffer);
		}
	}
}


static bool CompileShaderGroup(
	FVulkanShaderCompilerInternalState& InternalState,
	const FShaderSource::FStringType& OriginalPreprocessedShaderSource,
	FShaderCompilerOutput& MergedOutput
)
{
	checkf(InternalState.bSupportsBindless && InternalState.bUseBindlessUniformBuffer, TEXT("Ray tracing requires full bindless in Vulkan."));

	// Compile each one of the shader modules seperately and create one big blob for the engine
	auto CompilePartialExport = [&OriginalPreprocessedShaderSource, &InternalState, &MergedOutput](
		FVulkanShaderCompilerInternalState::EHitGroupShaderType HitGroupShaderType,
		const TCHAR* PartialFileExtension,
		VulkanShaderCompilerSerializedOutput& PartialSerializedOutput)
	{
		InternalState.HitGroupShaderType = HitGroupShaderType;

		FShaderCompilerOutput TempOutput;
		const bool bIsClosestHit = (HitGroupShaderType == FVulkanShaderCompilerInternalState::EHitGroupShaderType::ClosestHit);
		FShaderCompilerOutput& PartialOutput = bIsClosestHit ? MergedOutput : TempOutput;

		FShaderSource::FViewType OrigSourceView(OriginalPreprocessedShaderSource);
		FShaderSource PartialPreprocessedShaderSource(OrigSourceView);
		UE::ShaderCompilerCommon::RemoveDeadCode(PartialPreprocessedShaderSource, InternalState.GetEntryPointName(), PartialOutput.Errors);

		if (InternalState.bDebugDump)
		{
			DumpDebugShaderText(InternalState.Input, PartialPreprocessedShaderSource.GetView().GetData(), *FString::Printf(TEXT("%s.hlsl"), PartialFileExtension));
		}

		const bool bPartialSuccess = CompileWithShaderConductor(InternalState, PartialPreprocessedShaderSource.GetView(), PartialSerializedOutput, PartialOutput);

		if (!bIsClosestHit)
		{
			MergedOutput.NumInstructions = FMath::Max(MergedOutput.NumInstructions, PartialOutput.NumInstructions);
			MergedOutput.NumTextureSamplers = FMath::Max(MergedOutput.NumTextureSamplers, PartialOutput.NumTextureSamplers);
			MergedOutput.Errors.Append(MoveTemp(PartialOutput.Errors));
		}

		return bPartialSuccess;
	};

	bool bSuccess = false;

	// Closest Hit Module, always present
	VulkanShaderCompilerSerializedOutput ClosestHitSerializedOutput;
	{
		bSuccess = CompilePartialExport(FVulkanShaderCompilerInternalState::EHitGroupShaderType::ClosestHit, TEXT("closest"), ClosestHitSerializedOutput);
	}

	// Any Hit Module, optional
	const bool bHasAnyHitModule = !InternalState.AnyHitEntry.IsEmpty();
	VulkanShaderCompilerSerializedOutput AnyHitSerializedOutput;
	if (bSuccess && bHasAnyHitModule)
	{
		bSuccess = CompilePartialExport(FVulkanShaderCompilerInternalState::EHitGroupShaderType::AnyHit, TEXT("anyhit"), AnyHitSerializedOutput);
	}

	// Intersection Module, optional
	const bool bHasIntersectionModule = !InternalState.IntersectionEntry.IsEmpty();
	VulkanShaderCompilerSerializedOutput IntersectionSerializedOutput;
	if (bSuccess && bHasIntersectionModule)
	{
		bSuccess = CompilePartialExport(FVulkanShaderCompilerInternalState::EHitGroupShaderType::Intersection, TEXT("intersection"), IntersectionSerializedOutput);
	}

	// Collapse the bindless UB usage into one set and then update the headers
	ClosestHitSerializedOutput.UsedBindlessUB.Append(AnyHitSerializedOutput.UsedBindlessUB);
	ClosestHitSerializedOutput.UsedBindlessUB.Append(IntersectionSerializedOutput.UsedBindlessUB);
	UpdateBindlessUBs(InternalState, ClosestHitSerializedOutput, MergedOutput);

	{
		// :todo-jn: Having multiple entrypoints in a single SPIRV blob crashes on FLumenHardwareRayTracingMaterialHitGroup for some reason
		// Adjust the header before we write it out
		ClosestHitSerializedOutput.Header.RayGroupAnyHit = bHasAnyHitModule ? FVulkanShaderHeader::ERayHitGroupEntrypoint::SeparateBlob : FVulkanShaderHeader::ERayHitGroupEntrypoint::NotPresent;
		ClosestHitSerializedOutput.Header.RayGroupIntersection = bHasIntersectionModule ? FVulkanShaderHeader::ERayHitGroupEntrypoint::SeparateBlob : FVulkanShaderHeader::ERayHitGroupEntrypoint::NotPresent;

		check(ClosestHitSerializedOutput.Spirv.Data.Num() != 0);
		FMemoryWriter Ar(MergedOutput.ShaderCode.GetWriteAccess(), true);
		Ar << ClosestHitSerializedOutput.Header;
		Ar << ClosestHitSerializedOutput.ShaderResourceTable;

		{
			uint32 SpirvCodeSizeBytes = ClosestHitSerializedOutput.Spirv.GetByteSize();
			Ar << SpirvCodeSizeBytes;
			Ar.Serialize((uint8*)ClosestHitSerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}

		if (bHasAnyHitModule)
		{
			uint32 SpirvCodeSizeBytes = AnyHitSerializedOutput.Spirv.GetByteSize();
			Ar << SpirvCodeSizeBytes;
			Ar.Serialize((uint8*)AnyHitSerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}

		if (bHasIntersectionModule)
		{
			uint32 SpirvCodeSizeBytes = IntersectionSerializedOutput.Spirv.GetByteSize();
			Ar << SpirvCodeSizeBytes;
			Ar.Serialize((uint8*)IntersectionSerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}
	}

	MergedOutput.bSucceeded = bSuccess;
	return bSuccess;
}

struct FPS5ShaderParameterParserPlatformConfiguration : public FShaderParameterParser::FPlatformConfiguration
{
	FPS5ShaderParameterParserPlatformConfiguration(const FShaderCompilerInput& Input)
		: FShaderParameterParser::FPlatformConfiguration()
	{
		EnumAddFlags(Flags, EShaderParameterParserConfigurationFlags::SupportsBindless | EShaderParameterParserConfigurationFlags::BindlessUsesArrays);

		// Create a _RootShaderParameters and bind it in slot 0 like any other uniform buffer
		if (Input.Target.GetFrequency() == SF_RayGen && Input.RootParametersStructure != nullptr)
		{
			ConstantBufferType = TEXTVIEW("cbuffer");
			EnumAddFlags(Flags, EShaderParameterParserConfigurationFlags::UseStableConstantBuffer);
		}
	}

	virtual FString GenerateBindlessAccess(EBindlessConversionType BindlessType, FStringView ShaderTypeString, FStringView IndexString) const final
	{
		checkf(false, TEXT("Vulkan does not use GenerateBindlessAccess"));
		return FString();
	}
};

void CompileVulkanShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& InPreprocessOutput, FShaderCompilerOutput& Output, const class FString& WorkingDirectory)
{
	check(IsVulkanShaderFormat(Input.ShaderFormat));

	FString EntryPointName = Input.EntryPointName;
	FString PreprocessedSource(InPreprocessOutput.GetSourceViewWide());

	FPS5ShaderParameterParserPlatformConfiguration PlatformConfiguration(Input);
	FShaderParameterParser ShaderParameterParser(PlatformConfiguration);
	if (!ShaderParameterParser.ParseAndModify(Input, Output.Errors, PreprocessedSource))
	{
		// The FShaderParameterParser will add any relevant errors.
		return;
	}

	FVulkanShaderCompilerInternalState InternalState(Input, &ShaderParameterParser);

	//TODO: this additional step causes problems for the error remapping that occurs when the preprocessed job cache is enabled
	// (the additional deadstripping step causes further changes to line numbers, removed blocks, etc).
	//if (InternalState.bSupportsBindless)
	//{
	//	// Clean up the code a bit, it's unreadable otherwise with all the unused heaps left around
	//	// Re-run the dead stripper after removing these unused heaps
	//	RemoveUnusedBindlessHeaps(PreprocessedSource, TEXT("SAMPLER"));
	//	RemoveUnusedBindlessHeaps(PreprocessedSource, TEXT("RESOURCE"));
	//	UE::ShaderCompilerCommon::RemoveDeadCode(PreprocessedSource, Input.EntryPointName, Input.RequiredSymbols, Output.Errors);

	//	if (InternalState.bDebugDump)
	//	{
	//		DumpDebugShaderText(Input, PreprocessedSource, TEXT("bindless.final.hlsl"));
	//	}
	//}

	const EHlslShaderFrequency HlslFrequency = InternalState.GetHlslShaderFrequency();
	if (HlslFrequency == HSF_InvalidFrequency)
	{
		Output.bSucceeded = false;
		FShaderCompilerError& NewError = Output.Errors.AddDefaulted_GetRef();
		NewError.StrippedErrorMessage = FString::Printf(
			TEXT("%s shaders not supported for use in Vulkan."),
			CrossCompiler::GetFrequencyName(InternalState.GetShaderFrequency()));
		return;
	}

	if (InternalState.bUseBindlessUniformBuffer)
	{
		InternalState.AllBindlessUBs = ConvertUBToBindless(PreprocessedSource);
	}

	if (ShaderParameterParser.DidModifyShader() || InternalState.AllBindlessUBs.Num() > 0)
	{
		Output.ModifiedShaderSource = PreprocessedSource;
	}

	bool bSuccess = false;

#if SHADER_SOURCE_ANSI
	// Convert to ANSI prior to calling into ShaderConductor. This copy would have been incurred
	// by SC itself anyways, but would (will?) also be unnecessary if (when) shader parameter parser
	// is modified to operate on ANSI strings.
	const FShaderSource::FStringType PreprocessedSourceToCompile(PreprocessedSource);
#else
	const FShaderSource::FStringType& PreprocessedSourceToCompile = PreprocessedSource;
#endif

#if PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX
	// HitGroup shaders might have multiple entrypoints that we combine into a single blob
	if (InternalState.HasMultipleEntryPoints())
	{
		bSuccess = CompileShaderGroup(InternalState, PreprocessedSourceToCompile, Output);
	}
	else
	{
		// Compile regular shader via ShaderConductor (DXC)
		VulkanShaderCompilerSerializedOutput SerializedOutput;
		bSuccess = CompileWithShaderConductor(InternalState, PreprocessedSourceToCompile, SerializedOutput, Output);

		if (InternalState.bUseBindlessUniformBuffer)
		{
			UpdateBindlessUBs(InternalState, SerializedOutput, Output);
		}

		// Write out the header and shader source code (except for the extra shaders in hit groups)
		checkf(!(bSuccess && SerializedOutput.Spirv.Data.Num() == 0), TEXT("shader compilation was reported as successful but SPIR-V module is empty"));
		FMemoryWriter Ar(Output.ShaderCode.GetWriteAccess(), true);
		Ar << SerializedOutput.Header;
		Ar << SerializedOutput.ShaderResourceTable;

		uint32 SpirvCodeSizeBytes = SerializedOutput.Spirv.GetByteSize();
		Ar << SpirvCodeSizeBytes;
		if (SerializedOutput.Spirv.Data.Num() > 0)
		{
			Ar.Serialize((uint8*)SerializedOutput.Spirv.Data.GetData(), SpirvCodeSizeBytes);
		}
	}
#endif // PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX
	
	if (Input.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
	{
		Output.ShaderCode.AddOptionalData(FShaderCodeName::Key, TCHAR_TO_UTF8(*Input.GenerateShaderName()));
	}

	Output.SerializeShaderCodeValidation();

	ShaderParameterParser.ValidateShaderParameterTypes(Input, InternalState.IsMobileES31(), Output);
	
	if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::CompileFromDebugUSF))
	{
		for (const auto& Error : Output.Errors)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), *Error.GetErrorStringWithLineMarker());
		}
		ensure(bSuccess);
	}
}

void OutputVulkanDebugData(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, const FShaderCompilerOutput& Output)
{
	UE::ShaderCompilerCommon::DumpExtendedDebugShaderData(Input, PreprocessOutput, Output);
}
