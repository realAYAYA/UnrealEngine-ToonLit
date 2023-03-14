// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderCompilerCommon.h"
#include "ShaderParameterParser.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "HlslccDefinitions.h"
#include "HAL/FileManager.h"
#include "String/RemoveFrom.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ShaderCompilerCommon);


int16 GetNumUniformBuffersUsed(const FShaderCompilerResourceTable& InSRT)
{
	auto CountLambda = [&](const TArray<uint32>& In)
					{
						int16 LastIndex = -1;
						for (int32 i = 0; i < In.Num(); ++i)
						{
							auto BufferIndex = FRHIResourceTableEntry::GetUniformBufferIndex(In[i]);
							if (BufferIndex != static_cast<uint16>(FRHIResourceTableEntry::GetEndOfStreamToken()) )
							{
								LastIndex = FMath::Max(LastIndex, (int16)BufferIndex);
							}
						}

						return LastIndex + 1;
					};
	int16 Num = CountLambda(InSRT.SamplerMap);
	Num = FMath::Max(Num, (int16)CountLambda(InSRT.ShaderResourceViewMap));
	Num = FMath::Max(Num, (int16)CountLambda(InSRT.TextureMap));
	Num = FMath::Max(Num, (int16)CountLambda(InSRT.UnorderedAccessViewMap));
	return Num;
}


void BuildResourceTableTokenStream(const TArray<uint32>& InResourceMap, int32 MaxBoundResourceTable, TArray<uint32>& OutTokenStream, bool bGenerateEmptyTokenStreamIfNoResources)
{
	if (bGenerateEmptyTokenStreamIfNoResources)
	{
		if (InResourceMap.Num() == 0)
		{
			return;
		}
	}

	// First we sort the resource map.
	TArray<uint32> SortedResourceMap = InResourceMap;
	SortedResourceMap.Sort();

	// The token stream begins with a table that contains offsets per bound uniform buffer.
	// This offset provides the start of the token stream.
	OutTokenStream.AddZeroed(MaxBoundResourceTable+1);
	auto LastBufferIndex = FRHIResourceTableEntry::GetEndOfStreamToken();
	for (int32 i = 0; i < SortedResourceMap.Num(); ++i)
	{
		auto BufferIndex = FRHIResourceTableEntry::GetUniformBufferIndex(SortedResourceMap[i]);
		if (BufferIndex != LastBufferIndex)
		{
			// Store the offset for resources from this buffer.
			OutTokenStream[BufferIndex] = OutTokenStream.Num();
			LastBufferIndex = BufferIndex;
		}
		OutTokenStream.Add(SortedResourceMap[i]);
	}

	// Add a token to mark the end of the stream. Not needed if there are no bound resources.
	if (OutTokenStream.Num())
	{
		OutTokenStream.Add(FRHIResourceTableEntry::GetEndOfStreamToken());
	}
}


bool BuildResourceTableMapping(
	const TMap<FString, FResourceTableEntry>& ResourceTableMap,
	const TMap<FString, FUniformBufferEntry>& UniformBufferMap,
	TBitArray<>& UsedUniformBufferSlots,
	FShaderParameterMap& ParameterMap,
	FShaderCompilerResourceTable& OutSRT)
{
	check(OutSRT.ResourceTableBits == 0);
	check(OutSRT.ResourceTableLayoutHashes.Num() == 0);

	// Build resource table mapping
	int32 MaxBoundResourceTable = -1;
	TArray<uint32> ResourceTableSRVs;
	TArray<uint32> ResourceTableSamplerStates;
	TArray<uint32> ResourceTableUAVs;

	// Go through ALL the members of ALL the UB resources
	for( auto MapIt = ResourceTableMap.CreateConstIterator(); MapIt; ++MapIt )
	{
		const FString& Name	= MapIt->Key;
		const FResourceTableEntry& Entry = MapIt->Value;

		uint16 BufferIndex, BaseIndex, Size;

		// If the shaders uses this member (eg View_PerlinNoise3DTexture)...
		if (ParameterMap.FindParameterAllocation( *Name, BufferIndex, BaseIndex, Size ) )
		{
			ParameterMap.RemoveParameterAllocation(*Name);

			uint16 UniformBufferIndex = INDEX_NONE;
			uint16 UBBaseIndex, UBSize;

			// Add the UB itself as a parameter if not there
			if (!ParameterMap.FindParameterAllocation(*Entry.UniformBufferName, UniformBufferIndex, UBBaseIndex, UBSize))
			{
				UniformBufferIndex = UsedUniformBufferSlots.FindAndSetFirstZeroBit();
				ParameterMap.AddParameterAllocation(*Entry.UniformBufferName,UniformBufferIndex,0,0,EShaderParameterType::UniformBuffer);
			}

			// Mark used UB index
			if (UniformBufferIndex >= sizeof(OutSRT.ResourceTableBits) * 8)
			{
				return false;
			}
			OutSRT.ResourceTableBits |= (1 << UniformBufferIndex);

			// How many resource tables max we'll use, and fill it with zeroes
			MaxBoundResourceTable = FMath::Max<int32>(MaxBoundResourceTable, (int32)UniformBufferIndex);

			auto ResourceMap = FRHIResourceTableEntry::Create(UniformBufferIndex, Entry.ResourceIndex, BaseIndex);
			switch( Entry.Type )
			{
			case UBMT_TEXTURE:
			case UBMT_RDG_TEXTURE:
				OutSRT.TextureMap.Add(ResourceMap);
				break;
			case UBMT_SAMPLER:
				OutSRT.SamplerMap.Add(ResourceMap);
				break;
			case UBMT_SRV:
			case UBMT_RDG_TEXTURE_SRV:
			case UBMT_RDG_BUFFER_SRV:
				OutSRT.ShaderResourceViewMap.Add(ResourceMap);
				break;
			case UBMT_UAV:
			case UBMT_RDG_TEXTURE_UAV:
			case UBMT_RDG_BUFFER_UAV:
				OutSRT.UnorderedAccessViewMap.Add(ResourceMap);
				break;
			default:
				return false;
			}
		}
	}

	// Emit hashes for all uniform buffers in the parameter map. We need to include the ones without resources as well
	// (i.e. just constants), since the global uniform buffer bindings rely on valid hashes.
	for (const auto& KeyValue : ParameterMap.GetParameterMap())
	{
		const FString& UniformBufferName = KeyValue.Key;
		const FParameterAllocation& UniformBufferParameter = KeyValue.Value;

		if (UniformBufferParameter.Type == EShaderParameterType::UniformBuffer)
		{
			if (OutSRT.ResourceTableLayoutHashes.Num() <= UniformBufferParameter.BufferIndex)
			{
				OutSRT.ResourceTableLayoutHashes.SetNumZeroed(UniformBufferParameter.BufferIndex + 1);
			}

			// Data-driven uniform buffers will not have registered this information.
			if (const FUniformBufferEntry* UniformBufferEntry = UniformBufferMap.Find(UniformBufferName))
			{
				OutSRT.ResourceTableLayoutHashes[UniformBufferParameter.BufferIndex] = UniformBufferEntry->LayoutHash;
			}
		}
	}

	OutSRT.MaxBoundResourceTable = MaxBoundResourceTable;
	return true;
}

void CullGlobalUniformBuffers(const TMap<FString, FUniformBufferEntry>& UniformBufferMap, FShaderParameterMap& ParameterMap)
{
	TArray<FString> ParameterNames;
	ParameterMap.GetAllParameterNames(ParameterNames);

	for (const FString& Name : ParameterNames)
	{
		if (const FUniformBufferEntry* UniformBufferEntry = UniformBufferMap.Find(*Name))
		{
			// A uniform buffer that is bound per-shader keeps its allocation in the map.
			if (EnumHasAnyFlags(UniformBufferEntry->BindingFlags, EUniformBufferBindingFlags::Shader))
			{
				continue;
			}

			ParameterMap.RemoveParameterAllocation(*Name);
		}
	}
}

const TCHAR* FindNextWhitespace(const TCHAR* StringPtr)
{
	while (*StringPtr && !FChar::IsWhitespace(*StringPtr))
	{
		StringPtr++;
	}

	if (*StringPtr && FChar::IsWhitespace(*StringPtr))
	{
		return StringPtr;
	}
	else
	{
		return nullptr;
	}
}

const TCHAR* FindNextNonWhitespace(const TCHAR* StringPtr)
{
	bool bFoundWhitespace = false;

	while (*StringPtr && (FChar::IsWhitespace(*StringPtr) || !bFoundWhitespace))
	{
		bFoundWhitespace = true;
		StringPtr++;
	}

	if (bFoundWhitespace && *StringPtr && !FChar::IsWhitespace(*StringPtr))
	{
		return StringPtr;
	}
	else
	{
		return nullptr;
	}
}

const TCHAR* FindMatchingBlock(const TCHAR* OpeningCharPtr, char OpenChar, char CloseChar)
{
	const TCHAR* SearchPtr = OpeningCharPtr;
	int32 Depth = 0;

	while (*SearchPtr)
	{
		if (*SearchPtr == OpenChar)
		{
			Depth++;
		}
		else if (*SearchPtr == CloseChar)
		{
			if (Depth == 0)
			{
				return SearchPtr;
			}

			Depth--;
		}
		SearchPtr++;
	}

	return nullptr;
}
const TCHAR* FindMatchingClosingBrace(const TCHAR* OpeningCharPtr)			{ return FindMatchingBlock(OpeningCharPtr, '{', '}'); };
const TCHAR* FindMatchingClosingParenthesis(const TCHAR* OpeningCharPtr)	{ return FindMatchingBlock(OpeningCharPtr, '(', ')'); };

// See MSDN HLSL 'Symbol Name Restrictions' doc
inline bool IsValidHLSLIdentifierCharacter(TCHAR Char)
{
	return (Char >= 'a' && Char <= 'z') ||
		(Char >= 'A' && Char <= 'Z') ||
		(Char >= '0' && Char <= '9') ||
		Char == '_';
}

