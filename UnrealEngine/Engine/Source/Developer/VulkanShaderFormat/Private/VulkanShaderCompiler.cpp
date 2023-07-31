// Copyright Epic Games, Inc. All Rights Reserved.
// .

#include "VulkanShaderFormat.h"
#include "VulkanCommon.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"
#include "ShaderParameterParser.h"
#include "HlslccHeaderWriter.h"
#include "hlslcc.h"
#include "SpirvReflectCommon.h"
#include "RHIShaderFormatDefinitions.inl"

#if PLATFORM_MAC
// Horrible hack as we need the enum available but the Vulkan headers do not compile on Mac
enum VkDescriptorType {
	VK_DESCRIPTOR_TYPE_SAMPLER = 0,
	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
	VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
	VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
	VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
	VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
	VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
	VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10,
	VK_DESCRIPTOR_TYPE_BEGIN_RANGE = VK_DESCRIPTOR_TYPE_SAMPLER,
	VK_DESCRIPTOR_TYPE_END_RANGE = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
	VK_DESCRIPTOR_TYPE_RANGE_SIZE = (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1),
	VK_DESCRIPTOR_TYPE_MAX_ENUM = 0x7FFFFFFF
};
#else
#include "vulkan.h"
#endif
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

inline CrossCompiler::FShaderConductorOptions::ETargetEnvironment GetMinimumTargetEnvironment(EShaderPlatform ShaderPlatform)
{
	// All desktop Vulkan platforms create a Vulkan 1.1 instance (SP_VULKAN_SM5_ANDROID can still use 1.0)
	return (ShaderPlatform == SP_VULKAN_SM5) ? 
		CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_1: 
		CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_0;
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

static FString GetResourceEntryFromUBMember(const TMap<FString, FResourceTableEntry>& ResourceTableMap, const FString& UBName, uint16 ResourceIndex, FResourceTableEntry& OutEntry)
{
	for (const auto& Pair : ResourceTableMap)
	{
		if (Pair.Value.UniformBufferName == UBName && Pair.Value.ResourceIndex == ResourceIndex)
		{
			OutEntry = Pair.Value;
			return Pair.Key;
		}
	}

	check(0);
	return "";
}

static FString FindTextureNameForSamplerState(const CrossCompiler::FHlslccHeader& CCHeader, const FString& InSamplerName)
{
	for (const auto& Sampler : CCHeader.Samplers)
	{
		for (const auto& SamplerState : Sampler.SamplerStates)
		{
			if (SamplerState == InSamplerName)
			{
				return Sampler.Name;
			}
		}
	}
	return TEXT("");
}

static uint16 GetCombinedSamplerStateAlias(const FString& ParameterName,
											VkDescriptorType DescriptorType,
											const FVulkanBindingTable& BindingTable,
											const CrossCompiler::FHlslccHeader& CCHeader,
											const TArray<FString>& GlobalNames)
{
	if (DescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
	{
		if (!ContainsBinding(BindingTable, ParameterName))
		{
			// Not found as a regular parameter, find corresponding Texture and return that ResourceEntryIndex
			const FString& TextureName = FindTextureNameForSamplerState(CCHeader, ParameterName);
			check(TextureName.Len() > 0);

			int32 Found = GlobalNames.Find(TextureName);
			check(Found >= 0);
			return (uint16)Found;
		}
	}

	return UINT16_MAX;
}

struct FPatchType
{
	int32	HeaderGlobalIndex;
	uint16	CombinedAliasIndex;
};


class FVulkanShaderSerializedBindings : public CrossCompiler::FShaderBindings
{
public:
	FVulkanShaderSerializedBindings()
	{
		NumSamplers = 0;
		NumUniformBuffers = 0;
		NumUAVs = 0;
		NumAccelerationStructures = 0;
		bHasRegularUniformBuffers = 0;
	}
};

struct FOLDVulkanCodeHeader
{
	FVulkanShaderSerializedBindings SerializedBindings;

	struct FShaderDescriptorInfo
	{
		TArray<VkDescriptorType> DescriptorTypes;
		uint16 NumImageInfos;
		uint16 NumBufferInfos;
	};
	FShaderDescriptorInfo NEWDescriptorInfo;

	struct FPackedUBToVulkanBindingIndex
	{
		CrossCompiler::EPackedTypeName	TypeName;
		uint8							VulkanBindingIndex;
	};
	TArray<FPackedUBToVulkanBindingIndex> NEWPackedUBToVulkanBindingIndices;

	// List of memory copies from RHIUniformBuffer to packed uniforms when emulating UB's
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;

	FString ShaderName;
	FSHAHash SourceHash;

	uint64 UniformBuffersWithDescriptorMask;

	// Number of uniform buffers (not including PackedGlobalUBs) UNUSED
	uint32 UNUSED_NumNonGlobalUBs;

	// (Separated to improve cache) if this is non-zero, then we can assume all UBs are emulated
	TArray<uint32> NEWPackedGlobalUBSizes;

	// Number of copies per emulated buffer source index (to skip searching among UniformBuffersCopyInfo). Upper uint16 is the index, Lower uint16 is the count
	TArray<uint32> NEWEmulatedUBCopyRanges;
};

static void AddImmutable(FVulkanShaderHeader& OutHeader, int32 GlobalIndex)
{
	check(GlobalIndex < UINT16_MAX);
	OutHeader.Globals[GlobalIndex].bImmutableSampler = true;
}

static int32 AddGlobal(FOLDVulkanCodeHeader& OLDHeader,
						const FVulkanBindingTable& BindingTable,
						const CrossCompiler::FHlslccHeader& CCHeader,
						const FString& ParameterName,
						uint16 BindingIndex,
						const FVulkanSpirv& Spirv,
						FVulkanShaderHeader& OutHeader,
						const TArray<FString>& GlobalNames,
						TArray<FPatchType>& OutTypePatch,
						uint16 CombinedAliasIndex)
{
	int32 HeaderGlobalIndex = GlobalNames.Find(ParameterName);//OutHeader.Globals.AddZeroed();
	check(HeaderGlobalIndex != INDEX_NONE);
	check(GlobalNames[HeaderGlobalIndex] == ParameterName);

	FVulkanShaderHeader::FGlobalInfo& GlobalInfo = OutHeader.Globals[HeaderGlobalIndex];
	const FVulkanSpirv::FEntry* Entry = Spirv.GetEntry(ParameterName);
	bool bIsCombinedSampler = false;
	if (Entry)
	{
		if (Entry->Binding == -1)
		{
			// Texel buffers get put into a uniform block
			Entry = Spirv.GetEntry(ParameterName + TEXT("_BUFFER"));
			check(Entry);
			check(Entry->Binding != -1);
		}
	}
	else
	{
		Entry = CombinedAliasIndex == UINT16_MAX ? Spirv.GetEntryByBindingIndex(BindingIndex) : Spirv.GetEntry(GlobalNames[CombinedAliasIndex]);
		check(Entry);
		check(Entry->Binding != -1);
		if (!Entry->Name.EndsWith(TEXT("_BUFFER")))
		{
			bIsCombinedSampler = true;
		}
	}

	VkDescriptorType DescriptorType = bIsCombinedSampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : OLDHeader.NEWDescriptorInfo.DescriptorTypes[Entry->Binding];

	GlobalInfo.OriginalBindingIndex = Entry->Binding;
	OutHeader.GlobalSpirvInfos[HeaderGlobalIndex] = FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex);
	if (bIsCombinedSampler)
	{
		uint16 NewCombinedAliasIndex = GetCombinedSamplerStateAlias(ParameterName, DescriptorType, BindingTable, CCHeader, GlobalNames);
		check(NewCombinedAliasIndex != UINT16_MAX);

		{
			// Ideally we would set up the type index here, but we might not have processed the aliased texture yet:
			//		GlobalInfo.TypeIndex = OutHeader.Globals[NewCombinedAliasIndex].TypeIndex;
			// Instead postpone this patching
			GlobalInfo.TypeIndex = UINT16_MAX;
			OutTypePatch.Add({HeaderGlobalIndex, NewCombinedAliasIndex});
		}

		GlobalInfo.CombinedSamplerStateAliasIndex = CombinedAliasIndex == UINT16_MAX ? NewCombinedAliasIndex : CombinedAliasIndex;
	}
	else
	{
		int32 GlobalDescriptorTypeIndex = OutHeader.GlobalDescriptorTypes.Add(DescriptorTypeToBinding(DescriptorType));
		GlobalInfo.TypeIndex = GlobalDescriptorTypeIndex;
		check(GetCombinedSamplerStateAlias(ParameterName, DescriptorType, BindingTable, CCHeader, GlobalNames) == UINT16_MAX);
		GlobalInfo.CombinedSamplerStateAliasIndex = UINT16_MAX;
	}
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	GlobalInfo.DebugName = ParameterName;
#endif

	return HeaderGlobalIndex;
}

static int32 AddGlobalForUBEntry(FOLDVulkanCodeHeader& OLDHeader,
									const FVulkanBindingTable& BindingTable,
									const CrossCompiler::FHlslccHeader& CCHeader,
									const FString& ParameterName,
									uint16 BindingIndex,
									const FVulkanSpirv& Spirv,
									const TArray<FString>&
									GlobalNames,
									EUniformBufferBaseType UBEntryType,
									TArray<FPatchType>& OutTypePatch,
									FVulkanShaderHeader& OutHeader)
{
	uint16 CombinedAliasIndex = UINT16_MAX;
	if (UBEntryType == UBMT_SAMPLER)
	{
		if (!ContainsBinding(BindingTable, ParameterName))
		{
			// Not found as a regular parameter, find corresponding Texture and return that ResourceEntryIndex
			const FString& TextureName = FindTextureNameForSamplerState(CCHeader, ParameterName);
			check(TextureName.Len() > 0);

			int32 TextureGlobalIndex = GlobalNames.Find(TextureName);
			check(TextureGlobalIndex >= 0);

			CombinedAliasIndex = (uint16)TextureGlobalIndex;
		}
	}

	return AddGlobal(OLDHeader, BindingTable, CCHeader, ParameterName, BindingIndex, Spirv, OutHeader, GlobalNames, OutTypePatch, CombinedAliasIndex);
}

static void AddUBResources(FOLDVulkanCodeHeader& OLDHeader,
							const FString& UBName,
							const TMap<FString, FResourceTableEntry>& ResourceTableMap,
							uint32 BufferIndex,
							const TArray<uint32>& BindingArray,
							const FVulkanBindingTable& BindingTable,
							const TArray<VkDescriptorType>& DescriptorTypes,
							const FVulkanSpirv& Spirv,
							const CrossCompiler::FHlslccHeader& CCHeader,
							FVulkanShaderHeader::FUniformBufferInfo& OutUBInfo,
							FVulkanShaderHeader& OutHeader,
							TArray<FPatchType>& OutTypePatch,
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

				FResourceTableEntry ResourceTableEntry;
				FString MemberName = GetResourceEntryFromUBMember(ResourceTableMap, UBName, ResourceIndex, ResourceTableEntry);

				int32 HeaderUBResourceInfoIndex = OutUBInfo.ResourceEntries.AddZeroed();
				FVulkanShaderHeader::FUBResourceInfo& UBResourceInfo = OutUBInfo.ResourceEntries[HeaderUBResourceInfoIndex];

				int32 HeaderGlobalIndex = AddGlobalForUBEntry(OLDHeader, BindingTable, CCHeader, MemberName, BindingIndex, Spirv, GlobalNames, (EUniformBufferBaseType)ResourceTableEntry.Type, OutTypePatch, OutHeader);
				UBResourceInfo.SourceUBResourceIndex = ResourceIndex;
				UBResourceInfo.OriginalBindingIndex = BindingIndex;
				UBResourceInfo.GlobalIndex = HeaderGlobalIndex;
				UBResourceInfo.UBBaseType = (EUniformBufferBaseType)ResourceTableEntry.Type;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
				UBResourceInfo.DebugName = MemberName;
#endif
				// Iterate to next info
				ResourceInfo = *ResourceInfos++;
			}
			while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
		}
	}
}

