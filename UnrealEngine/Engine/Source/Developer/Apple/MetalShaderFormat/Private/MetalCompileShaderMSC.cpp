// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalCompileShaderMSC.h"
#include "MetalShaderCompiler.h"

#include "MetalShaderResources.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "Misc/OutputDeviceRedirector.h"
#include "MetalBackend.h"
#include "RHIDefinitions.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "ShaderCompilerDefinitions.h"
#include "SpirvReflectCommon.h"
#include "ShaderParameterParser.h"

#include <regex>

#if UE_METAL_USE_METAL_SHADER_CONVERTER

#if PLATFORM_MAC

extern void BuildMetalShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	FSHAHash const& GUIDHash,
	const ANSICHAR* InShaderSource,
	uint32 SourceLen,
	uint32 SourceCRCLen,
	uint32 SourceCRC,
	uint32 Version,
	TCHAR const* Standard,
	TCHAR const* MinOSVersion,
	TArray<FShaderCompilerError>& OutErrors,
	uint32 TypedBuffers,
	uint32 InvariantBuffers,
	uint32 TypedUAVs,
	uint32 ConstantBuffers,
	bool bAllowFastIntriniscs,
	uint32 NumCBVs,
	uint32 OutputSizeVS,
	uint32 MaxInputPrimitivesPerMeshThreadgroupGS,
	const bool bUsesDiscard,
	char const* ShaderReflectionJSON,
	FMetalShaderBytecode const& CompiledShaderBytecode
);

#include "ShaderConductorContext.h"

#include "metal_irconverter.h"

#include "d3d12shader.h"
#include "dxc/dxcapi.h"

inline IRShaderStage ShaderFrequencyToStage(const EShaderFrequency UEStage)
{
	switch (UEStage)
	{
	case SF_Vertex        : return IRShaderStageVertex;
	case SF_Mesh          : return IRShaderStageMesh;
	case SF_Amplification : return IRShaderStageAmplification;
	case SF_Pixel         : return IRShaderStageFragment;
	case SF_Geometry      : return IRShaderStageGeometry;
	case SF_Compute       : return IRShaderStageCompute;
	case SF_RayGen        : return IRShaderStageRayGeneration;
	case SF_RayMiss       : return IRShaderStageMiss;
	case SF_RayHitGroup   : return IRShaderStageAnyHit; // TODO: How to distinguish AnyHit/ClosestHit/etc.?
	case SF_RayCallable   : return IRShaderStageCallable;
	default               : checkNoEntry();
	}
	return IRShaderStageInvalid;
}

inline IRShaderVisibility ShaderFrequencyToVisibility(const EShaderFrequency UEStage)
{
	switch (UEStage)
	{
	case SF_Vertex        : return IRShaderVisibilityVertex;
	case SF_Mesh          : return IRShaderVisibilityMesh;
	case SF_Amplification : return IRShaderVisibilityAmplification;
	case SF_Pixel         : return IRShaderVisibilityPixel;
	case SF_Geometry      : return IRShaderVisibilityGeometry;
	case SF_Compute       : return IRShaderVisibilityAll;
	case SF_RayGen        : return IRShaderVisibilityAll;
	case SF_RayMiss       : return IRShaderVisibilityAll;
	case SF_RayHitGroup   : return IRShaderVisibilityAll;
	case SF_RayCallable   : return IRShaderVisibilityAll;
	default               : checkNoEntry();
	}
	return IRShaderVisibilityAll;
}

inline IRResourceType QuantizeD3DResourceType(const D3D_SHADER_INPUT_TYPE Type)
{
	switch (Type)
	{
	case D3D_SIT_CBUFFER:                         return IRResourceTypeCBV;
	case D3D_SIT_TBUFFER:                         return IRResourceTypeCBV;
	case D3D_SIT_TEXTURE:                         return IRResourceTypeSRV;
	case D3D_SIT_SAMPLER:                         return IRResourceTypeSampler;
	case D3D_SIT_UAV_RWTYPED:                     return IRResourceTypeUAV;
	case D3D_SIT_STRUCTURED:                      return IRResourceTypeSRV;
	case D3D_SIT_UAV_RWSTRUCTURED:                return IRResourceTypeUAV;
	case D3D_SIT_BYTEADDRESS:                     return IRResourceTypeSRV;
	case D3D_SIT_UAV_RWBYTEADDRESS:               return IRResourceTypeUAV;
	case D3D_SIT_UAV_APPEND_STRUCTURED:           return IRResourceTypeUAV;
	case D3D_SIT_UAV_CONSUME_STRUCTURED:          return IRResourceTypeUAV;
	case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:   return IRResourceTypeUAV;
	case D3D_SIT_RTACCELERATIONSTRUCTURE:         return IRResourceTypeSRV;
	case D3D_SIT_UAV_FEEDBACKTEXTURE:             return IRResourceTypeUAV;
	default:                                      checkNoEntry();
	}

	return IRResourceTypeInvalid;
}

