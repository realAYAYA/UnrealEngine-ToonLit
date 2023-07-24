// Copyright Epic Games, Inc. All Rights Reserved.
// ..

#include "CoreMinimal.h"
#include "ShaderCore.h"
#include "MetalShaderResources.h"
#include "ShaderCompilerCommon.h"
#include "ShaderParameterParser.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformFileManager.h"

#include "MetalShaderFormat.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include "Windows/PreWindowsApi.h"
	#include <objbase.h>
	#include <assert.h>
	#include <stdio.h>
	#include "Windows/PostWindowsApi.h"
	#include "Windows/MinWindows.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "ShaderPreprocessor.h"
#include "MetalBackend.h"
#include "MetalDerivedData.h"

#if !PLATFORM_WINDOWS
#if PLATFORM_TCHAR_IS_CHAR16
#define FP_TEXT_PASTE(x) L ## x
#define WTEXT(x) FP_TEXT_PASTE(x)
#else
#define WTEXT TEXT
#endif
#endif

static bool CompileProcessAllowsRuntimeShaderCompiling(const FShaderCompilerInput& InputCompilerEnvironment)
{
	bool bArchiving = InputCompilerEnvironment.Environment.CompilerFlags.Contains(CFLAG_Archive);
	bool bDebug = InputCompilerEnvironment.Environment.CompilerFlags.Contains(CFLAG_Debug);

	return !bArchiving && bDebug;
}

constexpr uint16 GMetalMaxUniformBufferSlots = 32;
constexpr int32 GMetalDefaultShadingLanguageVersion = 0;

/*------------------------------------------------------------------------------
	Shader compiling.
------------------------------------------------------------------------------*/