void ParseHLSLTypeName(const TCHAR* SearchString, const TCHAR*& TypeNameStartPtr, const TCHAR*& TypeNameEndPtr)
{
	TypeNameStartPtr = FindNextNonWhitespace(SearchString);
	check(TypeNameStartPtr);

	TypeNameEndPtr = TypeNameStartPtr;
	int32 Depth = 0;

	const TCHAR* NextWhitespace = FindNextWhitespace(TypeNameStartPtr);
	const TCHAR* PotentialExtraTypeInfoPtr = NextWhitespace ? FindNextNonWhitespace(NextWhitespace) : nullptr;

	// Find terminating whitespace, but skip over trailing ' < float4 >'
	while (*TypeNameEndPtr)
	{
		if (*TypeNameEndPtr == '<')
		{
			Depth++;
		}
		else if (*TypeNameEndPtr == '>')
		{
			Depth--;
		}
		else if (Depth == 0 
			&& FChar::IsWhitespace(*TypeNameEndPtr)
			// If we found a '<', we must not accept any whitespace before it
			&& (!PotentialExtraTypeInfoPtr || *PotentialExtraTypeInfoPtr != '<' || TypeNameEndPtr > PotentialExtraTypeInfoPtr))
		{
			break;
		}

		TypeNameEndPtr++;
	}

	check(TypeNameEndPtr);
}

const TCHAR* ParseHLSLSymbolName(const TCHAR* SearchString, FString& SymboName)
{
	const TCHAR* SymbolNameStartPtr = FindNextNonWhitespace(SearchString);
	check(SymbolNameStartPtr);

	const TCHAR* SymbolNameEndPtr = SymbolNameStartPtr;
	while (*SymbolNameEndPtr && IsValidHLSLIdentifierCharacter(*SymbolNameEndPtr))
	{
		SymbolNameEndPtr++;
	}

	SymboName = FString(SymbolNameEndPtr - SymbolNameStartPtr, SymbolNameStartPtr);

	return SymbolNameEndPtr;
}

class FUniformBufferMemberInfo
{
public:
	// eg View.WorldToClip
	FString NameAsStructMember;
	// eg View_WorldToClip
	FString GlobalName;
};

const TCHAR* ParseStructRecursive(
	const TCHAR* StructStartPtr,
	FString& UniformBufferName,
	int32 StructDepth,
	const FString& StructNamePrefix, 
	const FString& GlobalNamePrefix, 
	TMap<FString, TArray<FUniformBufferMemberInfo>>& UniformBufferNameToMembers)
{
	const TCHAR* OpeningBracePtr = FCString::Strstr(StructStartPtr, TEXT("{"));
	check(OpeningBracePtr);

	const TCHAR* ClosingBracePtr = FindMatchingClosingBrace(OpeningBracePtr + 1);
	check(ClosingBracePtr);

	FString StructName;
	const TCHAR* StructNameEndPtr = ParseHLSLSymbolName(ClosingBracePtr + 1, StructName);
	check(StructName.Len() > 0);

	FString NestedStructNamePrefix = StructNamePrefix + StructName + TEXT(".");
	FString NestedGlobalNamePrefix = GlobalNamePrefix + StructName + TEXT("_");

	if (StructDepth == 0)
	{
		UniformBufferName = StructName;
	}

	const TCHAR* LastMemberSemicolon = ClosingBracePtr;

	// Search backward to find the last member semicolon so we know when to stop parsing members
	while (LastMemberSemicolon > OpeningBracePtr && *LastMemberSemicolon != ';')
	{
		LastMemberSemicolon--;
	}

	const TCHAR* MemberSearchPtr = OpeningBracePtr + 1;

	do
	{
		const TCHAR* MemberTypeStartPtr = nullptr;
		const TCHAR* MemberTypeEndPtr = nullptr;
		ParseHLSLTypeName(MemberSearchPtr, MemberTypeStartPtr, MemberTypeEndPtr);
		FString MemberTypeName(MemberTypeEndPtr - MemberTypeStartPtr, MemberTypeStartPtr);

		if (FCString::Strcmp(*MemberTypeName, TEXT("struct")) == 0)
		{
			MemberSearchPtr = ParseStructRecursive(MemberTypeStartPtr, UniformBufferName, StructDepth + 1, NestedStructNamePrefix, NestedGlobalNamePrefix, UniformBufferNameToMembers);
		}
		else
		{
			FString MemberName;
			const TCHAR* SymbolEndPtr = ParseHLSLSymbolName(MemberTypeEndPtr, MemberName);
			check(MemberName.Len() > 0);
			
			MemberSearchPtr = SymbolEndPtr;

			// Skip over trailing tokens '[1];'
			while (*MemberSearchPtr && *MemberSearchPtr != ';')
			{
				MemberSearchPtr++;
			}

			// Add this member to the map
			TArray<FUniformBufferMemberInfo>& UniformBufferMembers = UniformBufferNameToMembers.FindOrAdd(UniformBufferName);

			FUniformBufferMemberInfo NewMemberInfo;
			NewMemberInfo.NameAsStructMember = NestedStructNamePrefix + MemberName;
			NewMemberInfo.GlobalName = NestedGlobalNamePrefix + MemberName;
			UniformBufferMembers.Add(MoveTemp(NewMemberInfo));
		}
	} 
	while (MemberSearchPtr < LastMemberSemicolon);

	const TCHAR* StructEndPtr = StructNameEndPtr;

	// Skip over trailing tokens '[1];'
	while (*StructEndPtr && *StructEndPtr != ';')
	{
		StructEndPtr++;
	}

	return StructEndPtr;
}

bool MatchStructMemberName(const FString& SymbolName, const TCHAR* SearchPtr, const FString& PreprocessedShaderSource)
{
	// Only match whole symbol
	if (IsValidHLSLIdentifierCharacter(*(SearchPtr - 1)) || *(SearchPtr - 1) == '.')
	{
		return false;
	}

	for (int32 i = 0; i < SymbolName.Len(); i++)
	{
		if (*SearchPtr != SymbolName[i])
		{
			return false;
		}
		
		SearchPtr++;

		if (i < SymbolName.Len() - 1)
		{
			// Skip whitespace within the struct member reference before the end
			// eg 'View. ViewToClip'
			while (FChar::IsWhitespace(*SearchPtr))
			{
				SearchPtr++;
			}
		}
	}

	// Only match whole symbol
	if (IsValidHLSLIdentifierCharacter(*SearchPtr))
	{
		return false;
	}

	return true;
}

// Searches string SearchPtr for 'SearchString.' or 'SearchString .' and returns a pointer to the first character of the match.
TCHAR* FindNextUniformBufferReference(TCHAR* SearchPtr, const TCHAR* SearchString, uint32 SearchStringLength)
{
	TCHAR* FoundPtr = FCString::Strstr(SearchPtr, SearchString);
	
	while(FoundPtr)
	{
		if (FoundPtr == nullptr)
		{
			return nullptr;
		}
		else if (FoundPtr[SearchStringLength] == '.' || (FoundPtr[SearchStringLength] == ' ' && FoundPtr[SearchStringLength+1] == '.'))
		{
			return FoundPtr;
		}
		
		FoundPtr = FCString::Strstr(FoundPtr + SearchStringLength, SearchString);
	}
	
	return nullptr;
}

static const TCHAR* const s_AllSRVTypes[] =
{
	TEXT("Texture1D"),
	TEXT("Texture1DArray"),
	TEXT("Texture2D"),
	TEXT("Texture2DArray"),
	TEXT("Texture2DMS"),
	TEXT("Texture2DMSArray"),
	TEXT("Texture3D"),
	TEXT("TextureCube"),
	TEXT("TextureCubeArray"),

	TEXT("Buffer"),
	TEXT("ByteAddressBuffer"),
	TEXT("StructuredBuffer"),
	TEXT("ConstantBuffer"),
	TEXT("RaytracingAccelerationStructure"),
};

static const TCHAR* const s_AllUAVTypes[] =
{
	TEXT("AppendStructuredBuffer"),
	TEXT("RWBuffer"),
	TEXT("RWByteAddressBuffer"),
	TEXT("RWStructuredBuffer"),
	TEXT("RWTexture1D"),
	TEXT("RWTexture1DArray"),
	TEXT("RWTexture2D"),
	TEXT("RWTexture2DArray"),
	TEXT("RWTexture3D"),
	TEXT("RasterizerOrderedTexture2D"),
};

static const TCHAR* const s_AllSamplerTypes[] =
{
	TEXT("SamplerState"),
	TEXT("SamplerComparisonState"),
};

EShaderParameterType UE::ShaderCompilerCommon::ParseParameterType(
	FStringView InType,
	TArrayView<const TCHAR*> InExtraSRVTypes,
	TArrayView<const TCHAR*> InExtraUAVTypes)
{
	TArrayView<const TCHAR* const> AllSamplerTypes(s_AllSamplerTypes);
	TArrayView<const TCHAR* const> AllSRVTypes(s_AllSRVTypes);
	TArrayView<const TCHAR* const> AllUAVTypes(s_AllUAVTypes);

	if (AllSamplerTypes.Contains(InType))
	{
		return EShaderParameterType::Sampler;
	}

	FStringView UntemplatedType = InType;
	if (int32 Index = InType.Find(TEXT("<")); Index != INDEX_NONE)
	{
		const int32 NumChars = InType.Len() - Index;
		UntemplatedType = InType.LeftChop(NumChars);
	}

	if (AllSRVTypes.Contains(UntemplatedType) || InExtraSRVTypes.Contains(UntemplatedType))
	{
		return EShaderParameterType::SRV;
	}

	if (AllUAVTypes.Contains(UntemplatedType) || InExtraUAVTypes.Contains(UntemplatedType))
	{
		return EShaderParameterType::UAV;
	}

	return EShaderParameterType::LooseData;
}

FStringView UE::ShaderCompilerCommon::RemoveConstantBufferPrefix(FStringView InName)
{
	return UE::String::RemoveFromStart(InName, FStringView(UE::ShaderCompilerCommon::kUniformBufferConstantBufferPrefix));
}

FString UE::ShaderCompilerCommon::RemoveConstantBufferPrefix(const FString& InName)
{
	return FString(RemoveConstantBufferPrefix(FStringView(InName)));
}