inline bool IsD3DResourceTypeTyped(const D3D_SHADER_INPUT_TYPE Type)
{
	return Type == D3D_SIT_TBUFFER || Type == D3D_SIT_UAV_RWTYPED;
}

template<IRDescriptorRangeType DescriptorType>
static IRDescriptorRange1 CreateDescriptorRange(const uint32 NumDescriptors)
{
	IRDescriptorRange1 DescRange;
	DescRange.RangeType = DescriptorType;
	DescRange.NumDescriptors = NumDescriptors;
	DescRange.BaseShaderRegister = 0;
	DescRange.RegisterSpace = 0;
	DescRange.OffsetInDescriptorsFromTableStart = IRDescriptorRangeOffsetAppend;

	switch (DescriptorType)
	{
	case IRDescriptorRangeTypeCBV:
	case IRDescriptorRangeTypeSRV:
		DescRange.Flags = IRDescriptorRangeFlagDataStaticWhileSetAtExecute;
		break;
	case IRDescriptorRangeTypeUAV:
		DescRange.Flags = IRDescriptorRangeFlagDataVolatile;
		break;
	case IRDescriptorRangeTypeSampler:
		DescRange.Flags = IRDescriptorRangeFlagNone;
		break;
	default:
		checkNoEntry();
		break;
	}

	return DescRange;
}