static inline uint32 ParseNumber(const TCHAR* Str)
{
	uint32 Num = 0;
	while (*Str && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}

static inline uint32 ParseNumber(const ANSICHAR* Str)
{
	uint32 Num = 0;
	while (*Str && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}

struct FHlslccMetalHeader : public CrossCompiler::FHlslccHeader
{
	FHlslccMetalHeader(uint32 const Version);
	virtual ~FHlslccMetalHeader();
	
	// After the standard header, different backends can output their own info
	virtual bool ParseCustomHeaderEntries(const ANSICHAR*& ShaderSource) override;
	
	TMap<uint8, TArray<uint8>> ArgumentBuffers;
	int8 SideTable;
	uint32 RayTracingInstanceIndexBuffer;
	uint32 Version;
};

FHlslccMetalHeader::FHlslccMetalHeader(uint32 const InVersion)
{
	SideTable = -1;
	RayTracingInstanceIndexBuffer = UINT_MAX;
	Version = InVersion;
}

FHlslccMetalHeader::~FHlslccMetalHeader()
{
	
}

bool FHlslccMetalHeader::ParseCustomHeaderEntries(const ANSICHAR*& ShaderSource)
{
#define DEF_PREFIX_STR(Str) \
static const ANSICHAR* Str##Prefix = "// @" #Str ": "; \
static const int32 Str##PrefixLen = FCStringAnsi::Strlen(Str##Prefix)
	DEF_PREFIX_STR(ArgumentBuffers);
	DEF_PREFIX_STR(SideTable);
	DEF_PREFIX_STR(RayTracingInstanceIndexBuffer);
#undef DEF_PREFIX_STR
	
	const ANSICHAR* SideTableString = FCStringAnsi::Strstr(ShaderSource, SideTablePrefix);
	if (SideTableString)
	{
		ShaderSource = SideTableString;
		ShaderSource += SideTablePrefixLen;
		while (*ShaderSource && *ShaderSource != '\n')
		{
			if (*ShaderSource == '(')
			{
				ShaderSource++;
				if (*ShaderSource && *ShaderSource != '\n')
				{
					SideTable = (int8)ParseNumber(ShaderSource);
				}
			}
			else
			{
				ShaderSource++;
			}
		}
		
		if (*ShaderSource && !CrossCompiler::Match(ShaderSource, '\n'))
		{
			return false;
		}
		
		if (SideTable < 0)
		{
			UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Couldn't parse the SideTable buffer index for bounds checking"));
			return false;
		}
	}
	
	const ANSICHAR* RayTracingInstanceIndexBufferString = FCStringAnsi::Strstr(ShaderSource, RayTracingInstanceIndexBufferPrefix);
	if (RayTracingInstanceIndexBufferString)
	{
		ShaderSource += RayTracingInstanceIndexBufferPrefixLen;
		if (!CrossCompiler::ParseIntegerNumber(ShaderSource, RayTracingInstanceIndexBuffer))
		{
			return false;
		}

		if (*ShaderSource && !CrossCompiler::Match(ShaderSource, '\n'))
		{
			return false;
		}
	}

	const ANSICHAR* ArgumentTable = FCStringAnsi::Strstr(ShaderSource, ArgumentBuffersPrefix);
	if (ArgumentTable)
	{
		ShaderSource = ArgumentTable;
		ShaderSource += ArgumentBuffersPrefixLen;
		while (*ShaderSource && *ShaderSource != '\n')
		{
			int32 ArgumentBufferIndex = -1;
			if (!CrossCompiler::ParseIntegerNumber(ShaderSource, ArgumentBufferIndex))
			{
				return false;
			}
			check(ArgumentBufferIndex >= 0);
			
			if (!CrossCompiler::Match(ShaderSource, '['))
			{
				return false;
			}
			
			TArray<uint8> Mask;
			while (*ShaderSource && *ShaderSource != ']')
			{
				int32 MaskIndex = -1;
				if (!CrossCompiler::ParseIntegerNumber(ShaderSource, MaskIndex))
				{
					return false;
				}
				
				check(MaskIndex >= 0);
				Mask.Add((uint8)MaskIndex);
				
				if (!CrossCompiler::Match(ShaderSource, ',') && *ShaderSource != ']')
				{
					return false;
				}
			}
			
			if (!CrossCompiler::Match(ShaderSource, ']'))
			{
				return false;
			}
			
			if (!CrossCompiler::Match(ShaderSource, ',') && *ShaderSource != '\n')
			{
				return false;
			}
			
			ArgumentBuffers.Add((uint8)ArgumentBufferIndex, Mask);
		}
	}
	
	return true;
}

/**
 * Construct the final microcode from the compiled and verified shader source.
 * @param ShaderOutput - Where to store the microcode and parameter map.
 * @param InShaderSource - Metal source with input/output signature.
 * @param SourceLen - The length of the Metal source code.
 */
void BuildMetalShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	FSHAHash const& GUIDHash,
	uint32 CCFlags,
	const ANSICHAR* InShaderSource,
	uint32 SourceLen,
	uint32 SourceCRCLen,
	uint32 SourceCRC,
	uint32 Version,
	TCHAR const* Standard,
	TCHAR const* MinOSVersion,
	EMetalTypeBufferMode TypeMode,
	TArray<FShaderCompilerError>& OutErrors,
	uint32 TypedBuffers,
	uint32 InvariantBuffers,
	uint32 TypedUAVs,
	uint32 ConstantBuffers,
	TArray<uint8> const& TypedBufferFormats,
	bool bAllowFastIntriniscs
	)
{
	ShaderOutput.bSucceeded = false;
	
	const ANSICHAR* USFSource = InShaderSource;
	
	uint32 NumLines = 0;
	const ANSICHAR* Main = FCStringAnsi::Strstr(USFSource, "Main_");
	while (Main && *Main)
	{
		if (*Main == '\n')
		{
			NumLines++;
		}
		Main++;
	}
	
	FHlslccMetalHeader CCHeader(Version);
	if (!CCHeader.Read(USFSource, SourceLen))
	{
		UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Bad hlslcc header found"));
	}
	
	const EShaderFrequency Frequency = ShaderOutput.Target.GetFrequency();

	//TODO read from toolchain
	const bool bIsMobile = (ShaderInput.Target.Platform == SP_METAL || ShaderInput.Target.Platform == SP_METAL_MRT || ShaderInput.Target.Platform == SP_METAL_TVOS || ShaderInput.Target.Platform == SP_METAL_MRT_TVOS);
	bool bNoFastMath = ShaderInput.Environment.CompilerFlags.Contains(CFLAG_NoFastMath);
	FString const* UsingWPO = ShaderInput.Environment.GetDefinitions().Find(TEXT("USES_WORLD_POSITION_OFFSET"));
	if (UsingWPO && FString("1") == *UsingWPO && (ShaderInput.Target.Platform == SP_METAL_MRT || ShaderInput.Target.Platform == SP_METAL_MRT_TVOS) && Frequency == SF_Vertex)
	{
		// WPO requires that we make all multiply/sincos instructions invariant :(
		bNoFastMath = true;
	}

	FMetalCodeHeader Header;
	Header.CompileFlags = (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Debug) ? (1 << CFLAG_Debug) : 0);
	Header.CompileFlags |= (bNoFastMath ? (1 << CFLAG_NoFastMath) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData) ? (1 << CFLAG_ExtraShaderData) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_ZeroInitialise) ? (1 <<  CFLAG_ZeroInitialise) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_BoundsChecking) ? (1 << CFLAG_BoundsChecking) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive) ? (1 << CFLAG_Archive) : 0);

	Header.CompilerVersion = FMetalCompilerToolchain::Get()->GetCompilerVersion((EShaderPlatform)ShaderInput.Target.Platform).Version;
	Header.CompilerBuild = FMetalCompilerToolchain::Get()->GetTargetVersion((EShaderPlatform)ShaderInput.Target.Platform).Version;
	Header.Version = Version;
	Header.SideTable = -1;
	Header.SourceLen = SourceCRCLen;
	Header.SourceCRC = SourceCRC;
	Header.Bindings.bDiscards = false;
	Header.Bindings.ConstantBuffers = ConstantBuffers;
	{
		Header.Bindings.TypedBuffers = TypedBuffers;
		for (uint32 i = 0; i < (uint32)TypedBufferFormats.Num(); i++)
		{
			if ((TypedBuffers & (1 << i)) != 0)
			{
				check(TypedBufferFormats[i] > (uint8)EMetalBufferFormat::Unknown);
				check(TypedBufferFormats[i] < (uint8)EMetalBufferFormat::Max);
				if ((TypeMode > EMetalTypeBufferModeRaw)
					&& (TypeMode <= EMetalTypeBufferModeTB)
					&& (TypedBufferFormats[i] < (uint8)EMetalBufferFormat::RGB8Sint || TypedBufferFormats[i] > (uint8)EMetalBufferFormat::RGB32Float)
					&& (TypeMode == EMetalTypeBufferMode2D || TypeMode == EMetalTypeBufferModeTB || !(TypedUAVs & (1 << i))))
				{
					Header.Bindings.LinearBuffer |= (1 << i);
					Header.Bindings.TypedBuffers &= ~(1 << i);
				}
			}
		}
		
		Header.Bindings.LinearBuffer = Header.Bindings.TypedBuffers;
		Header.Bindings.TypedBuffers = 0;
		
		// Raw mode means all buffers are invariant
		if (TypeMode == EMetalTypeBufferModeRaw)
		{
			Header.Bindings.TypedBuffers = 0;
		}
	}
	
	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false,32);

	// Write out the magic markers.
	Header.Frequency = Frequency;

	// Only inputs for vertex shaders must be tracked.
	if (Frequency == SF_Vertex)
	{
		static const FString AttributePrefix = TEXT("in_ATTRIBUTE");
		for (auto& Input : CCHeader.Inputs)
		{
			// Only process attributes.
			if (Input.Name.StartsWith(AttributePrefix))
			{
				uint8 AttributeIndex = ParseNumber(*Input.Name + AttributePrefix.Len());
				Header.Bindings.InOutMask.EnableField(AttributeIndex);
			}
		}
	}

	// Then the list of outputs.
	static const FString TargetPrefix = "FragColor";
	static const FString TargetPrefix2 = "SV_Target";
	// Only outputs for pixel shaders must be tracked.
	if (Frequency == SF_Pixel)
	{
		for (auto& Output : CCHeader.Outputs)
		{
			// Handle targets.
			if (Output.Name.StartsWith(TargetPrefix))
			{
				uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix.Len());
				Header.Bindings.InOutMask.EnableField(TargetIndex);
			}
			else if (Output.Name.StartsWith(TargetPrefix2))
			{
				uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix2.Len());
				Header.Bindings.InOutMask.EnableField(TargetIndex);
			}
		}
		
		// For fragment shaders that discard but don't output anything we need at least a depth-stencil surface, so we need a way to validate this at runtime.
		if (FCStringAnsi::Strstr(USFSource, "[[ depth(") != nullptr || FCStringAnsi::Strstr(USFSource, "[[depth(") != nullptr)
		{
			Header.Bindings.InOutMask.EnableField(CrossCompiler::FShaderBindingInOutMask::DepthStencilMaskIndex);
		}
        
		// For fragment shaders that discard but don't output anything we need at least a depth-stencil surface, so we need a way to validate this at runtime.
		if (FCStringAnsi::Strstr(USFSource, "discard_fragment()") != nullptr)
		{
			Header.Bindings.bDiscards = true;
		}
	}

	// Then 'normal' uniform buffers.
	bool bOutOfBounds = false;
	for (auto& UniformBlock : CCHeader.UniformBlocks)
	{
		uint16 UBIndex = UniformBlock.Index;
		if (UBIndex >= Header.Bindings.NumUniformBuffers)
		{
			Header.Bindings.NumUniformBuffers = UBIndex + 1;
		}
		if (UBIndex >= GMetalMaxUniformBufferSlots)
		{
			bOutOfBounds = true;
			new(OutErrors) FShaderCompilerError(*FString::Printf(TEXT("Uniform buffer index (%d) exceeded upper bound of slots (%d) for Metal API: %s"), UBIndex, GMetalMaxUniformBufferSlots, *UniformBlock.Name));
			continue;
		}
		UsedUniformBufferSlots[UBIndex] = true;

		HandleReflectedUniformBuffer(UniformBlock.Name, UBIndex, ShaderOutput);
	}
	
	if (bOutOfBounds)
	{
		ShaderOutput.bSucceeded = false;
		return;
	}

	// Packed global uniforms
	const uint16 BytesPerComponent = 4;
	TMap<ANSICHAR, uint16> PackedGlobalArraySize;
	for (auto& PackedGlobal : CCHeader.PackedGlobals)
	{
		HandleReflectedGlobalConstantBufferMember(
			PackedGlobal.Name,
			PackedGlobal.PackedType,
			PackedGlobal.Offset * BytesPerComponent,
			PackedGlobal.Count * BytesPerComponent,
			ShaderOutput
		);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(PackedGlobal.PackedType);
		Size = FMath::Max<uint16>(BytesPerComponent * (PackedGlobal.Offset + PackedGlobal.Count), Size);
	}

	// Packed Uniform Buffers
	TMap<int, TMap<CrossCompiler::EPackedTypeName, uint16> > PackedUniformBuffersSize;
	for (auto& PackedUB : CCHeader.PackedUBs)
	{
		for (auto& Member : PackedUB.Members)
		{
			HandleReflectedGlobalConstantBufferMember(
				Member.Name,
				(uint32)CrossCompiler::EPackedTypeName::HighP,
				Member.Offset * BytesPerComponent,
				Member.Count * BytesPerComponent,
				ShaderOutput
			);
			
			uint16& Size = PackedUniformBuffersSize.FindOrAdd(PackedUB.Attribute.Index).FindOrAdd(CrossCompiler::EPackedTypeName::HighP);
			Size = FMath::Max<uint16>(BytesPerComponent * (Member.Offset + Member.Count), Size);
		}
	}

	// Setup Packed Array info
	Header.Bindings.PackedGlobalArrays.Reserve(PackedGlobalArraySize.Num());
	for (auto Iterator = PackedGlobalArraySize.CreateIterator(); Iterator; ++Iterator)
	{
		ANSICHAR TypeName = Iterator.Key();
		uint16 Size = Iterator.Value();
		Size = (Size + 0xf) & (~0xf);
		CrossCompiler::FPackedArrayInfo Info;
		Info.Size = Size;
		Info.TypeName = TypeName;
		Info.TypeIndex = (uint8)CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
		Header.Bindings.PackedGlobalArrays.Add(Info);
	}

	// Setup Packed Uniform Buffers info
	Header.Bindings.PackedUniformBuffers.Reserve(PackedUniformBuffersSize.Num());
	
	// In this mode there should only be 0 or 1 packed UB that contains all the aligned & named global uniform parameters
	check(PackedUniformBuffersSize.Num() <= 1);
	for (auto Iterator = PackedUniformBuffersSize.CreateIterator(); Iterator; ++Iterator)
	{
		int BufferIndex = Iterator.Key();
		auto& ArraySizes = Iterator.Value();
		for (auto IterSizes = ArraySizes.CreateIterator(); IterSizes; ++IterSizes)
		{
			CrossCompiler::EPackedTypeName TypeName = IterSizes.Key();
			uint16 Size = IterSizes.Value();
			Size = (Size + 0xf) & (~0xf);
			CrossCompiler::FPackedArrayInfo Info;
			Info.Size = Size;
			Info.TypeName = (ANSICHAR)TypeName;
			Info.TypeIndex = BufferIndex;
			Header.Bindings.PackedGlobalArrays.Add(Info);
		}
	}

	uint32 NumTextures = 0;
	
	// Then samplers.
	TMap<FString, uint32> SamplerMap;
	for (auto& Sampler : CCHeader.Samplers)
	{
		HandleReflectedShaderResource(Sampler.Name, Sampler.Offset, Sampler.Count, ShaderOutput);

		NumTextures += Sampler.Count;

		for (auto& SamplerState : Sampler.SamplerStates)
		{
			SamplerMap.Add(SamplerState, Sampler.Count);
		}
	}
	
	Header.Bindings.NumSamplers = CCHeader.SamplerStates.Num();

	// Then UAVs (images in Metal)
	for (auto& UAV : CCHeader.UAVs)
	{
		HandleReflectedShaderUAV(UAV.Name, UAV.Offset, UAV.Count, ShaderOutput);

		Header.Bindings.NumUAVs = FMath::Max<uint8>(
			Header.Bindings.NumSamplers,
			UAV.Offset + UAV.Count
			);
	}

	for (auto& SamplerState : CCHeader.SamplerStates)
	{
		if (!SamplerMap.Contains(SamplerState.Name))
		{
			SamplerMap.Add(SamplerState.Name, 1);
		}
		
		HandleReflectedShaderSampler(SamplerState.Name, SamplerState.Index, SamplerMap[SamplerState.Name], ShaderOutput);
	}

	Header.NumThreadsX = CCHeader.NumThreads[0];
	Header.NumThreadsY = CCHeader.NumThreads[1];
	Header.NumThreadsZ = CCHeader.NumThreads[2];
	
	// TODO: Should be for inline RT only.
	if (Frequency == SF_Compute)
	{
		Header.RayTracing.InstanceIndexBuffer = CCHeader.RayTracingInstanceIndexBuffer;
	}

	Header.bDeviceFunctionConstants = (FCStringAnsi::Strstr(USFSource, "#define __METAL_DEVICE_CONSTANT_INDEX__ 1") != nullptr);
	Header.SideTable = CCHeader.SideTable;
	Header.Bindings.ArgumentBufferMasks = CCHeader.ArgumentBuffers;
	Header.Bindings.ArgumentBuffers = 0;
	for (auto const& Pair : Header.Bindings.ArgumentBufferMasks)
	{
		Header.Bindings.ArgumentBuffers |= (1 << Pair.Key);
	}
	
	// Build the SRT for this shader.
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.UniformBufferMap, UsedUniformBufferSlots, ShaderOutput.ParameterMap, GenericSRT);
		CullGlobalUniformBuffers(ShaderInput.Environment.UniformBufferMap, ShaderOutput.ParameterMap);

		// Copy over the bits indicating which resource tables are active.
		Header.Bindings.ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;

		Header.Bindings.ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.TextureMap);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.ShaderResourceViewMap);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.SamplerMap);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.UnorderedAccessViewMap);

		Header.Bindings.NumUniformBuffers = FMath::Max((uint8)GetNumUniformBuffersUsed(GenericSRT), Header.Bindings.NumUniformBuffers);
	}

	FString MetalCode = FString(USFSource);

	if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Debug))
	{
		MetalCode.InsertAt(0, FString::Printf(TEXT("// %s\n"), *CCHeader.Name));
	}
	
	if (Header.Bindings.NumSamplers > MaxMetalSamplers)
	{
		ShaderOutput.bSucceeded = false;
		FShaderCompilerError* NewError = new(OutErrors) FShaderCompilerError();
		
		FString SamplerList;
		for (int32 i = 0; i < CCHeader.SamplerStates.Num(); i++)
		{
			auto const& Sampler = CCHeader.SamplerStates[i];
			SamplerList += FString::Printf(TEXT("%d:%s\n"), Sampler.Index, *Sampler.Name);
		}
		
		NewError->StrippedErrorMessage =
			FString::Printf(TEXT("shader uses %d (%d) samplers exceeding the limit of %d\nSamplers:\n%s"),
				Header.Bindings.NumSamplers, CCHeader.SamplerStates.Num(), MaxMetalSamplers, *SamplerList);
	}
	// TODO read from toolchain? this check isn't really doing exactly what it says
	else if(CompileProcessAllowsRuntimeShaderCompiling(ShaderInput))
	{
		// Write out the header and shader source code.
		FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
		uint8 PrecompiledFlag = 0;
		Ar << PrecompiledFlag;
		Ar << Header;
		Ar.Serialize((void*)USFSource, SourceLen + 1 - (USFSource - InShaderSource));
		
		if (ShaderInput.ExtraSettings.bExtractShaderSource)
		{
			ShaderOutput.OptionalFinalShaderSource = MetalCode;
		}

		ShaderOutput.NumInstructions = NumLines;
		ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
		ShaderOutput.bSucceeded = true;
	}
	else
	{
		// TODO technically should probably check the version of the metal compiler to make sure it's recent enough to support -MO.
		FString DebugInfo = TEXT("");
		if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
		{
			DebugInfo = TEXT("-gline-tables-only -MO");
		}
		
		FString MathMode = bNoFastMath ? TEXT("-fno-fast-math") : TEXT("-ffast-math");
        
		// at this point, the shader source is ready to be compiled
		// we will need a working temp directory.
		const FString& TempDir = FMetalCompilerToolchain::Get()->GetLocalTempDir();
		
		int32 ReturnCode = 0;
		FString Results;
		FString Errors;
		bool bSucceeded = false;

		EShaderPlatform ShaderPlatform = EShaderPlatform(ShaderInput.Target.Platform);
		const bool bMetalCompilerAvailable = FMetalCompilerToolchain::Get()->IsCompilerAvailable();
		
		bool bDebugInfoSucceded = false;
		FMetalShaderBytecode Bytecode;
		FMetalShaderDebugInfo DebugCode;
		
		FString HashedName = FString::Printf(TEXT("%u_%u"), SourceCRCLen, SourceCRC);
		
		if(!bMetalCompilerAvailable)
		{
			// No Metal Compiler - just put the source code directly into /tmp and report error - we are now using text shaders when this was not the requested configuration
			// Move it into place using an atomic move - ensures only one compile "wins"
			FString InputFilename = (TempDir / HashedName) + FMetalCompilerToolchain::MetalExtention;
			FString SaveFile = FPaths::CreateTempFilename(*TempDir, TEXT("ShaderTemp"), TEXT(""));
			FFileHelper::SaveStringToFile(MetalCode, *SaveFile);
			IFileManager::Get().Move(*InputFilename, *SaveFile, false, false, true, true);
			IFileManager::Get().Delete(*SaveFile);
			
			TCHAR const* Message = nullptr;
			if (PLATFORM_MAC)
			{
				Message = TEXT("Xcode's metal shader compiler was not found, verify Xcode has been installed on this Mac and that it has been selected in Xcode > Preferences > Locations > Command-line Tools.");
			}
			
			FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
			Error->ErrorVirtualFilePath = InputFilename;
			Error->ErrorLineString = TEXT("0");
			Error->StrippedErrorMessage = FString(Message);
		}
		else
		{
			// Compiler available - more intermediate files will be created. 
			// TODO How to handle multiple streams on the same machine? Needs more uniqueness in the temp dir
			const FString& CompilerVersionString = FMetalCompilerToolchain::Get()->GetCompilerVersionString(ShaderPlatform);
			
			// The base name (which is <temp>/CRCHash_Length)
			FString BaseFileName = FPaths::Combine(TempDir, HashedName);
			// The actual metal shadertext, a .metal file
			FString MetalFileName = BaseFileName + FMetalCompilerToolchain::MetalExtention;
			// The compiled shader, as AIR. An .air file.
			FString AIRFileName = BaseFileName + FMetalCompilerToolchain::MetalObjectExtension;
			// A metallib containing just this shader (gross). A .metallib file.
			FString MetallibFileName = BaseFileName + FMetalCompilerToolchain::MetalLibraryExtension;

			// 'MetalCode' contains the cross compiled MetalSL version of the shader.
			// Save it out.
			{
				// This is the previous behavior, which attempts an atomic move in case there is a race here.
				// TODO can we ever actually race on this? TempDir should be process specific now.
				FString SaveFile = FPaths::CreateTempFilename(*TempDir, TEXT("ShaderTemp"), TEXT(""));
				bool bSuccess = FFileHelper::SaveStringToFile(MetalCode, *SaveFile);
				if (!bSuccess)
				{
					UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Failed to write Metal shader out to %s\nShaderText:\n%s"), *SaveFile, *MetalCode);
				}
				bSuccess = IFileManager::Get().Move(*MetalFileName, *SaveFile, false, false, true, true);
				if (!bSuccess && !FPaths::FileExists(MetalFileName))
				{
					UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Failed to move %s to %s"), *SaveFile, *MetalFileName);
				}

				if (FPaths::FileExists(SaveFile))
				{
					IFileManager::Get().Delete(*SaveFile);
				}
			}

			// For iOS 14.0+ this is required. Version==0 is IOSMetalSLStandard_Minimum
			const bool bPreserveInvariance = Frequency == SF_Vertex && (Version == 0 || Version > 5);
			
			// TODO This is the actual MetalSL -> AIR piece
			FMetalShaderBytecodeJob Job;
			Job.IncludeDir = TempDir;
			Job.ShaderFormat = ShaderInput.ShaderFormat;
			Job.Hash = GUIDHash;
			Job.TmpFolder = TempDir;
			Job.InputFile = MetalFileName;
			Job.OutputFile = MetallibFileName;
			Job.OutputObjectFile = AIRFileName;
			Job.CompilerVersion = CompilerVersionString;
			Job.MinOSVersion = MinOSVersion;
			Job.PreserveInvariance = bPreserveInvariance ? TEXT("-fpreserve-invariance") : TEXT("");
			Job.DebugInfo = DebugInfo;
			Job.MathMode = MathMode;
			Job.Standard = Standard;
			Job.SourceCRCLen = SourceCRCLen;
			Job.SourceCRC = SourceCRC;
			Job.bRetainObjectFile = ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive);
			Job.bCompileAsPCH = false;
			Job.ReturnCode = 0;

			bSucceeded = FMetalCompilerToolchain::Get()->CompileMetalShader(Job, Bytecode);
			if (bSucceeded)
			{
				if (!bIsMobile && !ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive))
				{
					uint32 CodeSize = FCStringAnsi::Strlen(TCHAR_TO_UTF8(*MetalCode)) + 1;

					int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, CodeSize);
					DebugCode.CompressedData.SetNum(CompressedSize);

					if (FCompression::CompressMemory(NAME_Zlib, DebugCode.CompressedData.GetData(), CompressedSize, TCHAR_TO_UTF8(*MetalCode), CodeSize))
					{
						DebugCode.UncompressedSize = CodeSize;
						DebugCode.CompressedData.SetNum(CompressedSize);
						DebugCode.CompressedData.Shrink();
					}
				}
			}
			else
			{
				FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
				Error->ErrorVirtualFilePath = MetalFileName;
				Error->ErrorLineString = TEXT("0");
				Error->StrippedErrorMessage = Job.Message;
			}
		}
		
		if (bSucceeded)
		{
			// Write out the header and compiled shader code
			FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
			uint8 PrecompiledFlag = 1;
			Ar << PrecompiledFlag;
			Ar << Header;

			// jam it into the output bytes
			Ar.Serialize(Bytecode.OutputFile.GetData(), Bytecode.OutputFile.Num());

			if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive))
			{
				ShaderOutput.ShaderCode.AddOptionalData('o', Bytecode.ObjectFile.GetData(), Bytecode.ObjectFile.Num());
			}
			
			if (bDebugInfoSucceded && !ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive) && DebugCode.CompressedData.Num())
			{
				ShaderOutput.ShaderCode.AddOptionalData('z', DebugCode.CompressedData.GetData(), DebugCode.CompressedData.Num());
				ShaderOutput.ShaderCode.AddOptionalData('p', TCHAR_TO_UTF8(*Bytecode.NativePath));
				ShaderOutput.ShaderCode.AddOptionalData('u', (const uint8*)&DebugCode.UncompressedSize, sizeof(DebugCode.UncompressedSize));
			}
			
			if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_ExtraShaderData))
			{
				// store data we can pickup later with ShaderCode.FindOptionalData(FShaderCodeName::Key), could be removed for shipping
				ShaderOutput.ShaderCode.AddOptionalData(FShaderCodeName::Key, TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));
				if (DebugCode.CompressedData.Num() == 0)
				{
					ShaderOutput.ShaderCode.AddOptionalData('c', TCHAR_TO_UTF8(*MetalCode));
					ShaderOutput.ShaderCode.AddOptionalData('p', TCHAR_TO_UTF8(*Bytecode.NativePath));
				}
			}
			else if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive))
			{
				ShaderOutput.ShaderCode.AddOptionalData('c', TCHAR_TO_UTF8(*MetalCode));
				ShaderOutput.ShaderCode.AddOptionalData('p', TCHAR_TO_UTF8(*Bytecode.NativePath));
			}
			
			ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
		}

		if (ShaderInput.ExtraSettings.bExtractShaderSource)
		{
			ShaderOutput.OptionalFinalShaderSource = MetalCode;
		}
		
		ShaderOutput.NumInstructions = NumLines;
		ShaderOutput.bSucceeded = bSucceeded;
	}
}