EShaderParameterType UE::ShaderCompilerCommon::ParseAndRemoveBindlessParameterPrefix(FStringView& InName)
{
	const FStringView OriginalName = InName;

	if (InName = UE::String::RemoveFromStart(InName, FStringView(UE::ShaderCompilerCommon::kBindlessResourcePrefix)); InName != OriginalName)
	{
		return EShaderParameterType::BindlessResourceIndex;
	}

	if (InName = UE::String::RemoveFromStart(InName, FStringView(UE::ShaderCompilerCommon::kBindlessSamplerPrefix)); InName != OriginalName)
	{
		return EShaderParameterType::BindlessSamplerIndex;
	}

	return EShaderParameterType::LooseData;
}

EShaderParameterType UE::ShaderCompilerCommon::ParseAndRemoveBindlessParameterPrefix(FString& InName)
{
	FStringView Name(InName);
	const EShaderParameterType ParameterType = ParseAndRemoveBindlessParameterPrefix(Name);
	InName = FString(Name);

	return ParameterType;
}

bool UE::ShaderCompilerCommon::RemoveBindlessParameterPrefix(FString& InName)
{
	return InName.RemoveFromStart(UE::ShaderCompilerCommon::kBindlessResourcePrefix)
		|| InName.RemoveFromStart(UE::ShaderCompilerCommon::kBindlessSamplerPrefix);
}

bool UE::ShaderCompilerCommon::ValidatePackedResourceCounts(FShaderCompilerOutput& Output, const FShaderCodePackedResourceCounts& PackedResourceCounts)
{
	if (Output.bSucceeded)
	{
		auto GetAllResourcesOfType = [&](EShaderParameterType InType)
		{
			const TArray<FString> AllNames = Output.ParameterMap.GetAllParameterNamesOfType(InType);
			if (AllNames.IsEmpty())
			{
				return FString();
			}
			return FString::Join(AllNames, TEXT(", "));
		};

		if (EnumHasAnyFlags(PackedResourceCounts.UsageFlags, EShaderResourceUsageFlags::BindlessResources) && PackedResourceCounts.NumSRVs > 0)
		{
			const FString Names = GetAllResourcesOfType(EShaderParameterType::SRV);
			Output.Errors.Add(FString::Printf(TEXT("Shader is mixing bindless resources with non-bindless resources. %d SRV slots were detected: %s"), PackedResourceCounts.NumSRVs, *Names));
			Output.bSucceeded = false;
		}

		if (EnumHasAnyFlags(PackedResourceCounts.UsageFlags, EShaderResourceUsageFlags::BindlessResources) && PackedResourceCounts.NumUAVs > 0)
		{
			const FString Names = GetAllResourcesOfType(EShaderParameterType::UAV);
			Output.Errors.Add(FString::Printf(TEXT("Shader is mixing bindless resources with non-bindless resources. %d UAV slots were detected: %s"), PackedResourceCounts.NumUAVs, *Names));
			Output.bSucceeded = false;
		}

		if (EnumHasAnyFlags(PackedResourceCounts.UsageFlags, EShaderResourceUsageFlags::BindlessSamplers) && PackedResourceCounts.NumSamplers > 0)
		{
			const FString Names = GetAllResourcesOfType(EShaderParameterType::Sampler);
			Output.Errors.Add(FString::Printf(TEXT("Shader is mixing bindless samplers with non-bindless samplers. %d sampler slots were detected: %s"), PackedResourceCounts.NumSamplers, *Names));
			Output.bSucceeded = false;
		}
	}

	return Output.bSucceeded;
}

void UE::ShaderCompilerCommon::ParseRayTracingEntryPoint(const FString& Input, FString& OutMain, FString& OutAnyHit, FString& OutIntersection)
{
	auto ParseEntry = [&Input](const TCHAR* Marker)
	{
		FString Result;
		const int32 BeginIndex = Input.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (BeginIndex != INDEX_NONE)
		{
			int32 EndIndex = Input.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromStart, BeginIndex);
			if (EndIndex == INDEX_NONE)
			{
				EndIndex = Input.Len() + 1;
			}
			const int32 MarkerLen = FCString::Strlen(Marker);
			const int32 Count = EndIndex - BeginIndex;
			Result = Input.Mid(BeginIndex + MarkerLen, Count - MarkerLen);
		}
		return Result;
	};

	OutMain = ParseEntry(TEXT("closesthit="));
	OutAnyHit = ParseEntry(TEXT("anyhit="));
	OutIntersection = ParseEntry(TEXT("intersection="));

	// If complex hit group entry is not specified, assume a single verbatim entry point
	if (OutMain.IsEmpty() && OutAnyHit.IsEmpty() && OutIntersection.IsEmpty())
	{
		OutMain = Input;
	}
}

void HandleReflectedGlobalConstantBufferMember(
	const FString& InMemberName,
	uint32 ConstantBufferIndex,
	int32 ReflectionOffset,
	int32 ReflectionSize,
	FShaderCompilerOutput& Output
)
{
	FString MemberName = InMemberName;
	const EShaderParameterType ParameterType = UE::ShaderCompilerCommon::ParseAndRemoveBindlessParameterPrefix(MemberName);

	Output.ParameterMap.AddParameterAllocation(
		*MemberName,
		ConstantBufferIndex,
		ReflectionOffset,
		ReflectionSize,
		ParameterType);
}

void HandleReflectedRootConstantBufferMember(
	const FShaderCompilerInput& Input,
	const FShaderParameterParser& ShaderParameterParser,
	const FString& InMemberName,
	int32 ReflectionOffset,
	int32 ReflectionSize,
	FShaderCompilerOutput& Output
)
{
	ShaderParameterParser.ValidateShaderParameterType(Input, InMemberName, ReflectionOffset, ReflectionSize, Output);

	FString MemberName = InMemberName;
	const EShaderParameterType ParameterType = UE::ShaderCompilerCommon::ParseAndRemoveBindlessParameterPrefix(MemberName);

	if (ParameterType != EShaderParameterType::LooseData)
	{
		Output.ParameterMap.AddParameterAllocation(
			*MemberName,
			FShaderParametersMetadata::kRootCBufferBindingIndex,
			ReflectionOffset,
			1,
			ParameterType);
	}
}

void HandleReflectedRootConstantBuffer(
	int32 ConstantBufferSize,
	FShaderCompilerOutput& CompilerOutput
)
{
	CompilerOutput.ParameterMap.AddParameterAllocation(
		FShaderParametersMetadata::kRootUniformBufferBindingName,
		FShaderParametersMetadata::kRootCBufferBindingIndex,
		0,
		static_cast<uint16>(ConstantBufferSize),
		EShaderParameterType::LooseData);
}

void HandleReflectedUniformBuffer(
	const FString& UniformBufferName,
	int32 ReflectionSlot,
	int32 BaseIndex,
	int32 BufferSize,
	FShaderCompilerOutput& CompilerOutput
)
{
	FString AdjustedUniformBufferName(UE::ShaderCompilerCommon::RemoveConstantBufferPrefix(UniformBufferName));

	CompilerOutput.ParameterMap.AddParameterAllocation(
		*AdjustedUniformBufferName,
		ReflectionSlot,
		BaseIndex,
		BufferSize,
		EShaderParameterType::UniformBuffer
	);
}

void HandleReflectedShaderResource(
	const FString& ResourceName,
	int32 BindOffset,
	int32 ReflectionSlot,
	int32 BindCount,
	FShaderCompilerOutput& CompilerOutput
)
{
	CompilerOutput.ParameterMap.AddParameterAllocation(
		*ResourceName,
		BindOffset,
		ReflectionSlot,
		BindCount,
		EShaderParameterType::SRV
	);
}

void HandleReflectedShaderUAV(
	const FString& UAVName,
	int32 BindOffset,
	int32 ReflectionSlot,
	int32 BindCount,
	FShaderCompilerOutput& CompilerOutput
)
{
	CompilerOutput.ParameterMap.AddParameterAllocation(
		*UAVName,
		BindOffset,
		ReflectionSlot,
		BindCount,
		EShaderParameterType::UAV
	);
}

void HandleReflectedShaderSampler(
	const FString& SamplerName,
	int32 BindOffset,
	int32 ReflectionSlot,
	int32 BindCount,
	FShaderCompilerOutput& CompilerOutput
)
{
	CompilerOutput.ParameterMap.AddParameterAllocation(
		*SamplerName,
		BindOffset,
		ReflectionSlot,
		BindCount,
		EShaderParameterType::Sampler
	);
}

void AddNoteToDisplayShaderParameterStructureOnCppSide(
	const FShaderParametersMetadata* ParametersStructure,
	FShaderCompilerOutput& CompilerOutput)
{
	FShaderCompilerError Error;
	Error.StrippedErrorMessage = FString::Printf(
		TEXT("Note: Definition of structure %s"),
		ParametersStructure->GetStructTypeName());
	Error.ErrorVirtualFilePath = ANSI_TO_TCHAR(ParametersStructure->GetFileName());
	Error.ErrorLineString = FString::FromInt(ParametersStructure->GetFileLine());

	CompilerOutput.Errors.Add(Error);
}

void AddUnboundShaderParameterError(
	const FShaderCompilerInput& CompilerInput,
	const FShaderParameterParser& ShaderParameterParser,
	const FString& ParameterBindingName,
	FShaderCompilerOutput& CompilerOutput)
{
	check(CompilerInput.RootParametersStructure);

	const FShaderParameterParser::FParsedShaderParameter& Member = ShaderParameterParser.FindParameterInfos(ParameterBindingName);
	check(!Member.IsBindable());

	FShaderCompilerError Error(FString::Printf(
		TEXT("Error: Shader parameter %s could not be bound to %s's shader parameter structure %s."),
		*ParameterBindingName,
		*CompilerInput.ShaderName,
		CompilerInput.RootParametersStructure->GetStructTypeName()));
	ShaderParameterParser.GetParameterFileAndLine(Member, Error.ErrorVirtualFilePath, Error.ErrorLineString);

	CompilerOutput.Errors.Add(Error);
	CompilerOutput.bSucceeded = false;

	AddNoteToDisplayShaderParameterStructureOnCppSide(CompilerInput.RootParametersStructure, CompilerOutput);
}