static void AddUniformBuffer(FOLDVulkanCodeHeader& OLDHeader,
	const FVulkanBindingTable& BindingTable,
	const FShaderCompilerInput& ShaderInput,
	const CrossCompiler::FHlslccHeader& CCHeader,
	const FVulkanSpirv& Spirv,
	const FString& UBName,
	uint16 BindingIndex,
	FShaderParameterMap& InOutParameterMap,
	FVulkanShaderHeader& OutHeader,
	TArray<FPatchType>& OutTypePatch,
	TArray<FString>& GlobalNames)
{
	int32 HeaderUBIndex = -1;
	HeaderUBIndex = OutHeader.UniformBuffers.AddZeroed();

	FVulkanShaderHeader::FUniformBufferInfo& UBInfo = OutHeader.UniformBuffers[HeaderUBIndex];
	const FUniformBufferEntry* UniformBufferEntry = ShaderInput.Environment.UniformBufferMap.Find(UBName);
	UBInfo.LayoutHash = UniformBufferEntry ? UniformBufferEntry->LayoutHash : 0;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	UBInfo.DebugName = UBName;
#endif
	const FVulkanSpirv::FEntry* Entry = Spirv.GetEntry(UBName);
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
	const FShaderCompilerResourceTable& SRT = OLDHeader.SerializedBindings.ShaderResourceTable;
	if (SRT.ResourceTableBits & (1 << BindingIndex))
	{
		// Make sure to process in the same order as when gathering names below
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.TextureMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.SamplerMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.ShaderResourceViewMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
		AddUBResources(OLDHeader, UBName, ShaderInput.Environment.ResourceTableMap, BindingIndex, SRT.UnorderedAccessViewMap, BindingTable, OLDHeader.NEWDescriptorInfo.DescriptorTypes, Spirv, CCHeader, UBInfo, OutHeader, OutTypePatch, GlobalNames);
	}
	else
	{
		// If we're using real uniform buffers we have to have resources at least
		check(!UBInfo.bOnlyHasResources);
	}

	// Currently we don't support mismatched uniform buffer layouts/cbuffers with resources!
	check(UniformBufferEntry || UBInfo.ResourceEntries.Num() == 0);

	InOutParameterMap.RemoveParameterAllocation(*UBName);
	InOutParameterMap.AddParameterAllocation(*UBName, HeaderUBIndex, (uint16)FVulkanShaderHeader::UniformBuffer, 1, EShaderParameterType::UniformBuffer);
}

static int32 DoAddGlobal(const FString& Name, FVulkanShaderHeader& OutHeader, TArray<FString>& OutGlobalNames)
{
	check(!OutGlobalNames.Contains(Name));
	int32 NameIndex = OutGlobalNames.Add(Name);
	int32 GlobalIndex = OutHeader.Globals.AddDefaulted();
	check(NameIndex == GlobalIndex);
	int32 GlobalSpirvIndex = OutHeader.GlobalSpirvInfos.AddDefaulted();
	check(GlobalSpirvIndex == GlobalIndex);
	return GlobalIndex;
}

struct FVulkanHlslccHeader : public CrossCompiler::FHlslccHeader
{
	virtual bool ParseCustomHeaderEntries(const ANSICHAR*& ShaderSource) override
	{
		if (FCStringAnsi::Strncmp(ShaderSource, "// @ExternalTextures: ", 22) == 0)
		{
			ShaderSource += 22;
			while (*ShaderSource && *ShaderSource != '\n')
			{
				FString ExternalTextureName;
				if (!CrossCompiler::ParseIdentifier(ShaderSource, ExternalTextureName))
				{
					return false;
				}

				ExternalTextures.Add(ExternalTextureName);

				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				if (Match(ShaderSource, ','))
				{
					continue;
				}
			}
		}

		return true;
	}

	TArray<FString> ExternalTextures;
};

static void PrepareUBResourceEntryGlobals(const FVulkanHlslccHeader& CCHeader, const TArray<uint32>& BindingArray, const TMap<FString, FResourceTableEntry>& ResourceTableMap,
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

				// Extract binding index from ResourceInfo
				const uint32 BindingIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

				// Extract index of the resource stored in the resource table from ResourceInfo
				const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);

				FResourceTableEntry ResourceTableEntry;
				FString MemberName = GetResourceEntryFromUBMember(ResourceTableMap, UBName, ResourceIndex, ResourceTableEntry);

				int32 GlobalIndex = DoAddGlobal(MemberName, OutHeader, OutGlobalNames);
				if (CCHeader.ExternalTextures.Contains(MemberName))
				{
					AddImmutable(OutHeader, GlobalIndex);
				}

				// Iterate to next info
				ResourceInfo = *ResourceInfos++;
			}
			while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
		}
	}
}

static bool IsSamplerState(const CrossCompiler::FHlslccHeader& CCHeader, const FString& ParameterName)
{
	for (const auto& Sampler : CCHeader.Samplers)
	{
		if (Sampler.SamplerStates.Contains(ParameterName))
		{
			return true;
		}
	}

	return false;
}