/*------------------------------------------------------------------------------
	External interface.
------------------------------------------------------------------------------*/

FString CreateRemoteDataFromEnvironment(const FShaderCompilerEnvironment& Environment)
{
	FString Line = TEXT("\n#if 0 /*BEGIN_REMOTE_SERVER*/\n");
	for (auto Pair : Environment.RemoteServerData)
	{
		Line += FString::Printf(TEXT("%s=%s\n"), *Pair.Key, *Pair.Value);
	}
	Line += TEXT("#endif /*END_REMOTE_SERVER*/\n");
	return Line;
}

void CreateEnvironmentFromRemoteData(const FString& String, FShaderCompilerEnvironment& OutEnvironment)
{
	FString Prolog = TEXT("#if 0 /*BEGIN_REMOTE_SERVER*/");
	int32 FoundBegin = String.Find(Prolog, ESearchCase::CaseSensitive);
	if (FoundBegin == INDEX_NONE)
	{
		return;
	}
	int32 FoundEnd = String.Find(TEXT("#endif /*END_REMOTE_SERVER*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FoundBegin);
	if (FoundEnd == INDEX_NONE)
	{
		return;
	}

	// +1 for EOL
	const TCHAR* Ptr = &String[FoundBegin + 1 + Prolog.Len()];
	const TCHAR* PtrEnd = &String[FoundEnd];
	while (Ptr < PtrEnd)
	{
		FString Key;
		if (!CrossCompiler::ParseIdentifier(Ptr, Key))
		{
			return;
		}
		if (!CrossCompiler::Match(Ptr, TEXT("=")))
		{
			return;
		}
		FString Value;
		if (!CrossCompiler::ParseString(Ptr, Value))
		{
			return;
		}
		if (!CrossCompiler::Match(Ptr, '\n'))
		{
			return;
		}
		OutEnvironment.RemoteServerData.FindOrAdd(Key) = Value;
	}
}

void CompileShader_Metal(const FShaderCompilerInput& _Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	auto Input = _Input;
	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;

	// Work out which standard we need, this is dependent on the shader platform.
	// TODO: Read from toolchain class
	const bool bIsMobile = FMetalCompilerToolchain::Get()->IsMobile((EShaderPlatform) Input.Target.Platform);
	TCHAR const* StandardPlatform = nullptr;
	if (bIsMobile)
	{
		StandardPlatform = TEXT("ios");
		AdditionalDefines.SetDefine(TEXT("IOS"), 1);
	}
	else
	{
		StandardPlatform = TEXT("macos");
		AdditionalDefines.SetDefine(TEXT("MAC"), 1);
	}
	
	AdditionalDefines.SetDefine(TEXT("COMPILER_METAL"), 1);
	
	EMetalGPUSemantics Semantics = EMetalGPUSemanticsMobile;
	
    const int32 VersionEnum = [&Input, &Output]() -> int32
	{
		if (const FString* VersionEnumEntry = Input.Environment.GetDefinitions().Find(TEXT("SHADER_LANGUAGE_VERSION")))
		{
			return FCString::Atoi(*FString(*VersionEnumEntry));
		}
		else
		{
			new(Output.Errors) FShaderCompilerError(*FString::Printf(TEXT("Missing definition of SHADER_LANGUAGE_VERSION; Falling back to default value %d"), GMetalDefaultShadingLanguageVersion));
			return GMetalDefaultShadingLanguageVersion;
		}
	}();

	#if PLATFORM_MAC_ENABLE_EXPERIMENTAL_NANITE_SUPPORT
		AdditionalDefines.SetDefine(TEXT("METAL_ENABLE_EXPERIMENTAL_NANITE_SUPPORT"), 1);
	#endif 

	// TODO read from toolchain
	bool bAppleTV = (Input.ShaderFormat == NAME_SF_METAL_TVOS || Input.ShaderFormat == NAME_SF_METAL_MRT_TVOS);
	if (Input.ShaderFormat == NAME_SF_METAL || Input.ShaderFormat == NAME_SF_METAL_TVOS)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_PROFILE"), 1);
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MRT || Input.ShaderFormat == NAME_SF_METAL_MRT_TVOS)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_MRT_PROFILE"), 1);
		Semantics = EMetalGPUSemanticsTBDRDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MACES3_1)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_PROFILE"), 1);
		Semantics = EMetalGPUSemanticsImmediateDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_SM5)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_SM5_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("USING_VERTEX_SHADER_LAYER"), 1);
		Semantics = EMetalGPUSemanticsImmediateDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MRT_MAC)
	{
		AdditionalDefines.SetDefine(TEXT("METAL_MRT_PROFILE"), 1);
		Semantics = EMetalGPUSemanticsTBDRDesktop;
	}
	else
	{
		Output.bSucceeded = false;
		new(Output.Errors) FShaderCompilerError(*FString::Printf(TEXT("Invalid shader format '%s' passed to compiler."), *Input.ShaderFormat.ToString()));
		return;
	}
	

	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 2);
	
	EMetalTypeBufferMode TypeMode = EMetalTypeBufferModeRaw;
	FString MinOSVersion;
	FString StandardVersion;
    TypeMode = EMetalTypeBufferModeTB;
	switch(VersionEnum)
	{
    case 8:
        StandardVersion = TEXT("3.0");
        if (bAppleTV)
        {
            MinOSVersion = TEXT("-mtvos-version-min=16.0");
        }
        else if (bIsMobile)
        {
            MinOSVersion = TEXT("-mios-version-min=16.0");
        }
        else
        {
            MinOSVersion = TEXT("-mmacosx-version-min=13");
        }
        break;

	case 7:
		StandardVersion = TEXT("2.4");
		if (bAppleTV)
		{
			MinOSVersion = TEXT("-mtvos-version-min=15.0");
		}
		else if (bIsMobile)
		{
			MinOSVersion = TEXT("-mios-version-min=15.0");
		}
		else
		{
			MinOSVersion = TEXT("-mmacosx-version-min=12");
		}
		break;
	case 6:
		StandardVersion = TEXT("2.4");
		if (bAppleTV)
		{
			MinOSVersion = TEXT("-mtvos-version-min=15.0");
		}
		else if (bIsMobile)
		{
			MinOSVersion = TEXT("-mios-version-min=15.0");
		}
		else
		{
			// TODO - This is a workaround for an issue with the Apple Shader Compiler
			// leading to corruption on M1/AMD when > 2.3 versions are used. 
			// This should be bumped to 2.4 after it's resolved
			StandardVersion = TEXT("2.3");
			MinOSVersion = TEXT("-mmacosx-version-min=11");
		}
		break;
	case 5:
		// Fall through
	case 0:
		StandardVersion = TEXT("2.4");
		if (bAppleTV)
		{
			MinOSVersion = TEXT("-mtvos-version-min=15.0");
		}
		else if (bIsMobile)
		{
			MinOSVersion = TEXT("-mios-version-min=15.0");
		}
		else
		{
			// TODO - This is a workaround for an issue with the Apple Shader Compiler
			// leading to corruption on M1/AMD when > 2.3 versions are used. 
			// This should be bumped to 2.4 after it's resolved
			StandardVersion = TEXT("2.2");
			MinOSVersion = TEXT("-mmacosx-version-min=10.15");
		}
		//EIOSMetalShaderStandard::IOSMetalSLStandard_Minimum
		break;
	default:
		Output.bSucceeded = false;
		{
            FString EngineIdentifier = FEngineVersion::Current().ToString(EVersionComponent::Minor);
			FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
            NewError->StrippedErrorMessage = FString::Printf(TEXT("Minimum Metal Version is 2.4 in UE %s"), *EngineIdentifier);
			return;
		}
		break;
	}
	
    if (Input.Environment.FullPrecisionInPS)
    {
        AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
    }

    FString Standard;
    if (VersionEnum >= 8)
    {
        Standard = FString::Printf(TEXT("-std=metal%s"), *StandardVersion);
    }
    else
    {
        Standard = FString::Printf(TEXT("-std=%s-metal%s"), StandardPlatform, *StandardVersion);
    }
	
	bool const bDirectCompile = FParse::Param(FCommandLine::Get(), TEXT("directcompile"));
	if (bDirectCompile)
	{
		Input.DumpDebugInfoPath = FPaths::GetPath(Input.VirtualSourceFilePath);
	}
	
	const bool bDumpDebugInfo = Input.DumpDebugInfoEnabled();

	// Allow the shader pipeline to override the platform default in here.
	uint32 MaxUnrollLoops = 32;
	if (Input.Environment.CompilerFlags.Contains(CFLAG_AvoidFlowControl))
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)0);
		MaxUnrollLoops = 1024; // Max. permitted by hlslcc
	}
	else if (Input.Environment.CompilerFlags.Contains(CFLAG_PreferFlowControl))
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)0);
		MaxUnrollLoops = 0;
	}
	else
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
	}

	bool bUsesInlineRayTracing = Input.Environment.CompilerFlags.Contains(CFLAG_InlineRayTracing);
	if (bUsesInlineRayTracing)
	{
		AdditionalDefines.SetDefine(TEXT("PLATFORM_SUPPORTS_INLINE_RAY_TRACING"), 1);
	}

	AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_DUAL_SOURCE_BLENDING_SLOT_DECORATION"), (uint32)1);

	const double StartPreprocessTime = FPlatformTime::Seconds();

	if (Input.bSkipPreprocessedCache)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShader, *Input.VirtualSourceFilePath))
		{
			return;
		}

		// Remove const as we are on debug-only mode
		CrossCompiler::CreateEnvironmentFromResourceTable(PreprocessedShader, (FShaderCompilerEnvironment&)Input.Environment);
		CreateEnvironmentFromRemoteData(PreprocessedShader, (FShaderCompilerEnvironment&)Input.Environment);
	}
	else
	{
		if (!PreprocessShader(PreprocessedShader, Output, Input, AdditionalDefines))
		{
			// The preprocessing stage will add any relevant errors.
			return;
		}
	}

	char* MetalShaderSource = NULL;
	char* ErrorLog = NULL;

	const EShaderFrequency Frequency = (EShaderFrequency)Input.Target.Frequency;
	if (!(Frequency == SF_Vertex || Frequency == SF_Pixel || Frequency == SF_Compute))
	{
		Output.bSucceeded = false;
		FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(
			TEXT("%s shaders not supported for use in Metal."),
			CrossCompiler::GetFrequencyName(Frequency)
			);
		return;
	}

	FShaderParameterParser ShaderParameterParser;
	if (!ShaderParameterParser.ParseAndModify(Input, Output, PreprocessedShader))
	{
		// The FShaderParameterParser will add any relevant errors.
		return;
	}

	// This requires removing the HLSLCC_NoPreprocess flag later on!
	RemoveUniformBuffersFromSource(Input.Environment, PreprocessedShader);

	// Process TEXT macro.
	TransformStringIntoCharacterArray(PreprocessedShader);

	// Run the experimental shader minifier
	#if UE_METAL_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL
	if (Input.Environment.CompilerFlags.Contains(CFLAG_RemoveDeadCode))
	{
		UE::ShaderCompilerCommon::RemoveDeadCode(PreprocessedShader, Input.EntryPointName, Output.Errors);
	}
	#endif // UE_METAL_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL

	Output.PreprocessTime = FPlatformTime::Seconds() - StartPreprocessTime;
	
	uint32 CCFlags = HLSLCC_NoPreprocess | HLSLCC_PackUniformsIntoUniformBufferWithNames | HLSLCC_FixAtomicReferences | HLSLCC_RetainSizes | HLSLCC_KeepSamplerAndImageNames;
	if (!bDirectCompile || UE_BUILD_DEBUG)
	{
		// Validation is expensive - only do it when compiling directly for debugging
		CCFlags |= HLSLCC_NoValidation;
	}

	// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
	CCFlags &= ~HLSLCC_NoPreprocess;

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (bDumpDebugInfo && !bDirectCompile)
	{
		UE::ShaderCompilerCommon::FDebugShaderDataOptions DebugDataOptions;
		DebugDataOptions.HlslCCFlags = CCFlags;
		DebugDataOptions.AppendPostSource = [&Input]()
		{
			// add the remote data if necessary
//			if (IsRemoteBuildingConfigured(&Input.Environment))
			{
				return CreateRemoteDataFromEnvironment(Input.Environment);
			}
		};
		UE::ShaderCompilerCommon::DumpDebugShaderData(Input, PreprocessedShader, DebugDataOptions);
	}

	FSHAHash GUIDHash;
	if (!bDirectCompile)
	{
		TArray<FString> GUIDFiles;
		GUIDFiles.Add(FPaths::ConvertRelativePathToFull(TEXT("/Engine/Public/Platform/Metal/MetalCommon.ush")));
		GUIDFiles.Add(FPaths::ConvertRelativePathToFull(TEXT("/Engine/Public/ShaderVersion.ush")));
		GUIDHash = GetShaderFilesHash(GUIDFiles, Input.Target.GetPlatform());
	}
	else
	{
		FGuid Guid = FGuid::NewGuid();
		FSHA1::HashBuffer(&Guid, sizeof(FGuid), GUIDHash.Hash);
	}

	bool bCompiled = DoCompileMetalShader(Input, Output, WorkingDirectory, PreprocessedShader, GUIDHash, VersionEnum, CCFlags, Semantics, TypeMode, MaxUnrollLoops, Frequency, bDumpDebugInfo, Standard, MinOSVersion);
	if (bCompiled && Output.bSucceeded)
	{

	}

	ShaderParameterParser.ValidateShaderParameterTypes(Input, bIsMobile, Output);
}