// The cross compiler doesn't yet support struct initializers needed to construct static structs for uniform buffers
// Replace all uniform buffer struct member references (View.WorldToClip) with a flattened name that removes the struct dependency (View_WorldToClip)
void RemoveUniformBuffersFromSource(const FShaderCompilerEnvironment& Environment, FString& PreprocessedShaderSource)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RemoveUniformBuffersFromSource);

	TMap<FString, TArray<FUniformBufferMemberInfo>> UniformBufferNameToMembers;
	UniformBufferNameToMembers.Reserve(Environment.UniformBufferMap.Num());

	// Build a mapping from uniform buffer name to its members
	{
		const TCHAR* UniformBufferStructIdentifier = TEXT("static const struct");
		const int32 StructPrefixLen = FCString::Strlen(TEXT("static const "));
		const int32 StructIdentifierLen = FCString::Strlen(UniformBufferStructIdentifier);
		TCHAR* SearchPtr = FCString::Strstr(&PreprocessedShaderSource[0], UniformBufferStructIdentifier);

		while (SearchPtr)
		{
			FString UniformBufferName;
			const TCHAR* ConstStructEndPtr = ParseStructRecursive(SearchPtr + StructPrefixLen, UniformBufferName, 0, TEXT(""), TEXT(""), UniformBufferNameToMembers);
			TCHAR* StructEndPtr = &PreprocessedShaderSource[ConstStructEndPtr - &PreprocessedShaderSource[0]];

			// Comment out the uniform buffer struct and initializer
			*SearchPtr = '/';
			*(SearchPtr + 1) = '*';
			*(StructEndPtr - 1) = '*';
			*StructEndPtr = '/';

			SearchPtr = FCString::Strstr(StructEndPtr, UniformBufferStructIdentifier);
		}
	}

	// Replace all uniform buffer struct member references (View.WorldToClip) with a flattened name that removes the struct dependency (View_WorldToClip)
	for (TMap<FString, TArray<FUniformBufferMemberInfo>>::TConstIterator It(UniformBufferNameToMembers); It; ++It)
	{
		const FString& UniformBufferName = It.Key();
		FString UniformBufferAccessString = UniformBufferName + TEXT(".");
		// MCPP inserts spaces after defines
		FString UniformBufferAccessStringWithSpace = UniformBufferName + TEXT(" .");

		// Search for the uniform buffer name first, as an optimization (instead of searching the entire source for every member)
		TCHAR* SearchPtr = FindNextUniformBufferReference(&PreprocessedShaderSource[0], *UniformBufferName, UniformBufferName.Len());

		while (SearchPtr)
		{
			const TArray<FUniformBufferMemberInfo>& UniformBufferMembers = It.Value();

			// Find the matching member we are replacing
			for (int32 MemberIndex = 0; MemberIndex < UniformBufferMembers.Num(); MemberIndex++)
			{
				const FString& MemberNameAsStructMember = UniformBufferMembers[MemberIndex].NameAsStructMember;

				if (MatchStructMemberName(MemberNameAsStructMember, SearchPtr, PreprocessedShaderSource))
				{
					const FString& MemberNameGlobal = UniformBufferMembers[MemberIndex].GlobalName;
					int32 NumWhitespacesToAdd = 0;

					for (int32 i = 0; i < MemberNameAsStructMember.Len(); i++)
					{
						if (i < MemberNameAsStructMember.Len() - 1)
						{
							if (FChar::IsWhitespace(SearchPtr[i]))
							{
								NumWhitespacesToAdd++;
							}
						}

						SearchPtr[i] = MemberNameGlobal[i];
					}

					// MCPP inserts spaces after defines
					// #define ReflectionStruct OpaqueBasePass.Shared.Reflection
					// 'ReflectionStruct.SkyLightCubemapBrightness' becomes 'OpaqueBasePass.Shared.Reflection .SkyLightCubemapBrightness' after MCPP
					// In order to convert this struct member reference into a globally unique variable we move the spaces to the end
					// 'OpaqueBasePass.Shared.Reflection .SkyLightCubemapBrightness' -> 'OpaqueBasePass_Shared_Reflection_SkyLightCubemapBrightness '
					for (int32 i = 0; i < NumWhitespacesToAdd; i++)
					{
						// If we passed MatchStructMemberName, it should not be possible to overwrite the null terminator
						check(SearchPtr[MemberNameAsStructMember.Len() + i] != 0);
						SearchPtr[MemberNameAsStructMember.Len() + i] = ' ';
					}
							
					break;
				}
			}

			SearchPtr = FindNextUniformBufferReference(SearchPtr + UniformBufferAccessString.Len(), *UniformBufferName, UniformBufferName.Len());
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Process TEXT() macro to convert them into GPU ASCII characters

FString ParseText(const TCHAR* StartPtr, const TCHAR*& EndPtr)
{
	const TCHAR* OpeningBracePtr = FCString::Strstr(StartPtr, TEXT("("));
	check(OpeningBracePtr);

	const TCHAR* ClosingBracePtr = FindMatchingClosingParenthesis(OpeningBracePtr + 1);
	check(ClosingBracePtr);

	FString Out;
	if (OpeningBracePtr && ClosingBracePtr)
	{
		const TCHAR* CurrPtr = OpeningBracePtr;
		do
		{
			Out += *CurrPtr;
			CurrPtr++;
		} while (CurrPtr != ClosingBracePtr+1);
	}
	EndPtr = ClosingBracePtr;
	return Out;
}

void ConvertTextToAsciiCharacter(const FString& InText, FString& OutText, FString& OutEncodedText)
{
	const uint32 CharCount = InText.Len();
	OutEncodedText.Reserve(CharCount * 3); // ~2 digits per character + a comma
	OutText = InText;
	for (uint32 CharIt = 0; CharIt < CharCount; ++CharIt)
	{
		const char C = InText[CharIt];
		OutEncodedText.AppendInt(uint8(C));
		if (CharIt + 1 != CharCount)
		{
			OutEncodedText += ',';
		}
	}
}

// Simple token matching and expansion to replace TEXT macro into supported character string
void TransformStringIntoCharacterArray(FString& PreprocessedShaderSource)
{
	struct FTextEntry
	{
		uint32  Index;
		uint32  Hash;
		uint32  Offset;
		FString SourceText;
		FString ConvertedText;
		FString EncodedText;
	};
	TArray<FTextEntry> Entries;

	// 1. Find all TEXT strings
	// 2. Add a text entry
	// 3. Replace TEXT by its entry number
	uint32 GlobalCount = 0;
	{
		const FString InitHashBegin(TEXT("InitShaderPrintText("));
		const FString InitHashEnd(TEXT(")"));

		const TCHAR* TextIdentifier = TEXT("TEXT(");
		const TCHAR* SearchPtr = FCString::Strstr(&PreprocessedShaderSource[0], TextIdentifier);
		while (SearchPtr)
		{
			const TCHAR* EndPtr = nullptr;
			FString Text = ParseText(SearchPtr, EndPtr);
			if (EndPtr)
			{
				// Trim enclosing
				Text.RemoveFromEnd("\")");
				Text.RemoveFromStart("(\"");

				// Register entry and convert text
				const uint32 EntryIndex = Entries.Num();
				uint32 ValidCharCount = 0;
				FTextEntry& Entry = Entries.AddDefaulted_GetRef();
				Entry.Index			= EntryIndex;
				Entry.Offset		= GlobalCount;
				Entry.SourceText	= Text;
				ConvertTextToAsciiCharacter(Entry.SourceText, Entry.ConvertedText, Entry.EncodedText);
				Entry.Hash			= CityHash32((const char*)&Entry.SourceText.GetCharArray(), sizeof(FString::ElementType) * Entry.SourceText.Len());

				GlobalCount += Entry.ConvertedText.Len();

				// Replace string
				const TCHAR* StartPtr = &PreprocessedShaderSource[0];
				const uint32 StartIndex = SearchPtr - StartPtr;
				const uint32 CharCount = (EndPtr - SearchPtr) + 1;
				PreprocessedShaderSource.RemoveAt(StartIndex, CharCount);

				const FString HashText = InitHashBegin + FString::FromInt(EntryIndex) + InitHashEnd;
				PreprocessedShaderSource.InsertAt(StartIndex, HashText);

				// Update SearchPtr, as PreprocessedShaderSource has been modified, and its memory could have been reallocated, causing SearchPtr to be invalid.
				SearchPtr = &PreprocessedShaderSource[0] + StartIndex;
			}
			SearchPtr = FCString::Strstr(SearchPtr, TextIdentifier);
		}
	}

	// 4. Write a global struct containing all the entries
	// 5. Write the function for fetching character for a given entry index
	const uint32 EntryCount = Entries.Num();
	FString TextChars;
	if (EntryCount>0 && GlobalCount>0)
	{
		// 1. Encoded character for each text entry within a single global char array
		TextChars = FString::Printf(TEXT("static const uint TEXT_CHARS[%d] = {\n"), GlobalCount);
		for (FTextEntry& Entry : Entries)
		{
			TextChars += FString::Printf(TEXT("\t%s%s // %d: \"%s\"\n"), *Entry.EncodedText, Entry.Index < EntryCount - 1 ? TEXT(",") : TEXT(""), Entry.Index, * Entry.SourceText);
		}
		TextChars += TEXT("};\n\n");

		// 2. Offset within the global array
		TextChars += FString::Printf(TEXT("static const uint TEXT_OFFSETS[%d] = {\n"), EntryCount+1);
		for (FTextEntry& Entry : Entries)
		{
			TextChars += FString::Printf(TEXT("\t%d, // %d: \"%s\"\n"), Entry.Offset, Entry.Index, *Entry.SourceText);
		}
		TextChars += FString::Printf(TEXT("\t%d // end\n"), GlobalCount);
		TextChars += TEXT("};\n\n");

		// 3. Entry hashes
		TextChars += TEXT("// Hashes are computed using the CityHash32 function\n");
		TextChars += FString::Printf(TEXT("static const uint TEXT_HASHES[%d] = {\n"), EntryCount);
		for (FTextEntry& Entry : Entries)
		{
			TextChars += FString::Printf(TEXT("\t0x%x%s // %d: \"%s\"\n"), Entry.Hash, Entry.Index < EntryCount - 1 ? TEXT(",") : TEXT(""), Entry.Index, * Entry.SourceText);
		}
		TextChars += TEXT("};\n\n");

		TextChars += TEXT("uint ShaderPrintGetChar(uint InIndex)              { return TEXT_CHARS[InIndex]; }\n");
		TextChars += TEXT("uint ShaderPrintGetOffset(FShaderPrintText InText) { return TEXT_OFFSETS[InText.Index]; }\n");
		TextChars += TEXT("uint ShaderPrintGetHash(FShaderPrintText InText)   { return TEXT_HASHES[InText.Index]; }\n");
	}
	else
	{	
		TextChars += TEXT("uint ShaderPrintGetChar(uint Index)                { return 0; }\n");
		TextChars += TEXT("uint ShaderPrintGetOffset(FShaderPrintText InText) { return 0; }\n");
		TextChars += TEXT("uint ShaderPrintGetHash(FShaderPrintText InText)   { return 0; }\n");
	}
	
	// 6. Insert global struct data + print function
	{
		const TCHAR* InsertToken = TEXT("GENERATED_SHADER_PRINT");
		const TCHAR* SearchPtr = FCString::Strstr(&PreprocessedShaderSource[0], InsertToken);
		if (SearchPtr)
		{
			// Replace string
			const TCHAR* StartPtr = &PreprocessedShaderSource[0];
			const uint32 StartIndex = SearchPtr - StartPtr;
			const uint32 CharCount = FCString::Strlen(InsertToken);
			PreprocessedShaderSource.RemoveAt(StartIndex, CharCount);
			PreprocessedShaderSource.InsertAt(StartIndex, TextChars);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString CreateShaderCompilerWorkerDirectCommandLine(const FShaderCompilerInput& Input, uint32 CCFlags)
{
	FString Text(TEXT("-directcompile -format="));
	Text += Input.ShaderFormat.GetPlainNameString();
	Text += TEXT(" -entry=");
	Text += Input.EntryPointName;

	Text += TEXT(" -shaderPlatformName=");
	Text += Input.ShaderPlatformName.GetPlainNameString();

	switch (Input.Target.Frequency)
	{
	case SF_Vertex:			Text += TEXT(" -vs"); break;
	case SF_Mesh:			Text += TEXT(" -ms"); break;
	case SF_Amplification:	Text += TEXT(" -as"); break;
	case SF_Geometry:		Text += TEXT(" -gs"); break;
	case SF_Pixel:			Text += TEXT(" -ps"); break;
	case SF_Compute:		Text += TEXT(" -cs"); break;
#if RHI_RAYTRACING
	case SF_RayGen:			Text += TEXT(" -rgs"); break;
	case SF_RayMiss:		Text += TEXT(" -rms"); break;
	case SF_RayHitGroup:	Text += TEXT(" -rhs"); break;
	case SF_RayCallable:	Text += TEXT(" -rcs"); break;
#endif // RHI_RAYTRACING
	default: break;
	}
	if (Input.bCompilingForShaderPipeline)
	{
		Text += TEXT(" -pipeline");
	}
	if (Input.bIncludeUsedOutputs)
	{
		Text += TEXT(" -usedoutputs=");
		for (int32 Index = 0; Index < Input.UsedOutputs.Num(); ++Index)
		{
			if (Index != 0)
			{
				Text += TEXT("+");
			}
			Text += Input.UsedOutputs[Index];
		}
	}

	Text += TEXT(" ");
	Text += Input.DumpDebugInfoPath / Input.GetSourceFilename();

	Text += TEXT(" -cflags=");
	Text += FString::Printf(TEXT("%llu"), Input.Environment.CompilerFlags.GetData());

	if (CCFlags)
	{
		Text += TEXT(" -hlslccflags=");
		Text += FString::Printf(TEXT("%llu"), CCFlags);
	}
	// When we're running in directcompile mode, we don't to spam the crash reporter
	Text += TEXT(" -nocrashreports");
	return Text;
}

static FString CreateShaderConductorCommandLine(const FShaderCompilerInput& Input, const FString& SourceFilename, EShaderConductorTarget SCTarget)
{
	const TCHAR* Stage = nullptr;
	switch (Input.Target.GetFrequency())
	{
	case SF_Vertex:			Stage = TEXT("vs"); break;
	case SF_Pixel:			Stage = TEXT("ps"); break;
	case SF_Geometry:		Stage = TEXT("gs"); break;
	case SF_Compute:		Stage = TEXT("cs"); break;
	default:				return FString();
	}

	const TCHAR* Target = nullptr;
	switch (SCTarget)
	{
	case EShaderConductorTarget::Dxil:		Target = TEXT("dxil"); break;
	case EShaderConductorTarget::Spirv:		Target = TEXT("spirv"); break;
	default:								return FString();
	}

	FString CmdLine = TEXT("-E ") + Input.EntryPointName;
	//CmdLine += TEXT("-O ") + *(CompilerInfo.Input.D);
	CmdLine += TEXT(" -S ") + FString(Stage);
	CmdLine += TEXT(" -T ");
	CmdLine += Target;
	CmdLine += TEXT(" -I ") + (Input.DumpDebugInfoPath / SourceFilename);

	return CmdLine;
}

SHADERCOMPILERCOMMON_API void WriteShaderConductorCommandLine(const FShaderCompilerInput& Input, const FString& SourceFilename, EShaderConductorTarget Target)
{
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / TEXT("ShaderConductorCmdLine.txt")));
	if (FileWriter)
	{
		FString CmdLine = CreateShaderConductorCommandLine(Input, SourceFilename, Target);

		FileWriter->Serialize(TCHAR_TO_ANSI(*CmdLine), CmdLine.Len());
		FileWriter->Close();
		delete FileWriter;
	}
}

static int Mali_ExtractNumberInstructions(const FString &MaliOutput)
{
	int ReturnedNum = 0;

	// Parse the instruction count
	int32 InstructionStringLength = FPlatformString::Strlen(TEXT("Instructions Emitted:"));
	int32 InstructionsIndex = MaliOutput.Find(TEXT("Instructions Emitted:"));

	// new version of mali offline compiler uses a different string in its output
	if (InstructionsIndex == INDEX_NONE)
	{
		InstructionStringLength = FPlatformString::Strlen(TEXT("Total instruction cycles:"));
		InstructionsIndex = MaliOutput.Find(TEXT("Total instruction cycles:"));
	}

	if (InstructionsIndex != INDEX_NONE && InstructionsIndex + InstructionStringLength < MaliOutput.Len())
	{
		const int32 EndIndex = MaliOutput.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, InstructionsIndex + InstructionStringLength);

		if (EndIndex != INDEX_NONE)
		{
			int32 StartIndex = InstructionsIndex + InstructionStringLength;

			bool bFoundNrStart = false;
			int32 NumberIndex = 0;

			while (StartIndex < EndIndex)
			{
				if (FChar::IsDigit(MaliOutput[StartIndex]) && !bFoundNrStart)
				{
					// found number's beginning
					bFoundNrStart = true;
					NumberIndex = StartIndex;
				}
				else if (FChar::IsWhitespace(MaliOutput[StartIndex]) && bFoundNrStart)
				{
					// found number's end
					bFoundNrStart = false;
					const FString NumberString = MaliOutput.Mid(NumberIndex, StartIndex - NumberIndex);
					const float fNrInstructions = FCString::Atof(*NumberString);
					ReturnedNum += ceil(fNrInstructions);
				}

				++StartIndex;
			}
		}
	}

	return ReturnedNum;
}

static FString Mali_ExtractErrors(const FString &MaliOutput)
{
	FString ReturnedErrors;

	const int32 GlobalErrorIndex = MaliOutput.Find(TEXT("Compilation failed."));

	// find each 'line' that begins with token "ERROR:" and copy it to the returned string
	if (GlobalErrorIndex != INDEX_NONE)
	{
		int32 CompilationErrorIndex = MaliOutput.Find(TEXT("ERROR:"));
		while (CompilationErrorIndex != INDEX_NONE)
		{
			int32 EndLineIndex = MaliOutput.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CompilationErrorIndex + 1);
			EndLineIndex = EndLineIndex == INDEX_NONE ? MaliOutput.Len() - 1 : EndLineIndex;

			ReturnedErrors += MaliOutput.Mid(CompilationErrorIndex, EndLineIndex - CompilationErrorIndex + 1);

			CompilationErrorIndex = MaliOutput.Find(TEXT("ERROR:"), ESearchCase::CaseSensitive, ESearchDir::FromStart, EndLineIndex);
		}
	}

	return ReturnedErrors;
}

void CompileOfflineMali(const FShaderCompilerInput& Input, FShaderCompilerOutput& ShaderOutput, const ANSICHAR* ShaderSource, const int32 SourceSize, bool bVulkanSpirV, const ANSICHAR* VulkanSpirVEntryPoint)
{
	const bool bCompilerExecutableExists = FPaths::FileExists(Input.ExtraSettings.OfflineCompilerPath);

	if (bCompilerExecutableExists)
	{
		const auto Frequency = (EShaderFrequency)Input.Target.Frequency;
		const FString WorkingDir(FPlatformProcess::ShaderDir());

		FString CompilerPath = Input.ExtraSettings.OfflineCompilerPath;

		FString CompilerCommand = "";

		// add process and thread ids to the file name to avoid collision between workers
		auto ProcID = FPlatformProcess::GetCurrentProcessId();
		auto ThreadID = FPlatformTLS::GetCurrentThreadId();
		FString GLSLSourceFile = WorkingDir / TEXT("GLSLSource#") + FString::FromInt(ProcID) + TEXT("#") + FString::FromInt(ThreadID);

		// setup compilation arguments
		TCHAR *FileExt = nullptr;
		switch (Frequency)
		{
			case SF_Vertex:
				GLSLSourceFile += bVulkanSpirV ? TEXT(".spv") : TEXT(".vert");
				CompilerCommand += TEXT(" -v");
			break;
			case SF_Pixel:
				GLSLSourceFile += bVulkanSpirV ? TEXT(".spv") : TEXT(".frag");
				CompilerCommand += TEXT(" -f");
			break;
			case SF_Geometry:
				GLSLSourceFile += bVulkanSpirV ? TEXT(".spv") : TEXT(".geom");
				CompilerCommand += TEXT(" -g");
			break;
			case SF_Compute:
				GLSLSourceFile += bVulkanSpirV ? TEXT(".spv") : TEXT(".comp");
				CompilerCommand += TEXT(" -C");
			break;

			default:
				GLSLSourceFile += TEXT(".shd");
			break;
		}

		if (bVulkanSpirV)
		{
			CompilerCommand += FString::Printf(TEXT(" -y %s -p"), ANSI_TO_TCHAR(VulkanSpirVEntryPoint));
		}
		else
		{
			CompilerCommand += TEXT(" -s");
		}

		FArchive* Ar = IFileManager::Get().CreateFileWriter(*GLSLSourceFile, FILEWRITE_EvenIfReadOnly);

		if (Ar == nullptr)
		{
			return;
		}

		// write out the shader source to a file and use it below as input for the compiler
		Ar->Serialize((void*)ShaderSource, SourceSize);
		delete Ar;

		FString StdOut;
		FString StdErr;
		int32 ReturnCode = 0;

		// Since v6.2.0, Mali compiler needs to be started in the executable folder or it won't find "external/glslangValidator" for Vulkan
		FString CompilerWorkingDirectory = FPaths::GetPath(CompilerPath);

		if (!CompilerWorkingDirectory.IsEmpty() && FPaths::DirectoryExists(CompilerWorkingDirectory))
		{
			// compiler command line contains flags and the GLSL source file name
			CompilerCommand += " " + FPaths::ConvertRelativePathToFull(GLSLSourceFile);

			// Run Mali shader compiler and wait for completion
			FPlatformProcess::ExecProcess(*CompilerPath, *CompilerCommand, &ReturnCode, &StdOut, &StdErr, *CompilerWorkingDirectory);
		}
		else
		{
			StdErr = "Couldn't find Mali offline compiler at " + CompilerPath;
		}

		// parse Mali's output and extract instruction count or eventual errors
		ShaderOutput.bSucceeded = (ReturnCode >= 0);
		if (ShaderOutput.bSucceeded)
		{
			// check for errors
			if (StdErr.Len())
			{
				ShaderOutput.bSucceeded = false;

				FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
				NewError->StrippedErrorMessage = TEXT("[Mali Offline Complier]\n") + StdErr;
			}
			else
			{
				FString Errors = Mali_ExtractErrors(StdOut);

				if (Errors.Len())
				{
					FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
					NewError->StrippedErrorMessage = TEXT("[Mali Offline Complier]\n") + Errors;
					ShaderOutput.bSucceeded = false;
				}
			}

			// extract instruction count
			if (ShaderOutput.bSucceeded)
			{
				ShaderOutput.NumInstructions = Mali_ExtractNumberInstructions(StdOut);
			}
		}

		// we're done so delete the shader file
		IFileManager::Get().Delete(*GLSLSourceFile, true, true);
	}
}


FString GetDumpDebugUSFContents(const FShaderCompilerInput& Input, const FString& Source, uint32 HlslCCFlags)
{
	FString Contents = Source;
	Contents += TEXT("\n");
	Contents += CrossCompiler::CreateResourceTableFromEnvironment(Input.Environment);
	Contents += TEXT("#if 0 /*DIRECT COMPILE*/\n");
	Contents += CreateShaderCompilerWorkerDirectCommandLine(Input, HlslCCFlags);
	Contents += TEXT("\n#endif /*DIRECT COMPILE*/\n");

	return Contents;
}

void DumpDebugUSF(const FShaderCompilerInput& Input, const ANSICHAR* Source, uint32 HlslCCFlags, const TCHAR* OverrideBaseFilename)
{
	FString NewSource = Source ? Source : "";
	FString Contents = GetDumpDebugUSFContents(Input, NewSource, HlslCCFlags);
	DumpDebugUSF(Input, NewSource, HlslCCFlags, OverrideBaseFilename);
}

void DumpDebugUSF(const FShaderCompilerInput& Input, const FString& Source, uint32 HlslCCFlags, const TCHAR* OverrideBaseFilename)
{
	FString BaseSourceFilename = (OverrideBaseFilename && *OverrideBaseFilename) ? OverrideBaseFilename : *Input.GetSourceFilename();
	FString Filename = Input.DumpDebugInfoPath / BaseSourceFilename;

	if (TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*Filename)))
	{
		FString Contents = GetDumpDebugUSFContents(Input, Source, HlslCCFlags);
		FileWriter->Serialize(TCHAR_TO_ANSI(*Contents), Contents.Len());
		FileWriter->Close();
	}
}

