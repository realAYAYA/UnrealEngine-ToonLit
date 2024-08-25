// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalBaseShader.h: Metal RHI Base Shader Class Template.
=============================================================================*/

#pragma once

#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Shaders/Debugging/MetalShaderDebugCache.h"
#include "Shaders/MetalCompiledShaderKey.h"
#include "Shaders/MetalCompiledShaderCache.h"
#include "RHICoreShader.h"

//------------------------------------------------------------------------------

#pragma mark - Metal RHI Base Shader Class Support Routines


extern MTL::LanguageVersion ValidateVersion(uint32 Version);


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Base Shader Class Template Defines


/** Set to 1 to enable shader debugging (makes the driver save the shader source) */
#define DEBUG_METAL_SHADERS (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Base Shader Class Template

template<typename BaseResourceType, int32 ShaderType>
class TMetalBaseShader : public BaseResourceType, public IRefCountedObject
{
public:
	enum
	{
		StaticFrequency = ShaderType
	};

	TMetalBaseShader()
	{
		// void
	}

	virtual ~TMetalBaseShader()
	{
		this->Destroy();
	}

	void Init(TArrayView<const uint8> InCode, FMetalCodeHeader& Header, MTLLibraryPtr InLibrary);
	void Destroy();

	/**
	 * Gets the Metal source code as an NSString if available or nullptr if not.  Note
	 * that this will dynamically decompress from compressed data on first
	 * invocation.
	 */
	NS::String* GetSourceCode();

	// IRefCountedObject interface:
	virtual uint32 AddRef() const override final;
	virtual uint32 Release() const override final;
	virtual uint32 GetRefCount() const override final;

	/** External bindings for this shader. */
	FMetalShaderBindings Bindings;

	// List of memory copies from RHIUniformBuffer to packed uniforms
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;

	/* Argument encoders for shader IABs */
	TMap<uint32, MTL::ArgumentEncoder*> ArgumentEncoders;

	/* Tier1 Argument buffer bitmasks */
	TMap<uint32, TBitArray<>> ArgumentBitmasks;

	/* Uniform buffer static slots */
	TArray<FUniformBufferStaticSlot> StaticSlots;

	/** The binding for the buffer side-table if present */
	int32 SideTableBinding = -1;

	/** CRC & Len for name disambiguation */
	uint32 SourceLen = 0;
	uint32 SourceCRC = 0;

	/** Hash for the shader/material permutation constants */
	uint32 ConstantValueHash = 0;

protected:
	MTLFunctionPtr GetCompiledFunction(bool const bAsync = false, const int32 FunctionIndex = -1);

	// this is the compiler shader
	MTLFunctionPtr Function;

private:
	// This is the MTLLibrary for the shader so we can dynamically refine the MTLFunction
	MTLLibraryPtr Library;

	/** The debuggable text source */
	NS::String* GlslCodeNSString = nullptr;

	/** The compressed text source */
	TArray<uint8> CompressedSource;

	/** The uncompressed text source size */
	uint32 CodeSize = 0;

	// Function constant states
	bool bHasFunctionConstants = false;
	bool bDeviceFunctionConstants = false;

    /** Index of the function (in the library) pointing to the function requested by the user (when GetCompiledFunction() is called with an explicit index). */
    uint32 LibraryFunctionIndex = -1;
};


//------------------------------------------------------------------------------

#pragma mark - Metal RHI Base Shader Class Template Member Functions