bool StripShader_Metal(TArray<uint8>& Code, class FString const& DebugPath, bool const bNative)
{
	bool bSuccess = false;
	
	FShaderCodeReader ShaderCode(Code);
	FMemoryReader Ar(Code, true);
	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());
	
	// was the shader already compiled offline?
	uint8 OfflineCompiledFlag;
	Ar << OfflineCompiledFlag;
	
	if(bNative && OfflineCompiledFlag == 1)
	{
		// get the header
		FMetalCodeHeader Header;
		Ar << Header;
		
		const FString ShaderName = ShaderCode.FindOptionalData(FShaderCodeName::Key);

		// Must be compiled for archiving or something is very wrong.
		if(bNative == false || Header.CompileFlags & (1 << CFLAG_Archive))
		{
			bSuccess = true;
			
			// remember where the header ended and code (precompiled or source) begins
			int32 CodeOffset = Ar.Tell();
			const uint8* SourceCodePtr = (uint8*)Code.GetData() + CodeOffset;
			
			// Copy the non-optional shader bytecode
			TArray<uint8> SourceCode;
			SourceCode.Append(SourceCodePtr, ShaderCode.GetActualShaderCodeSize() - CodeOffset);
			
			const ANSICHAR* ShaderSource = ShaderCode.FindOptionalData('c');
			const size_t ShaderSourceLength = ShaderSource ? FCStringAnsi::Strlen(ShaderSource) : 0;
			bool const bHasShaderSource = ShaderSourceLength > 0;
			
			const ANSICHAR* ShaderPath = ShaderCode.FindOptionalData('p');
			bool const bHasShaderPath = (ShaderPath && FCStringAnsi::Strlen(ShaderPath) > 0);
			
			if (bHasShaderSource && bHasShaderPath)
			{
				FString DebugFilePath = DebugPath / FString(ShaderPath);
				FString DebugFolderPath = FPaths::GetPath(DebugFilePath);
				if (IFileManager::Get().MakeDirectory(*DebugFolderPath, true))
				{
					FString TempPath = FPaths::CreateTempFilename(*DebugFolderPath, TEXT("MetalShaderFile-"), TEXT(".metal"));
					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					IFileHandle* FileHandle = PlatformFile.OpenWrite(*TempPath);
					if (FileHandle)
					{
						FileHandle->Write((const uint8 *)ShaderSource, ShaderSourceLength);
						delete FileHandle;

						IFileManager::Get().Move(*DebugFilePath, *TempPath, true, false, true, false);
						IFileManager::Get().Delete(*TempPath);
					}
					else
					{
						UE_LOG(LogShaders, Error, TEXT("Shader stripping failed: shader %s (Len: %0.8x, CRC: %0.8x) failed to create file %s!"), *ShaderName, Header.SourceLen, Header.SourceCRC, *TempPath);
					}
				}
			}
			
			if (bNative)
			{
				int32 ObjectSize = 0;
				const uint8* ShaderObject = ShaderCode.FindOptionalDataAndSize('o', ObjectSize);
				
				// If ShaderObject and ObjectSize is zero then the code has already been stripped - source code should be the byte code
				if(ShaderObject && ObjectSize)
				{
					TArray<uint8> ObjectCodeArray;
					ObjectCodeArray.Append(ShaderObject, ObjectSize);
					SourceCode = ObjectCodeArray;
				}
			}
			
			// Strip any optional data
			if (bNative || ShaderCode.GetOptionalDataSize() > 0)
			{
				// Write out the header and compiled shader code
				FShaderCode NewCode;
				FMemoryWriter NewAr(NewCode.GetWriteAccess(), true);
				NewAr << OfflineCompiledFlag;
				NewAr << Header;
				
				// jam it into the output bytes
				NewAr.Serialize(SourceCode.GetData(), SourceCode.Num());
				
				Code = NewCode.GetReadAccess();
			}
		}
		else
		{
			UE_LOG(LogShaders, Error, TEXT("Shader stripping failed: shader %s (Len: %0.8x, CRC: %0.8x) was not compiled for archiving into a native library (Native: %s, Compile Flags: %0.8x)!"), *ShaderName, Header.SourceLen, Header.SourceCRC, bNative ? TEXT("true") : TEXT("false"), (uint32)Header.CompileFlags);
		}
	}
	else
	{
		UE_LOG(LogShaders, Error, TEXT("Shader stripping failed: shader %s (Native: %s, Offline Compiled: %d) was not compiled to bytecode for native archiving!"), *DebugPath, bNative ? TEXT("true") : TEXT("false"), OfflineCompiledFlag);
	}
	
	return bSuccess;
}