void DumpDebugShaderText(const FShaderCompilerInput& Input, const FString& InSource, const FString& FileExtension)
{
	FTCHARToUTF8 StringConverter(InSource.GetCharArray().GetData(), InSource.Len());

	// Provide mutable container to pass string to FArchive inside inner function
	TArray<ANSICHAR> SourceAnsi;
	SourceAnsi.SetNum(InSource.Len() + 1);
	FCStringAnsi::Strncpy(SourceAnsi.GetData(), (ANSICHAR*)StringConverter.Get(), SourceAnsi.Num());

	// Forward temporary container to primary function
	DumpDebugShaderText(Input, SourceAnsi.GetData(), InSource.Len(), FileExtension);
}

void DumpDebugShaderText(const FShaderCompilerInput& Input, ANSICHAR* InSource, int32 InSourceLength, const FString& FileExtension)
{
	DumpDebugShaderBinary(Input, InSource, InSourceLength * sizeof(ANSICHAR), FileExtension);
}

void DumpDebugShaderText(const FShaderCompilerInput& Input, ANSICHAR* InSource, int32 InSourceLength, const FString& FileName, const FString& FileExtension)
{
	DumpDebugShaderBinary(Input, InSource, InSourceLength * sizeof(ANSICHAR), FileName, FileExtension);
}