template<typename BaseResourceType, int32 ShaderType>
void TMetalBaseShader<BaseResourceType, ShaderType>::Init(TArrayView<const uint8> InShaderCode, FMetalCodeHeader& Header, MTLLibraryPtr InLibrary)
{
	FShaderCodeReader ShaderCode(InShaderCode);

	FMemoryReaderView Ar(InShaderCode, true);

	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	// was the shader already compiled offline?
	uint8 OfflineCompiledFlag;
	Ar << OfflineCompiledFlag;
	check(OfflineCompiledFlag == 0 || OfflineCompiledFlag == 1);

	// get the header
	Ar << Header;

	ValidateVersion(Header.Version);

	SourceLen = Header.SourceLen;
	SourceCRC = Header.SourceCRC;

	// If this triggers than a level above us has failed to provide valid shader data and the cook is probably bogus
	UE_CLOG(Header.SourceLen == 0 || Header.SourceCRC == 0, LogMetal, Fatal, TEXT("Invalid Shader Bytecode provided."));

	bDeviceFunctionConstants = Header.bDeviceFunctionConstants;

	// remember where the header ended and code (precompiled or source) begins
	int32 CodeOffset = Ar.Tell();
	uint32 BufferSize = ShaderCode.GetActualShaderCodeSize() - CodeOffset;
	const ANSICHAR* SourceCode = (ANSICHAR*)InShaderCode.GetData() + CodeOffset;

	// Only archived shaders should be in here.
	UE_CLOG(InLibrary && !(Header.CompileFlags & (1 << CFLAG_Archive)), LogMetal, Warning, TEXT("Shader being loaded wasn't marked for archiving but a MTLLibrary was provided - this is unsupported."));

	if (!OfflineCompiledFlag)
	{
		UE_LOG(LogMetal, Display, TEXT("Loaded a text shader (will be slower to load)"));
	}

	bool bOfflineCompile = (OfflineCompiledFlag > 0);

	const ANSICHAR* ShaderSource = ShaderCode.FindOptionalData(EShaderOptionalDataKey::SourceCode);
	bool bHasShaderSource = (ShaderSource && FCStringAnsi::Strlen(ShaderSource) > 0);

	static bool bForceTextShaders = FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUTrace);
	if (!bHasShaderSource)
	{
		int32 LZMASourceSize = 0;
		int32 SourceSize = 0;
		const uint8* LZMASource = ShaderCode.FindOptionalDataAndSize(EShaderOptionalDataKey::CompressedDebugCode, LZMASourceSize);
		const uint8* UnSourceLen = ShaderCode.FindOptionalDataAndSize(EShaderOptionalDataKey::UncompressedSize, SourceSize);
		if (LZMASource && LZMASourceSize > 0 && UnSourceLen && SourceSize == sizeof(uint32))
		{
			CompressedSource.Append(LZMASource, LZMASourceSize);
			memcpy(&CodeSize, UnSourceLen, sizeof(uint32));
			bHasShaderSource = false;
		}
#if !UE_BUILD_SHIPPING
		else if(bForceTextShaders)
		{
            GlslCodeNSString = FMetalShaderDebugCache::Get().GetShaderCode(SourceLen, SourceCRC);
            check(GlslCodeNSString);
            GlslCodeNSString->retain();
		}
#endif
		if (bForceTextShaders && CodeSize && CompressedSource.Num())
		{
			bHasShaderSource = (GetSourceCode() != nullptr);
		}
	}
	else if (bOfflineCompile && bHasShaderSource)
	{
		GlslCodeNSString = NS::String::string(ShaderSource, NS::UTF8StringEncoding);
		check(GlslCodeNSString);
        
		GlslCodeNSString->retain();
	}

	bHasFunctionConstants = (Header.bDeviceFunctionConstants);

	ConstantValueHash = 0;

	Library = InLibrary;

	bool bNeedsCompiling = false;

	// Find the existing compiled shader in the cache.
	uint32 FunctionConstantHash = ConstantValueHash;
	FMetalCompiledShaderKey Key(Header.SourceLen, Header.SourceCRC, FunctionConstantHash);

	Function = GetMetalCompiledShaderCache().FindRef(Key);
	if (!Library && Function)
	{
		Library = GetMetalCompiledShaderCache().FindLibrary(Function);
	}
	else
	{
		bNeedsCompiling = true;
	}

	Bindings = Header.Bindings;
	if (bNeedsCompiling || !Library)
	{
		if (bOfflineCompile METAL_DEBUG_OPTION(&& !(bHasShaderSource && bForceTextShaders)))
		{
			if (InLibrary)
			{
				Library = InLibrary;
			}
			else
			{
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewLibraryBinary: %d_%d"), SourceLen, SourceCRC)));

				// Archived shaders should never get in here.
				check(!(Header.CompileFlags & (1 << CFLAG_Archive)) || BufferSize > 0);

				// allow GCD to copy the data into its own buffer
				//		dispatch_data_t GCDBuffer = dispatch_data_create(InShaderCode.GetTypedData() + CodeOffset, ShaderCode.GetActualShaderCodeSize() - CodeOffset, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
				NS::Error* AError;
				void* Buffer = FMemory::Malloc( BufferSize );
				FMemory::Memcpy( Buffer, InShaderCode.GetData() + CodeOffset, BufferSize );
				dispatch_data_t GCDBuffer = dispatch_data_create(Buffer, BufferSize, dispatch_get_main_queue(), ^(void) { FMemory::Free(Buffer); } );

				// load up the already compiled shader
				Library = NS::TransferPtr(GetMetalDeviceContext().GetDevice()->newLibrary(GCDBuffer, &AError));
				dispatch_release(GCDBuffer);

				if (!Library)
				{
                    UE_LOG(LogMetal, Display, TEXT("Failed to create library: %s"), *NSStringToFString(AError->description()));
				}
			}
		}
		else
		{
			METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewLibrarySource: %d_%d"), SourceLen, SourceCRC)));
			NS::String* ShaderString = ((OfflineCompiledFlag == 0) ? NS::String::string(SourceCode, NS::UTF8StringEncoding) : GlslCodeNSString);

            FString FinalShaderString(TEXT(""));
			const FString ShaderName = ShaderCode.FindOptionalData(FShaderCodeName::Key);
			if(ShaderName.Len())
			{
                FinalShaderString = FString::Printf(TEXT("// %s\n"), *ShaderName);
			}

            FinalShaderString += NSStringToFString(ShaderString);
            FinalShaderString.Replace(TEXT("#pragma once"), TEXT(""));
            NS::String* NewShaderString = FStringToNSString(FinalShaderString);

            MTL::CompileOptions* CompileOptions = MTL::CompileOptions::alloc()->init();
            check(CompileOptions);

#if DEBUG_METAL_SHADERS
			static bool bForceFastMath = FParse::Param(FCommandLine::Get(), TEXT("metalfastmath"));
			static bool bForceNoFastMath = FParse::Param(FCommandLine::Get(), TEXT("metalnofastmath"));
			if (bForceNoFastMath)
			{
				CompileOptions->setFastMathEnabled(NO);
			}
			else if (bForceFastMath)
			{
				CompileOptions->setFastMathEnabled(YES);
			}
			else
#endif
			{
				CompileOptions->setFastMathEnabled((BOOL)(!(Header.CompileFlags & (1 << CFLAG_NoFastMath))));
			}

#if !PLATFORM_MAC || DEBUG_METAL_SHADERS
            NS::Dictionary* PreprocessorMacros = nullptr;;
#if !PLATFORM_MAC // Pretty sure that as_type-casts work on macOS, but they don't for half2<->uint on older versions of the iOS runtime compiler.
			PreprocessorMacros = NS::Dictionary::dictionary(NS::String::string("1", NS::UTF8StringEncoding),
                                                            NS::String::string("METAL_RUNTIME_COMPILER", NS::UTF8StringEncoding));
#endif
#if DEBUG_METAL_SHADERS
            PreprocessorMacros = NS::Dictionary::dictionary(NS::String::string("1", NS::UTF8StringEncoding),
                                                            NS::String::string("MTLSL_ENABLE_DEBUG_INFO", NS::UTF8StringEncoding));
#endif
            if(PreprocessorMacros)
            {
                CompileOptions->setPreprocessorMacros(PreprocessorMacros);
                PreprocessorMacros->release();
            }
#endif

			MTL::LanguageVersion MetalVersion;
			switch (Header.Version)
			{
                case 8:
                    MetalVersion = MTL::LanguageVersion3_0;
                    break;
				case 7:
					MetalVersion = MTL::LanguageVersion2_4;
					break;
#if PLATFORM_MAC
				case 6:
					MetalVersion = MTL::LanguageVersion2_3;
					break;
				case 5:
					// Fall through
				case 0:
					// Fall through
				default:
					MetalVersion = MTL::LanguageVersion2_2;
					break;
#else
				case 0:
                    MetalVersion = MTL::LanguageVersion2_4;
                    break;
				default:
					UE_LOG(LogRHI, Fatal, TEXT("Failed to create shader with unknown version %d: %s"), Header.Version, *NSStringToFString(NewShaderString));
					MetalVersion = MTL::LanguageVersion2_4;
					break;
#endif
			}
			CompileOptions->setLanguageVersion(MetalVersion);

			if(ShaderType == SF_Vertex && MetalVersion > MTL::LanguageVersion2_2)
			{
				CompileOptions->setPreserveInvariance(YES);
			}

			NS::Error* Error = nullptr;
			Library = NS::TransferPtr(GetMetalDeviceContext().GetDevice()->newLibrary(NewShaderString, CompileOptions, &Error));
			if (Library.get() == nullptr)
			{
				UE_LOG(LogRHI, Error, TEXT("*********** Error\n%s"), *NSStringToFString(NewShaderString));
				UE_LOG(LogRHI, Fatal, TEXT("Failed to create shader: %s"), *NSStringToFString(Error->description()));
			}
			else if (Error != nullptr)
			{
				// Warning...
				UE_LOG(LogRHI, Warning, TEXT("*********** Warning\n%s"), *NSStringToFString(NewShaderString));
				UE_LOG(LogRHI, Warning, TEXT("Created shader with warnings: %s"), *NSStringToFString(Error->description()));
			}

			GlslCodeNSString = NewShaderString;
			GlslCodeNSString->retain();
		}

		GetCompiledFunction(true);
	}
	UniformBuffersCopyInfo = Header.UniformBuffersCopyInfo;
	SideTableBinding = Header.SideTable;

	UE::RHICore::InitStaticUniformBufferSlots(StaticSlots, Bindings.ShaderResourceTable);