static void PrepareGlobals(const FVulkanBindingTable& BindingTable, const FVulkanHlslccHeader& CCHeader, const FShaderCompilerResourceTable& SRT, const TMap<FString, FVulkanShaderHeader::EType>& EntryTypes, const FShaderCompilerInput& ShaderInput, const TArray<FString>& ParameterNames, FShaderParameterMap& ParameterMap, TArray<FString>& OutGlobalNames, FVulkanShaderHeader& OutHeader)
{
	// First pass, gather names for all the Globals that are NOT Samplers
	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		const FString& ParameterName = *ParameterNames[ParameterIndex];
		ParameterMap.FindParameterAllocation(*ParameterName, BufferIndex, BaseIndex, Size);

		auto AddGlobalNamesForUB = [&]()
		{
			// Add used resources...
			if (SRT.ResourceTableBits & (1 << BufferIndex))
			{
				PrepareUBResourceEntryGlobals(CCHeader, SRT.TextureMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
				PrepareUBResourceEntryGlobals(CCHeader, SRT.ShaderResourceViewMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
				PrepareUBResourceEntryGlobals(CCHeader, SRT.UnorderedAccessViewMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
			}
		};

		const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
		if (FoundType)
		{
			switch (*FoundType)
			{
			case FVulkanShaderHeader::Global:
				if (!IsSamplerState(CCHeader, ParameterName))
				{
					int32 GlobalIndex = DoAddGlobal(ParameterName, OutHeader, OutGlobalNames);
					if (CCHeader.ExternalTextures.Contains(ParameterName))
					{
						AddImmutable(OutHeader, GlobalIndex);
					}
				}
				break;
			case FVulkanShaderHeader::UniformBuffer:
				AddGlobalNamesForUB();
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
			AddGlobalNamesForUB();
		}
	}

	// Second pass, add all samplers
	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		const FString& ParameterName = *ParameterNames[ParameterIndex];
		ParameterMap.FindParameterAllocation(*ParameterName, BufferIndex, BaseIndex, Size);

		auto AddGlobalNamesForUB = [&]()
		{
			// Add used resources...
			if (SRT.ResourceTableBits & (1 << BufferIndex))
			{
				PrepareUBResourceEntryGlobals(CCHeader, SRT.SamplerMap, ShaderInput.Environment.ResourceTableMap, BufferIndex, ParameterName, OutGlobalNames, OutHeader);
			}
		};

		const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
		if (FoundType)
		{
			switch (*FoundType)
			{
			case FVulkanShaderHeader::Global:
				if (IsSamplerState(CCHeader, ParameterName))
				{
					int32 GlobalIndex = DoAddGlobal(ParameterName, OutHeader, OutGlobalNames);
					if (CCHeader.ExternalTextures.Contains(ParameterName))
					{
						AddImmutable(OutHeader, GlobalIndex);
					}
				}
				break;
			case FVulkanShaderHeader::UniformBuffer:
				AddGlobalNamesForUB();
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
			AddGlobalNamesForUB();
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

static void ConvertToNEWHeader(FOLDVulkanCodeHeader& OLDHeader,
	const FVulkanBindingTable& BindingTable,
	const FVulkanSpirv& Spirv,
	const TMap<FString, FVulkanShaderHeader::EType>& EntryTypes,
	const FShaderCompilerInput& ShaderInput,
	FVulkanHlslccHeader& CCHeader,
	FShaderParameterMap& InOutParameterMap,
	FVulkanShaderHeader& OutHeader)
{
	// Names that match the Header.Globals array
	TArray<FString> GlobalNames;

	TArray<FPatchType> TypePatchList;

	TArray<FString> ParameterNames;
	InOutParameterMap.GetAllParameterNames(ParameterNames);

	const FShaderCompilerResourceTable& SRT = OLDHeader.SerializedBindings.ShaderResourceTable;

	PrepareGlobals(BindingTable, CCHeader, SRT, EntryTypes, ShaderInput, ParameterNames, InOutParameterMap, GlobalNames, OutHeader);

	for (int32 ParameterIndex = 0; ParameterIndex < ParameterNames.Num(); ++ParameterIndex)
	{
		uint16 BufferIndex;
		uint16 BaseIndex;
		uint16 Size;
		const FString& ParameterName = *ParameterNames[ParameterIndex];
		InOutParameterMap.FindParameterAllocation(*ParameterName, BufferIndex, BaseIndex, Size);

		const FVulkanShaderHeader::EType* FoundType = EntryTypes.Find(ParameterName);
		if (FoundType)
		{
			switch (*FoundType)
			{
			case FVulkanShaderHeader::Global:
				{
					int32 HeaderGlobalIndex = AddGlobal(OLDHeader, BindingTable, CCHeader, ParameterName, BaseIndex, Spirv, OutHeader, GlobalNames, TypePatchList, UINT16_MAX);

					const FParameterAllocation* ParameterAllocation = InOutParameterMap.GetParameterMap().Find(*ParameterName);
					check(ParameterAllocation);
					const EShaderParameterType ParamType = ParameterAllocation->Type;

					InOutParameterMap.RemoveParameterAllocation(*ParameterName);
					InOutParameterMap.AddParameterAllocation(*ParameterName, (uint16)FVulkanShaderHeader::Global, HeaderGlobalIndex, Size, ParamType);
				}
				break;
			case FVulkanShaderHeader::PackedGlobal:
				{
					int32 HeaderPackedGlobalIndex = OutHeader.PackedGlobals.AddZeroed();
					FVulkanShaderHeader::FPackedGlobalInfo& PackedGlobalInfo = OutHeader.PackedGlobals[HeaderPackedGlobalIndex];
					PackedGlobalInfo.PackedTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(OLDHeader.NEWPackedUBToVulkanBindingIndices[BufferIndex].TypeName);
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
				AddUniformBuffer(OLDHeader, BindingTable, ShaderInput, CCHeader, Spirv, ParameterName, BufferIndex, InOutParameterMap, OutHeader, TypePatchList, GlobalNames);
				break;
			default:
				check(0);
				break;
			}
		}
		else
		{
			// Not found means it's a new resource-only UniformBuffer
			AddUniformBuffer(OLDHeader, BindingTable, ShaderInput, CCHeader, Spirv, ParameterName, BufferIndex, InOutParameterMap, OutHeader, TypePatchList, GlobalNames);
		}
	}

	// Process the type patch list
	for (const FPatchType& Patch : TypePatchList)
	{
		check(OutHeader.Globals[Patch.HeaderGlobalIndex].TypeIndex == UINT16_MAX);
		OutHeader.Globals[Patch.HeaderGlobalIndex].TypeIndex = OutHeader.Globals[Patch.CombinedAliasIndex].TypeIndex;
	}

	// Add the packed global UBs
	const FString UBOGlobalsNameSpv = ANSI_TO_TCHAR(CrossCompiler::FShaderConductorContext::GetIdentifierTable().GlobalsUniformBuffer);

	for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
	{
		const FOLDVulkanCodeHeader::FPackedUBToVulkanBindingIndex& PackedArrayInfo = OLDHeader.NEWPackedUBToVulkanBindingIndices[Index];
		FVulkanShaderHeader::FPackedUBInfo& PackedUB = OutHeader.PackedUBs[OutHeader.PackedUBs.AddZeroed()];
		PackedUB.OriginalBindingIndex = PackedArrayInfo.VulkanBindingIndex;
		PackedUB.PackedTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(PackedArrayInfo.TypeName);
		PackedUB.SizeInBytes = OLDHeader.NEWPackedGlobalUBSizes[Index];

		const FVulkanSpirv::FEntry* Entry = Spirv.GetEntryByBindingIndex(PackedArrayInfo.VulkanBindingIndex);
		check(Entry);

		// We are dealing with "HLSLCC_CB" for HLSLcc, and "$Globals" for DXC
		check(Entry->Name.StartsWith(TEXT("HLSLCC_CB")) || Entry->Name.StartsWith(UBOGlobalsNameSpv));

		PackedUB.SPIRVDescriptorSetOffset = Entry->WordDescriptorSetIndex;
		PackedUB.SPIRVBindingIndexOffset = Entry->WordBindingIndex;
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

			const FString& AttachmentName = VULKAN_SUBPASS_FETCH_VAR_W[Index];
			const FVulkanBindingTable::FBinding* Found = BindingTable.GetBindings().FindByPredicate([&AttachmentName](const FVulkanBindingTable::FBinding& Entry)
				{
					return Entry.Name == AttachmentName;
				});
			check(Found);
			int32 BindingIndex = (int32)(Found - BindingTable.GetBindings().GetData());
			check(BindingIndex >= 0 && BindingIndex <= BindingTable.GetBindings().Num());
			FVulkanShaderHeader::EAttachmentType AttachmentType = AttachmentTypes[Index];
			{
				int32 HeaderGlobalIndex = GlobalNames.Find(AttachmentName);
				check(HeaderGlobalIndex != INDEX_NONE);
				check(GlobalNames[HeaderGlobalIndex] == AttachmentName);
				FVulkanShaderHeader::FGlobalInfo& GlobalInfo = OutHeader.Globals[HeaderGlobalIndex];
				const FVulkanSpirv::FEntry* Entry = Spirv.GetEntry(AttachmentName);
				check(Entry);
				check(Entry->Binding != -1);

				VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				GlobalInfo.OriginalBindingIndex = Entry->Binding;
				OutHeader.GlobalSpirvInfos[HeaderGlobalIndex] = FVulkanShaderHeader::FSpirvInfo(Entry->WordDescriptorSetIndex, Entry->WordBindingIndex);
				int32 GlobalDescriptorTypeIndex = OutHeader.GlobalDescriptorTypes.Add(DescriptorTypeToBinding(DescriptorType));
				GlobalInfo.TypeIndex = GlobalDescriptorTypeIndex;
				GlobalInfo.CombinedSamplerStateAliasIndex = UINT16_MAX;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
				GlobalInfo.DebugName = AttachmentName;
#endif
				int32 HeaderAttachmentIndex = OutHeader.InputAttachments.AddZeroed();
				FVulkanShaderHeader::FInputAttachment& AttachmentInfo = OutHeader.InputAttachments[HeaderAttachmentIndex];
				AttachmentInfo.GlobalIndex = HeaderGlobalIndex;
				AttachmentInfo.Type = AttachmentType;
			}
		}
	}

	check(OLDHeader.UniformBuffersCopyInfo.Num() == 0);
	OutHeader.EmulatedUBsCopyInfo = OLDHeader.UniformBuffersCopyInfo;
	OutHeader.EmulatedUBCopyRanges = OLDHeader.NEWEmulatedUBCopyRanges;
	OutHeader.SourceHash = OLDHeader.SourceHash;
	OutHeader.SpirvCRC = Spirv.CRC;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	OutHeader.DebugName = OLDHeader.ShaderName;
#endif
	OutHeader.InOutMask = OLDHeader.SerializedBindings.InOutMask.Bitmask;
}


static void BuildShaderOutput(
	FShaderCompilerOutput&		ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const ANSICHAR*				InShaderSource,
	int32						SourceLen,
	const FVulkanBindingTable&	BindingTable,
	uint32						NumLines,
	FVulkanSpirv&				Spirv,
	const FString&				DebugName,
	bool						bSourceContainsMetaDataOnly)
{
	const ANSICHAR* USFSource = InShaderSource;
	FVulkanHlslccHeader CCHeader;
	if (!CCHeader.Read(USFSource, SourceLen))
	{
		UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Bad hlslcc header found: %s"), *ShaderInput.GenerateShaderName());
		return;
	}

	if (!bSourceContainsMetaDataOnly && *USFSource != '#')
	{
		UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Bad hlslcc header found with missing '#' character: %s"), *ShaderInput.GenerateShaderName());
		return;
	}

	FOLDVulkanCodeHeader OLDHeader;

	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;
	EShaderFrequency Frequency = (EShaderFrequency)ShaderOutput.Target.Frequency;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false, 32);


	static const FString AttributePrefix = TEXT("in_ATTRIBUTE");
	static const FString GL_Prefix = TEXT("gl_");
	for (auto& Input : CCHeader.Inputs)
	{
		// Only process attributes for vertex shaders.
		if (Frequency == SF_Vertex && Input.Name.StartsWith(AttributePrefix))
		{
			int32 AttributeIndex = ParseNumber(*Input.Name + AttributePrefix.Len(), /*bEmptyIsZero:*/ true);
			int32 Count = FMath::Max(1, Input.ArrayCount);
			for(int32 Index = 0; Index < Count; ++Index)
			{
				OLDHeader.SerializedBindings.InOutMask.EnableField(Index + AttributeIndex);
			}
		}
	}

	static const FString TargetPrefix = "out_Target";
	static const FString GL_FragDepth = "gl_FragDepth";
	for (auto& Output : CCHeader.Outputs)
	{
		// Only targets for pixel shaders must be tracked.
		if (Frequency == SF_Pixel && Output.Name.StartsWith(TargetPrefix))
		{
			uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix.Len(), /*bEmptyIsZero:*/ true);
			OLDHeader.SerializedBindings.InOutMask.EnableField(TargetIndex);
		}
		// Only depth writes for pixel shaders must be tracked.
		else if (Frequency == SF_Pixel && Output.Name.Equals(GL_FragDepth))
		{
			OLDHeader.SerializedBindings.InOutMask.EnableField(CrossCompiler::FShaderBindingInOutMask::DepthStencilMaskIndex);
		}
	}


	TMap<FString, FVulkanShaderHeader::EType> NEWEntryTypes;

	// Then 'normal' uniform buffers.
	for (auto& UniformBlock : CCHeader.UniformBlocks)
	{
		// DXC's generated "$Globals" has been converted to "_Globals" at this point
		uint16 UBIndex = UniformBlock.Index;
		if (UniformBlock.Name.StartsWith(TEXT("HLSLCC_CB")) || UniformBlock.Name.StartsWith(TEXT("_Globals")))
		{
			// Skip...
		}
		else
		{
			// Regular UB
			int32 VulkanBindingIndex = Spirv.FindBinding(UniformBlock.Name, true);
			check(VulkanBindingIndex != -1);
			check(!UsedUniformBufferSlots[VulkanBindingIndex]);
			UsedUniformBufferSlots[VulkanBindingIndex] = true;

			HandleReflectedUniformBuffer(UniformBlock.Name, VulkanBindingIndex, ShaderOutput);

			++OLDHeader.SerializedBindings.NumUniformBuffers;
			NEWEntryTypes.Add(*UniformBlock.Name, FVulkanShaderHeader::UniformBuffer);
		}
	}

	const TArray<FVulkanBindingTable::FBinding>& HlslccBindings = BindingTable.GetBindings();
	OLDHeader.NEWDescriptorInfo.NumBufferInfos = 0;
	OLDHeader.NEWDescriptorInfo.NumImageInfos = 0;
	for (int32 Index = 0; Index < HlslccBindings.Num(); ++Index)
	{
		const FVulkanBindingTable::FBinding& Binding = HlslccBindings[Index];

		OLDHeader.NEWDescriptorInfo.DescriptorTypes.Add(BindingToDescriptorType(Binding.Type));

		switch (Binding.Type)
		{
		case EVulkanBindingType::Sampler:
		case EVulkanBindingType::CombinedImageSampler:
		case EVulkanBindingType::Image:
		case EVulkanBindingType::StorageImage:
		case EVulkanBindingType::InputAttachment:
			++OLDHeader.NEWDescriptorInfo.NumImageInfos;
			break;
		case EVulkanBindingType::UniformBuffer:
		case EVulkanBindingType::StorageBuffer:
			++OLDHeader.NEWDescriptorInfo.NumBufferInfos;
			break;
		case EVulkanBindingType::PackedUniformBuffer:
			{
				FOLDVulkanCodeHeader::FPackedUBToVulkanBindingIndex* New = new(OLDHeader.NEWPackedUBToVulkanBindingIndices) FOLDVulkanCodeHeader::FPackedUBToVulkanBindingIndex;
				New->TypeName = (CrossCompiler::EPackedTypeName)Binding.SubType;
				New->VulkanBindingIndex = Index;
				++OLDHeader.NEWDescriptorInfo.NumBufferInfos;
			}
			break;
		case EVulkanBindingType::UniformTexelBuffer:
		case EVulkanBindingType::StorageTexelBuffer:
		case EVulkanBindingType::AccelerationStructure:
			break;
		default:
			checkf(0, TEXT("Binding Type %d not found"), (int32)Binding.Type);
			break;
		}
	}

	const uint16 BytesPerComponent = 4;

	// Packed global uniforms
	TMap<CrossCompiler::EPackedTypeName, uint32> PackedGlobalArraySize;
	for (auto& PackedGlobal : CCHeader.PackedGlobals)
	{
		int32 Found = -1;
		for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
		{
			if (OLDHeader.NEWPackedUBToVulkanBindingIndices[Index].TypeName == (CrossCompiler::EPackedTypeName)PackedGlobal.PackedType)
			{
				Found = Index;
				break;
			}
		}
		check(Found != -1);

		HandleReflectedGlobalConstantBufferMember(
			PackedGlobal.Name,
			Found,
			PackedGlobal.Offset * BytesPerComponent,
			PackedGlobal.Count * BytesPerComponent,
			ShaderOutput
		);

		NEWEntryTypes.Add(PackedGlobal.Name, FVulkanShaderHeader::PackedGlobal);

		uint32& Size = PackedGlobalArraySize.FindOrAdd((CrossCompiler::EPackedTypeName)PackedGlobal.PackedType);
		Size = FMath::Max<uint32>(BytesPerComponent * (PackedGlobal.Offset + PackedGlobal.Count), Size);
	}

	// Packed Uniform Buffers
	TMap<int, TMap<CrossCompiler::EPackedTypeName, uint16> > PackedUniformBuffersSize;
	OLDHeader.UNUSED_NumNonGlobalUBs = 0;
	for (auto& PackedUB : CCHeader.PackedUBs)
	{
		//check(PackedUB.Attribute.Index == Header.SerializedBindings.NumUniformBuffers);
		check(!UsedUniformBufferSlots[OLDHeader.UNUSED_NumNonGlobalUBs]);
		UsedUniformBufferSlots[OLDHeader.UNUSED_NumNonGlobalUBs] = true;

		HandleReflectedUniformBuffer(PackedUB.Attribute.Name, OLDHeader.UNUSED_NumNonGlobalUBs++, PackedUB.Attribute.Index, 0, ShaderOutput);

		NEWEntryTypes.Add(PackedUB.Attribute.Name, FVulkanShaderHeader::PackedGlobal);
	}

	//#todo-rco: When using regular UBs, also set UsedUniformBufferSlots[] = 1

	// Remap the destination UB index into the packed global array index
	auto RemapDestIndexIntoPackedUB = [&OLDHeader](int8 DestUBTypeName)
	{
		for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
		{
			if (OLDHeader.NEWPackedUBToVulkanBindingIndices[Index].TypeName == (CrossCompiler::EPackedTypeName)DestUBTypeName)
			{
				return Index;
			}
		}
		check(0);
		return -1;
	};

	for (auto& PackedUBCopy : CCHeader.PackedUBCopies)
	{
		// Not used: For flattening each UB into its own packed array (not a global one)
		ensure(0);
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBIndex = RemapDestIndexIntoPackedUB(CopyInfo.DestUBTypeName);
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		OLDHeader.UniformBuffersCopyInfo.Add(CopyInfo);

		auto& UniformBufferSize = PackedUniformBuffersSize.FindOrAdd(CopyInfo.DestUBIndex);
		uint16& Size = UniformBufferSize.FindOrAdd((CrossCompiler::EPackedTypeName)CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}

	for (auto& PackedUBCopy : CCHeader.PackedUBGlobalCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBIndex = RemapDestIndexIntoPackedUB(CopyInfo.DestUBTypeName);
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		OLDHeader.UniformBuffersCopyInfo.Add(CopyInfo);

		uint32& Size = PackedGlobalArraySize.FindOrAdd((CrossCompiler::EPackedTypeName)CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint32>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}

	// Generate a shortcut table for the PackedUBGlobalCopies
	TMap<uint32, uint32> PackedUBGlobalCopiesRanges;
	{
		int32 MaxDestUBIndex = -1;
		{
			// Verify table is sorted
			int32 PrevSourceUB = -1;
			int32 Index = 0;
			for (auto& Copy : OLDHeader.UniformBuffersCopyInfo)
			{
				if (PrevSourceUB < Copy.SourceUBIndex)
				{
					PrevSourceUB = Copy.SourceUBIndex;
					MaxDestUBIndex = FMath::Max(MaxDestUBIndex, (int32)Copy.SourceUBIndex);
					PackedUBGlobalCopiesRanges.Add(Copy.SourceUBIndex) = (Index << 16) | 1;
				}
				else if (PrevSourceUB == Copy.SourceUBIndex)
				{
					++PackedUBGlobalCopiesRanges.FindChecked(Copy.SourceUBIndex);
				}
				else
				{
					// Internal error
					check(0);
				}
				++Index;
			}
		}

		OLDHeader.NEWEmulatedUBCopyRanges.AddZeroed(MaxDestUBIndex + 1);
		for (int32 Index = 0; Index <= MaxDestUBIndex; ++Index)
		{
			uint32* Found = PackedUBGlobalCopiesRanges.Find(Index);
			if (Found)
			{
				OLDHeader.NEWEmulatedUBCopyRanges[Index] = *Found;
			}
		}
	}

	// Update Packed global array sizes
	OLDHeader.NEWPackedGlobalUBSizes.AddZeroed(OLDHeader.NEWPackedUBToVulkanBindingIndices.Num());
	for (auto& Pair : PackedGlobalArraySize)
	{
		CrossCompiler::EPackedTypeName TypeName = Pair.Key;
		int32 PackedArrayIndex = -1;
		for (int32 Index = 0; Index < OLDHeader.NEWPackedUBToVulkanBindingIndices.Num(); ++Index)
		{
			if (OLDHeader.NEWPackedUBToVulkanBindingIndices[Index].TypeName == TypeName)
			{
				PackedArrayIndex = Index;
				break;
			}
		}
		check(PackedArrayIndex != -1);
		// In bytes
		OLDHeader.NEWPackedGlobalUBSizes[PackedArrayIndex] = Align((uint32)Pair.Value, (uint32)16);
	}

	TSet<FString> SharedSamplerStates;
	for (int32 i = 0; i < CCHeader.SamplerStates.Num(); i++)
	{
		const FString& Name = CCHeader.SamplerStates[i].Name;
		int32 HlslccBindingIndex = Spirv.FindBinding(Name);
		check(HlslccBindingIndex != -1);

		SharedSamplerStates.Add(Name);
		auto& Binding = HlslccBindings[HlslccBindingIndex];
		int32 BindingIndex = Spirv.FindBinding(Binding.Name, true);
		check(BindingIndex != -1);

		HandleReflectedShaderSampler(Name, BindingIndex, ShaderOutput);

		NEWEntryTypes.Add(Name, FVulkanShaderHeader::Global);

		// Count only samplers states, not textures
		OLDHeader.SerializedBindings.NumSamplers++;
	}

	for (auto& Sampler : CCHeader.Samplers)
	{
		int32 VulkanBindingIndex = Spirv.FindBinding(Sampler.Name, true);
		check(VulkanBindingIndex != -1);

		HandleReflectedShaderResource(Sampler.Name, Sampler.Offset, VulkanBindingIndex, Sampler.Count, ShaderOutput);

		NEWEntryTypes.Add(Sampler.Name, FVulkanShaderHeader::Global);

		for (auto& SamplerState : Sampler.SamplerStates)
		{
			if (!SharedSamplerStates.Contains(SamplerState))
			{
				// ParameterMap does not use a TMultiMap, so we cannot push the same entry to it more than once!  if we try to, we've done something wrong...
				check(!ParameterMap.ContainsParameterAllocation(*SamplerState));

				HandleReflectedShaderSampler(SamplerState, Sampler.Offset, VulkanBindingIndex, Sampler.Count, ShaderOutput);

				NEWEntryTypes.Add(SamplerState, FVulkanShaderHeader::Global);

				// Count compiled texture-samplers as output samplers
				OLDHeader.SerializedBindings.NumSamplers += Sampler.Count;
			}
		}
	}

	for (auto& UAV : CCHeader.UAVs)
	{
		int32 VulkanBindingIndex = Spirv.FindBinding(UAV.Name);
		check(VulkanBindingIndex != -1);

		HandleReflectedShaderUAV(UAV.Name, UAV.Offset, VulkanBindingIndex, UAV.Count, ShaderOutput);

		NEWEntryTypes.Add(UAV.Name, FVulkanShaderHeader::Global);

		OLDHeader.SerializedBindings.NumUAVs = FMath::Max<uint8>(
			OLDHeader.SerializedBindings.NumUAVs,
			UAV.Offset + UAV.Count
			);
	}

	for (auto& AccelerationStructure : CCHeader.AccelerationStructures)
	{
		int32 VulkanBindingIndex = Spirv.FindBinding(AccelerationStructure.Name);
		check(VulkanBindingIndex != -1);

		HandleReflectedShaderResource(AccelerationStructure.Name, AccelerationStructure.Offset, VulkanBindingIndex, 1, ShaderOutput);

		NEWEntryTypes.Add(AccelerationStructure.Name, FVulkanShaderHeader::Global);

		OLDHeader.SerializedBindings.NumAccelerationStructures = FMath::Max<uint8>(
			OLDHeader.SerializedBindings.NumAccelerationStructures,
			AccelerationStructure.Offset + 1
			);
	}

	// Lats make sure that there is some type of name visible
	OLDHeader.ShaderName = CCHeader.Name.Len() > 0 ? CCHeader.Name : DebugName;

	FSHA1::HashBuffer(USFSource, FCStringAnsi::Strlen(USFSource), (uint8*)&OLDHeader.SourceHash);

	TArray<FString> OriginalParameters;
	ShaderOutput.ParameterMap.GetAllParameterNames(OriginalParameters);

	// Build the SRT for this shader.
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		if (!BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.UniformBufferMap, UsedUniformBufferSlots, ShaderOutput.ParameterMap, /*MaxBoundResourceTable, */GenericSRT))
		{
			ShaderOutput.Errors.Add(TEXT("Internal error on BuildResourceTableMapping."));
			return;
		}

		// Copy over the bits indicating which resource tables are active.
		OLDHeader.SerializedBindings.ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;

		OLDHeader.SerializedBindings.ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.TextureMap, true);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.ShaderResourceViewMap, true);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.SamplerMap, true);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, OLDHeader.SerializedBindings.ShaderResourceTable.UnorderedAccessViewMap, true);
	}

	TArray<FString> NewParameters;
	ShaderOutput.ParameterMap.GetAllParameterNames(NewParameters);

	// Mark all used uniform buffer indices; however some are empty (eg GBuffers) so gather those as NewParameters
	OLDHeader.UniformBuffersWithDescriptorMask = *UsedUniformBufferSlots.GetData();
	uint16 NumParams = 0;
	for (int32 Index = NewParameters.Num() - 1; Index >= 0; --Index)
	{
		uint16 OutIndex, OutBase, OutSize;
		bool bFound = ShaderOutput.ParameterMap.FindParameterAllocation(*NewParameters[Index], OutIndex, OutBase, OutSize);
		ensure(bFound);
		NumParams = FMath::Max((uint16)(OutIndex + 1), NumParams);
		if (OriginalParameters.Contains(NewParameters[Index]))
		{
			NewParameters.RemoveAtSwap(Index, 1, false);
		}
	}

	// All newly added parameters are empty uniform buffers (with no constant data used), so no Vulkan Binding is required: remove from the mask
	for (int32 Index = 0; Index < NewParameters.Num(); ++Index)
	{
		uint16 OutIndex, OutBase, OutSize;
		ShaderOutput.ParameterMap.FindParameterAllocation(*NewParameters[Index], OutIndex, OutBase, OutSize);
		OLDHeader.UniformBuffersWithDescriptorMask = OLDHeader.UniformBuffersWithDescriptorMask & ~((uint64)1 << (uint64)OutIndex);
	}

	FVulkanShaderHeader NEWHeader(FVulkanShaderHeader::EZero);
	ConvertToNEWHeader(OLDHeader, BindingTable, Spirv, NEWEntryTypes, ShaderInput, CCHeader, ShaderOutput.ParameterMap, NEWHeader);

	if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
	{
		NEWHeader.DebugName = ShaderInput.GenerateShaderName();
	}

	// Write out the header and shader source code.
	FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
	Ar << NEWHeader;

	check(Spirv.Data.Num() != 0);
	uint32 SpirvCodeSizeBytes = Spirv.Data.Num() * Spirv.Data.GetTypeSize();
	Ar << SpirvCodeSizeBytes;
	Ar.Serialize((uint8*)Spirv.Data.GetData(), SpirvCodeSizeBytes);

	// Something to compare.
	ShaderOutput.NumInstructions = NumLines;
	ShaderOutput.NumTextureSamplers = OLDHeader.SerializedBindings.NumSamplers;
	ShaderOutput.bSucceeded = true;

	if (ShaderInput.ExtraSettings.bExtractShaderSource)
	{
		TArray<ANSICHAR> AssemblyText;
		if (CrossCompiler::FShaderConductorContext::Disassemble(CrossCompiler::EShaderConductorIR::Spirv, Spirv.GetByteData(), Spirv.GetByteSize(), AssemblyText))
		{
			ShaderOutput.OptionalFinalShaderSource = FString(AssemblyText.GetData());
		}
	}
	if (ShaderInput.ExtraSettings.OfflineCompilerPath.Len() > 0)
	{
		if (SupportsOfflineCompiler(ShaderInput.ShaderFormat))
		{
			CompileOfflineMali(ShaderInput, ShaderOutput, (const ANSICHAR*)Spirv.GetByteData(), Spirv.GetByteSize(), true, Spirv.EntryPointName);
		}
	}

	CullGlobalUniformBuffers(ShaderInput.Environment.UniformBufferMap, ShaderOutput.ParameterMap);
}