static void ProcessReflection(ID3D12ShaderReflection* ShaderReflection, const uint32 BoundResources, const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, CrossCompiler::FHlslccHeaderWriter& CCHeaderWriter, FMetalShaderOutputMetaData& OutputData, uint32& NumCBVs)
{
	NumCBVs = 0;

	uint32 NumSRVs = 0;
	uint32 NumUAVs = 0;
	uint32 NumSamplers = 0;

	const bool bBindlessEnabled = (Input.Environment.CompilerFlags.Contains(CFLAG_BindlessResources) || Input.Environment.CompilerFlags.Contains(CFLAG_BindlessSamplers));
	
	bool bFoundGlobalOrRoot = false;
	bool bFoundGlobal = false;
	bool bFoundRoot = false;
	
	// Build output metadata and collect infos for each resource type ranges.
	for (uint32 ResourceIndex = 0; ResourceIndex < BoundResources; ResourceIndex++)
	{
		D3D12_SHADER_INPUT_BIND_DESC BindDesc;
		ShaderReflection->GetResourceBindingDesc(ResourceIndex, &BindDesc);

		IRResourceType ResourceType = QuantizeD3DResourceType(BindDesc.Type);
		bool bIsResourceTyped = IsD3DResourceTypeTyped(BindDesc.Type);
		const uint32 BindIndex = BindDesc.BindPoint;

		const bool bRootConstantsCB = (FCStringAnsi::Strcmp(BindDesc.Name, "UERootConstants") == 0);
		const bool bIsRootCB = FCString::Strcmp(ANSI_TO_TCHAR(BindDesc.Name), FShaderParametersMetadata::kRootUniformBufferBindingName) == 0;
		
		switch (ResourceType)
		{
		case IRResourceTypeSRV:
			if (bIsResourceTyped)
				OutputData.TypedBuffers |= (1 << BindIndex);
			else
				OutputData.InvariantBuffers |= (1 << BindIndex);

			CCHeaderWriter.WriteSRV(ANSI_TO_TCHAR(BindDesc.Name), BindIndex, BindDesc.BindCount);
			NumSRVs = FMath::Max(NumSRVs, BindIndex + BindDesc.BindCount);
			break;

		case IRResourceTypeUAV:
			if (bIsResourceTyped)
			{
				OutputData.TypedUAVs |= (1 << BindIndex);
				OutputData.TypedBuffers |= (1 << BindIndex);
			}
			else
			{
				OutputData.InvariantBuffers |= (1 << BindIndex);
			}

			CCHeaderWriter.WriteUAV(ANSI_TO_TCHAR(BindDesc.Name), BindIndex, BindDesc.BindCount);
			NumUAVs = FMath::Max(NumUAVs, BindIndex + BindDesc.BindCount);
			break;

		case IRResourceTypeSampler:
			CCHeaderWriter.WriteSamplerState(ANSI_TO_TCHAR(BindDesc.Name), BindIndex);
			NumSamplers = FMath::Max(NumSamplers, BindIndex + BindDesc.BindCount);
			break;

		case IRResourceTypeCBV:
		{
			bool bIsGlobalCB = (FCStringAnsi::Strcmp(BindDesc.Name, "$Globals") == 0);
			
			int32 ConstantBufferSize = 0;
			
			OutputData.ConstantBuffers |= (1 << BindIndex);
			
			// Global uniform buffer - handled specially as we care about the internal layout
			if (bIsGlobalCB || bIsRootCB)
			{
				TCBDMARangeMap CBRanges;
				CCHeaderWriter.WritePackedUB(BindIndex);

				ID3D12ShaderReflectionConstantBuffer* ConstantBuffer = ShaderReflection->GetConstantBufferByName(BindDesc.Name);

				D3D12_SHADER_BUFFER_DESC CBDesc;
				ConstantBuffer->GetDesc(&CBDesc);

				const uint32 CBIndex = BindIndex;

				FString MbrString;

				// Track all of the variables in this constant buffer.
				for (uint32 ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
				{
					ID3D12ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);

					D3D12_SHADER_VARIABLE_DESC VariableDesc;
					Variable->GetDesc(&VariableDesc);

					if (VariableDesc.uFlags & D3D_SVF_USED)
					{
						CCHeaderWriter.WritePackedUBField(ANSI_TO_TCHAR(VariableDesc.Name), VariableDesc.StartOffset, VariableDesc.Size);
						
						const uint32 MbrOffset = VariableDesc.StartOffset / sizeof(float);
						const uint32 MbrSize = VariableDesc.Size / sizeof(float);
						unsigned DestCBPrecision = TEXT('h');
						unsigned SourceOffset = MbrOffset;
						unsigned DestOffset = MbrOffset;
						unsigned DestSize = MbrSize;
						unsigned DestCBIndex = 0;
						InsertRange(CBRanges, BindIndex, SourceOffset, DestSize, DestCBIndex, DestCBPrecision, DestOffset);
						
						{
							HandleReflectedGlobalConstantBufferMember(
													FString(VariableDesc.Name),
													BindIndex,
													VariableDesc.StartOffset,
													VariableDesc.Size,
													Output);
						}

					}
				}
			}
			else
			{
				ID3D12ShaderReflectionConstantBuffer* ConstantBuffer = ShaderReflection->GetConstantBufferByName(BindDesc.Name);

				D3D12_SHADER_BUFFER_DESC CBDesc;
				ConstantBuffer->GetDesc(&CBDesc);

				for (uint32 ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
				{
					ID3D12ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);

					D3D12_SHADER_VARIABLE_DESC VariableDesc;
					Variable->GetDesc(&VariableDesc);

					if (VariableDesc.uFlags & D3D_SVF_USED)
					{
						HandleReflectedUniformBufferConstantBufferMember(
							BindIndex,
							FString(VariableDesc.Name),
							VariableDesc.StartOffset,
							VariableDesc.Size,
							Output
						);
					}
				}
				
				// Regular uniform buffer - we only care about the binding index
				CCHeaderWriter.WriteUniformBlock(ANSI_TO_TCHAR(BindDesc.Name), BindIndex);
				HandleReflectedUniformBuffer(ANSI_TO_TCHAR(BindDesc.Name), BindIndex, Output);
			}
			NumCBVs = FMath::Max(NumCBVs, BindIndex + BindDesc.BindCount);
		}
		break;
		default:
			checkNoEntry();
		};
	}

	// DXIL fetches resources from the resources heaps.
	check(NumSRVs == 0 && NumUAVs == 0 && NumSamplers == 0);
}