uint64 AppendShader_Metal(FString const& WorkingDir, const FSHAHash& Hash, TArray<uint8>& InShaderCode)
{
	uint64 Id = 0;
	
	const bool bCompilerAvailable = FMetalCompilerToolchain::Get()->IsCompilerAvailable();
	
	if (bCompilerAvailable)
	{
		// Parse the existing data and extract the source code. We have to recompile it
		FShaderCodeReader ShaderCode(InShaderCode);
		FMemoryReader Ar(InShaderCode, true);
		Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());
		
		// was the shader already compiled offline?
		uint8 OfflineCompiledFlag;
		Ar << OfflineCompiledFlag;
		if (OfflineCompiledFlag == 1)
		{
			// get the header
			FMetalCodeHeader Header;
			Ar << Header;

			const FString ShaderName = ShaderCode.FindOptionalData(FShaderCodeName::Key);

			// Must be compiled for archiving or something is very wrong.
			if(Header.CompileFlags & (1 << CFLAG_Archive))
			{
				// remember where the header ended and code (precompiled or source) begins
				int32 CodeOffset = Ar.Tell();
				const uint8* SourceCodePtr = (uint8*)InShaderCode.GetData() + CodeOffset;
				
				// Copy the non-optional shader bytecode
				int32 ObjectCodeDataSize = 0;
				uint8 const* Object = ShaderCode.FindOptionalDataAndSize('o', ObjectCodeDataSize);

				// 'o' segment missing this is a pre stripped shader
				if(!Object)
				{
					ObjectCodeDataSize = ShaderCode.GetActualShaderCodeSize() - CodeOffset;
					Object = SourceCodePtr;
				}
				
				TArrayView<const uint8> ObjectCodeArray(Object, ObjectCodeDataSize);
				
				// Object code segment
				FString ObjFilename = WorkingDir / FString::Printf(TEXT("Main_%0.8x_%0.8x.o"), Header.SourceLen, Header.SourceCRC);
				
				bool const bHasObjectData = (ObjectCodeDataSize > 0) || IFileManager::Get().FileExists(*ObjFilename);
				if (bHasObjectData)
				{
					// metal commandlines
					int32 ReturnCode = 0;
					FString Results;
					FString Errors;
					
					bool bHasObjectFile = IFileManager::Get().FileExists(*ObjFilename);
					if (ObjectCodeDataSize > 0)
					{
						// write out shader object code source (IR) for archiving to a single library file later
						if( FFileHelper::SaveArrayToFile(ObjectCodeArray, *ObjFilename) )
						{
							bHasObjectFile = true;
						}
					}
					
					if (bHasObjectFile)
					{
						Id = ((uint64)Header.SourceLen << 32) | Header.SourceCRC;
						
						// This is going to get serialised into the shader resource archive we don't anything but the header info now with the archive flag set
						Header.CompileFlags |= (1 << CFLAG_Archive);
						
						// Write out the header and compiled shader code
						FShaderCode NewCode;
						FMemoryWriter NewAr(NewCode.GetWriteAccess(), true);
						NewAr << OfflineCompiledFlag;
						NewAr << Header;
						
						InShaderCode = NewCode.GetReadAccess();
						
						UE_LOG(LogShaders, Verbose, TEXT("Archiving succeeded: shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s)"), *ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString());
					}
					else
					{
						UE_LOG(LogShaders, Error, TEXT("Archiving failed: failed to write temporary file %s for shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s)"), *ObjFilename, *ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString());
					}
				}
				else
				{
					UE_LOG(LogShaders, Error, TEXT("Archiving failed: shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s) has no object data"), *ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString());
				}
			}
			else
			{
				UE_LOG(LogShaders, Error, TEXT("Archiving failed: shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s) was not compiled for archiving (Compile Flags: %0.8x)!"), *ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString(), (uint32)Header.CompileFlags);
			}
		}
		else
		{
			UE_LOG(LogShaders, Error, TEXT("Archiving failed: shader SHA: %s was not compiled to bytecode (%d)!"), *Hash.ToString(), OfflineCompiledFlag);
		}
	}
	else
	{
		UE_LOG(LogShaders, Error, TEXT("Archiving failed: no Xcode install on the local machine or a remote Mac."));
	}
	return Id;
}