FCompilerInfo::FCompilerInfo(const FShaderCompilerInput& InInput, const FString& InWorkingDirectory, EHlslShaderFrequency InFrequency) :
	Input(InInput),
	WorkingDirectory(InWorkingDirectory),
	CCFlags(0),
	Frequency(InFrequency),
	bDebugDump(false)
{
	bDebugDump = Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath);
	BaseSourceFilename = Input.GetSourceFilename();
}

#if PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX

static void GatherSpirvReflectionBindings(
	spv_reflect::ShaderModule&	Reflection,
	FSpirvReflectBindings&		OutBindings,
	const EShaderFrequency		ShaderFrequency)
{
	SpvReflectResult SpvResult = SPV_REFLECT_RESULT_NOT_READY;

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

	// Change descriptor set numbers
	TArray<SpvReflectDescriptorSet*> DescriptorSets;
	uint32 NumDescriptorSets = 0;

	SpvResult = Reflection.EnumerateDescriptorSets(&NumDescriptorSets, nullptr);
	check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);
	if (NumDescriptorSets > 0)
	{
		DescriptorSets.SetNum(NumDescriptorSets);
		SpvResult = Reflection.EnumerateDescriptorSets(&NumDescriptorSets, DescriptorSets.GetData());
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		for (const SpvReflectDescriptorSet* DescSet : DescriptorSets)
		{
			const uint32 DescSetNo = ShaderStage::GetStageForFrequency(ShaderFrequency);
			Reflection.ChangeDescriptorSetNumber(DescSet, DescSetNo);
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
	FVulkanSpirv&							Spirv,
	const FShaderCompilerInput&				Input,
	FShaderCompilerOutput&					Output,
	FVulkanBindingTable&					BindingTable,
	const FString&							EntryPointName,
	bool									bStripReflect,
	bool									bIsRayTracingShader,
	bool									bDebugDump)
{
	FShaderParameterMap& ParameterMap = Output.ParameterMap;

	SpvReflectResult SpvResult = SPV_REFLECT_RESULT_SUCCESS;

	uint8 UAVIndices = 0xff;
	uint64 TextureIndices = 0xffffffffffffffff;
	uint16 SamplerIndices = 0xffff;
	const uint32 GlobalSetId = 32;

	TMap<const SpvReflectDescriptorBinding*, uint32> SamplerStatesUseCount;

	// Reflect SPIR-V module with SPIRV-Reflect library
	const size_t SpirvDataSize = Spirv.GetByteSize();
	spv_reflect::ShaderModule Reflection(SpirvDataSize, Spirv.GetByteData(), SPV_REFLECT_RETURN_FLAG_SAMPLER_IMAGE_USAGE);
	check(Reflection.GetResult() == SPV_REFLECT_RESULT_SUCCESS);

	// Ray tracing shaders are not being rewritten to remove unreferenced entry points due to a bug in dxc.
	// Until it's fixed and integrated, we need to pull out the requested entry point manually.
	int32 EntryPointIndex = (!bIsRayTracingShader) ? 0 : -1;
	if (bIsRayTracingShader)
	{
		// For now only use the primary entry point for hit groups until we decide how to best support hit group library shaders.
		FString OutMain, OutAnyHit, OutIntersection;
		UE::ShaderCompilerCommon::ParseRayTracingEntryPoint(EntryPointName, OutMain, OutAnyHit, OutIntersection);

		for (uint32 i = 0; i < Reflection.GetEntryPointCount(); ++i)
		{
			if (OutMain.Equals(Reflection.GetEntryPointName(i)))
			{
				EntryPointIndex = static_cast<int32>(i);
				break;
			}
		}
		checkf(EntryPointIndex >= 0, TEXT("Failed to find entry point %s in SPIRV-V module."), *EntryPointName);
	}

	// Change final entry point name in SPIR-V module
	{
		checkf(bIsRayTracingShader || Reflection.GetEntryPointCount() == 1, TEXT("Too many entry points in SPIR-V module: Expected 1, but got %d"), Reflection.GetEntryPointCount());
		SpvReflectResult Result = Reflection.ChangeEntryPointName(EntryPointIndex, "main_00000000_00000000");
		check(Result == SPV_REFLECT_RESULT_SUCCESS);
	}

	FSpirvReflectBindings Bindings;
	GatherSpirvReflectionBindings(Reflection, Bindings, static_cast<EShaderFrequency>(Input.Target.Frequency));

	// Register how often a sampler-state is used
	for (const SpvReflectDescriptorBinding* Binding : Bindings.TextureSRVs)
	{
		if (Binding->usage_binding_count > 0)
		{
			for (uint32 UsageIndex = 0; UsageIndex < Binding->usage_binding_count; ++UsageIndex)
			{
				const SpvReflectDescriptorBinding* AssociatedResource = Binding->usage_bindings[UsageIndex];
				SamplerStatesUseCount.FindOrAdd(AssociatedResource)++;
			}
		}
	}

	// Build binding table
	TMap<const SpvReflectDescriptorBinding*, int32> BindingToIndexMap;

	/*for (const SpvReflectInterfaceVariable* Attribute : Bindings.InputAttributes)
	{
		check(Attribute->semantic != nullptr);
		BindingTable.RegisterBinding(Attribute->semantic, "a", EVulkanBindingType::InputAttachment);
	}*/

	const FString UBOGlobalsNameSpv = ANSI_TO_TCHAR(CrossCompiler::FShaderConductorContext::GetIdentifierTable().GlobalsUniformBuffer);

	for (const SpvReflectDescriptorBinding* Binding : Bindings.UniformBuffers)
	{
		FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		if (ResourceName == UBOGlobalsNameSpv)
		{
			int32 BindingIndex = BindingTable.RegisterBinding(TCHAR_TO_ANSI(*UBOGlobalsNameSpv), "h", EVulkanBindingType::PackedUniformBuffer);
			BindingToIndexMap.Add(Binding, BindingIndex);
			break;
		}
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.UniformBuffers)
	{
		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		if (ResourceName != UBOGlobalsNameSpv)
		{
			int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "u", EVulkanBindingType::UniformBuffer);
			BindingToIndexMap.Add(Binding, BindingIndex);
		}
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.InputAttachments)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "a", EVulkanBindingType::InputAttachment);
		BindingToIndexMap.Add(Binding, BindingIndex);
		BindingTable.InputAttachmentsMask |= (1u << Binding->input_attachment_index);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TBufferUAVs)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "u", EVulkanBindingType::StorageTexelBuffer);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.SBufferUAVs)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "u", EVulkanBindingType::StorageBuffer);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TextureUAVs)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "u", EVulkanBindingType::StorageImage);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TBufferSRVs)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "s", EVulkanBindingType::UniformTexelBuffer);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.SBufferSRVs)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "s", EVulkanBindingType::UniformTexelBuffer);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TextureSRVs)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "s", EVulkanBindingType::Image);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.Samplers)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "z", EVulkanBindingType::Sampler);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.AccelerationStructures)
	{
		int32 BindingIndex = BindingTable.RegisterBinding(Binding->name, "r", EVulkanBindingType::AccelerationStructure);
		BindingToIndexMap.Add(Binding, BindingIndex);
	}

	// Sort binding table
	BindingTable.SortBindings();

	CrossCompiler::FHlslccHeaderWriter CCHeaderWriter;

	// Iterate over all resource bindings grouped by resource type
	for (const SpvReflectInterfaceVariable* Attribute : Bindings.InputAttributes)
	{
		CCHeaderWriter.WriteInputAttribute(*Attribute);
	}

	for (const SpvReflectInterfaceVariable* Attribute : Bindings.OutputAttributes)
	{
		CCHeaderWriter.WriteOutputAttribute(*Attribute);
	}

	int32 UBOBindings = 0, UAVBindings = 0, SRVBindings = 0, SMPBindings = 0, GLOBindings = 0, ASBindings = 0;

	auto MapDescriptorBindingToIndex = [&BindingTable, &BindingToIndexMap](const SpvReflectDescriptorBinding* Binding)
	{
		return BindingTable.GetRealBindingIndex(BindingToIndexMap[Binding]);
	};

	for (const SpvReflectDescriptorBinding* Binding : Bindings.UniformBuffers)
	{
		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));

		if (ResourceName == UBOGlobalsNameSpv)
		{
			// Register binding for uniform buffer
			int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

			SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);// , GlobalSetId);
			check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

			Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(UBOGlobalsNameSpv, BindingIndex));

			CCHeaderWriter.WriteUniformBlock(TEXT("_Globals_h"), UBOBindings++);

			// Register all uniform buffer members as loose data
			FString MbrString;

			for (uint32 MemberIndex = 0; MemberIndex < Binding->block.member_count; ++MemberIndex)
			{
				const SpvReflectBlockVariable* Member = &(Binding->block.members[MemberIndex]);
				CCHeaderWriter.WritePackedGlobal(ANSI_TO_TCHAR(Member->name), CrossCompiler::EPackedTypeName::HighP, Member->absolute_offset, Member->size);
			}

			// Stop after we found $Globals uniform buffer
			break;
		}
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.UniformBuffers)
	{
		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));

		if (ResourceName != UBOGlobalsNameSpv)
		{
			// Register uniform buffer

			int32 BindingIndex = MapDescriptorBindingToIndex(Binding);
			SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
			check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

			Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));

			CCHeaderWriter.WriteUniformBlock(*ResourceName, UBOBindings++);
		}
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.InputAttachments)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TBufferUAVs)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		CCHeaderWriter.WriteUAV(*ResourceName, UAVBindings++);

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.SBufferUAVs)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		CCHeaderWriter.WriteUAV(*ResourceName, UAVBindings++);

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TextureUAVs)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		CCHeaderWriter.WriteUAV(*ResourceName, UAVBindings++);

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TBufferSRVs)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		CCHeaderWriter.WriteSRV(*ResourceName, UAVBindings++);

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.SBufferSRVs)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		CCHeaderWriter.WriteSRV(*ResourceName, SRVBindings++);

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.TextureSRVs)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		if (Binding->usage_binding_count > 0)
		{
			TArray<FString> AssociatedResourceNames;
			AssociatedResourceNames.SetNum(Binding->usage_binding_count);

			for (uint32 UsageIndex = 0; UsageIndex < Binding->usage_binding_count; ++UsageIndex)
			{
				const SpvReflectDescriptorBinding* AssociatedResource = Binding->usage_bindings[UsageIndex];
				AssociatedResourceNames[UsageIndex] = ANSI_TO_TCHAR(AssociatedResource->name);
			}

			CCHeaderWriter.WriteSRV(*ResourceName, SRVBindings++, /*Count:*/ 1, AssociatedResourceNames);
		}
		else
		{
			CCHeaderWriter.WriteSRV(*ResourceName, SRVBindings++);
		}

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.Samplers)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));

		// Only emit sampler state when its shared, i.e. used with at least 2 textures