#if RHI_INCLUDE_SHADER_DEBUG_DATA
    this->Debug.ShaderName = FString::Printf(TEXT("Main_%0.8x_%0.8x"), Header.SourceLen, Header.SourceCRC);
#endif
}

template<typename BaseResourceType, int32 ShaderType>
void TMetalBaseShader<BaseResourceType, ShaderType>::Destroy()
{
    if(GlslCodeNSString)
    {
        GlslCodeNSString->release();
        GlslCodeNSString = nullptr;
    }
}

template<typename BaseResourceType, int32 ShaderType>
inline NS::String* TMetalBaseShader<BaseResourceType, ShaderType>::GetSourceCode()
{
	if (!GlslCodeNSString && CodeSize && CompressedSource.Num())
	{
		GlslCodeNSString = DecodeMetalSourceCode(CodeSize, CompressedSource);
	}
	if (!GlslCodeNSString)
	{
        FString ShaderString = FString::Printf(TEXT("Hash: %s, Name: Main_%0.8x_%0.8x"), *BaseResourceType::GetHash().ToString(), SourceLen, SourceCRC);
        GlslCodeNSString = FStringToNSString(ShaderString);
        GlslCodeNSString->retain();
	}
	return GlslCodeNSString;
}

template<typename BaseResourceType, int32 ShaderType>
uint32 TMetalBaseShader<BaseResourceType, ShaderType>::AddRef() const
{
	return FRHIResource::AddRef();
}