static bool ReflectDXILAndBuildDescriptorRanges(const TArray<uint32>& DXILReflection, const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, CrossCompiler::FHlslccHeaderWriter& CCHeaderWriter, FMetalShaderOutputMetaData& OutputData, uint32& NumCBVs)
{
	// Reflect DXIL
	TRefCountPtr<IDxcUtils> Utils;
	HRESULT Result = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(Utils.GetInitReference()));
	if (!SUCCEEDED(Result))
	{
		UE_LOG(LogShaders, Warning, TEXT("Failed to create DxcUtils"));
		return false;
	}

	DxcBuffer ReflBuffer = {0};
	ReflBuffer.Ptr = DXILReflection.GetData();
	ReflBuffer.Size = DXILReflection.Num() * sizeof(uint32_t);
	
	// Stolen from D3DShaderCompilerDXC (do we really need this for Metal?)
	uint32 ShaderRequiresFlags = 0;

	if (!Input.IsRayTracingShader())
	{
		TRefCountPtr<ID3D12ShaderReflection> ShaderReflection;
		Result = Utils->CreateReflection(&ReflBuffer, IID_PPV_ARGS(ShaderReflection.GetInitReference()));
		if (!SUCCEEDED(Result))
		{
			UE_LOG(LogShaders, Warning, TEXT("Failed to create shader reflection (CreateReflection returned 0x%x)"), Result);
			return false;
		}

		D3D12_SHADER_DESC ShaderDesc = {};
		ShaderReflection->GetDesc(&ShaderDesc);

		ProcessReflection(ShaderReflection.GetReference(), ShaderDesc.BoundResources, Input, Output, CCHeaderWriter, OutputData, NumCBVs);

		// Vertex Input
		for (uint32 InputIndex = 0; InputIndex < ShaderDesc.InputParameters; InputIndex++)
		{
			D3D12_SIGNATURE_PARAMETER_DESC SignatureParamDesc;
			ShaderReflection->GetInputParameterDesc(InputIndex, &SignatureParamDesc);

			FString TypeQualifier;
			switch (SignatureParamDesc.ComponentType)
			{
			case D3D_REGISTER_COMPONENT_UINT32:
				TypeQualifier = TEXT("u");
				break;
			case D3D_REGISTER_COMPONENT_SINT32:
				TypeQualifier = TEXT("i");
				break;
			case D3D_REGISTER_COMPONENT_FLOAT32:
				TypeQualifier = TEXT("f");
				break;
			case D3D_REGISTER_COMPONENT_UNKNOWN:
			default:
				checkNoEntry();
				break;
			}

			CCHeaderWriter.WriteInputAttribute(TEXT("in_ATTRIBUTE"), *TypeQualifier, SignatureParamDesc.SemanticIndex, /*bLocationPrefix:*/ false, /*bLocationSuffix:*/ true);
		}

		// Pixel Output
		for (uint32 OutputIndex = 0; OutputIndex < ShaderDesc.OutputParameters; OutputIndex++)
		{
			D3D12_SIGNATURE_PARAMETER_DESC SignatureParamDesc;
			ShaderReflection->GetOutputParameterDesc(OutputIndex, &SignatureParamDesc);

			FString TypeQualifier;
			switch (SignatureParamDesc.ComponentType)
			{
			case D3D_REGISTER_COMPONENT_UINT32:
				TypeQualifier = TEXT("u");
				break;
			case D3D_REGISTER_COMPONENT_SINT32:
				TypeQualifier = TEXT("i");
				break;
			case D3D_REGISTER_COMPONENT_FLOAT32:
				TypeQualifier = TEXT("f");
				break;
			case D3D_REGISTER_COMPONENT_UNKNOWN:
			default:
				checkNoEntry();
				break;
			}

			FString SemanticName = SignatureParamDesc.SemanticName;
			CCHeaderWriter.WriteOutputAttribute(*SemanticName, *TypeQualifier, SignatureParamDesc.SemanticIndex, /*bLocationPrefix:*/ false, /*bLocationSuffix:*/ true);
		}
	}
	else
	{
		check (false);
	}

	return true;
}

struct FMetalShaderParameterParserPlatformConfiguration : public FShaderParameterParser::FPlatformConfiguration
{
	FMetalShaderParameterParserPlatformConfiguration()
		: FShaderParameterParser::FPlatformConfiguration(TEXTVIEW("cbuffer"), EShaderParameterParserConfigurationFlags::UseStableConstantBuffer|EShaderParameterParserConfigurationFlags::SupportsBindless)
	{
	}

	virtual FString GenerateBindlessAccess(EBindlessConversionType BindlessType, FStringView ShaderTypeString, FStringView IndexString) const final
	{
		// GetResourceFromHeap(Type, Index) ResourceDescriptorHeap[Index]
		// GetSamplerFromHeap(Type, Index)  SamplerDescriptorHeap[Index]

		const TCHAR* HeapString = BindlessType == EBindlessConversionType::Sampler ? TEXT("SamplerDescriptorHeap") : TEXT("ResourceDescriptorHeap");

		return FString::Printf(TEXT("%s[%.*s]"),
			HeapString,
			IndexString.Len(), IndexString.GetData()
		);
	}
};