//		if (const uint32* UseCount = SamplerStatesUseCount.Find(Binding))
		{
//			if (*UseCount >= 2)
			{
				CCHeaderWriter.WriteSamplerState(*ResourceName, SMPBindings++);
			}
		}
	}

	for (const SpvReflectDescriptorBinding* Binding : Bindings.AccelerationStructures)
	{
		int32 BindingIndex = MapDescriptorBindingToIndex(Binding);

		SpvResult = Reflection.ChangeDescriptorBindingNumbers(Binding, BindingIndex);//, GlobalSetId);
		check(SpvResult == SPV_REFLECT_RESULT_SUCCESS);

		const FString ResourceName(ANSI_TO_TCHAR(Binding->name));
		
		CCHeaderWriter.WriteAccelerationStructures(*ResourceName, ASBindings++);

		Spirv.ReflectionInfo.Add(FVulkanSpirv::FEntry(ResourceName, BindingIndex));
	}

	// Build final shader output with meta data
	FString DebugName = Input.DumpDebugInfoPath.Right(Input.DumpDebugInfoPath.Len() - Input.DumpDebugInfoRootPath.Len());

	CCHeaderWriter.WriteSourceInfo(*Input.GetSourceFilename(), *Input.EntryPointName, *Input.DebugGroupName);
	CCHeaderWriter.WriteCompilerInfo();

	const FString MetaData = CCHeaderWriter.ToString();

	Output.Target = Input.Target;

	// Overwrite updated SPIRV code
	Spirv.Data = TArray<uint32>(Reflection.GetCode(), Reflection.GetCodeSize() / 4);

	// We have to strip out most debug instructions (except OpName) for Vulkan mobile
	if (bStripReflect)
	{
		const char* OptArgs[] = { "--strip-reflect" };
		if (!CompilerContext.OptimizeSpirv(Spirv.Data, OptArgs, UE_ARRAY_COUNT(OptArgs)))
		{
			UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Failed to strip debug instructions from SPIR-V module"));
			return false;
		}
	}

	// For Android run an additional pass to patch spirv to be compatible across drivers
	if(IsAndroidShaderFormat(Input.ShaderFormat))
	{
		const char* OptArgs[] = { "--android-driver-patch" };
		if (!CompilerContext.OptimizeSpirv(Spirv.Data, OptArgs, UE_ARRAY_COUNT(OptArgs)))
		{
			UE_LOG(LogVulkanShaderCompiler, Error, TEXT("Failed to apply driver patches for Android"));
			return false;
		}
	}

	PatchSpirvReflectionEntries(Spirv);
	Spirv.EntryPointName = PatchSpirvEntryPointWithCRC(Spirv, Spirv.CRC);

	const uint32 ApproxInstructionCount = CalculateSpirvInstructionCount(Spirv);

	BuildShaderOutput(
		Output,
		Input,
		TCHAR_TO_ANSI(*MetaData),
		MetaData.Len(),
		BindingTable,
		ApproxInstructionCount,
		Spirv,
		DebugName,
		true // source contains meta data only
	);

	if (bDebugDump)
	{
		// Write meta data to debug output file and write SPIR-V dump in binary and text form
		DumpDebugShaderText(Input, MetaData, TEXT("meta.txt"));
		DumpDebugShaderBinary(Input, Spirv.GetByteData(), Spirv.GetByteSize(), TEXT("spv"));
		DumpDebugShaderDisassembledSpirv(Input, Spirv.GetByteData(), Spirv.GetByteSize(), TEXT("spvasm"));
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

static FString VulkanGetShaderProfileDXC(const FCompilerInfo& CompilerInfo, const CrossCompiler::FShaderConductorOptions Options)
{
	const TCHAR* ShaderProfile = TEXT("unknown");
	switch (CompilerInfo.Input.Target.Frequency)
	{
	case SF_Vertex:
		ShaderProfile = TEXT("vs");
		break;

	case SF_Pixel:
		ShaderProfile = TEXT("ps");
		break;

	case SF_Geometry:
		ShaderProfile = TEXT("gs");
		break;

	case SF_Compute:
		ShaderProfile = TEXT("cs");
		break;

	case SF_RayGen:
	case SF_RayMiss:
	case SF_RayHitGroup:
	case SF_RayCallable:
		return TEXT("lib_6_3");
	}

	return FString::Printf(TEXT("%s_%d_%d"), ShaderProfile, Options.ShaderModel.Major, Options.ShaderModel.Minor); 
}

static void VulkanCreateDXCCompileBatchFiles(
	const FString& EntryPointName,
	EShaderFrequency Frequency,
	const FCompilerInfo& CompilerInfo,
	const CrossCompiler::FShaderConductorOptions Options)
{

	FString USFFilename = CompilerInfo.Input.GetSourceFilename();
	FString SPVFilename = FPaths::GetBaseFilename(USFFilename) + TEXT(".DXC.spv");
	FString GLSLFilename = FPaths::GetBaseFilename(USFFilename) + TEXT(".SPV.glsl");

	FString DxcPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());

	DxcPath = FPaths::Combine(DxcPath, TEXT("Binaries/ThirdParty/ShaderConductor/Win64"));
	FPaths::MakePlatformFilename(DxcPath);

	FString DxcFilename = FPaths::Combine(DxcPath, TEXT("dxc.exe"));
	FPaths::MakePlatformFilename(DxcFilename);

	const TCHAR* VulkanVersion = TEXT("vulkanUNKNOWN");
	if (Options.TargetEnvironment == CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_0)
	{
		VulkanVersion = TEXT("vulkan1.0");
	}
	else if (Options.TargetEnvironment == CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_1)
	{
		VulkanVersion = TEXT("vulkan1.1");
	}
	else if (Options.TargetEnvironment == CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_2)
	{
		VulkanVersion = TEXT("vulkan1.2");
	}
	else
	{
		ensure(false);
	}

	FString ShaderProfile = VulkanGetShaderProfileDXC(CompilerInfo, Options);

	// CompileDXC.bat
	{
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
				"%%DXC%% -HV %d -T %s -E %s -spirv -fspv-target-env=%s -Fo %s %s\n"
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
			Options.HlslVersion,
			*ShaderProfile,
			*EntryPointName,
			VulkanVersion,
			*SPVFilename,
			*USFFilename,
			*GLSLFilename,
			*SPVFilename
		);

		FFileHelper::SaveStringToFile(BatchFileContents, *(CompilerInfo.Input.DumpDebugInfoPath / TEXT("CompileDXC.bat")));
	}
}