bool FinalizeLibrary_Metal(FName const& Format, FString const& WorkingDir, FString const& LibraryPath, TSet<uint64> const& Shaders, class FString const& DebugOutputDir)
{
	bool bOK = false;

	FString FullyQualifiedWorkingDir = FPaths::ConvertRelativePathToFull(WorkingDir);

	const FMetalCompilerToolchain* Toolchain = FMetalCompilerToolchain::Get();
	const bool bCompilerAvailable = Toolchain->IsCompilerAvailable();
	EShaderPlatform Platform = FMetalCompilerToolchain::MetalShaderFormatToLegacyShaderPlatform(Format);
	const EAppleSDKType SDK = FMetalCompilerToolchain::MetalShaderPlatformToSDK(Platform);

	if (bCompilerAvailable)
	{
		int32 ReturnCode = 0;
		FString Results;
		FString Errors;
		
		// WARNING: This may be called from multiple threads using the same WorkingDir. All the temporary files must be uniquely named.
		// The local path that will end up with the archive.
		FString LocalArchivePath = FPaths::CreateTempFilename(*FullyQualifiedWorkingDir, TEXT("MetalArchive"), TEXT("")) + TEXT(".metalar");
		IFileManager::Get().Delete(*LocalArchivePath);
		IFileManager::Get().Delete(*LibraryPath);

		UE_LOG(LogMetalShaderCompiler, Display, TEXT("Creating Native Library %s"), *LibraryPath);
	
		bool bArchiveFileValid = false;	
		// Archive build phase - like unix ar, build metal archive from all the object files
		{
			UE_LOG(LogMetalShaderCompiler, Display, TEXT("Archiving %d shaders for shader platform: %s"), Shaders.Num(), *Format.GetPlainNameString());

			/*
				AR type utils can use 'M' scripts
				They look like this:
				CREATE MyArchive.metalar
				ADDMOD Shader1.metal
				ADDMOD Shader2.metal
				SAVE
				END
			*/
			FString M_Script = FString::Printf(TEXT("CREATE \"%s\"\n"), *FPaths::ConvertRelativePathToFull(LocalArchivePath));

			uint32 Index = 0;
			for (auto Shader : Shaders)
			{
				uint32 Len = (Shader >> 32);
				uint32 CRC = (Shader & 0xffffffff);

				FString FileName = FString::Printf(TEXT("Main_%0.8x_%0.8x.o"), Len, CRC);

				UE_LOG(LogMetalShaderCompiler, Verbose, TEXT("[%d/%d] %s %s"), ++Index, Shaders.Num(), *Format.GetPlainNameString(), *FileName);
				FString SourceFilePath = FString::Printf(TEXT("\"%s/%s\""), *FullyQualifiedWorkingDir, *FileName);

				M_Script += FString::Printf(TEXT("ADDMOD %s\n"), *SourceFilePath);
			}

			M_Script += FString(TEXT("SAVE\n"));
			M_Script += FString(TEXT("END\n"));

			FString LocalScriptFilePath = FPaths::CreateTempFilename(*FullyQualifiedWorkingDir, TEXT("MetalArScript"), TEXT(".M"));

			FFileHelper::SaveStringToFile(M_Script, *LocalScriptFilePath);

			if (!FPaths::FileExists(LocalScriptFilePath))
			{
				UE_LOG(LogMetalShaderCompiler, Error, TEXT("Failed to create metal-ar .M script at %s"), *LocalScriptFilePath);
				return false;
			}

			FPaths::MakePlatformFilename(LocalScriptFilePath);
			bool bSuccess = Toolchain->ExecMetalAr(SDK, *LocalScriptFilePath, &ReturnCode, &Results, &Errors);
			bArchiveFileValid = FPaths::FileExists(LocalArchivePath);
					
			if (ReturnCode != 0 || !bArchiveFileValid)
			{
				UE_LOG(LogMetalShaderCompiler, Error, TEXT("Archiving failed: metal-ar failed with code %d: %s %s"), ReturnCode, *Results, *Errors);
				return false;
			}
		}
		
		// Lib build phase, metalar to metallib 
		{
			// handle compile error
			if (ReturnCode == 0 && bArchiveFileValid)
			{
				UE_LOG(LogMetalShaderCompiler, Display, TEXT("Post-processing archive for shader platform: %s"), *Format.GetPlainNameString());

				FString LocalMetalLibPath = LibraryPath;

				if (FPaths::FileExists(LocalMetalLibPath))
				{
					UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Archiving warning: target metallib already exists and will be overwritten: %s"), *LocalMetalLibPath);
				}
			
				FString MetallibParams = FString::Printf(TEXT("-o \"%s\" \"%s\""), *LocalMetalLibPath, *LocalArchivePath);
				ReturnCode = 0;
				Results.Empty();
				Errors.Empty();
				
				bool bSuccess = Toolchain->ExecMetalLib(SDK, *MetallibParams, &ReturnCode, &Results, &Errors);
	
				// handle compile error
				if (bSuccess && ReturnCode == 0)
				{
					check(LocalMetalLibPath == LibraryPath);

					bOK = (IFileManager::Get().FileSize(*LibraryPath) > 0);

					if (!bOK)
					{
						UE_LOG(LogShaders, Error, TEXT("Archiving failed: failed to copy to local destination: %s"), *LibraryPath);
					}
				}
				else
				{
					UE_LOG(LogShaders, Error, TEXT("Archiving failed: metallib failed with code %d: %s %s"), ReturnCode, *Results, *Errors);
				}
			}
			else
			{
				UE_LOG(LogShaders, Error, TEXT("Archiving failed: no valid input for metallib."));
			}
		}
	}
	else
	{
		UE_LOG(LogShaders, Error, TEXT("Archiving failed: no Xcode install."));
	}
	
	return bOK;
}
