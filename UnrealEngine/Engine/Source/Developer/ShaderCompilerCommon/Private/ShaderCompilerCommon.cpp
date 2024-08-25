// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderCompilerCommon.h"
#include "ShaderParameterParser.h"
#include "Misc/Base64.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Modules/ModuleManager.h"
#include "HlslccDefinitions.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "String/RemoveFrom.h"
#include "ShaderPreprocessor.h"
#include "ShaderPreprocessTypes.h"
#include "ShaderSymbolExport.h"
#include "ShaderMinifier.h"
#include "Algo/Sort.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

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
	const FShaderResourceTableMap& ResourceTableMap,
	const TMap<FString, FUniformBufferEntry>& UniformBufferMap,
	TBitArray<>& UsedUniformBufferSlots,
	FShaderParameterMap& ParameterMap,
	FShaderCompilerResourceTable& OutSRT)
{
	check(OutSRT.ResourceTableBits == 0);
	check(OutSRT.ResourceTableLayoutHashes.Num() == 0);

	// Build resource table mapping
	int32 MaxBoundResourceTable = -1;

	// Go through ALL the members of ALL the UB resources
	for (const FUniformResourceEntry& Entry : ResourceTableMap.Resources)
	{
		const FString& Name = Entry.UniformBufferMemberName;

		// If the shaders uses this member (eg View_PerlinNoise3DTexture)...
		if (TOptional<FParameterAllocation> Allocation = ParameterMap.FindParameterAllocation(Name))
		{
			const EShaderParameterType ParameterType = Allocation->Type;
			const bool bBindlessParameter = IsParameterBindless(ParameterType);

			// Force bindless "indices" to zero since they're not needed in SetResourcesFromTables
			const uint16 BaseIndex = bBindlessParameter ? 0 : Allocation->BaseIndex;

			ParameterMap.RemoveParameterAllocation(*Name);

			uint16 UniformBufferIndex = INDEX_NONE;
			uint16 UBBaseIndex, UBSize;

			// Add the UB itself as a parameter if not there
			FString UniformBufferName(Entry.GetUniformBufferName());
			if (!ParameterMap.FindParameterAllocation(*UniformBufferName, UniformBufferIndex, UBBaseIndex, UBSize))
			{
				UniformBufferIndex = UsedUniformBufferSlots.FindAndSetFirstZeroBit();
				ParameterMap.AddParameterAllocation(*UniformBufferName, UniformBufferIndex,0,0,EShaderParameterType::UniformBuffer);
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
	for (const TPair<FString, FParameterAllocation>& KeyValue : ParameterMap.GetParameterMap())
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

PRAGMA_DISABLE_DEPRECATION_WARNINGS
// Deprecated version of function
bool BuildResourceTableMapping(
	const TMap<FString, FResourceTableEntry>& ResourceTableMap,
	const TMap<FString, FUniformBufferEntry>& UniformBufferMap,
	TBitArray<>& UsedUniformBufferSlots,
	FShaderParameterMap& ParameterMap,
	FShaderCompilerResourceTable& OutSRT)
{
	UE_LOG(LogShaders, Error, TEXT("Using unimplemented deprecated version of BuildResourceTableMapping -- use version that accepts FShaderResourceTableMap instead."));
	return false;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

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

template <typename CharType>
static bool IsSpaceOrTabOrEOL(CharType Char)
{
	return Char == ' ' || Char == '\t' || Char == '\n' || Char == '\r';
}

template <typename StrCharType, typename SearchCharType>
static const StrCharType* FindNextChar(const StrCharType* ReadStart, SearchCharType SearchChar)
{
	const StrCharType* SearchPtr = ReadStart;
	while (*SearchPtr && *SearchPtr != SearchChar)
	{
		SearchPtr++;
	}
	return SearchPtr;
}

template <typename CharType>
const CharType* FindNextWhitespace(const CharType* StringPtr)
{
	while (*StringPtr && !IsSpaceOrTabOrEOL(*StringPtr))
	{
		StringPtr++;
	}

	if (*StringPtr && IsSpaceOrTabOrEOL(*StringPtr))
	{
		return StringPtr;
	}
	else
	{
		return nullptr;
	}
}

template <typename CharType>
const CharType* FindNextNonWhitespace(const CharType* StringPtr)
{
	while (*StringPtr && IsSpaceOrTabOrEOL(*StringPtr))
	{
		StringPtr++;
	}

	if (*StringPtr && !IsSpaceOrTabOrEOL(*StringPtr))
	{
		return StringPtr;
	}

	return nullptr;
}

template <typename CharType>
const CharType* FindPreviousNonWhitespace(const CharType* StringPtr)
{
	do
	{
		StringPtr--;
	} while (*StringPtr && IsSpaceOrTabOrEOL(*StringPtr));

	if (*StringPtr && !IsSpaceOrTabOrEOL(*StringPtr))
	{
		return StringPtr;
	}

	return nullptr;
}

template <typename CharType>
const CharType* FindMatchingClosingParenthesis(const CharType* OpeningCharPtr)	{ return FindMatchingBlock<CharType>(OpeningCharPtr, '(', ')'); };

// See MSDN HLSL 'Symbol Name Restrictions' doc
template <typename CharType>
inline bool IsValidHLSLIdentifierCharacter(CharType Char)
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
			&& IsSpaceOrTabOrEOL(*TypeNameEndPtr)
			// If we found a '<', we must not accept any whitespace before it
			&& (!PotentialExtraTypeInfoPtr || *PotentialExtraTypeInfoPtr != '<' || TypeNameEndPtr > PotentialExtraTypeInfoPtr))
		{
			break;
		}

		TypeNameEndPtr++;
	}

	check(TypeNameEndPtr);
}

template<typename CharType, typename ViewType>
ViewType ParseHLSLSymbolName(const CharType* SearchString)
{
	const CharType* SymbolNameStartPtr = FindNextNonWhitespace(SearchString);
	check(SymbolNameStartPtr);

	const CharType* SymbolNameEndPtr = SymbolNameStartPtr;
	while (*SymbolNameEndPtr && IsValidHLSLIdentifierCharacter(*SymbolNameEndPtr))
	{
		SymbolNameEndPtr++;
	}

	return ViewType(SymbolNameStartPtr, SymbolNameEndPtr - SymbolNameStartPtr);
}

const TCHAR* ParseHLSLSymbolName(const TCHAR* SearchString, FString& SymbolName)
{
	FStringView Result = ParseHLSLSymbolName<TCHAR, FStringView>(SearchString);

	SymbolName = FString(Result);

	return Result.GetData() + Result.Len();
}

FStringView FindNextHLSLDefinitionOfType(FStringView Typename, FStringView StartPos)
{
	// handle both the case where identifier for declaration immediately precedes a ; and has whitespace separating the two
	const TCHAR* NextWhitespace;
	const TCHAR* NextNonWhitespace;
	FStringView SymbolName;

	NextWhitespace = FindNextWhitespace(StartPos.GetData());
	if (NextWhitespace == StartPos.GetData())
	{
		NextNonWhitespace = FindNextNonWhitespace(NextWhitespace);
		SymbolName = ParseHLSLSymbolName<TCHAR, FStringView>(NextNonWhitespace);	
		NextNonWhitespace = FindNextNonWhitespace(NextNonWhitespace + SymbolName.Len());
		if (NextNonWhitespace && (*NextNonWhitespace == ';'))
		{
			return SymbolName;
		}
	}
	return {};
}

FStringView UE::ShaderCompilerCommon::RemoveConstantBufferPrefix(FStringView InName)
{
	return UE::String::RemoveFromStart(InName, FStringView(UE::ShaderCompilerCommon::kUniformBufferConstantBufferPrefix));
}

FString UE::ShaderCompilerCommon::RemoveConstantBufferPrefix(const FString& InName)
{
	return FString(RemoveConstantBufferPrefix(FStringView(InName)));
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

void UE::ShaderCompilerCommon::ParseRayTracingEntryPoint(const FStringView& Input, FStringView& OutMain, FStringView& OutAnyHit, FStringView& OutIntersection)
{
	auto ParseEntry = [&Input](const FStringView& Marker)
	{
		FStringView Result;
		const int32 BeginIndex = UE::String::FindFirst(Input, Marker, ESearchCase::IgnoreCase);
		if (BeginIndex != INDEX_NONE)
		{
			int32 EndIndex = UE::String::FindFirst(Input.Mid(BeginIndex), TEXTVIEW(" "), ESearchCase::IgnoreCase);
			if (EndIndex == INDEX_NONE)
			{
				EndIndex = Input.Len() + 1;
			}
			else
			{
				EndIndex += BeginIndex;
			}
			const int32 MarkerLen = Marker.Len();
			const int32 Count = EndIndex - BeginIndex;
			Result = Input.Mid(BeginIndex + MarkerLen, Count - MarkerLen);
		}
		return Result;
	};

	OutMain = ParseEntry(TEXTVIEW("closesthit="));
	OutAnyHit = ParseEntry(TEXTVIEW("anyhit="));
	OutIntersection = ParseEntry(TEXTVIEW("intersection="));

	// If complex hit group entry is not specified, assume a single verbatim entry point
	if (OutMain.IsEmpty() && OutAnyHit.IsEmpty() && OutIntersection.IsEmpty())
	{
		OutMain = Input;
	}
}

void UE::ShaderCompilerCommon::ParseRayTracingEntryPoint(const FString& Input, FString& OutMain, FString& OutAnyHit, FString& OutIntersection)
{
	FStringView OutMainView;
	FStringView OutAnyHitView;
	FStringView OutIntersectionView;
	ParseRayTracingEntryPoint(Input, OutMainView, OutAnyHitView, OutIntersectionView);

	OutMain = OutMainView;
	OutAnyHit = OutAnyHitView;
	OutIntersection = OutIntersectionView;
}


bool UE::ShaderCompilerCommon::RemoveDeadCode(FShaderSource& InOutPreprocessedShaderSource, TConstArrayView<FStringView> InRequiredSymbols, TArray<FShaderCompilerError>& OutErrors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RemoveDeadCode);

	UE::ShaderMinifier::EMinifyShaderFlags ExtraFlags = UE::ShaderMinifier::EMinifyShaderFlags::None;

#if 0 // Extra features that may be useful during development / debugging
	ExtraFlags |= UE::ShaderMinifier::EMinifyShaderFlags::OutputReasons // Output a comment every struct/function describing why it was included (i.e. which code block uses it)
	           |  UE::ShaderMinifier::EMinifyShaderFlags::OutputStats;  // Output a comment detailing how many blocks of each type (functions/structs/etc.) were emitted
#endif

#if SHADER_SOURCE_ANSI
	TArray<FShaderSource::FStringType> ConvertedRequiredSymbols;
	TArray<FShaderSource::FViewType> RequiredSymbolViews;
	for (FStringView InSymbol : InRequiredSymbols)
	{
		FShaderSource::FStringType& ConvertedString = ConvertedRequiredSymbols.AddDefaulted_GetRef();
		ConvertedString.Append(InSymbol);
		RequiredSymbolViews.Add(FShaderSource::FViewType(ConvertedString));
	}
#else
	TConstArrayView<FStringView> RequiredSymbolViews = InRequiredSymbols;
#endif

	UE::ShaderMinifier::FMinifiedShader Minified  = UE::ShaderMinifier::Minify(InOutPreprocessedShaderSource, RequiredSymbolViews,
		  UE::ShaderMinifier::EMinifyShaderFlags::OutputCommentLines // Preserve comments that were left after preprocessing
		| UE::ShaderMinifier::EMinifyShaderFlags::OutputLines        // Emit #line directives
		| ExtraFlags);

	if (Minified.Success())
	{
		InOutPreprocessedShaderSource = MoveTemp(Minified.Code);
		return true;
	}
	else
	{
		OutErrors.Add(TEXT("warning: Shader minification failed."));
		return false;
	}
}

bool UE::ShaderCompilerCommon::RemoveDeadCode(FShaderSource& InOutPreprocessedShaderSource, const FString& EntryPoint, TArray<FShaderCompilerError>& OutErrors)
{
	return UE::ShaderCompilerCommon::RemoveDeadCode(InOutPreprocessedShaderSource, EntryPoint, {}, OutErrors);
}

bool UE::ShaderCompilerCommon::RemoveDeadCode(FShaderSource& InOutPreprocessedShaderSource, const FString& EntryPoint, TConstArrayView<FStringView> InRequiredSymbols, TArray<FShaderCompilerError>& OutErrors)
{
	TArray<FStringView> RequiredSymbols;

	FStringView EntryMain;
	FStringView EntryAnyHit;
	FStringView EntryIntersection;
	UE::ShaderCompilerCommon::ParseRayTracingEntryPoint(EntryPoint, EntryMain, EntryAnyHit, EntryIntersection);

	RequiredSymbols.Add(EntryMain);

	if (!EntryAnyHit.IsEmpty())
	{
		RequiredSymbols.Add(EntryAnyHit);
	}

	if (!EntryIntersection.IsEmpty())
	{
		RequiredSymbols.Add(EntryIntersection);
	}

	for (FStringView Symbol : InRequiredSymbols)
	{
		RequiredSymbols.Add(Symbol);
	}

	return UE::ShaderCompilerCommon::RemoveDeadCode(InOutPreprocessedShaderSource, RequiredSymbols, OutErrors);
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
	const EShaderParameterType ParameterType = FShaderParameterParser::ParseAndRemoveBindlessParameterPrefix(MemberName);

	Output.ParameterMap.AddParameterAllocation(
		*MemberName,
		ConstantBufferIndex,
		ReflectionOffset,
		ReflectionSize,
		ParameterType);
}

void HandleReflectedUniformBufferConstantBufferMember(
	int32 UniformBufferSlot,
	const FString& InMemberName,
	int32 ReflectionOffset,
	int32 ReflectionSize,
	FShaderCompilerOutput& Output
)
{
	FString MemberName = InMemberName;
	const EShaderParameterType ParameterType = FShaderParameterParser::ParseAndRemoveBindlessParameterPrefix(MemberName);

	if (ParameterType != EShaderParameterType::LooseData)
	{
		Output.ParameterMap.AddParameterAllocation(
			*MemberName,
			UniformBufferSlot,
			ReflectionOffset,
			1,
			ParameterType
		);
	}
}

void HandleReflectedRootConstantBufferMember(
	const FShaderCompilerInput& Input,
	const FShaderParameterParser& ShaderParameterParser,
	const FString& MemberName,
	int32 ReflectionOffset,
	int32 ReflectionSize,
	FShaderCompilerOutput& Output
)
{
	ShaderParameterParser.ValidateShaderParameterType(Input, MemberName, ReflectionOffset, ReflectionSize, Output);

	HandleReflectedUniformBufferConstantBufferMember(
		FShaderParametersMetadata::kRootCBufferBindingIndex,
		MemberName,
		ReflectionOffset,
		ReflectionSize,
		Output
	);
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

void UpdateStructuredBufferStride(
	const FShaderCompilerInput& Input,
	const FString& ResourceName,
	uint16 BindPoint,
	uint16 Stride,
	FShaderCompilerOutput& CompilerOutput
)
{
	if (BindPoint <= UINT16_MAX && Stride <= UINT16_MAX)
	{
		CompilerOutput.ParametersStrideToValidate.Add(FShaderCodeValidationStride{ BindPoint, Stride });
	}
	else
	{
		FString ErrorMessage = FString::Printf(TEXT("%s: Failed to set stride on parameter %s: Bind point %d, Stride %d"), *Input.GenerateShaderName(), *ResourceName, BindPoint, Stride);
		CompilerOutput.Errors.Add(FShaderCompilerError(*ErrorMessage));
	}
}

void AddShaderValidationSRVType(uint16 BindPoint,
							EShaderCodeResourceBindingType TypeDecl,
							FShaderCompilerOutput& CompilerOutput)
{
	if (BindPoint <= UINT16_MAX)
	{
		CompilerOutput.ParametersSRVTypeToValidate.Add(FShaderCodeValidationType{ BindPoint, TypeDecl });
	}
}

void AddShaderValidationUAVType(uint16 BindPoint,
							EShaderCodeResourceBindingType TypeDecl,
							FShaderCompilerOutput& CompilerOutput)
{
	if (BindPoint <= UINT16_MAX)
	{
		CompilerOutput.ParametersUAVTypeToValidate.Add(FShaderCodeValidationType{ BindPoint, TypeDecl });
	}
}

void AddShaderValidationUBSize(uint16 BindPoint,
							uint32_t Size,
							FShaderCompilerOutput& CompilerOutput)
{
	if (BindPoint <= UINT16_MAX)
	{
		CompilerOutput.ParametersUBSizeToValidate.Add(FShaderCodeValidationUBSize{ BindPoint, Size });
	}
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
	check(!Member.bIsBindable);

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

struct FUniformBufferMemberInfo
{
	// eg View.WorldToClip
	FString NameAsStructMember;
	// eg View_WorldToClip
	FString GlobalName;
};

struct FUniformBufferInfo
{
	int32 DefinitionEndOffset;
	TArray<FUniformBufferMemberInfo> Members;
};

struct FUniformBufferMemberInfoNew
{
	// eg View.WorldToClip
	FShaderSource::FViewType NameAsStructMember;
	// eg View_WorldToClip
	FShaderSource::FViewType GlobalName;

	bool operator<(const FUniformBufferMemberInfoNew& Other)
	{
		if (NameAsStructMember.Len() != Other.NameAsStructMember.Len())
		{
			return NameAsStructMember.Len() < Other.NameAsStructMember.Len();
		}
		else
		{
			return NameAsStructMember.Compare(Other.NameAsStructMember, ESearchCase::CaseSensitive) < 0;
		}
	}
};

// Index and count of subset of members
struct FUniformBufferMemberView
{
	int32 MemberOffset;
	int32 MemberCount;
};

struct FUniformBufferInfoNew
{
	FShaderSource::FViewType Name;
	int32 NextWithSameLength;							// Linked list of uniform buffer infos with same name length
	TArray<FUniformBufferMemberInfoNew> Members;		// Members sorted by length
	TArray<FUniformBufferMemberView> MembersByLength;	// Offset and count of members of a given length
};

// Tracks the offset and length of commented out uniform buffer declarations in the source code, so we can compact them out
struct FUniformBufferSpan
{
	int32 Offset;
	int32 Length;
};

// Compacts spaces out of a compound identifier.  Returns the new end pointer of the compacted identifier.
// End and result pointers are exclusive (length of the string is End - Start).
static FShaderSource::CharType* CompactCompoundIdentifier(FShaderSource::CharType* Start, FShaderSource::CharType* End)
{
	// Find first whitespace in the identifier, if present
	FShaderSource::CharType* ReadChar;
	for (ReadChar = Start; ReadChar < End; ++ReadChar)
	{
		if (IsSpaceOrTabOrEOL(*ReadChar))
		{
			break;
		}
	}
	if (ReadChar == End)
	{
		// No whitespace, we're done!
		return End;
	}

	// Found some whitespace, so we need to compact the non-whitespace, swapping the whitespace to the end of the range
	// WriteChar here will be the first whitespace character that we need to compact into.
	FShaderSource::CharType* WriteChar = ReadChar;
	for (++ReadChar; ReadChar < End; ++ReadChar)
	{
		// If the current read character is non-whitespace, compact it down
		if (!IsSpaceOrTabOrEOL(*ReadChar))
		{
			Swap(*ReadChar, *WriteChar);
			WriteChar++;
		}
	}
	return WriteChar;
}

const FShaderSource::CharType* ParseUniformBufferDefinition(const FShaderSource::CharType* ReadStart, TArray<FUniformBufferInfoNew>& UniformBufferInfos, uint64 UniformBufferFilter[64], int32 UniformBuffersByLength[64])
{
	// TODO:  should we check for an existing item?  In my testing, there's only one uniform buffer declaration with a given name,
	// but the original code used a map, theoretically allowing for multiple.
	int32 InfoIndex = UniformBufferInfos.AddDefaulted();
	FUniformBufferInfoNew& Info = UniformBufferInfos[InfoIndex];

	Info.Name = ParseHLSLSymbolName<FShaderSource::CharType, FShaderSource::FViewType>(ReadStart);
	check(Info.Name.Len() < 64);

	const FShaderSource::CharType* OpeningBrace = FindNextChar(ReadStart, '{');
	const FShaderSource::CharType* ClosingBrace = FindMatchingClosingBrace(OpeningBrace + 1);

	const FShaderSource::CharType* CurrentParseStart = OpeningBrace + 1;
	const FShaderSource::CharType* NextSemicolon = FindNextChar(CurrentParseStart, ';');

	while (NextSemicolon < ClosingBrace)
	{
		const FShaderSource::CharType* NextSeparator = FindNextChar(CurrentParseStart, '=');
		if (NextSeparator < NextSemicolon)
		{
			const FShaderSource::CharType* StructStart = CurrentParseStart;
			const FShaderSource::CharType* StructEnd = NextSeparator - 1;

			const FShaderSource::CharType* GlobalStart = NextSeparator + 1;
			const FShaderSource::CharType* GlobalEnd = NextSemicolon - 1;

			while (IsSpaceOrTabOrEOL(*StructStart))
			{
				StructStart++;
			}
			while (IsSpaceOrTabOrEOL(*GlobalStart))
			{
				GlobalStart++;
			}

			StructEnd = CompactCompoundIdentifier(const_cast<FShaderSource::CharType*>(StructStart), const_cast<FShaderSource::CharType*>(StructEnd));
			GlobalEnd = CompactCompoundIdentifier(const_cast<FShaderSource::CharType*>(GlobalStart), const_cast<FShaderSource::CharType*>(GlobalEnd));

			FShaderSource::FViewType StructName(StructStart, StructEnd - StructStart);
			FShaderSource::FViewType GlobalName(GlobalStart, GlobalEnd - GlobalStart);

			// Avoid unnecessary conversions
			if (StructName.Len() == GlobalName.Len() && FShaderSource::FCStringType::Strncmp(StructName.GetData(), GlobalName.GetData(), StructName.Len()) != 0)
			{
				FUniformBufferMemberInfoNew NewMemberInfo;
				NewMemberInfo.NameAsStructMember = StructName;
				NewMemberInfo.GlobalName = GlobalName;

				// Need to be able to replace strings in place, so make sure GlobalName will fit in space of NameAsStructMember
				check(NewMemberInfo.NameAsStructMember.Len() >= NewMemberInfo.GlobalName.Len());

				Info.Members.Add(NewMemberInfo);
			}
		}

		CurrentParseStart = NextSemicolon + 1;
		NextSemicolon = FindNextChar(CurrentParseStart, ';');
	}

	const FShaderSource::CharType* EndPtr = ClosingBrace;

	// Skip to the end of the UniformBuffer
	while (*EndPtr && *EndPtr != ';')
	{
		EndPtr++;
	}

	if (Info.Members.Num())
	{
		// We have members.  Sort them.  Note that the sort is by length first, not alphabetical, so the last item will be the longest.
		Algo::Sort(Info.Members);

		int32 MaxLen = Info.Members.Last().NameAsStructMember.Len();

		// Initialize table with offset of first member with a given length, and the count of members of that length (going backwards so the
		// index of the first element of a given size is the last one written to "MemberOffset").
		Info.MembersByLength.SetNumZeroed(MaxLen + 1);

		for (int32 MemberIndex = Info.Members.Num() - 1; MemberIndex >= 0; MemberIndex--)
		{
			int32 CurrentMemberLen = Info.Members[MemberIndex].NameAsStructMember.Len();
			Info.MembersByLength[CurrentMemberLen].MemberOffset = MemberIndex;
			Info.MembersByLength[CurrentMemberLen].MemberCount++;
		}

		// Initialize the uniform buffer name filter.  The filter is a mask based on the first character of the name (minus 64 so valid token
		// starting characters which are in ASCII range 64..127 fit in 64 bits).  We can quickly check if a token of the given length and start
		// character might be one we care about.
		UniformBufferFilter[Info.Name.Len()] |= 1ull << (Info.Name[0] - 64);

		// Add to linked list of uniform buffers by name length
		Info.NextWithSameLength = UniformBuffersByLength[Info.Name.Len()];
		UniformBuffersByLength[Info.Name.Len()] = InfoIndex;
	}
	else
	{
		// If no members, we don't care about it
		UniformBufferInfos.RemoveAt(UniformBufferInfos.Num() - 1);
	}

	return EndPtr;
}

enum class AsciiFlags
{
	TerminatorOrSlash = (1 << 0),	// Null terminator OR slash (latter we care about for skipping commented out uniform blocks)
	Whitespace = (1 << 1),			// Includes other special characters below 32 (in addition to tab / newline)
	Other = (1 << 2),				// Anything else not one of the other types
	SymbolStart = (1<<3),			// Letters plus underscore (anything that can start a symbol)
	Digit = (1 << 4),
	Dot = (1 << 5),
	Quote = (1 << 6),
	Hash = (1 << 7),
};

static uint8 AsciiFlagTable[256] =
{
	1,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,		// Treat all special characters as whitespace

	2,4,64,128,4,4,4,4,			// 34 == Quote  35 == Hash
	4,4,4,4,4,4,32,1,			// 46 == Dot    47 == Slash
	16,16,16,16,16,16,16,16,	// Digits 0-7
	16,16,4,4,4,4,4,4,			// Digits 8-9

	4,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,4,4,4,4,8,		// Upper case letters,  95 == Underscore
	4,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,8,8,8,8,8, 8,8,8,4,4,4,4,4,		// Lower case letters

	4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,		// Treat all non-ASCII characters as Other
	4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,
	4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,
	4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,
};

struct FCompoundIdentifierResult
{
	const FShaderSource::CharType* Identifier;			// Start of identifier
	const FShaderSource::CharType* IdentifierEnd;			// End of entire identifier
	const FShaderSource::CharType* IdentifierRootEnd;		// End of root token of identifier
};

// Searches for a "compound identifier" (series of symbol tokens separated by dots) that also passes the "RootIdentifierFilter".
// The filter is a mask table of valid identifier start characters indexed by identifier length.  Since identifier characters start
// with letters or underscore, we can store a 64-bit mask representing ASCII characters 64..127, as all valid start characters are
// in that range.  As an example, if "View" is a valid root identifier, RootIdentifierFilter[4] will have the bit ('V' - 64) set,
// and any other 4 character identifier that doesn't start with that letter can be skipped, saving overhead in the caller.
bool FindNextCompoundIdentifier(const FShaderSource::CharType*& Search, const uint64 RootIdentifierFilter[64], FCompoundIdentifierResult& OutResult)
{
	const FShaderSource::CharType* SearchChar = Search;
	uint8 SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];

	// Scanning loop
	while (1)
	{
		static constexpr uint8 AsciiFlagsEchoVerbatim = (uint8)AsciiFlags::Whitespace | (uint8)AsciiFlags::Other;
		static constexpr uint8 AsciiFlagsSymbol = (uint8)AsciiFlags::SymbolStart | (uint8)AsciiFlags::Digit;
		static constexpr uint8 AsciiFlagsStartNumberOrDirective = (uint8)AsciiFlags::Digit | (uint8)AsciiFlags::Dot | (uint8)AsciiFlags::Hash;
		static constexpr uint8 AsciiFlagsEndNumberOrDirective = (uint8)AsciiFlags::Whitespace | (uint8)AsciiFlags::Other | (uint8)AsciiFlags::Quote | (uint8)AsciiFlags::TerminatorOrSlash;

		// Conditions here are organized in expected order of frequency
		if (SearchCharFlag & AsciiFlagsEchoVerbatim)
		{
			SearchChar++;
			SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
		}
		else if (SearchCharFlag & (uint8)AsciiFlags::SymbolStart)
		{
			OutResult.Identifier = SearchChar;
			SearchChar++;
			while ((SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar]) & AsciiFlagsSymbol)
			{
				SearchChar++;
			}

			// Track end of our root identifier
			OutResult.IdentifierRootEnd = SearchChar;

			// Skip any whitespace before a potential dot
			while (SearchCharFlag & ((uint8)AsciiFlags::Whitespace))
			{
				SearchChar++;
				SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
			}

			// If we didn't find a dot, go back to initial scanning state
			if (!(SearchCharFlag & ((uint8)AsciiFlags::Dot)))
			{
				continue;
			}
			SearchChar++;
			SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];

			// Determine if this root identifier passes the filter.  If so, we'll continue to parse the rest of the identifier,
			// but then go back to scanning.  The mask in RootIdentifierFilter starts with ASCII character 64, as token start
			// characters are in the range [64..127].
			ptrdiff_t IdentifierRootLen = OutResult.IdentifierRootEnd - OutResult.Identifier;
			if (IdentifierRootLen >= 64 || !(RootIdentifierFilter[IdentifierRootLen] & (1ull << (*OutResult.Identifier - 64))))
			{
				// Clear this, marking that we didn't find a candidate root identifier
				OutResult.IdentifierRootEnd = nullptr;
			}

			// Skip any whitespace after dot
			while (SearchCharFlag & ((uint8)AsciiFlags::Whitespace))
			{
				SearchChar++;
				SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
			}

			// Check for the start of another symbol after the dot -- if it's not a symbol, switch back to scanning -- some kind of incorrect code
			if (!(SearchCharFlag & (uint8)AsciiFlags::SymbolStart))
			{
				continue;
			}

			// Repeatedly scan for additional parts of the identifier separated by dots
			while (1)
			{
				SearchChar++;
				while ((SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar]) & AsciiFlagsSymbol)
				{
					SearchChar++;
				}

				// Track that this may be the end of the identifier (if there's not more dot separated tokens)
				OutResult.IdentifierEnd = SearchChar;

				// Skip any whitespace before a potential dot
				while (SearchCharFlag & ((uint8)AsciiFlags::Whitespace))
				{
					SearchChar++;
					SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
				}

				// If we found something other than a dot, we're done!
				if (!(SearchCharFlag & ((uint8)AsciiFlags::Dot)))
				{
					// Is the root token for this identifier a candidate based on the filter?
					if (OutResult.IdentifierRootEnd)
					{
						Search = SearchChar;
						return true;
					}
					else
					{
						// If not, go back to initial scanning state
						break;
					}
				}

				// Skip the dot
				SearchChar++;
				SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];

				// Skip any whitespace after dot
				while (SearchCharFlag & ((uint8)AsciiFlags::Whitespace))
				{
					SearchChar++;
					SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
				}

				// Did we find the start of another symbol after the dot?  If not, break out, some kind of invalid code...
				if (!(SearchCharFlag & (uint8)AsciiFlags::SymbolStart))
				{
					break;
				}
			}
		}
		else if (SearchCharFlag & AsciiFlagsStartNumberOrDirective)
		{
			// Number or directive, skip to Whitespace, Other, or Quote (numbers may contain letters or #, i.e. "1.#INF" for infinity, or "e" for an exponent)
			SearchChar++;
			while (!((SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar]) & AsciiFlagsEndNumberOrDirective))
			{
				SearchChar++;
			}
		}
		else if (SearchCharFlag & (uint8)AsciiFlags::Quote)
		{
			// Quote, skip to next Quote (or maybe end of string if text is malformed), ignoring the quote if it's escaped
			SearchChar++;
			while (*SearchChar && (*SearchChar != '\"' || *(SearchChar - 1) == '\\'))
			{
				SearchChar++;
			}

			// Could be end of string or the quote -- skip over the quote if not the null terminator
			if (*SearchChar)
			{
				SearchChar++;
			}
			SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
		}
		// Must be null terminator or slash at this point -- we've tested all other possibilities
		else if (*SearchChar == '/')
		{
			// Check if this is a commented out block (typically a commented out uniform declaration) and skip over it.
			// If the text is bad, there could be a /* right at the end of the string, so we need to check there is at least
			// one more character.
			if (SearchChar[1] == '*' && SearchChar[2] != 0)
			{
				// Search for slash (or end of string), starting at SearchChar + 3.  If we find a slash, we'll check the previous
				// character to see if it's the end of the comment.  Starting at +3 is necessary to avoid matching a slash as the
				// first character of the comment, i.e. "/*/".
				SearchChar += 3;

				while (1)
				{
					while ((SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar]) != (uint8)AsciiFlags::TerminatorOrSlash)
					{
						SearchChar++;
					}

					// Is this the end of the comment?
					if (*(SearchChar - 1) == '*')
					{
						if (*SearchChar)
						{
							SearchChar++;
							SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
							break;
						}
					}
					else
					{
						// More characters, continue the comment scanning loop, or if somehow at end of string, return false...
						if (*SearchChar)
						{
							SearchChar++;
						}
						else
						{
							return false;
						}
					}
				}
			}
			else
			{
				// Just a slash, not part of a block comment
				SearchChar++;
				SearchCharFlag = AsciiFlagTable[(uint8)*SearchChar];
			}
		}
		else
		{
			// End of string
			Search = SearchChar;
			return false;
		}
	}
}