void FMetalCompileShaderMSC::DoCompileMetalShader(
	const FShaderCompilerInput& Input,
	FShaderCompilerOutput& Output,
	const FString& InPreprocessedShader,
	FSHAHash GUIDHash,
	uint32 VersionEnum,
	EMetalGPUSemantics Semantics,
	uint32 MaxUnrollLoops,
	EShaderFrequency Frequency,
	bool bDumpDebugInfo,
	const FString& Standard,
	const FString& MinOSVersion)
{
	int32 IABTier = VersionEnum >= 4 ? Input.Environment.GetCompileArgument(TEXT("METAL_INDIRECT_ARGUMENT_BUFFERS"), 0) : 0;

	Output.bSucceeded = false;

	std::string MetalSource;
	FString MetalErrors;
	
	bool const bZeroInitialise = Input.Environment.CompilerFlags.Contains(CFLAG_ZeroInitialise);
	bool const bBoundsChecks = Input.Environment.CompilerFlags.Contains(CFLAG_BoundsChecking);

	bool bAllowFastIntrinsics = true;

	// WPO requires that we make all multiply/sincos instructions invariant :(
	bool bForceInvariance = Input.Environment.GetCompileArgument(TEXT("USES_WORLD_POSITION_OFFSET"), false);
	
	FMetalShaderOutputMetaData OutputData;
	
	uint32 CRCLen = 0;
	uint32 CRC = 0;
	uint32 SourceLen = 0;
	int32 Result = 0;
	
	struct FMetalResourceTableEntry : FUniformResourceEntry
	{
		FString Name;
		uint32 Size;
		uint32 SetIndex;
		bool bUsed;
	};
	TMap<FString, TArray<FMetalResourceTableEntry>> IABs;

	FString PreprocessedShader = InPreprocessedShader;
	const bool bBindlessEnabled = (Input.Environment.CompilerFlags.Contains(CFLAG_BindlessResources) || Input.Environment.CompilerFlags.Contains(CFLAG_BindlessSamplers));
	
	uint32 NumCBVs = 0;
	const char* ReflectionJSON = nullptr;
	bool bUsesDiscard = false;
	uint32 OutputSizeVS = 0;
	uint32 MaxInputPrimitivesPerMeshThreadgroupGS = 0;
	
	FMetalShaderBytecode MetalBytecode;

#if PLATFORM_MAC || PLATFORM_WINDOWS
	{
		std::string EntryPointNameAnsi(TCHAR_TO_UTF8(*Input.EntryPointName));

		CrossCompiler::FShaderConductorContext CompilerContext;

		// Initialize compilation options for ShaderConductor
		CrossCompiler::FShaderConductorOptions Options;

		Options.TargetEnvironment = CrossCompiler::FShaderConductorOptions::ETargetEnvironment::Vulkan_1_2;

		// Enable HLSL 2021 if specified
		if (Input.Environment.CompilerFlags.Contains(CFLAG_HLSL2021))
		{
			Options.HlslVersion = 2021;
		}
		
		Options.bEnable16bitTypes = true;
		
		FMetalShaderParameterParserPlatformConfiguration PlatformConfiguration;
		FShaderParameterParser ShaderParameterParser(PlatformConfiguration);
		if (!ShaderParameterParser.ParseAndModify(Input, Output.Errors, PreprocessedShader))
		{
			// The FShaderParameterParser will add any relevant errors.
			return;
		}
		
		// Load shader source into compiler context
		CompilerContext.LoadSource(PreprocessedShader, Input.VirtualSourceFilePath, Input.EntryPointName, Frequency);

		// Rewrite HLSL source code to remove unused global resources and variables
		Options.bRemoveUnusedGlobals = true;
		if (!CompilerContext.RewriteHlsl(Options, &PreprocessedShader))
		{
			CompilerContext.FlushErrors(Output.Errors);
		}
		Options.bRemoveUnusedGlobals = false;

		// Convert shader source to ANSI string
		std::string SourceData(CompilerContext.GetSourceString(), static_cast<size_t>(CompilerContext.GetSourceLength()));

		// Replace special case texture "gl_LastFragData" by native subpass fetch operation
		static const uint32 MaxMetalSubpasses = 8;
		uint32 SubpassInputsDim[MaxMetalSubpasses];

		bool bSourceDataWasModified = PatchSpecialTextureInHlslSource(SourceData, SubpassInputsDim, MaxMetalSubpasses);
		
		// If source data was modified, reload it into the compiler context
		if (bSourceDataWasModified)
		{
			CompilerContext.LoadSource(FAnsiStringView(SourceData.c_str(), SourceData.length()), Input.VirtualSourceFilePath, Input.EntryPointName, Frequency);
		}

		if (bDumpDebugInfo)
		{
			DumpDebugShaderText(Input, &SourceData[0], SourceData.size(), TEXT("rewritten.hlsl"));
		}
		
		CrossCompiler::FHlslccHeaderWriter CCHeaderWriter;

		FString ALNString;
		FString RTString;
		uint32 IABOffsetIndex = 0;
		uint64 BufferIndices = 0xffffffffffffffff;
		// Make sure int64 atomics and dynamic heap indexing are available.
		Options.ShaderModel = {6, 6};
		
		// Compile HLSL source to DXIL binary
		TArray<uint32> DxilData;
		if (!CompilerContext.CompileHlslToDxil(Options, DxilData))
		{
			Result = 0;
			
			UE_LOG(LogShaders, Error, TEXT("Failed to produce DXIL bytecode for '%s' '%s'!"), *Input.EntryPointName, *Input.DumpDebugInfoPath);
			CompilerContext.FlushErrors(Output.Errors);
			
			for (const FShaderCompilerError& Error : Output.Errors)
			{
				UE_LOG(LogShaders, Error, TEXT("%s"), *Error.GetErrorStringWithLineMarker());
			}
			
			Output.bSucceeded = false;
			
			return;
		}
		
		if (bDumpDebugInfo)
		{
			DumpDebugShaderBinary(Input, DxilData.GetData(), DxilData.Num() * sizeof(uint32), TEXT("dxil"));
		}
		
		Result = 1;
		
		ANSICHAR MainCRC[25];
		CRCLen = DxilData.Num() * sizeof( uint32_t );
		CRC = FCrc::MemCrc_DEPRECATED(DxilData.GetData(), CRCLen);
		FCStringAnsi::Snprintf(MainCRC, 25, "Main_%0.8x_%0.8x", CRCLen, CRC);
		
		// Build shader metadata and root signature parameters
		bool bSuccessfulReflection = ReflectDXILAndBuildDescriptorRanges(DxilData, Input, Output, CCHeaderWriter, OutputData, NumCBVs);
		check(bSuccessfulReflection);
		
		// Build root parameters
		const IRShaderVisibility ShaderVisibility = ShaderFrequencyToVisibility(Frequency);
		
		// Bind CBVs as root parameters (this way, we avoid creating a descriptor table and an extra indirection at runtime).
		TArray<IRRootParameter1> RootParams;
		for (uint32 i = 0; i < NumCBVs; i++)
		{
			IRRootParameter1 RootParam;
			RootParam.ParameterType = IRRootParameterTypeCBV;
			RootParam.ShaderVisibility = ShaderVisibility;
			RootParam.Descriptor.ShaderRegister = i;
			RootParam.Descriptor.RegisterSpace = 0;
			RootParam.Descriptor.Flags = IRRootDescriptorFlagDataStaticWhileSetAtExecute;
			
			RootParams.Add(RootParam);
		}
		
		// Create the root signature for air generation.
		IRVersionedRootSignatureDescriptor RootSignatureDesc;
		RootSignatureDesc.version = IRRootSignatureVersion_1_1;
		RootSignatureDesc.desc_1_1.Flags = IRRootSignatureFlagNone;
		RootSignatureDesc.desc_1_1.pStaticSamplers = nullptr;
		RootSignatureDesc.desc_1_1.NumStaticSamplers = 0;
		RootSignatureDesc.desc_1_1.pParameters = RootParams.GetData();
		RootSignatureDesc.desc_1_1.NumParameters = RootParams.Num();
		
		IRError* RootSignatureCreationError = nullptr;
		IRRootSignature* RootSignature = IRRootSignatureCreateFromDescriptor(&RootSignatureDesc, &RootSignatureCreationError);
		if (RootSignature == nullptr || RootSignatureCreationError != nullptr)
		{
			FShaderCompilerError Error(FString::Printf(TEXT("Error: MetalShaderConverter failed to create a root signature for '%s' (error code: %u)!"), *Input.EntryPointName, IRErrorGetCode(RootSignatureCreationError)));
			Output.Errors.Add(Error);
			Output.bSucceeded = false;
			
			return;
		}
		
		// Convert DXIL to air
		IRObject* DXILBytecode = IRObjectCreateFromDXIL(reinterpret_cast<const uint8_t*>(DxilData.GetData()), DxilData.Num() * sizeof(uint32), IRBytecodeOwnershipCopy);
		
		IRCompiler* CompilerInstance = IRCompilerCreate();
		IRCompilerSetEntryPointName(CompilerInstance, MainCRC);
		IRCompilerSetGlobalRootSignature(CompilerInstance, RootSignature);
		IRCompilerSetStageInGenerationMode(CompilerInstance, IRStageInCodeGenerationModeUseSeparateStageInFunction);
		IRCompilerSetCompatibilityFlags(CompilerInstance, IRCompatibilityFlagBoundsCheck);
		
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		IRCompilerEnableGeometryAndTessellationEmulation(CompilerInstance, Input.Environment.CompilerFlags.Contains(CFLAG_VertexToGeometryShader));
#endif
		 
		// TODO: Is there a flag we could check to avoid this string lookup?
		bool bUsesDualSourceBlending = (SourceData.find("vk::location") != std::string::npos);
		if (bUsesDualSourceBlending)
		{
			IRCompilerSetDualSourceBlendingConfiguration(CompilerInstance, IRDualSourceBlendingConfigurationForceEnabled);
		}
		
		// Uncomment to enable IR validation.
		//IRCompilerSetValidationFlags(CompilerInstance, IRCompilerValidationFlagAll);
		
		IRError* CompileError = nullptr;
		IRObject* AirBytecode = IRCompilerAllocCompileAndLink(CompilerInstance, nullptr, DXILBytecode, &CompileError);
		if (!AirBytecode || CompileError != nullptr)
		{
			FShaderCompilerError Error(FString::Printf(TEXT("Error: MetalShaderConverter failed to produce air bytecode for '%s' (error code: %u)!"), *Input.EntryPointName, IRErrorGetCode(CompileError)));
			Output.Errors.Add(Error);
			Output.bSucceeded = false;
			
			return;
		}
		const IRShaderStage ShaderStage = ShaderFrequencyToStage(Frequency);
		
		// Reflect air
		bool bNeedsAirReflection = (ShaderStage == IRShaderStageVertex || ShaderStage == IRShaderStageFragment || ShaderStage == IRShaderStageCompute
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
									|| ShaderStage == IRShaderStageGeometry
#endif
		);
		
		if (bNeedsAirReflection)
		{
			IRShaderReflection* AirReflection = IRShaderReflectionCreate();
			IRObjectGetReflection(AirBytecode, ShaderStage, AirReflection);
			
			if(bDumpDebugInfo)
			{
				ReflectionJSON = IRShaderReflectionAllocStringAndSerialize(AirReflection);
				FString ReflectionString = ANSI_TO_TCHAR(ReflectionJSON);
				DumpDebugShaderText(Input, ReflectionString, TEXT("reflection.json"));
				checkSlow(ReflectionJSON);
			}
			
			switch (ShaderStage)
			{
				case IRShaderStageVertex:
				{
					// Retrieve VS infos only if GS emulation is used (VS output size is useless otherwise).
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
					IRVersionedVSInfo Info;
					bool bSuccessfulReflectionVS = IRShaderReflectionCopyVertexInfo(AirReflection, IRReflectionVersion_1_0, &Info);
					check(bSuccessfulReflectionVS);
					
					OutputSizeVS = Info.info_1_0.vertex_output_size_in_bytes;
					
					IRShaderReflectionReleaseVertexInfo(&Info);
#endif
					
					if(!ReflectionJSON)
					{
						// Serialize Reflection for vs (required to generate stage_in functions at PSO creation-time)
						ReflectionJSON = IRShaderReflectionAllocStringAndSerialize(AirReflection);
						checkSlow(ReflectionJSON);
					}
					break;
				}
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
				case IRShaderStageGeometry:
					{
						IRVersionedGSInfo Info;
						bool bSuccessfulReflectionGS = IRShaderReflectionCopyGeometryInfo(AirReflection, IRReflectionVersion_1_0, &Info);
						check(bSuccessfulReflectionGS);
						
						MaxInputPrimitivesPerMeshThreadgroupGS = Info.info_1_0.max_input_primitives_per_mesh_threadgroup;
						
						IRShaderReflectionReleaseGeometryInfo(&Info);
					}
					break;
#endif
				case IRShaderStageFragment:
					{
						IRVersionedFSInfo Info;
						bool bSuccessfulReflectionPS = IRShaderReflectionCopyFragmentInfo(AirReflection, IRReflectionVersion_1_0, &Info);
						check(bSuccessfulReflectionPS);
						
						bUsesDiscard = Info.info_1_0.discards;
						
						IRShaderReflectionReleaseFragmentInfo(&Info);
					}
					break;
					
				case IRShaderStageCompute:
					{
						IRVersionedCSInfo Info;
						bool bSuccessfulReflectionCS = IRShaderReflectionCopyComputeInfo(AirReflection, IRReflectionVersion_1_0, &Info);
						check(bSuccessfulReflectionCS);
						
						CCHeaderWriter.WriteNumThreads(Info.info_1_0.tg_size[0], Info.info_1_0.tg_size[1], Info.info_1_0.tg_size[2]);
						
						IRShaderReflectionReleaseComputeInfo(&Info);
					}
					break;
				default:
					break;
			}
			IRShaderReflectionDestroy(AirReflection);
		}
		
		// Retrieve the generated .metallib
		IRMetalLibBinary* GeneratedMetalLib = IRMetalLibBinaryCreate();
		if (!IRObjectGetMetalLibBinary(AirBytecode, ShaderStage, GeneratedMetalLib))
		{
			FShaderCompilerError Error(FString::Printf(TEXT("Error: MetalShaderConverter failed to produce a metallib for '%s'!"), *Input.EntryPointName));
			Output.Errors.Add(Error);
			Output.bSucceeded = false;
			
			return;
		}
		
		size_t MetalLibSize = IRMetalLibGetBytecodeSize(GeneratedMetalLib);
		
		MetalBytecode.OutputFile.Reserve(MetalLibSize);
		MetalBytecode.OutputFile.SetNum(MetalLibSize);
		
		size_t OutMetalLibSize = IRMetalLibGetBytecode(GeneratedMetalLib, reinterpret_cast<uint8_t*>(MetalBytecode.OutputFile.GetData()));
		//checkSlow(OutMetalLibSize != MetalLibSize);
		
		MetalBytecode.ObjectFile.SetNum(MetalLibSize);
		
		// Copy the AIR (needed for serialization below)
		MetalBytecode.ObjectFile = MetalBytecode.OutputFile;
		IRRootSignatureDestroy(RootSignature);
		
		IRMetalLibBinaryDestroy(GeneratedMetalLib);
		IRObjectDestroy(AirBytecode);
		IRObjectDestroy(DXILBytecode);
		IRCompilerDestroy(CompilerInstance);
		
		CCHeaderWriter.WriteSourceInfo(*Input.VirtualSourceFilePath, *Input.EntryPointName);
		CCHeaderWriter.WriteCompilerInfo();
		
		FString MetaData = CCHeaderWriter.ToString();
		MetaData += RTString;
		MetaData += TEXT("\n\n");
		if (ALNString.Len())
		{
			MetaData += TEXT("// Attributes: ");
			MetaData += ALNString;
			MetaData += TEXT("\n\n");
		}
		
		MetalSource = TCHAR_TO_UTF8(*MetaData);
		
		if (bDumpDebugInfo)
		{
			DumpDebugShaderBinary(Input, MetalBytecode.ObjectFile.GetData(), MetalBytecode.ObjectFile.Num() * sizeof(uint8), TEXT("air"));
		}
	}
#endif

	// Attribute [[clang::optnone]] causes performance hit with WPO on M1 Macs => replace with empty space
	const std::string ClangOptNoneString = "[[clang::optnone]]";
	for (size_t Begin = 0, End = 0; (Begin = MetalSource.find(ClangOptNoneString, End)) != std::string::npos; End = Begin)
	{
		MetalSource.replace(Begin, ClangOptNoneString.length(), " ");
	}
	
	if (bDumpDebugInfo && !MetalSource.empty())
	{
		DumpDebugShaderText(Input, &MetalSource[0], MetalSource.size(), TEXT("metal"));
	}

	if (Result != 0)
	{
		Output.Target = Input.Target;
		BuildMetalShaderOutput(Output, Input, GUIDHash, MetalSource.c_str(), MetalSource.length(), CRCLen, CRC, VersionEnum, *Standard, *MinOSVersion, Output.Errors, OutputData.TypedBuffers, OutputData.InvariantBuffers, OutputData.TypedUAVs, OutputData.ConstantBuffers, bAllowFastIntrinsics
		  , NumCBVs, OutputSizeVS, MaxInputPrimitivesPerMeshThreadgroupGS, bUsesDiscard, ReflectionJSON, MetalBytecode
		);
	}
	else
	{
		// Log errors on failed compilation in this backend only when compiling from a debug dump USF.
		if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::CompileFromDebugUSF))
		{
			for (const FShaderCompilerError& Error : Output.Errors)
			{
				UE_LOG(LogShaders, Error, TEXT("%s"), *Error.GetErrorStringWithLineMarker());
			}
			GLog->Flush();
		}
	}
}

#else

// TODO: Remove this when we have a dll version of MetalShaderConverter
void FMetalCompileShaderMSC::DoCompileMetalShader(
	const FShaderCompilerInput& Input,
	FShaderCompilerOutput& Output,
	const FString& InPreprocessedShader,
	FSHAHash GUIDHash,
	uint32 VersionEnum,
	EMetalGPUSemantics Semantics,
	uint32 MaxUnrollLoops,
	EShaderFrequency Frequency,
	bool bDumpDebugInfo,
	const FString& Standard,
	const FString& MinOSVersion)
{
	UE_LOG(LogShaders, Error, TEXT("Metal Shader Converter is currently unsupported on platforms other than MacOS"));
}

#endif // PLATFORM_MAC
#endif // UE_METAL_USE_METAL_SHADER_CONVERTER