static bool CompileWithShaderConductor(
	const FString&			PreprocessedShader,
	const FString&			EntryPointName,
	EShaderFrequency		Frequency,
	const FCompilerInfo&	CompilerInfo,
	FShaderCompilerOutput&	Output,
	FVulkanBindingTable&	BindingTable,
	bool					bStripReflect)
{
	const FShaderCompilerInput& Input = CompilerInfo.Input;

	const bool bIsRayTracingShader = Input.IsRayTracingShader();
	const bool bRewriteHlslSource = !bIsRayTracingShader;
	const bool bDebugDump = CompilerInfo.bDebugDump;

	CrossCompiler::FShaderConductorContext CompilerContext;

	// Inject additional macro definitions to circumvent missing features: external textures
	FShaderCompilerDefinitions AdditionalDefines;

	// Load shader source into compiler context
	CompilerContext.LoadSource(PreprocessedShader, Input.VirtualSourceFilePath, EntryPointName, Frequency, &AdditionalDefines);

	// Initialize compilation options for ShaderConductor
	CrossCompiler::FShaderConductorOptions Options;
	Options.bDisableScalarBlockLayout = !bIsRayTracingShader;

	// Enable HLSL 2021 if specified
	if (Input.Environment.CompilerFlags.Contains(CFLAG_HLSL2021))
	{
		Options.HlslVersion = 2021;
	}

	// Ray tracing features require Vulkan 1.2 environment.
	if (bIsRayTracingShader || Input.Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing))
	{
		Options.TargetEnvironment = CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_2;
	}
	else
	{
		Options.TargetEnvironment = GetMinimumTargetEnvironment(Input.Target.GetPlatform());
	}

	if (bDebugDump)
	{
		VulkanCreateDXCCompileBatchFiles(
			EntryPointName,
			Frequency,
			CompilerInfo,
			Options);

		DumpDebugUSF(Input, PreprocessedShader, CompilerInfo.CCFlags);
	}

	if (bRewriteHlslSource)
	{
		// Rewrite HLSL source code to remove unused global resources and variables
		FString RewrittenHlslSource;

		Options.bRemoveUnusedGlobals = true;
		if (!CompilerContext.RewriteHlsl(Options, (bDebugDump ? &RewrittenHlslSource : nullptr)))
		{
			CompilerContext.FlushErrors(Output.Errors);
			return false;
		}
		Options.bRemoveUnusedGlobals = false;

		if (bDebugDump)
		{
			DumpDebugShaderText(Input, RewrittenHlslSource, TEXT("rewritten.hlsl"));
		}
	}

	// Compile HLSL source to SPIR-V binary
	FVulkanSpirv Spirv;
	if (!CompilerContext.CompileHlslToSpirv(Options, Spirv.Data))
	{
		CompilerContext.FlushErrors(Output.Errors);
		return false;
	}

	// If this shader samples R64 image formats, they need to get converted to STORAGE_IMAGE
	// todo-jnmo: Scope this with a CFLAG if it affects compilation times 
	Patch64bitSamplers(Spirv);

	// Build shader output and binding table
	Output.bSucceeded = BuildShaderOutputFromSpirv(CompilerContext, Spirv, Input, Output, BindingTable, EntryPointName, bStripReflect, bIsRayTracingShader, bDebugDump);

	if (Input.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
	{
		Output.ShaderCode.AddOptionalData(FShaderCodeName::Key, TCHAR_TO_UTF8(*Input.GenerateShaderName()));
	}

	if (bDebugDump)
	{
		DumpDebugShaderBinary(Input, Spirv.GetByteData(), Spirv.GetByteSize(), TEXT("spv"));
	}

	// Flush warnings
	CompilerContext.FlushErrors(Output.Errors);
	return true;
}