FShaderSource::CharType* FindNextUniformBufferDefinition(FShaderSource::CharType* SearchPtr, FShaderSource::CharType* SourceStart, FShaderSource::FViewType UniformBufferStructIdentifier)
{
	while (SearchPtr)
	{
		SearchPtr = FShaderSource::FCStringType::Strstr(SearchPtr, UniformBufferStructIdentifier.GetData());

		if (SearchPtr)
		{
			if (SearchPtr > SourceStart && IsSpaceOrTabOrEOL(*(SearchPtr - 1)) && IsSpaceOrTabOrEOL(*(SearchPtr + UniformBufferStructIdentifier.Len())))
			{
				break;
			}
			else
			{
				SearchPtr = SearchPtr + 1;
			}
		}
	}
	return SearchPtr;
}

const FShaderSource::CharType* FindPreviousDot(const FShaderSource::CharType* SearchPtr, const FShaderSource::CharType* SearchMin)
{
	while ((SearchPtr > SearchMin) && (*SearchPtr != '.'))
	{
		SearchPtr--;
	}
	return SearchPtr;
}

// The cross compiler doesn't yet support struct initializers needed to construct static structs for uniform buffers
// Replace all uniform buffer struct member references (View.WorldToClip) with a flattened name that removes the struct dependency (View_WorldToClip)
void CleanupUniformBufferCode(const FShaderCompilerEnvironment& Environment, FShaderSource& PreprocessedShaderSource)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CleanupUniformBufferCode);

	TArray<FUniformBufferInfoNew> UniformBufferInfos;
	TArray<FUniformBufferSpan> UniformBufferSpans;
	uint64 UniformBufferFilter[64] = { 0 };			// A bit set for valid start characters for uniform buffer name of given length
	int32 UniformBuffersByLength[64];				// Linked list head index into UniformBufferInfos by length (connected by "NextWithSameLength")

	UniformBufferInfos.Reserve(Environment.UniformBufferMap.Num());
	UniformBufferSpans.Reserve(Environment.UniformBufferMap.Num());
	memset(UniformBuffersByLength, 0xff, sizeof(UniformBuffersByLength));

	FShaderSource::FViewType UniformBufferStructIdentifier = SHADER_SOURCE_VIEWLITERAL("UniformBuffer");

	FShaderSource::CharType* SourceStart = PreprocessedShaderSource.GetData();
	FShaderSource::CharType* SearchPtr = SourceStart;
	FShaderSource::CharType* EndOfPreviousUniformBuffer = SourceStart;
	bool bUniformBufferFound;

	do
	{
		// Find the next uniform buffer definition
		SearchPtr = FindNextUniformBufferDefinition(SearchPtr, SourceStart, UniformBufferStructIdentifier);

		if (SearchPtr)
		{
			// Track that we found a uniform buffer, and temporarily null terminate the string so we can parse to this point
			bUniformBufferFound = true;
			*SearchPtr = 0;
		}
		else
		{
			bUniformBufferFound = false;
		}

		// Parse the source between the last uniform buffer and the current uniform buffer (or potentially the end of the source if no more
		// were found).  If there are no uniform buffers yet, we don't need to parse anything.
		if (UniformBufferInfos.Num())
		{
			const FShaderSource::CharType* ParsePtr = EndOfPreviousUniformBuffer;

			FCompoundIdentifierResult Result;
			while (FindNextCompoundIdentifier(ParsePtr, UniformBufferFilter, Result))
			{
				// Check if the identifier corresponds to a uniform buffer
				FShaderSource::FViewType IdentifierRoot(Result.Identifier, Result.IdentifierRootEnd - Result.Identifier);
				for (int32 UniformInfoIndex = UniformBuffersByLength[IdentifierRoot.Len()]; UniformInfoIndex != INDEX_NONE; UniformInfoIndex = UniformBufferInfos[UniformInfoIndex].NextWithSameLength)
				{
					FUniformBufferInfoNew& Info = UniformBufferInfos[UniformInfoIndex];
					if (IdentifierRoot.Equals(Info.Name, ESearchCase::CaseSensitive))
					{
						// Found the uniform buffer, clean up potential whitespace
						Result.IdentifierEnd = CompactCompoundIdentifier(const_cast<FShaderSource::CharType*>(Result.Identifier), const_cast<FShaderSource::CharType*>(Result.IdentifierEnd));

						// Now try to find a matching member.  We need to check subsets of the full "identifier", to strip away function calls, components, or child structures.
						bool bMatchFound = false;

						for (; Result.IdentifierEnd > Result.IdentifierRootEnd; Result.IdentifierEnd = FindPreviousDot(Result.IdentifierEnd - 1, Result.IdentifierRootEnd))
						{
							FShaderSource::FViewType Identifier(Result.Identifier, Result.IdentifierEnd - Result.Identifier);
							if (Identifier.Len() < Info.MembersByLength.Num())
							{
								const FUniformBufferMemberView& MemberView = Info.MembersByLength[Identifier.Len()];

								for (int32 MemberIndex = MemberView.MemberOffset; MemberIndex < MemberView.MemberOffset + MemberView.MemberCount; MemberIndex++)
								{
									if (Info.Members[MemberIndex].NameAsStructMember.Equals(Identifier, ESearchCase::CaseSensitive))
									{
										bMatchFound = true;

										const int32 OriginalTextLen = Info.Members[MemberIndex].NameAsStructMember.Len();
										const int32 ReplacementTextLen = Info.Members[MemberIndex].GlobalName.Len();

										const FShaderSource::CharType* GlobalNameStart = GetData(Info.Members[MemberIndex].GlobalName);
										FShaderSource::CharType* IdentifierStart = const_cast<FShaderSource::CharType*>(Result.Identifier);

										int32 Index = 0;
										for (; Index < ReplacementTextLen; Index++)
										{
											IdentifierStart[Index] = GlobalNameStart[Index];
										}
										for (; Index < OriginalTextLen; Index++)
										{
											IdentifierStart[Index] = ' ';
										}
										break;
									}
								}

								if (bMatchFound)
								{
									break;
								}
							}
						}

						break;
					}
				}
			}
		}

		// Parse the current uniform buffer.
		if (bUniformBufferFound)
		{
			// Unterminate the string (put the first character of the struct identifier back in place) and parse it
			*SearchPtr = UniformBufferStructIdentifier[0];

			const FShaderSource::CharType* ConstStructEndPtr = ParseUniformBufferDefinition(SearchPtr + UniformBufferStructIdentifier.Len(), UniformBufferInfos, UniformBufferFilter, UniformBuffersByLength);
			FShaderSource::CharType* StructEndPtr = &SourceStart[ConstStructEndPtr - &SourceStart[0]];

			// Comment out the uniform buffer struct and initializer
			*SearchPtr = '/';
			*(SearchPtr + 1) = '*';
			*(StructEndPtr - 1) = '*';
			*StructEndPtr = '/';

			UniformBufferSpans.Add({ (int32)(SearchPtr - SourceStart), (int32)(StructEndPtr + 1 - SearchPtr) });

			EndOfPreviousUniformBuffer = StructEndPtr + 1;
			SearchPtr = StructEndPtr + 1;
		}

	} while (bUniformBufferFound);

	// Compact commented out uniform buffers out of the output source.  This costs around 10x less to do here than later in the minifier.  Note that
	// it's not necessary to add a line directive to fix up line numbers because uniform buffer declarations are always in generated files, and there
	// will be a line directive already there for the transition from the generated file back to whatever file included it.  The destination offset
	// for the first move is the start of the first uniform buffer declaration we are overwriting, then advances as characters are copied.
	int32 DestOffset = UniformBufferSpans.Num() ? UniformBufferSpans[0].Offset : PreprocessedShaderSource.Len();

	for (int32 SpanIndex = 0; SpanIndex < UniformBufferSpans.Num(); SpanIndex++)
	{
		// The source code we are compacting down is from the end of one span to the start of the next span, or end of the string.
		// We do not need to account for null terminator as the ShrinkToLen call below will null terminate for us.
		int32 SourceOffset = UniformBufferSpans[SpanIndex].Offset + UniformBufferSpans[SpanIndex].Length;
		int32 MoveCount = (SpanIndex < UniformBufferSpans.Num() - 1 ? UniformBufferSpans[SpanIndex + 1].Offset : PreprocessedShaderSource.Len()) - SourceOffset;

		check(DestOffset >= 0 && DestOffset < SourceOffset && SourceOffset + MoveCount <= PreprocessedShaderSource.Len());

		memmove(SourceStart + DestOffset, SourceStart + SourceOffset, MoveCount * sizeof(FShaderSource::CharType));
		DestOffset += MoveCount;
	}
	PreprocessedShaderSource.ShrinkToLen(DestOffset, EAllowShrinking::No);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FString CreateShaderCompilerWorkerDirectCommandLine(const FShaderCompilerInput& Input)
{
	FString Text(TEXT("-directcompile -format="));
	Text += Input.ShaderFormat.GetPlainNameString();
	Text += TEXT(" -entry=\"");
	Text += Input.EntryPointName;

	Text += TEXT("\" -shaderPlatformName=");
	Text += Input.ShaderPlatformName.GetPlainNameString();

	Text += FString::Printf(TEXT(" -supportedHardwareMask=%u"), Input.SupportedHardwareMask);

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

static uint32 Mali_ExtractNumberInstructions(const FString &MaliOutput)
{
	uint32 ReturnedNum = 0;

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
					ReturnedNum += (uint32)FMath::Max(0.0, ceil(fNrInstructions));
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

				FShaderCompilerError& NewError = ShaderOutput.Errors.AddDefaulted_GetRef();
				NewError.StrippedErrorMessage = TEXT("[Mali Offline Complier]\n") + StdErr;
			}
			else
			{
				FString Errors = Mali_ExtractErrors(StdOut);

				if (Errors.Len())
				{
					FShaderCompilerError& NewError = ShaderOutput.Errors.AddDefaulted_GetRef();
					NewError.StrippedErrorMessage = TEXT("[Mali Offline Complier]\n") + Errors;
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

// sensible default path size; TStringBuilder will allocate if it needs to
const FString GetDebugFileName(
	const FShaderCompilerInput& Input, 
	const UE::ShaderCompilerCommon::FDebugShaderDataOptions& Options, 
	const TCHAR* BaseFilename)
{
	TStringBuilder<512> PathBuilder;
	const TCHAR* Prefix = (Options.FilenamePrefix && *Options.FilenamePrefix) ? Options.FilenamePrefix : TEXT("");
	FStringView Filename = (BaseFilename && *BaseFilename) ? BaseFilename : Input.GetSourceFilenameView();
	FPathViews::Append(PathBuilder, Input.DumpDebugInfoPath, Prefix);
	PathBuilder << Filename;
	return PathBuilder.ToString();
}

namespace UE::ShaderCompilerCommon
{
	bool ExecuteShaderPreprocessingSteps(
		FShaderPreprocessOutput& PreprocessOutput,
		const FShaderCompilerInput& Input,
		const FShaderCompilerEnvironment& Environment,
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const FShaderCompilerDefinitions& AdditionalDefines
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBaseShaderFormat_PreprocessShader);

		if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::CompileFromDebugUSF))
		{
			// the "VirtualSourceFilePath" given is actually an absolute path to a dumped debug USF file; load it directly.
			// this occurs when running SCW in "direct compile" mode; this file will already be preprocessed.
			FString DebugUSF;
			bool bSuccess = FFileHelper::LoadFileToString(DebugUSF, *Input.VirtualSourceFilePath);

			if (bSuccess)
			{
				// const_cast for compile environment; need to populate a subset of environment parameters from parsing comments in the preprocessed code
				UE::ShaderCompilerCommon::SerializeEnvironmentFromBase64(const_cast<FShaderCompilerEnvironment&>(Input.Environment), DebugUSF);

				// strip comments from source when loading from a debug USF. some backends don't handle the comments that the debug dump inserts properly.
				TArray<ANSICHAR> Stripped;
				ShaderConvertAndStripComments(DebugUSF, Stripped);
				PreprocessOutput.EditSource().Set({ Stripped.GetData(), Stripped.Num() });
			}

			return bSuccess;
		}

		check(CheckVirtualShaderFilePath(Input.VirtualSourceFilePath));

		bool bSuccess = ::PreprocessShader(PreprocessOutput, Input, Environment, AdditionalDefines);
		if (bSuccess)
		{
			CleanupUniformBufferCode(Environment, PreprocessOutput.EditSource());

			if (Input.Environment.CompilerFlags.Contains(CFLAG_RemoveDeadCode))
			{
				const TArray<FStringView> RequiredSymbols(MakeArrayView(Input.RequiredSymbols));
				UE::ShaderCompilerCommon::RemoveDeadCode(PreprocessOutput.EditSource(), Input.EntryPointName, RequiredSymbols, PreprocessOutput.EditErrors());
			}
		}

		return bSuccess;
	}

	FString FDebugShaderDataOptions::GetDebugShaderPath(const FShaderCompilerInput& Input) const
	{
		return GetDebugFileName(Input, *this, OverrideBaseFilename);
	}

	bool FBaseShaderFormat::PreprocessShader(
		const FShaderCompilerInput& Input,
		const FShaderCompilerEnvironment& Environment,
		FShaderPreprocessOutput& PreprocessOutput) const
	{
		return ExecuteShaderPreprocessingSteps(PreprocessOutput, Input, Environment);
	}

	void FBaseShaderFormat::OutputDebugData(
		const FShaderCompilerInput& Input,
		const FShaderPreprocessOutput& PreprocessOutput,
		const FShaderCompilerOutput& Output) const
	{
		DumpExtendedDebugShaderData(Input, PreprocessOutput, Output);
	}

	void DumpDebugShaderData(const FShaderCompilerInput& Input, FStringView PreprocessedSource, const FDebugShaderDataOptions& Options)
	{
		if (!Input.DumpDebugInfoEnabled())
		{
			return;
		}

		FString Contents = UE::ShaderCompilerCommon::GetDebugShaderContents(Input, PreprocessedSource, Options);
		FFileHelper::SaveStringToFile(Contents, *Options.GetDebugShaderPath(Input));

		if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::DirectCompileCommandLine) && !Options.bSourceOnly)
		{
			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *GetDebugFileName(Input, Options, TEXT("DirectCompile.txt")));
		}
	}

	void DumpExtendedDebugShaderData(
		const FShaderCompilerInput& Input,
		const FShaderPreprocessOutput& PreprocessOutput,
		const FShaderCompilerOutput& Output,
		const FDebugShaderDataOptions& Options)
	{
		if (Input.bCachePreprocessed && EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::DetailedSource))
		{
			FDebugShaderDataOptions PrefixedOptions(Options);
			uint32 SlackLen = Options.FilenamePrefix ? FCString::Strlen(Options.FilenamePrefix) : 0;
			FString StrippedPrefix(TEXT("Stripped_"), SlackLen);
			FString PreprocessedPrefix(TEXT("Preprocessed_"), SlackLen);
			if (Options.FilenamePrefix)
			{
				StrippedPrefix += Options.FilenamePrefix;
				PreprocessedPrefix += Options.FilenamePrefix;
			}
			
			PrefixedOptions.FilenamePrefix = *StrippedPrefix;
			FFileHelper::SaveStringToFile(PreprocessOutput.GetSourceViewWide(), *PrefixedOptions.GetDebugShaderPath(Input));

			PrefixedOptions.FilenamePrefix = *PreprocessedPrefix;
			FFileHelper::SaveStringToFile(PreprocessOutput.GetUnstrippedSourceView(), *PrefixedOptions.GetDebugShaderPath(Input));
		}
		if (Output.ModifiedShaderSource.IsEmpty())
		{
			DumpDebugShaderData(Input, PreprocessOutput.GetSourceViewWide(), Options);
		}
		else
		{
			DumpDebugShaderData(Input, FStringView(Output.ModifiedShaderSource), Options);
		}
		FFileHelper::SaveStringToFile(Output.OutputHash.ToString(), *GetDebugFileName(Input, Options, TEXT("OutputHash.txt")), FFileHelper::EEncodingOptions::ForceAnsi);

		if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::Diagnostics))
		{
			FString Merged;
			for (const FShaderCompilerError& Diag : Output.Errors)
			{
				Merged += Diag.GetErrorStringWithLineMarker() + "\n";
			}
			if (!Merged.IsEmpty())
			{
				FFileHelper::SaveStringToFile(Merged, *GetDebugFileName(Input, Options, TEXT("Diagnostics.txt")), FFileHelper::EEncodingOptions::ForceAnsi);
			}
		}

		if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::InputHash))
		{
			FFileHelper::SaveStringToFile(LexToString(Input.Hash), *GetDebugFileName(Input, Options, TEXT("InputHash.txt")), FFileHelper::EEncodingOptions::ForceAnsi);
		}

		if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::ShaderCodeBinary))
		{
			FString ShaderCodeFileName = *GetDebugFileName(Input, Options, TEXT("ShaderCode.bin"));
			if (Output.ShaderCode.IsCompressed())
			{
				// always output decompressed code as it's slightly more useful for A/B comparisons
				TArray<uint8> DecompressedCode;
				DecompressedCode.SetNum(Output.ShaderCode.GetUncompressedSize());
				bool bSucceed = FCompression::UncompressMemory(NAME_Oodle, DecompressedCode.GetData(), DecompressedCode.Num(), Output.ShaderCode.GetReadAccess().GetData(), Output.ShaderCode.GetShaderCodeSize());
				FFileHelper::SaveArrayToFile(DecompressedCode, *ShaderCodeFileName);
			}
			else
			{
				FFileHelper::SaveArrayToFile(Output.ShaderCode.GetReadAccess(), *ShaderCodeFileName);
			}
		}

		for (const FDebugShaderDataOptions::FAdditionalOutput& AdditionalOutput : Options.AdditionalOutputs)
		{
			FFileHelper::SaveStringToFile(AdditionalOutput.Data, *GetDebugFileName(Input, Options, AdditionalOutput.BaseFileName), FFileHelper::EEncodingOptions::ForceAnsi);
		}
	}

	static const TCHAR* Base64EnvBegin = TEXT("/* BASE64_ENV\n");
	static const int32 Base64EnvBeginLen = FCString::Strlen(Base64EnvBegin);
	static const TCHAR* Base64EnvEnd = TEXT("\nBASE64_ENV */\n");
	
	FString SerializeEnvironmentToBase64(const FShaderCompilerEnvironment& Env)
	{
		TArray<uint8> Serialized;
		FMemoryWriter Ar(Serialized);
		const_cast<FShaderCompilerEnvironment&>(Env).SerializeCompilationDependencies(Ar);
		return FString::Printf(TEXT("%s%s%s"), Base64EnvBegin, *FBase64::Encode(Serialized), Base64EnvEnd);
	}

	void SerializeEnvironmentFromBase64(FShaderCompilerEnvironment& Env, const FString& DebugShaderSource)
	{
		int32 BeginIndex = DebugShaderSource.Find(Base64EnvBegin, ESearchCase::CaseSensitive);
		if (BeginIndex == INDEX_NONE)
		{
			return;
		}
		int32 EndIndex = DebugShaderSource.Find(Base64EnvEnd, ESearchCase::CaseSensitive, ESearchDir::FromStart, BeginIndex);
		if (EndIndex == INDEX_NONE)
		{
			return;
		}

		FString Base64Encoded = DebugShaderSource.Left(EndIndex).Mid(BeginIndex + Base64EnvBeginLen);

		TArray<uint8> Serialized;
		FBase64::Decode(Base64Encoded, Serialized);
		FMemoryReader Ar(Serialized);
		Env.SerializeCompilationDependencies(Ar);
	}

	FString GetDebugShaderContents(const FShaderCompilerInput& Input, FStringView PreprocessedSource, const FDebugShaderDataOptions& Options)
	{
		// If preprocessed cache is enabled, debug dump occurs in the cook process rather than the workers, and
		// in that case the env in Input.Environment has not been merged with the shared env. Do so here.
		FShaderCompilerEnvironment MergedEnvironment(Input.Environment);
		if (Input.bCachePreprocessed && IsValidRef(Input.SharedEnvironment))
		{
			MergedEnvironment.Merge(*Input.SharedEnvironment);
		}

		FString Contents = Options.AppendPreSource ? Options.AppendPreSource() : FString();

		if (Options.AppendPreSource)
		{
			Contents += Options.AppendPreSource();
		}

		Contents += PreprocessedSource;

		if (Options.AppendPostSource)
		{
			Contents += Options.AppendPostSource();
		}

		Contents += TEXT("\n");
		Contents += SerializeEnvironmentToBase64(MergedEnvironment);
		Contents += TEXT("/* DIRECT COMPILE\n");
		Contents += CreateShaderCompilerWorkerDirectCommandLine(Input);
		Contents += TEXT("\nDIRECT COMPILE */\n");
		if (!Input.DebugDescription.IsEmpty())
		{
			Contents += TEXT("//");
			Contents += Input.DebugDescription;
			Contents += TEXT("\n");
		}

		return Contents;
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
		for (const FUniformResourceEntry& Entry : Environment.ResourceTableMap.Resources)
		{
			Line += FString::Printf(TEXT("%s, %s, %d, %d\n"), Entry.UniformBufferMemberName, *FString(Entry.GetUniformBufferName()), Entry.Type, Entry.ResourceIndex);
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

			if (!UniformBufferEntry.MemberNameBuffer)
			{
				TArray<TCHAR>* MemberNameBuffer = new TArray<TCHAR>();
				UniformBufferEntry.MemberNameBuffer = MakeShareable(MemberNameBuffer);
			}
		}

		// Need to iterate through Uniform Buffer Map to add strings to correct MemberNameBuffer storage
		auto UniformBufferMapIt = OutEnvironment.UniformBufferMap.begin();
		
		// If we exit parse early due to error, we still want to fixup the string names for the members we found,
		// so the partial data isn't corrupt.
		struct FFixupOnExit
		{
			FFixupOnExit(FShaderCompilerEnvironment& OutEnvironment) : Environment(OutEnvironment) {}
			~FFixupOnExit()
			{
				Environment.ResourceTableMap.FixupOnLoad(Environment.UniformBufferMap);
			}
			
			FShaderCompilerEnvironment& Environment;
		};
		FFixupOnExit FixupOnExit(OutEnvironment);

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

			// Advance the uniform buffer map if this is a different UB name
			while (UniformBufferMapIt.Key() != UB)
			{
				++UniformBufferMapIt;
				if (UniformBufferMapIt == OutEnvironment.UniformBufferMap.end())
				{
					return;
				}
			}

			// Append the Name we parsed to the member name buffer
			TArray<TCHAR>& Buffer = *UniformBufferMapIt.Value().MemberNameBuffer.Get();
			uint32 MemberNameLength = Name.Len();
			
			Buffer.Append(*Name, MemberNameLength + 1);

			// The member name field of the entries is initialized at the end of parsing by the FixupOnLoad call from FFixupOnExit, so we can set it to nullptr here
			OutEnvironment.ResourceTableMap.Resources.Add({
				nullptr,
				(uint8)UB.Len(),
				(uint8)Type,
				(uint16)ResourceIndex
			});
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
		FShaderCompilerError& Error = OutErrors.AddDefaulted_GetRef();

		// Copy the filename.
		while (*p && *p != TEXT('('))
		{
			Error.ErrorVirtualFilePath += (*p++);
		}

		if (!bUseAbsolutePaths)
		{
			Error.ErrorVirtualFilePath = ParseVirtualShaderFilename(Error.ErrorVirtualFilePath);
		}
		p++;

		// Parse the line number.
		int32 LineNumber = 0;
		while (*p && *p >= TEXT('0') && *p <= TEXT('9'))
		{
			LineNumber = 10 * LineNumber + (*p++ - TEXT('0'));
		}
		Error.ErrorLineString = *FString::Printf(TEXT("%d"), LineNumber);

		// Skip to the warning message.
		while (*p && (*p == TEXT(')') || *p == TEXT(':') || *p == TEXT(' ') || *p == TEXT('\t')))
		{
			p++;
		}
		Error.StrippedErrorMessage = p;
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