void DumpDebugShaderBinary(const FShaderCompilerInput& Input, void* InData, int32 InDataByteSize, const FString& FileExtension)
{
	if (InData != nullptr && InDataByteSize > 0 && !FileExtension.IsEmpty())
	{
		const FString Filename = Input.DumpDebugInfoPath / FPaths::GetBaseFilename(Input.GetSourceFilename()) + TEXT(".") + FileExtension;
		if (TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*Filename)))
		{
			FileWriter->Serialize(InData, InDataByteSize);
			FileWriter->Close();
		}
	}
}

void DumpDebugShaderBinary(const FShaderCompilerInput& Input, void* InData, int32 InDataByteSize, const FString& FileName, const FString& FileExtension)
{
	if (InData != nullptr && InDataByteSize > 0 && !FileExtension.IsEmpty())
	{
		const FString Filename = Input.DumpDebugInfoPath / FileName + TEXT(".") + FileExtension;
		if (TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*Filename)))
		{
			FileWriter->Serialize(InData, InDataByteSize);
			FileWriter->Close();
		}
	}
}

static void DumpDebugShaderDisassembled(const FShaderCompilerInput& Input, CrossCompiler::EShaderConductorIR Language, void* InData, int32 InDataByteSize, const FString& FileExtension)
{
	if (InData != nullptr && InDataByteSize > 0 && !FileExtension.IsEmpty())
	{
		TArray<ANSICHAR> AssemblyText;
		if (CrossCompiler::FShaderConductorContext::Disassemble(Language, InData, InDataByteSize, AssemblyText))
		{
			// Assembly text contains NUL terminator, so text lenght is |array|-1
			DumpDebugShaderText(Input, AssemblyText.GetData(), AssemblyText.Num() - 1, FileExtension);
		}
	}
}

void DumpDebugShaderDisassembledSpirv(const FShaderCompilerInput& Input, void* InData, int32 InDataByteSize, const FString& FileExtension)
{
	DumpDebugShaderDisassembled(Input, CrossCompiler::EShaderConductorIR::Spirv, InData, InDataByteSize, FileExtension);
}

void DumpDebugShaderDisassembledDxil(const FShaderCompilerInput& Input, void* InData, int32 InDataByteSize, const FString& FileExtension)
{
	DumpDebugShaderDisassembled(Input, CrossCompiler::EShaderConductorIR::Dxil, InData, InDataByteSize, FileExtension);
}

namespace CrossCompiler
{
	FString CreateResourceTableFromEnvironment(const FShaderCompilerEnvironment& Environment)
	{
		FString Line = TEXT("\n#if 0 /*BEGIN_RESOURCE_TABLES*/\n");
		for (auto Pair : Environment.UniformBufferMap)
		{
			Line += FString::Printf(TEXT("%s, %d\n"), *Pair.Key, Pair.Value.LayoutHash);
		}
		Line += TEXT("NULL, 0\n");
		for (auto Pair : Environment.ResourceTableMap)
		{
			const FResourceTableEntry& Entry = Pair.Value;
			Line += FString::Printf(TEXT("%s, %s, %d, %d\n"), *Pair.Key, *Entry.UniformBufferName, Entry.Type, Entry.ResourceIndex);
		}
		Line += TEXT("NULL, NULL, 0, 0\n");

		Line += TEXT("#endif /*END_RESOURCE_TABLES*/\n");
		return Line;
	}