#endif // PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX


void DoCompileVulkanShader(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const class FString& WorkingDirectory, EVulkanShaderVersion Version)
{
	check(IsVulkanShaderFormat(Input.ShaderFormat));

	const bool bIsSM5 = (Version == EVulkanShaderVersion::SM5);
	const bool bIsMobile = (Version == EVulkanShaderVersion::ES3_1 || Version == EVulkanShaderVersion::ES3_1_ANDROID);
	bool bStripReflect = Input.IsRayTracingShader();
	// By default we strip reflecion information for Android platform to avoid issues with older drivers
	if (IsAndroidShaderFormat(Input.ShaderFormat))
	{
		const FString* StripReflect_Android = Input.Environment.GetDefinitions().Find(TEXT("STRIP_REFLECT_ANDROID"));
		bStripReflect = !(StripReflect_Android && *StripReflect_Android == TEXT("0"));
	}

	const CrossCompiler::FShaderConductorOptions::ETargetEnvironment TargetEnvironment = GetMinimumTargetEnvironment(Input.Target.GetPlatform());

	const EHlslShaderFrequency FrequencyTable[] =
	{
		HSF_VertexShader,
		HSF_InvalidFrequency,
		HSF_InvalidFrequency,
		HSF_PixelShader,
		bIsSM5 ? HSF_GeometryShader : HSF_InvalidFrequency,
		HSF_ComputeShader, 
		bIsSM5 ? HSF_RayGen : HSF_InvalidFrequency,
		bIsSM5 ? HSF_RayMiss : HSF_InvalidFrequency,
		bIsSM5 ? HSF_RayHitGroup : HSF_InvalidFrequency,
		bIsSM5 ? HSF_RayCallable : HSF_InvalidFrequency,
	};

	const EShaderFrequency Frequency = (EShaderFrequency)Input.Target.Frequency;

	const EHlslShaderFrequency HlslFrequency = FrequencyTable[Input.Target.Frequency];
	if (HlslFrequency == HSF_InvalidFrequency)
	{
		Output.bSucceeded = false;
		FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(
			TEXT("%s shaders not supported for use in Vulkan."),
			CrossCompiler::GetFrequencyName(Frequency));
		return;
	}

	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1);
	AdditionalDefines.SetDefine(TEXT("COMPILER_VULKAN"), 1);
	if (bIsMobile)
	{
		AdditionalDefines.SetDefine(TEXT("ES3_1_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE"), 1);
	}
	else if (bIsSM5)
	{
		AdditionalDefines.SetDefine(TEXT("VULKAN_PROFILE_SM5"), 1);
	}
	AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));

	AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
	AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_DUAL_SOURCE_BLENDING_SLOT_DECORATION"), (uint32)1);
	AdditionalDefines.SetDefine(TEXT("PLATFORM_SUPPORTS_ROV"), 0); // Disabled until DXC->SPRIV ROV support is implemented

	if (Input.Environment.FullPrecisionInPS)
	{
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing))
	{
		AdditionalDefines.SetDefine(TEXT("PLATFORM_SUPPORTS_INLINE_RAY_TRACING"), 1);
	}

	if (TargetEnvironment >= CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_1)
	{
		AdditionalDefines.SetDefine(TEXT("PLATFORM_SUPPORTS_SM6_0_WAVE_OPERATIONS"), 1);
	}

	const double StartPreprocessTime = FPlatformTime::Seconds();

	// Preprocess the shader.
	FString PreprocessedShaderSource;
	const bool bDirectCompile = FParse::Param(FCommandLine::Get(), TEXT("directcompile"));
	if (bDirectCompile)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShaderSource, *Input.VirtualSourceFilePath))
		{
			return;
		}

		// Remove const as we are on debug-only mode
		CrossCompiler::CreateEnvironmentFromResourceTable(PreprocessedShaderSource, (FShaderCompilerEnvironment&)Input.Environment);
	}
	else
	{
		if (!PreprocessShader(PreprocessedShaderSource, Output, Input, AdditionalDefines))
		{
			// The preprocessing stage will add any relevant errors.
			return;
		}
	}

	FShaderParameterParser ShaderParameterParser;
	if (!ShaderParameterParser.ParseAndModify(Input, Output, PreprocessedShaderSource, /* ConstantBufferType = */ nullptr))
	{
		// The FShaderParameterParser will add any relevant errors.
		return;
	}

	FString EntryPointName = Input.EntryPointName;

	RemoveUniformBuffersFromSource(Input.Environment, PreprocessedShaderSource);

	// Process TEXT macro.
	TransformStringIntoCharacterArray(PreprocessedShaderSource);

	Output.PreprocessTime = FPlatformTime::Seconds() - StartPreprocessTime;

	FCompilerInfo CompilerInfo(Input, WorkingDirectory, HlslFrequency);

	// Setup hlslcc flags. Needed here as it will be used when dumping debug info
	{
		CompilerInfo.CCFlags |= HLSLCC_PackUniforms;
		CompilerInfo.CCFlags |= HLSLCC_PackUniformsIntoUniformBuffers;

		// Only flatten structures inside UBs
		CompilerInfo.CCFlags |= HLSLCC_FlattenUniformBufferStructures;

		if (Input.Environment.FullPrecisionInPS)
		{
			CompilerInfo.CCFlags |= HLSLCC_UseFullPrecisionInPS;
		}

		CompilerInfo.CCFlags |= HLSLCC_SeparateShaderObjects;
		CompilerInfo.CCFlags |= HLSLCC_KeepSamplerAndImageNames;

		CompilerInfo.CCFlags |= HLSLCC_RetainSizes;

		// ES doesn't support origin layout
		CompilerInfo.CCFlags |= HLSLCC_DX11ClipSpace;

		// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
		CompilerInfo.CCFlags &= ~HLSLCC_NoPreprocess;

		if (!bDirectCompile || UE_BUILD_DEBUG)
		{
			// Validation is expensive - only do it when compiling directly for debugging
			CompilerInfo.CCFlags |= HLSLCC_NoValidation;
		}
	}

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (CompilerInfo.bDebugDump)
	{
		DumpDebugUSF(Input, PreprocessedShaderSource, CompilerInfo.CCFlags);
	}

	TArray<ANSICHAR> GeneratedGlslSource;
	FVulkanBindingTable BindingTable(CompilerInfo.Frequency);
	bool bSuccess = false;

#if PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX
	// Cross-compile shader via ShaderConductor (DXC, SPIRV-Tools, SPIRV-Cross)
	bSuccess = CompileWithShaderConductor(PreprocessedShaderSource, EntryPointName, Frequency, CompilerInfo, Output, BindingTable, bStripReflect);
#endif // PLATFORM_MAC || PLATFORM_WINDOWS || PLATFORM_LINUX
	
	ShaderParameterParser.ValidateShaderParameterTypes(Input, bIsMobile, Output);
	
	if (bDirectCompile)
	{
		for (const auto& Error : Output.Errors)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), *Error.GetErrorStringWithLineMarker());
		}
		ensure(bSuccess);
	}
}