template<typename BaseResourceType, int32 ShaderType>
uint32 TMetalBaseShader<BaseResourceType, ShaderType>::Release() const
{
	return FRHIResource::Release();
}

template<typename BaseResourceType, int32 ShaderType>
uint32 TMetalBaseShader<BaseResourceType, ShaderType>::GetRefCount() const
{
	return FRHIResource::GetRefCount();
}

template<typename BaseResourceType, int32 ShaderType>
MTLFunctionPtr TMetalBaseShader<BaseResourceType, ShaderType>::GetCompiledFunction(bool const bAsync, const int32 FunctionIndex)
{
	MTLFunctionPtr Func = Function;

	bool bNeedToRecreateFunction = (LibraryFunctionIndex != FunctionIndex);
    if (!Func || bNeedToRecreateFunction)
	{
		// Find the existing compiled shader in the cache.
		uint32 FunctionConstantHash = ConstantValueHash;
		FMetalCompiledShaderKey Key(SourceLen, SourceCRC, FunctionConstantHash);
		Func = Function = GetMetalCompiledShaderCache().FindRef(Key);

        if (bNeedToRecreateFunction)
        {
            Function = MTLFunctionPtr();
            Func = MTLFunctionPtr();
            LibraryFunctionIndex = FunctionIndex;
        }

		if (!Func)
		{
			// Get the function from the library - the function name is "Main" followed by the CRC32 of the source MTLSL as 0-padded hex.
			// This ensures that even if we move to a unified library that the function names will be unique - duplicates will only have one entry in the library.
            NS::String* Name = (LibraryFunctionIndex != -1)
            ? (NS::String*)Library->functionNames()->object(LibraryFunctionIndex) : FStringToNSString(FString::Printf(TEXT("Main_%0.8x_%0.8x"), SourceLen, SourceCRC));
			MTL::FunctionConstantValues* ConstantValues = nullptr;
			ON_SCOPE_EXIT { if (ConstantValues){ ConstantValues->release(); } };
			if (bHasFunctionConstants)
			{
				ConstantValues = MTL::FunctionConstantValues::alloc()->init();
                check(ConstantValues);

				if (bDeviceFunctionConstants)
				{
					// Index 33 is the device vendor id constant
					ConstantValues->setConstantValue((void*)&GRHIVendorId, MTL::DataTypeUInt, NS::String::string("GMetalDeviceManufacturer", NS::UTF8StringEncoding));
				}
			}

#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
            // TODO: Make those constants optional?
            static constexpr bool bEnableTessellation = false;
            
            ConstantValues = MTL::FunctionConstantValues::alloc()->init();
            bHasFunctionConstants = true;
            
            ConstantValues->setConstantValue(&bEnableTessellation, MTL::DataTypeBool,
											 NS::String::string("tessellationEnabled", NS::UTF8StringEncoding));
			ConstantValues->setConstantValue(&Bindings.OutputSizeVS, MTL::DataTypeInt,
											 NS::String::string("vertex_shader_output_size_fc", NS::UTF8StringEncoding));
#endif

			if (!bHasFunctionConstants || !bAsync)
			{
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewFunction: %s"), *NSStringToFString(Name))));
				if (!bHasFunctionConstants)
				{
					Function = NS::TransferPtr(Library->newFunction(Name));
				}
				else
				{
					NS::Error* Error;
					Function = NS::TransferPtr(Library->newFunction(Name, ConstantValues, &Error));
					UE_CLOG(Function.get() == nullptr, LogMetal, Error, TEXT("Failed to create function: %s"), *NSStringToFString(Error->description()));
					UE_CLOG(Function.get() == nullptr, LogMetal, Fatal, TEXT("*********** Error\n%s"), *NSStringToFString(GetSourceCode()));
				}

				check(Function);
				GetMetalCompiledShaderCache().Add(Key, Library, Function);

				Func = Function;
			}
			else
			{
				METAL_GPUPROFILE(FScopedMetalCPUStats CPUStat(FString::Printf(TEXT("NewFunctionAsync: %s"), *NSStringToFString(Name))));
				METAL_GPUPROFILE(uint64 CPUStart = CPUStat.Stats ? CPUStat.Stats->CPUStartTime : 0);
#if ENABLE_METAL_GPUPROFILE
                NS::String* nsName = Name;
                const std::function<void(MTL::Function* pFunction, NS::Error* pError)> Handler = [Key, this, CPUStart, nsName](MTL::Function* NewFunction, NS::Error* Error){
#else
                const std::function<void(MTL::Function* pFunction, NS::Error* pError)> Handler = [Key, this](MTL::Function* NewFunction, NS::Error* Error){
#endif
					METAL_GPUPROFILE(FScopedMetalCPUStats CompletionStat(FString::Printf(TEXT("NewFunctionCompletion: %s"), *NSStringToFString(nsName))));
					UE_CLOG(NewFunction == nullptr, LogMetal, Error, TEXT("Failed to create function: %s"), *NSStringToFString(Error->description()));
					UE_CLOG(NewFunction == nullptr, LogMetal, Fatal, TEXT("*********** Error\n%s"), *NSStringToFString(GetSourceCode()));

					GetMetalCompiledShaderCache().Add(Key, Library, NS::RetainPtr(NewFunction));
#if ENABLE_METAL_GPUPROFILE
					if (CompletionStat.Stats)
					{
						CompletionStat.Stats->CPUStartTime = CPUStart;
					}
#endif
				};
                    
                Library->newFunction(Name, ConstantValues, Handler);

				return MTLFunctionPtr();
			}
		}
	}

	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesIABs) && Bindings.ArgumentBuffers && ArgumentEncoders.Num() == 0)
	{
		uint32 ArgumentBuffers = Bindings.ArgumentBuffers;
		while(ArgumentBuffers)
		{
			uint32 Index = __builtin_ctz(ArgumentBuffers);
			ArgumentBuffers &= ~(1 << Index);

			MTL::ArgumentEncoder* ArgumentEncoder = Function->newArgumentEncoder(Index);
			ArgumentEncoders.Add(Index, ArgumentEncoder);

			TBitArray<> Resources;
			for (uint8 Id : Bindings.ArgumentBufferMasks[Index])
			{
				if (Id >= Resources.Num())
				{
					Resources.Add(false, (Id + 1) - Resources.Num());
				}
				Resources[Id] = true;
			}
			ArgumentBitmasks.Add(Index, Resources);
		}
	}

	check(Func);
	return Func;
}