	void CreateEnvironmentFromResourceTable(const FString& String, FShaderCompilerEnvironment& OutEnvironment)
	{
		FString Prolog = TEXT("#if 0 /*BEGIN_RESOURCE_TABLES*/");
		int32 FoundBegin = String.Find(Prolog, ESearchCase::CaseSensitive);
		if (FoundBegin == INDEX_NONE)
		{
			return;
		}
		int32 FoundEnd = String.Find(TEXT("#endif /*END_RESOURCE_TABLES*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FoundBegin);
		if (FoundEnd == INDEX_NONE)
		{
			return;
		}

		// +1 for EOL
		const TCHAR* Ptr = &String[FoundBegin + 1 + Prolog.Len()];
		while (*Ptr == '\r' || *Ptr == '\n')
		{
			++Ptr;
		}
		const TCHAR* PtrEnd = &String[FoundEnd];
		while (Ptr < PtrEnd)
		{
			FString UB;
			if (!CrossCompiler::ParseIdentifier(Ptr, UB))
			{
				return;
			}
			if (!CrossCompiler::Match(Ptr, TEXT(", ")))
			{
				return;
			}
			int32 Hash;
			if (!CrossCompiler::ParseSignedNumber(Ptr, Hash))
			{
				return;
			}
			// Optional \r
			CrossCompiler::Match(Ptr, '\r');
			if (!CrossCompiler::Match(Ptr, '\n'))
			{
				return;
			}

			if (UB == TEXT("NULL") && Hash == 0)
			{
				break;
			}

			FUniformBufferEntry& UniformBufferEntry = OutEnvironment.UniformBufferMap.FindOrAdd(UB);
			UniformBufferEntry.LayoutHash = (uint32)Hash;
		}

		while (Ptr < PtrEnd)
		{
			FString Name;
			if (!CrossCompiler::ParseIdentifier(Ptr, Name))
			{
				return;
			}
			if (!CrossCompiler::Match(Ptr, TEXT(", ")))
			{
				return;
			}
			FString UB;
			if (!CrossCompiler::ParseIdentifier(Ptr, UB))
			{
				return;
			}
			if (!CrossCompiler::Match(Ptr, TEXT(", ")))
			{
				return;
			}
			int32 Type;
			if (!CrossCompiler::ParseSignedNumber(Ptr, Type))
			{
				return;
			}
			if (!CrossCompiler::Match(Ptr, TEXT(", ")))
			{
				return;
			}
			int32 ResourceIndex;
			if (!CrossCompiler::ParseSignedNumber(Ptr, ResourceIndex))
			{
				return;
			}
			// Optional
			CrossCompiler::Match(Ptr, '\r');
			if (!CrossCompiler::Match(Ptr, '\n'))
			{
				return;
			}

			if (Name == TEXT("NULL") && UB == TEXT("NULL") && Type == 0 && ResourceIndex == 0)
			{
				break;
			}
			FResourceTableEntry& Entry = OutEnvironment.ResourceTableMap.FindOrAdd(Name);
			Entry.UniformBufferName = UB;
			Entry.Type = Type;
			Entry.ResourceIndex = ResourceIndex;
		}
	}

	/**
	 * Parse an error emitted by the HLSL cross-compiler.
	 * @param OutErrors - Array into which compiler errors may be added.
	 * @param InLine - A line from the compile log.
	 */
	void ParseHlslccError(TArray<FShaderCompilerError>& OutErrors, const FString& InLine, bool bUseAbsolutePaths)
	{
		const TCHAR* p = *InLine;
		FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();

		// Copy the filename.
		while (*p && *p != TEXT('('))
		{
			Error->ErrorVirtualFilePath += (*p++);
		}

		if (!bUseAbsolutePaths)
		{
			Error->ErrorVirtualFilePath = ParseVirtualShaderFilename(Error->ErrorVirtualFilePath);
		}
		p++;

		// Parse the line number.
		int32 LineNumber = 0;
		while (*p && *p >= TEXT('0') && *p <= TEXT('9'))
		{
			LineNumber = 10 * LineNumber + (*p++ - TEXT('0'));
		}
		Error->ErrorLineString = *FString::Printf(TEXT("%d"), LineNumber);

		// Skip to the warning message.
		while (*p && (*p == TEXT(')') || *p == TEXT(':') || *p == TEXT(' ') || *p == TEXT('\t')))
		{
			p++;
		}
		Error->StrippedErrorMessage = p;
	}


	/** Map shader frequency -> string for messages. */
	static const TCHAR* FrequencyStringTable[] =
	{
		TEXT("Vertex"),
		TEXT("Mesh"),
		TEXT("Amplification"),
		TEXT("Pixel"),
		TEXT("Geometry"),
		TEXT("Compute"),
		TEXT("RayGen"),
		TEXT("RayMiss"),
		TEXT("RayHitGroup"),
		TEXT("RayCallable"),
	};

	/** Compile time check to verify that the GL mapping tables are up-to-date. */
	static_assert(SF_NumFrequencies == UE_ARRAY_COUNT(FrequencyStringTable), "NumFrequencies changed. Please update tables.");

	const TCHAR* GetFrequencyName(EShaderFrequency Frequency)
	{
		check((int32)Frequency >= 0 && Frequency < SF_NumFrequencies);
		return FrequencyStringTable[Frequency];
	}

	FHlslccHeader::FHlslccHeader() :
		Name(TEXT(""))
	{
		NumThreads[0] = NumThreads[1] = NumThreads[2] = 0;
	}

	bool FHlslccHeader::Read(const ANSICHAR*& ShaderSource, int32 SourceLen)
	{
#define DEF_PREFIX_STR(Str) \
		static const ANSICHAR* Str##Prefix = "// @" #Str ": "; \
		static const int32 Str##PrefixLen = FCStringAnsi::Strlen(Str##Prefix)
		DEF_PREFIX_STR(Inputs);
		DEF_PREFIX_STR(Outputs);
		DEF_PREFIX_STR(UniformBlocks);
		DEF_PREFIX_STR(Uniforms);
		DEF_PREFIX_STR(PackedGlobals);
		DEF_PREFIX_STR(PackedUB);
		DEF_PREFIX_STR(PackedUBCopies);
		DEF_PREFIX_STR(PackedUBGlobalCopies);
		DEF_PREFIX_STR(Samplers);
		DEF_PREFIX_STR(UAVs);
		DEF_PREFIX_STR(SamplerStates);
		DEF_PREFIX_STR(AccelerationStructures);
		DEF_PREFIX_STR(NumThreads);
#undef DEF_PREFIX_STR

		// Skip any comments that come before the signature.
		while (FCStringAnsi::Strncmp(ShaderSource, "//", 2) == 0 &&
			FCStringAnsi::Strncmp(ShaderSource + 2, " !", 2) != 0 &&
			FCStringAnsi::Strncmp(ShaderSource + 2, " @", 2) != 0)
		{
			ShaderSource += 2;
			while (*ShaderSource && *ShaderSource++ != '\n')
			{
				// Do nothing
			}
		}

		// Read shader name if any
		if (FCStringAnsi::Strncmp(ShaderSource, "// !", 4) == 0)
		{
			ShaderSource += 4;
			while (*ShaderSource && *ShaderSource != '\n')
			{
				Name += (TCHAR)*ShaderSource;
				++ShaderSource;
			}

			if (*ShaderSource == '\n')
			{
				++ShaderSource;
			}
		}

		// Skip any comments that come before the signature.
		while (FCStringAnsi::Strncmp(ShaderSource, "//", 2) == 0 &&
			FCStringAnsi::Strncmp(ShaderSource + 2, " @", 2) != 0)
		{
			ShaderSource += 2;
			while (*ShaderSource && *ShaderSource++ != '\n')
			{
				// Do nothing
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, InputsPrefix, InputsPrefixLen) == 0)
		{
			ShaderSource += InputsPrefixLen;

			if (!ReadInOut(ShaderSource, Inputs))
			{
				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, OutputsPrefix, OutputsPrefixLen) == 0)
		{
			ShaderSource += OutputsPrefixLen;

			if (!ReadInOut(ShaderSource, Outputs))
			{
				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, UniformBlocksPrefix, UniformBlocksPrefixLen) == 0)
		{
			ShaderSource += UniformBlocksPrefixLen;

			while (*ShaderSource && *ShaderSource != '\n')
			{
				FAttribute UniformBlock;
				if (!ParseIdentifier(ShaderSource, UniformBlock.Name))
				{
					return false;
				}

				if (!Match(ShaderSource, '('))
				{
					return false;
				}
				
				if (!ParseIntegerNumber(ShaderSource, UniformBlock.Index))
				{
					return false;
				}

				if (!Match(ShaderSource, ')'))
				{
					return false;
				}

				UniformBlocks.Add(UniformBlock);

				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				if (Match(ShaderSource, ','))
				{
					continue;
				}
			
				//#todo-rco: Need a log here
				//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, UniformsPrefix, UniformsPrefixLen) == 0)
		{
			// @todo-mobile: Will we ever need to support this code path?
			check(0);
			return false;
/*
			ShaderSource += UniformsPrefixLen;

			while (*ShaderSource && *ShaderSource != '\n')
			{
				uint16 ArrayIndex = 0;
				uint16 Offset = 0;
				uint16 NumComponents = 0;

				FString ParameterName = ParseIdentifier(ShaderSource);
				verify(ParameterName.Len() > 0);
				verify(Match(ShaderSource, '('));
				ArrayIndex = ParseNumber(ShaderSource);
				verify(Match(ShaderSource, ':'));
				Offset = ParseNumber(ShaderSource);
				verify(Match(ShaderSource, ':'));
				NumComponents = ParseNumber(ShaderSource);
				verify(Match(ShaderSource, ')'));

				ParameterMap.AddParameterAllocation(
					*ParameterName,
					ArrayIndex,
					Offset * BytesPerComponent,
					NumComponents * BytesPerComponent
					);

				if (ArrayIndex < OGL_NUM_PACKED_UNIFORM_ARRAYS)
				{
					PackedUniformSize[ArrayIndex] = FMath::Max<uint16>(
						PackedUniformSize[ArrayIndex],
						BytesPerComponent * (Offset + NumComponents)
						);
				}

				// Skip the comma.
				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				verify(Match(ShaderSource, ','));
			}

			Match(ShaderSource, '\n');
*/
		}

		// @PackedGlobals: Global0(h:0,1),Global1(h:4,1),Global2(h:8,1)
		if (FCStringAnsi::Strncmp(ShaderSource, PackedGlobalsPrefix, PackedGlobalsPrefixLen) == 0)
		{
			ShaderSource += PackedGlobalsPrefixLen;
			while (*ShaderSource && *ShaderSource != '\n')
			{
				FPackedGlobal PackedGlobal;
				if (!ParseIdentifier(ShaderSource, PackedGlobal.Name))
				{
					return false;
				}

				if (!Match(ShaderSource, '('))
				{
					return false;
				}

				PackedGlobal.PackedType = *ShaderSource++;

				if (!Match(ShaderSource, ':'))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, PackedGlobal.Offset))
				{
					return false;
				}

				if (!Match(ShaderSource, ','))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, PackedGlobal.Count))
				{
					return false;
				}

				if (!Match(ShaderSource, ')'))
				{
					return false;
				}

				PackedGlobals.Add(PackedGlobal);

				// Break if EOL
				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				// Has to be a comma!
				if (Match(ShaderSource, ','))
				{
					continue;
				}

				//#todo-rco: Need a log here
				//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
				return false;
			}
		}

		// Packed Uniform Buffers (Multiple lines)
		// @PackedUB: CBuffer(0): CBMember0(0,1),CBMember1(1,1)
		while (FCStringAnsi::Strncmp(ShaderSource, PackedUBPrefix, PackedUBPrefixLen) == 0)
		{
			ShaderSource += PackedUBPrefixLen;

			FPackedUB PackedUB;

			if (!ParseIdentifier(ShaderSource, PackedUB.Attribute.Name))
			{
				return false;
			}

			if (!Match(ShaderSource, '('))
			{
				return false;
			}
			
			if (!ParseIntegerNumber(ShaderSource, PackedUB.Attribute.Index))
			{
				return false;
			}

			if (!Match(ShaderSource, ')'))
			{
				return false;
			}

			if (!Match(ShaderSource, ':'))
			{
				return false;
			}

			if (!Match(ShaderSource, ' '))
			{
				return false;
			}

			while (*ShaderSource && *ShaderSource != '\n')
			{
				FPackedUB::FMember Member;
				ParseIdentifier(ShaderSource, Member.Name);
				if (!Match(ShaderSource, '('))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, Member.Offset))
				{
					return false;
				}
				
				if (!Match(ShaderSource, ','))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, Member.Count))
				{
					return false;
				}

				if (!Match(ShaderSource, ')'))
				{
					return false;
				}

				PackedUB.Members.Add(Member);

				// Break if EOL
				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				// Has to be a comma!
				if (Match(ShaderSource, ','))
				{
					continue;
				}

				//#todo-rco: Need a log here
				//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
				return false;
			}

			PackedUBs.Add(PackedUB);
		}

		// @PackedUBCopies: 0:0-0:h:0:1,0:1-0:h:4:1,1:0-1:h:0:1
		if (FCStringAnsi::Strncmp(ShaderSource, PackedUBCopiesPrefix, PackedUBCopiesPrefixLen) == 0)
		{
			ShaderSource += PackedUBCopiesPrefixLen;
			if (!ReadCopies(ShaderSource, false, PackedUBCopies))
			{
				return false;
			}
		}

		// @PackedUBGlobalCopies: 0:0-h:12:1,0:1-h:16:1,1:0-h:20:1
		if (FCStringAnsi::Strncmp(ShaderSource, PackedUBGlobalCopiesPrefix, PackedUBGlobalCopiesPrefixLen) == 0)
		{
			ShaderSource += PackedUBGlobalCopiesPrefixLen;
			if (!ReadCopies(ShaderSource, true, PackedUBGlobalCopies))
			{
				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, SamplersPrefix, SamplersPrefixLen) == 0)
		{
			ShaderSource += SamplersPrefixLen;

			while (*ShaderSource && *ShaderSource != '\n')
			{
				FSampler Sampler;

				if (!ParseIdentifier(ShaderSource, Sampler.Name))
				{
					return false;
				}

				if (!Match(ShaderSource, '('))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, Sampler.Offset))
				{
					return false;
				}

				if (!Match(ShaderSource, ':'))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, Sampler.Count))
				{
					return false;
				}

				if (Match(ShaderSource, '['))
				{
					// Sampler States
					do
					{
						FString SamplerState;
						
						if (!ParseIdentifier(ShaderSource, SamplerState))
						{
							return false;
						}

						Sampler.SamplerStates.Add(SamplerState);
					}
					while (Match(ShaderSource, ','));

					if (!Match(ShaderSource, ']'))
					{
						return false;
					}
				}

				if (!Match(ShaderSource, ')'))
				{
					return false;
				}

				Samplers.Add(Sampler);

				// Break if EOL
				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				// Has to be a comma!
				if (Match(ShaderSource, ','))
				{
					continue;
				}

				//#todo-rco: Need a log here
				//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, UAVsPrefix, UAVsPrefixLen) == 0)
		{
			ShaderSource += UAVsPrefixLen;

			while (*ShaderSource && *ShaderSource != '\n')
			{
				FUAV UAV;

				if (!ParseIdentifier(ShaderSource, UAV.Name))
				{
					return false;
				}

				if (!Match(ShaderSource, '('))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, UAV.Offset))
				{
					return false;
				}

				if (!Match(ShaderSource, ':'))
				{
					return false;
				}

				if (!ParseIntegerNumber(ShaderSource, UAV.Count))
				{
					return false;
				}

				if (!Match(ShaderSource, ')'))
				{
					return false;
				}

				UAVs.Add(UAV);

				// Break if EOL
				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				// Has to be a comma!
				if (Match(ShaderSource, ','))
				{
					continue;
				}

				//#todo-rco: Need a log here
				//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, SamplerStatesPrefix, SamplerStatesPrefixLen) == 0)
		{
			ShaderSource += SamplerStatesPrefixLen;
			while (*ShaderSource && *ShaderSource != '\n')
			{
				FAttribute SamplerState;
				if (!ParseIntegerNumber(ShaderSource, SamplerState.Index))
				{
					return false;
				}

				if (!Match(ShaderSource, ':'))
				{
					return false;
				}

				if (!ParseIdentifier(ShaderSource, SamplerState.Name))
				{
					return false;
				}

				SamplerStates.Add(SamplerState);

				// Break if EOL
				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				// Has to be a comma!
				if (Match(ShaderSource, ','))
				{
					continue;
				}

				//#todo-rco: Need a log here
				//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, AccelerationStructuresPrefix, AccelerationStructuresPrefixLen) == 0)
		{
			ShaderSource += AccelerationStructuresPrefixLen;

			while (*ShaderSource && *ShaderSource != '\n')
			{
				FAccelerationStructure AccelerationStructure;

				if (!ParseIntegerNumber(ShaderSource, AccelerationStructure.Offset))
				{
					return false;
				}

				if (!Match(ShaderSource, ':'))
				{
					return false;
				}

				if (!ParseIdentifier(ShaderSource, AccelerationStructure.Name))
				{
					return false;
				}

				AccelerationStructures.Add(AccelerationStructure);

				if (Match(ShaderSource, '\n'))
				{
					break;
				}

				if (Match(ShaderSource, ','))
				{
					continue;
				}

				return false;
			}
		}

		if (FCStringAnsi::Strncmp(ShaderSource, NumThreadsPrefix, NumThreadsPrefixLen) == 0)
		{
			ShaderSource += NumThreadsPrefixLen;
			if (!ParseIntegerNumber(ShaderSource, NumThreads[0]))
			{
				return false;
			}
			if (!Match(ShaderSource, ','))
			{
				return false;
			}

			if (!Match(ShaderSource, ' '))
			{
				return false;
			}

			if (!ParseIntegerNumber(ShaderSource, NumThreads[1]))
			{
				return false;
			}

			if (!Match(ShaderSource, ','))
			{
				return false;
			}

			if (!Match(ShaderSource, ' '))
			{
				return false;
			}

			if (!ParseIntegerNumber(ShaderSource, NumThreads[2]))
			{
				return false;
			}

			if (!Match(ShaderSource, '\n'))
			{
				return false;
			}
		}
	
		return ParseCustomHeaderEntries(ShaderSource);
	}

	bool FHlslccHeader::ReadCopies(const ANSICHAR*& ShaderSource, bool bGlobals, TArray<FPackedUBCopy>& OutCopies)
	{
		while (*ShaderSource && *ShaderSource != '\n')
		{
			FPackedUBCopy PackedUBCopy;
			PackedUBCopy.DestUB = 0;

			if (!ParseIntegerNumber(ShaderSource, PackedUBCopy.SourceUB))
			{
				return false;
			}

			if (!Match(ShaderSource, ':'))
			{
				return false;
			}

			if (!ParseIntegerNumber(ShaderSource, PackedUBCopy.SourceOffset))
			{
				return false;
			}

			if (!Match(ShaderSource, '-'))
			{
				return false;
			}

			if (!bGlobals)
			{
				if (!ParseIntegerNumber(ShaderSource, PackedUBCopy.DestUB))
				{
					return false;
				}

				if (!Match(ShaderSource, ':'))
				{
					return false;
				}
			}

			PackedUBCopy.DestPackedType = *ShaderSource++;

			if (!Match(ShaderSource, ':'))
			{
				return false;
			}

			if (!ParseIntegerNumber(ShaderSource, PackedUBCopy.DestOffset))
			{
				return false;
			}

			if (!Match(ShaderSource, ':'))
			{
				return false;
			}

			if (!ParseIntegerNumber(ShaderSource, PackedUBCopy.Count))
			{
				return false;
			}

			OutCopies.Add(PackedUBCopy);

			// Break if EOL
			if (Match(ShaderSource, '\n'))
			{
				break;
			}

			// Has to be a comma!
			if (Match(ShaderSource, ','))
			{
				continue;
			}

			//#todo-rco: Need a log here
			//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
			return false;
		}

		return true;
	}

	bool FHlslccHeader::ReadInOut(const ANSICHAR*& ShaderSource, TArray<FInOut>& OutAttributes)
	{
		while (*ShaderSource && *ShaderSource != '\n')
		{
			FInOut Attribute;

			if (!ParseIdentifier(ShaderSource, Attribute.Type))
			{
				return false;
			}

			if (Match(ShaderSource, '['))
			{
				if (!ParseIntegerNumber(ShaderSource, Attribute.ArrayCount))
				{
					return false;
				}

				if (!Match(ShaderSource, ']'))
				{
					return false;
				}
			}
			else
			{
				Attribute.ArrayCount = 0;
			}

			if (Match(ShaderSource, ';'))
			{
				if (!ParseSignedNumber(ShaderSource, Attribute.Index))
				{
					return false;
				}
			}

			if (!Match(ShaderSource, ':'))
			{
				return false;
			}

			if (!ParseIdentifier(ShaderSource, Attribute.Name))
			{
				return false;
			}

			// Optional array suffix
			if (Match(ShaderSource, '['))
			{
				Attribute.Name += '[';
				while (*ShaderSource)
				{
					Attribute.Name += *ShaderSource;
					if (Match(ShaderSource, ']'))
					{
						break;
					}
					++ShaderSource;
				}
			}

			OutAttributes.Add(Attribute);

			// Break if EOL
			if (Match(ShaderSource, '\n'))
			{
				return true;
			}

			// Has to be a comma!
			if (Match(ShaderSource, ','))
			{
				continue;
			}

			//#todo-rco: Need a log here
			//UE_LOG(ShaderCompilerCommon, Warning, TEXT("Invalid char '%c'"), *ShaderSource);
			return false;
		}

		// Last character must be EOL
		return Match(ShaderSource, '\n');
	}

} // namespace CrossCompiler
