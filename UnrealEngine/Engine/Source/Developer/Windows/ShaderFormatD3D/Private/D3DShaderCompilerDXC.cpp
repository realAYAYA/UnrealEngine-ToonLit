// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatD3D.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"
#include "ShaderParameterParser.h"
#include "D3D11ShaderResources.h"
#include "D3D12RHI.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"
#include "RayTracingDefinitions.h"

DEFINE_LOG_CATEGORY_STATIC(LogD3D12ShaderCompiler, Log, All);

// D3D doesn't define a mask for this, so we do so here
#define SHADER_OPTIMIZATION_LEVEL_MASK (D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_OPTIMIZATION_LEVEL1 | D3DCOMPILE_OPTIMIZATION_LEVEL2 | D3DCOMPILE_OPTIMIZATION_LEVEL3)

// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition

#include "Windows/AllowWindowsPlatformTypes.h"
	#include <D3D11.h>
	#include <D3Dcompiler.h>
	#include <d3d11Shader.h>
	#include "amd_ags.h"
#include "Windows/HideWindowsPlatformTypes.h"
#undef DrawText

#pragma warning(pop)

MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable : 4191)) // warning C4191: 'type cast': unsafe conversion from 'FARPROC' to 'DxcCreateInstanceProc'
#include <dxc/dxcapi.h>
#include <dxc/Support/dxcapi.use.h>
#include <d3d12shader.h>
MSVC_PRAGMA(warning(pop))

THIRD_PARTY_INCLUDES_START
	#include <string>
	#include "ShaderConductor/ShaderConductor.hpp"
THIRD_PARTY_INCLUDES_END

#include "D3DShaderCompiler.inl"

FORCENOINLINE static void DXCFilterShaderCompileWarnings(const FString& CompileWarnings, TArray<FString>& FilteredWarnings)
{
	CompileWarnings.ParseIntoArray(FilteredWarnings, TEXT("\n"), true);
}

static bool IsGlobalConstantBufferSupported(const FShaderTarget& Target)
{
	switch (Target.Frequency)
	{
	case SF_RayGen:
	case SF_RayMiss:
	case SF_RayCallable:
		// Global CB is not currently implemented for RayGen, Miss and Callable ray tracing shaders.
		return false;
	default:
		return true;
	}
}

static uint32 GetAutoBindingSpace(const FShaderTarget& Target)
{
	switch (Target.Frequency)
	{
	case SF_RayGen:
		return RAY_TRACING_REGISTER_SPACE_GLOBAL;
	case SF_RayMiss:
	case SF_RayHitGroup:
	case SF_RayCallable:
		return RAY_TRACING_REGISTER_SPACE_LOCAL;
	default:
		return 0;
	}
}

// Utility variable so we can place a breakpoint while debugging
static int32 GBreakpointDXC = 0;

#define VERIFYHRESULT(expr) { HRESULT HR##__LINE__ = expr; if (FAILED(HR##__LINE__)) { UE_LOG(LogD3D12ShaderCompiler, Fatal, TEXT(#expr " failed: Result=%08x"), HR##__LINE__); } }

static dxc::DxcDllSupport& GetDxcDllHelper()
{
	static dxc::DxcDllSupport DxcDllSupport;
	static bool DxcDllInitialized = false;
	if (!DxcDllInitialized)
	{
		VERIFYHRESULT(DxcDllSupport.Initialize());
		DxcDllInitialized = true;
	}
	return DxcDllSupport;
}

static FString DxcBlobEncodingToFString(TRefCountPtr<IDxcBlobEncoding> DxcBlob)
{
	FString OutString;
	if (DxcBlob && DxcBlob->GetBufferSize())
	{
		ANSICHAR* Chars = new ANSICHAR[DxcBlob->GetBufferSize() + 1];
		FMemory::Memcpy(Chars, DxcBlob->GetBufferPointer(), DxcBlob->GetBufferSize());
		Chars[DxcBlob->GetBufferSize()] = 0;
		OutString = Chars;
		delete[] Chars;
	}

	return OutString;
}

#if !PLATFORM_SEH_EXCEPTIONS_DISABLED && PLATFORM_WINDOWS
#include "Windows/WindowsPlatformCrashContext.h"
#include "HAL/PlatformStackWalk.h"
static char GDxcStackTrace[65536] = "";
static int32 HandleException(LPEXCEPTION_POINTERS ExceptionInfo)
{
	constexpr int32 NumStackFramesToIgnore = 1;
	GDxcStackTrace[0] = 0;
	FPlatformStackWalk::StackWalkAndDump(GDxcStackTrace, UE_ARRAY_COUNT(GDxcStackTrace), NumStackFramesToIgnore, nullptr);
	return EXCEPTION_EXECUTE_HANDLER;
}
#else
static const char* GDxcStackTrace = "";
#endif

static HRESULT InnerDXCCompileWrapper(
	TRefCountPtr<IDxcCompiler3>& Compiler,
	TRefCountPtr<IDxcBlobEncoding>& TextBlob,
	LPCWSTR* Arguments,
	uint32 NumArguments,
	bool& bOutExceptionError,
	TRefCountPtr<IDxcResult>& OutCompileResult)
{
	bOutExceptionError = false;
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED && PLATFORM_WINDOWS
	__try
#endif
	{
		DxcBuffer SourceBuffer = { 0 };
		SourceBuffer.Ptr = TextBlob->GetBufferPointer();
		SourceBuffer.Size = TextBlob->GetBufferSize();
		BOOL bKnown = 0;
		uint32 Encoding = 0;
		if (SUCCEEDED(TextBlob->GetEncoding(&bKnown, (uint32*)&Encoding)))
		{
			if (bKnown)
			{
				SourceBuffer.Encoding = Encoding;
			}
		}
		return Compiler->Compile(
			&SourceBuffer,						// source text to compile
			Arguments,							// array of pointers to arguments
			NumArguments,						// number of arguments
			nullptr,							// user-provided interface to handle #include directives (optional)
			IID_PPV_ARGS(OutCompileResult.GetInitReference())	// compiler output status, buffer, and errors
		);
	}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED && PLATFORM_WINDOWS
	__except (HandleException(GetExceptionInformation()))
	{
		bOutExceptionError = true;
		return E_FAIL;
	}
#endif
}

static HRESULT DXCCompileWrapper(
	TRefCountPtr<IDxcCompiler3>& Compiler,
	TRefCountPtr<IDxcBlobEncoding>& TextBlob,
	FDxcArguments& Arguments,
	TRefCountPtr<IDxcResult>& OutCompileResult)
{
	bool bExceptionError = false;

	TArray<const WCHAR*> CompilerArgs;
	Arguments.GetCompilerArgs(CompilerArgs);

	HRESULT Result = InnerDXCCompileWrapper(Compiler, TextBlob,
		CompilerArgs.GetData(), CompilerArgs.Num(), bExceptionError, OutCompileResult);

	if (bExceptionError)
	{
		GSCWErrorCode = ESCWErrorCode::CrashInsidePlatformCompiler;

		FString ErrorMsg = TEXT("Internal error or exception inside dxcompiler.dll\n");
		ErrorMsg += GDxcStackTrace;

		FCString::Strcpy(GErrorExceptionDescription, *ErrorMsg);

#if !PLATFORM_SEH_EXCEPTIONS_DISABLED && PLATFORM_WINDOWS
		// Throw an exception so SCW can send it back in the output file
		FPlatformMisc::RaiseException(EXCEPTION_EXECUTE_HANDLER);
#endif
	}

	return Result;
}

static void SaveDxcBlobToFile(IDxcBlob* Blob, const FString& Filename)
{
	const uint8* DxilData = (const uint8*)Blob->GetBufferPointer();
	uint32 DxilSize = Blob->GetBufferSize();
	TArrayView<const uint8> Contents(DxilData, DxilSize);
	FFileHelper::SaveArrayToFile(Contents, *Filename);
}

static void DisassembleAndSave(TRefCountPtr<IDxcCompiler3>& Compiler, IDxcBlob* Dxil, const FString& DisasmFilename)
{
	TRefCountPtr<IDxcResult> DisasmResult;
	DxcBuffer DisasmBuffer = { 0 };
	DisasmBuffer.Size = Dxil->GetBufferSize();
	DisasmBuffer.Ptr = Dxil->GetBufferPointer();
	if (SUCCEEDED(Compiler->Disassemble(&DisasmBuffer, IID_PPV_ARGS(DisasmResult.GetInitReference()))))
	{
		HRESULT DisasmCodeResult;
		DisasmResult->GetStatus(&DisasmCodeResult);
		if (SUCCEEDED(DisasmCodeResult))
		{
			checkf(DisasmResult->HasOutput(DXC_OUT_DISASSEMBLY), TEXT("Disasm part missing but container said it has one!"));
			TRefCountPtr<IDxcBlobEncoding> DisasmBlob;
			TRefCountPtr<IDxcBlobUtf16> Dummy;
			VERIFYHRESULT(DisasmResult->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(DisasmBlob.GetInitReference()), Dummy.GetInitReference()));
			FString String = DxcBlobEncodingToFString(DisasmBlob);
			FFileHelper::SaveStringToFile(String, *DisasmFilename);
		}
	}
}

static void DumpFourCCParts(dxc::DxcDllSupport& DxcDllHelper, TRefCountPtr<IDxcBlob>& Blob)
{
#if UE_BUILD_DEBUG && IS_PROGRAM
	TRefCountPtr<IDxcContainerReflection> Refl;
	VERIFYHRESULT(DxcDllHelper.CreateInstance(CLSID_DxcContainerReflection, Refl.GetInitReference()));

	VERIFYHRESULT(Refl->Load(Blob));

	uint32 Count = 0;
	VERIFYHRESULT(Refl->GetPartCount(&Count));

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** Blob Size: %d, %d Parts\n"), Blob->GetBufferSize(), Count);

	for (uint32 Index = 0; Index < Count; ++Index)
	{
		char FourCC[5] = "\0\0\0\0";
		VERIFYHRESULT(Refl->GetPartKind(Index, (uint32*)FourCC));
		TRefCountPtr<IDxcBlob> Part;
		Refl->GetPartContent(Index, Part.GetInitReference());
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("* %d %s, Size %d\n"), Index, ANSI_TO_TCHAR(FourCC), (uint32)Part->GetBufferSize());
	}
#endif
}

static bool RemoveContainerReflection(dxc::DxcDllSupport& DxcDllHelper, TRefCountPtr<IDxcBlob>& Dxil, bool bRemovePDB)
{
	TRefCountPtr<IDxcOperationResult> Result;
	TRefCountPtr<IDxcContainerBuilder> Builder;
	TRefCountPtr<IDxcBlob> StrippedDxil;

	VERIFYHRESULT(DxcDllHelper.CreateInstance(CLSID_DxcContainerBuilder, Builder.GetInitReference()));
	VERIFYHRESULT(Builder->Load(Dxil));
	
	// Try and remove both the PDB & Reflection Data
	bool bPDBRemoved = bRemovePDB && SUCCEEDED(Builder->RemovePart(DXC_PART_PDB));
	bool bReflectionDataRemoved = bRemovePDB && SUCCEEDED(Builder->RemovePart(DXC_PART_REFLECTION_DATA));
	if (bPDBRemoved || bReflectionDataRemoved)
	{
		VERIFYHRESULT(Builder->SerializeContainer(Result.GetInitReference()));
		if (SUCCEEDED(Result->GetResult(StrippedDxil.GetInitReference())))
		{
			Dxil.SafeRelease();
			Dxil = StrippedDxil;
			return true;
		}
	}

	return false;
};

static HRESULT D3DCompileToDxil(const char* SourceText, FDxcArguments& Arguments,
	TRefCountPtr<IDxcBlob>& OutDxilBlob, TRefCountPtr<IDxcBlob>& OutReflectionBlob, TRefCountPtr<IDxcBlobEncoding>& OutErrorBlob)
{
	dxc::DxcDllSupport& DxcDllHelper = GetDxcDllHelper();

	TRefCountPtr<IDxcCompiler3> Compiler;
	VERIFYHRESULT(DxcDllHelper.CreateInstance(CLSID_DxcCompiler, Compiler.GetInitReference()));

	TRefCountPtr<IDxcLibrary> Library;
	VERIFYHRESULT(DxcDllHelper.CreateInstance(CLSID_DxcLibrary, Library.GetInitReference()));

	TRefCountPtr<IDxcBlobEncoding> TextBlob;
	VERIFYHRESULT(Library->CreateBlobWithEncodingFromPinned((LPBYTE)SourceText, FCStringAnsi::Strlen(SourceText), CP_UTF8, TextBlob.GetInitReference()));

	TRefCountPtr<IDxcResult> CompileResult;
	VERIFYHRESULT(DXCCompileWrapper(Compiler, TextBlob, Arguments, CompileResult));

	if (!CompileResult.IsValid())
	{
		return E_FAIL;
	}

	HRESULT CompileResultCode;
	CompileResult->GetStatus(&CompileResultCode);
	if (SUCCEEDED(CompileResultCode))
	{
		TRefCountPtr<IDxcBlobUtf16> ObjectCodeNameBlob; // Dummy name blob to silence static analysis warning
		checkf(CompileResult->HasOutput(DXC_OUT_OBJECT), TEXT("No object code found!"));
		VERIFYHRESULT(CompileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(OutDxilBlob.GetInitReference()), ObjectCodeNameBlob.GetInitReference()));

		TRefCountPtr<IDxcBlobUtf16> ReflectionNameBlob; // Dummy name blob to silence static analysis warning
		checkf(CompileResult->HasOutput(DXC_OUT_REFLECTION), TEXT("No reflection found!"));
		VERIFYHRESULT(CompileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(OutReflectionBlob.GetInitReference()), ReflectionNameBlob.GetInitReference()));

		if (Arguments.ShouldDump())
		{
			// Dump disassembly before we strip reflection out
			const FString& DisasmFilename = Arguments.GetDumpDisassemblyFilename();
			check(DisasmFilename.Len() > 0);
			DisassembleAndSave(Compiler, OutDxilBlob, DisasmFilename);

			// Dump dxil (.d3dasm -> .dxil)
			FString DxilFile = Arguments.GetDumpDisassemblyFilename().LeftChop(7) + TEXT("_refl.dxil");
			SaveDxcBlobToFile(OutDxilBlob, DxilFile);

			if (CompileResult->HasOutput(DXC_OUT_PDB))
			{
				TRefCountPtr<IDxcBlob> PdbBlob;
				TRefCountPtr<IDxcBlobUtf16> PdbNameBlob;
				VERIFYHRESULT(CompileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(PdbBlob.GetInitReference()), PdbNameBlob.GetInitReference()));

				const FString PdbName = PdbNameBlob->GetStringPointer();

				// Dump pdb (.d3dasm -> .pdb)
				const FString PdbFile = Arguments.GetDumpDebugInfoPath() / PdbName;
				SaveDxcBlobToFile(PdbBlob, PdbFile);
			}
		}

		DumpFourCCParts(DxcDllHelper, OutDxilBlob);
		if (RemoveContainerReflection(DxcDllHelper, OutDxilBlob, !Arguments.ShouldKeepEmbeddedPDB()))
		{
			DumpFourCCParts(DxcDllHelper, OutDxilBlob);
		}

		if (Arguments.ShouldDump())
		{
			// Dump dxil (.d3dasm -> .dxil)
			FString DxilFile = Arguments.GetDumpDisassemblyFilename().LeftChop(7) + TEXT("_norefl.dxil");
			SaveDxcBlobToFile(OutDxilBlob, DxilFile);
		}

		GBreakpointDXC++;
	}
	else
	{
		GBreakpointDXC++;
	}

	CompileResult->GetErrorBuffer(OutErrorBlob.GetInitReference());

	return CompileResultCode;
}

static FString D3DCreateDXCCompileBatchFile(const FDxcArguments& Args, const FString& ShaderPath)
{
	FString DxcPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());

	DxcPath = FPaths::Combine(DxcPath, TEXT("Binaries/ThirdParty/ShaderConductor/Win64"));
	FPaths::MakePlatformFilename(DxcPath);

	FString DxcFilename = FPaths::Combine(DxcPath, TEXT("dxc.exe"));
	FPaths::MakePlatformFilename(DxcFilename);

	const FString BatchCmdLineArgs = Args.GetBatchCommandLineString(ShaderPath);

	return FString::Printf(
		TEXT(
			"@ECHO OFF\n"
			"SET DXC=\"%s\"\n"
			"IF NOT EXIST %%DXC%% (\n"
			"\tECHO Couldn't find dxc.exe under \"%s\"\n"
			"\tGOTO :END\n"
			")\n"
			"%%DXC%%%s %s\n"
			":END\n"
			"PAUSE\n"
		),
		*DxcFilename,
		*DxcPath,
		*BatchCmdLineArgs,
		*ShaderPath
	);
}

inline bool IsCompatibleBinding(const D3D12_SHADER_INPUT_BIND_DESC& BindDesc, uint32 BindingSpace)
{
	bool bIsCompatibleBinding = (BindDesc.Space == BindingSpace);
	if (!bIsCompatibleBinding)
	{
		const bool bIsAMDExtensionDX12 = (FCStringAnsi::Strcmp(BindDesc.Name, "AmdExtD3DShaderIntrinsicsUAV") == 0);
		bIsCompatibleBinding = bIsAMDExtensionDX12 && (BindDesc.Space == AGS_DX12_SHADER_INSTRINSICS_SPACE_ID);
	}
	if (!bIsCompatibleBinding)
	{
		// #todo: there is currently no common header where a binding space number or buffer name could be defined. See D3DCommon.ush and D3D12RootSignature.cpp.
		const bool bIsUEDebugBuffer = (FCStringAnsi::Strcmp(BindDesc.Name, "UEDiagnosticBuffer") == 0);
		bIsCompatibleBinding = bIsUEDebugBuffer && (BindDesc.Space == 999);
	}

	return bIsCompatibleBinding;
}

static ShaderConductor::Compiler::ShaderModel ToDXCShaderModel(ELanguage Language)
{
	switch (Language)
	{
	case ELanguage::ES3_1:
	case ELanguage::SM5:
		return { 5, 0 };
	default:
		UE_LOG(LogD3D12ShaderCompiler, Error, TEXT("Invalid input shader target for enum ELanguage (%d)."), (int32)Language);
	}
	return { 6,0 };
}

static ShaderConductor::ShaderStage ToDXCShaderStage(EShaderFrequency Frequency)
{
	check(Frequency >= SF_Vertex && Frequency <= SF_Compute);
	switch (Frequency)
	{
	case SF_Vertex:		return ShaderConductor::ShaderStage::VertexShader;
	case SF_Pixel:		return ShaderConductor::ShaderStage::PixelShader;
	case SF_Geometry:	return ShaderConductor::ShaderStage::GeometryShader;
	case SF_Compute:	return ShaderConductor::ShaderStage::ComputeShader;
	default:			return ShaderConductor::ShaderStage::NumShaderStages;
	}
}

// Inner wrapper function is required here because '__try'-statement cannot be used with function that requires object unwinding
static void InnerScRewriteWrapper(
	const ShaderConductor::Compiler::SourceDesc& InDesc,
	const ShaderConductor::Compiler::Options& InOptions,
	ShaderConductor::Compiler::ResultDesc& OutResultDesc)
{
	OutResultDesc = ShaderConductor::Compiler::Rewrite(InDesc, InOptions);
}

static bool DXCRewriteWrapper(
	const ShaderConductor::Compiler::SourceDesc& InDesc,
	const ShaderConductor::Compiler::Options& InOptions,
	ShaderConductor::Compiler::ResultDesc& OutResultDesc,
	bool& bOutException)
{
	bOutException = false;
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__try
#endif
	{
		InnerScRewriteWrapper(InDesc, InOptions, OutResultDesc);
		return true;
	}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		GSCWErrorCode = ESCWErrorCode::CrashInsidePlatformCompiler;
		FMemory::Memzero(OutResultDesc);
		bOutException = true;
		return false;
	}
#endif
}

static const TCHAR* GRewrittenBaseFilename = TEXT("Output.dxc.hlsl");
static bool RewriteUsingSC(FString& PreprocessedShaderSource, const FShaderCompilerInput& Input, bool bIsRayTracingShader,
	bool bDumpDebugInfo, ELanguage Language, FShaderCompilerOutput& Output)
{
	bool bResult = true;
	if (bIsRayTracingShader)
	{
		bResult = false;
	}
	else
	{
		// Set up compile options for ShaderConductor (shader model, optimization settings etc.)
		ShaderConductor::Compiler::Options Options;
		Options.removeUnusedGlobals = false;
		Options.packMatricesInRowMajor = false;
		Options.enableDebugInfo = false;
		Options.enable16bitTypes = false;
		Options.disableOptimizations = false;
		Options.shaderModel = ToDXCShaderModel(Language);

		// Convert input source code from TCHAR to ANSI
		std::string CStrSourceData(TCHAR_TO_ANSI(*PreprocessedShaderSource));
		std::string CStrFileName(TCHAR_TO_ANSI(*Input.VirtualSourceFilePath));
		std::string CStrEntryPointName(TCHAR_TO_ANSI(*Input.EntryPointName));

		const ShaderConductor::MacroDefine BuiltinDefines[] =
		{
//			{ "COMPILER_HLSL", "1" },
			{ "TextureExternal", "Texture2D" },
		};

		// Set up source description for ShaderConductor
		ShaderConductor::Compiler::SourceDesc SourceDesc;
		FMemory::Memzero(SourceDesc);
		SourceDesc.source = CStrSourceData.c_str();
		SourceDesc.fileName = CStrFileName.c_str();
		SourceDesc.entryPoint = CStrEntryPointName.c_str();
		SourceDesc.numDefines = sizeof(BuiltinDefines) / sizeof(BuiltinDefines[0]);
		SourceDesc.defines = BuiltinDefines;
		SourceDesc.stage = ToDXCShaderStage(Input.Target.GetFrequency());

		ShaderConductor::Compiler::TargetDesc TargetDesc;
		FMemory::Memzero(TargetDesc);
		TargetDesc.language = ShaderConductor::ShadingLanguage::Dxil;

		// Rewrite HLSL source to remove unused global variables (DXC retains them when compiling)
		Options.removeUnusedGlobals = true;
		bool bException = false;
		ShaderConductor::Compiler::ResultDesc RewriteResultDesc;
		DXCRewriteWrapper(SourceDesc, Options, RewriteResultDesc, bException);
		Options.removeUnusedGlobals = false;
		if (RewriteResultDesc.hasError || bException)
		{
			if (bException)
			{
				Output.Errors.Add(TEXT("ShaderConductor exception during rewrite"));
			}
			// Append compile error to output reports
			if (RewriteResultDesc.errorWarningMsg.Size() > 0)
			{
				FUTF8ToTCHAR UTF8Converter(reinterpret_cast<const ANSICHAR*>(RewriteResultDesc.errorWarningMsg.Data()), RewriteResultDesc.errorWarningMsg.Size());
				const FString ErrorString(RewriteResultDesc.errorWarningMsg.Size(), UTF8Converter.Get());
				Output.Errors.Add(*ErrorString);
				RewriteResultDesc.errorWarningMsg.Reset();
				bResult = false;
			}
		}
		else
		{
			// Copy rewritten HLSL code into new source data string
			CStrSourceData.clear();
			CStrSourceData.resize(RewriteResultDesc.target.Size());
			FCStringAnsi::Strncpy(&CStrSourceData[0], static_cast<const char*>(RewriteResultDesc.target.Data()), RewriteResultDesc.target.Size());
			PreprocessedShaderSource = CStrSourceData.c_str();

			if (bDumpDebugInfo)
			{
				DumpDebugUSF(Input, CStrSourceData.c_str(), 0, GRewrittenBaseFilename);
			}
		}
	}

	return bResult;
}

// Generate the dumped usf file; call the D3D compiler, gather reflection information and generate the output data
bool CompileAndProcessD3DShaderDXC(FString& PreprocessedShaderSource,
	const uint32 CompileFlags,
	const FShaderCompilerInput& Input,
	const FShaderParameterParser& ShaderParameterParser,
	FString& EntryPointName,
	const TCHAR* ShaderProfile, ELanguage Language, bool bProcessingSecondTime,
	TArray<FString>& FilteredErrors, FShaderCompilerOutput& Output)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CompileAndProcessD3DShaderDXC);

	auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShaderSource);

	const bool bIsRayTracingShader = Input.IsRayTracingShader();

	const uint32 AutoBindingSpace = GetAutoBindingSpace(Input.Target);

	FString RayEntryPoint; // Primary entry point for all ray tracing shaders
	FString RayAnyHitEntryPoint; // Optional for hit group shaders
	FString RayIntersectionEntryPoint; // Optional for hit group shaders
	FString RayTracingExports;

	bool bEnable16BitTypes = false;
	if (Language == ELanguage::SM6)
	{
		// 16bit types are SM6.2 whereas Language == ELanguage::SM6 is SM6.6, so their support at runtime is guarented.
		bEnable16BitTypes = Input.Environment.CompilerFlags.Contains(CFLAG_AllowRealTypes);
	}

	if (bIsRayTracingShader)
	{
		UE::ShaderCompilerCommon::ParseRayTracingEntryPoint(Input.EntryPointName, RayEntryPoint, RayAnyHitEntryPoint, RayIntersectionEntryPoint);

		RayTracingExports = RayEntryPoint;

		if (!RayAnyHitEntryPoint.IsEmpty())
		{
			RayTracingExports += TEXT(";");
			RayTracingExports += RayAnyHitEntryPoint;
		}

		if (!RayIntersectionEntryPoint.IsEmpty())
		{
			RayTracingExports += TEXT(";");
			RayTracingExports += RayIntersectionEntryPoint;
		}

		// Enable 16bit_types to reduce DXIL size (compiler bug - will be fixed)
		bEnable16BitTypes = true;
	}

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	bool bDumpDebugInfo = DumpDebugShaderUSF(PreprocessedShaderSource, Input);

	FString Filename = Input.GetSourceFilename();

	if (Input.Environment.CompilerFlags.Contains(CFLAG_D3D12ForceShaderConductorRewrite))
	{
		if (RewriteUsingSC(PreprocessedShaderSource, Input, bIsRayTracingShader, bDumpDebugInfo, Language, Output))
		{
			Filename = GRewrittenBaseFilename;
		}
	}

	FString DisasmFilename;
	if (bDumpDebugInfo)
	{
		DisasmFilename = Input.DumpDebugInfoPath / Filename;
	}

	// Ignore backwards compatibility flag (/Gec) as it is deprecated.
	// #dxr_todo: this flag should not be even passed into this function from the higher level.
	uint32 DXCFlags = CompileFlags & (~D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY);
	if (Input.Environment.CompilerFlags.Contains(CFLAG_SkipOptimizationsDXC))
	{
		DXCFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
	}

	const TCHAR* ValidatorVersion = nullptr; // Whatever default validator chosen by DXC

	const bool bGenerateSymbols = Input.Environment.CompilerFlags.Contains(CFLAG_GenerateSymbols);
	const bool bSymbolsBasedOnSource = Input.Environment.CompilerFlags.Contains(CFLAG_AllowUniqueSymbols);
	const bool bHlslVersion2021 = Input.Environment.CompilerFlags.Contains(CFLAG_HLSL2021);
	const uint32 HlslVersion = (bHlslVersion2021 ? 2021 : 2018);

	FDxcArguments Args
	(
		EntryPointName,
		ShaderProfile,
		RayTracingExports,
		Input.DumpDebugInfoPath,
		Filename,
		bEnable16BitTypes,
		bGenerateSymbols,
		bSymbolsBasedOnSource,
		DXCFlags,
		AutoBindingSpace,
		ValidatorVersion,
		HlslVersion
	);

	if (bDumpDebugInfo)
	{
		FString BatchFileContents = D3DCreateDXCCompileBatchFile(Args, Filename);
		FFileHelper::SaveStringToFile(BatchFileContents, *(Input.DumpDebugInfoPath / TEXT("CompileDXC.bat")));

		if (Input.bGenerateDirectCompileFile)
		{
			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *(Input.DumpDebugInfoPath / TEXT("DirectCompile.txt")));
			FFileHelper::SaveStringToFile(Input.DebugDescription, *(Input.DumpDebugInfoPath / TEXT("permutation_info.txt")));
		}
	}

	TRefCountPtr<IDxcBlob> ShaderBlob;
	TRefCountPtr<IDxcBlob> ReflectionBlob;
	TRefCountPtr<IDxcBlobEncoding> DxcErrorBlob;

	const HRESULT D3DCompileToDxilResult = D3DCompileToDxil(AnsiSourceFile.Get(), Args, ShaderBlob, ReflectionBlob, DxcErrorBlob);

	if (DxcErrorBlob && DxcErrorBlob->GetBufferSize())
	{
		FString ErrorString = DxcBlobEncodingToFString(DxcErrorBlob);
		DXCFilterShaderCompileWarnings(ErrorString, FilteredErrors);
	}

	if (SUCCEEDED(D3DCompileToDxilResult))
	{
		// Gather reflection information
		TArray<FString> ShaderInputs;
		TArray<FShaderCodeVendorExtension> VendorExtensions;

		bool bGlobalUniformBufferUsed = false;
		bool bDiagnosticBufferUsed = false;
		uint32 NumInstructions = 0;
		uint32 NumSamplers = 0;
		uint32 NumSRVs = 0;
		uint32 NumCBs = 0;
		uint32 NumUAVs = 0;
		TArray<FString> UniformBufferNames;
		TArray<FString> ShaderOutputs;

		TBitArray<> UsedUniformBufferSlots;
		UsedUniformBufferSlots.Init(false, 32);

		uint64 ShaderRequiresFlags{};

		dxc::DxcDllSupport& DxcDllHelper = GetDxcDllHelper();
		TRefCountPtr<IDxcUtils> Utils;
		VERIFYHRESULT(DxcDllHelper.CreateInstance(CLSID_DxcUtils, Utils.GetInitReference()));
		DxcBuffer ReflBuffer = { 0 };
		ReflBuffer.Ptr = ReflectionBlob->GetBufferPointer();
		ReflBuffer.Size = ReflectionBlob->GetBufferSize();

		if (bIsRayTracingShader)
		{
			TRefCountPtr<ID3D12LibraryReflection> LibraryReflection;
			const HRESULT CreateReflectionResult = Utils->CreateReflection(&ReflBuffer, IID_PPV_ARGS(LibraryReflection.GetInitReference()));

			if (FAILED(CreateReflectionResult))
			{
				UE_LOG(LogD3D12ShaderCompiler, Fatal, TEXT("CreateReflection failed: Result=%08x"), CreateReflectionResult);
			}

			D3D12_LIBRARY_DESC LibraryDesc = {};
			LibraryReflection->GetDesc(&LibraryDesc);

			ID3D12FunctionReflection* FunctionReflection = nullptr;
			D3D12_FUNCTION_DESC FunctionDesc = {};

			// MangledEntryPoints contains partial mangled entry point signatures in a the following form:
			// ?QualifiedName@ (as described here: https://en.wikipedia.org/wiki/Name_mangling)
			// Entry point parameters are currently not included in the partial mangling.
			TArray<FString, TInlineAllocator<3>> MangledEntryPoints;

			if (!RayEntryPoint.IsEmpty())
			{
				MangledEntryPoints.Add(FString::Printf(TEXT("?%s@"), *RayEntryPoint));
			}
			if (!RayAnyHitEntryPoint.IsEmpty())
			{
				MangledEntryPoints.Add(FString::Printf(TEXT("?%s@"), *RayAnyHitEntryPoint));
			}
			if (!RayIntersectionEntryPoint.IsEmpty())
			{
				MangledEntryPoints.Add(FString::Printf(TEXT("?%s@"), *RayIntersectionEntryPoint));
			}

			uint32 NumFoundEntryPoints = 0;

			for (uint32 FunctionIndex = 0; FunctionIndex < LibraryDesc.FunctionCount; ++FunctionIndex)
			{
				FunctionReflection = LibraryReflection->GetFunctionByIndex(FunctionIndex);
				FunctionReflection->GetDesc(&FunctionDesc);

				ShaderRequiresFlags |= FunctionDesc.RequiredFeatureFlags;

				for (const FString& MangledEntryPoint : MangledEntryPoints)
				{
					// Entry point parameters are currently not included in the partial mangling, therefore partial substring match is used here.
					if (FCStringAnsi::Strstr(FunctionDesc.Name, TCHAR_TO_ANSI(*MangledEntryPoint)))
					{
						// Note: calling ExtractParameterMapFromD3DShader multiple times merges the reflection data for multiple functions
						ExtractParameterMapFromD3DShader<ID3D12FunctionReflection, D3D12_FUNCTION_DESC, D3D12_SHADER_INPUT_BIND_DESC,
							ID3D12ShaderReflectionConstantBuffer, D3D12_SHADER_BUFFER_DESC,
							ID3D12ShaderReflectionVariable, D3D12_SHADER_VARIABLE_DESC>(
								Input, ShaderParameterParser,
								AutoBindingSpace, FunctionReflection, FunctionDesc, 
								bGlobalUniformBufferUsed, bDiagnosticBufferUsed,
								NumSamplers, NumSRVs, NumCBs, NumUAVs,
								Output, UniformBufferNames, UsedUniformBufferSlots, VendorExtensions);

						NumFoundEntryPoints++;
					}
				}
			}

			if (NumFoundEntryPoints == MangledEntryPoints.Num())
			{
				Output.bSucceeded = true;

				bool bGlobalUniformBufferAllowed = false;

				if (bGlobalUniformBufferUsed && !IsGlobalConstantBufferSupported(Input.Target))
				{
					const TCHAR* ShaderFrequencyString = GetShaderFrequencyString(Input.Target.GetFrequency(), false);
					FString ErrorString = FString::Printf(TEXT("Global uniform buffer cannot be used in a %s shader."), ShaderFrequencyString);

					uint32 NumLooseParameters = 0;
					for (const auto& It : Output.ParameterMap.ParameterMap)
					{
						if (It.Value.Type == EShaderParameterType::LooseData)
						{
							NumLooseParameters++;
						}
					}

					if (NumLooseParameters)
					{
						ErrorString += TEXT(" Global parameters: ");
						uint32 ParameterIndex = 0;
						for (const auto& It : Output.ParameterMap.ParameterMap)
						{
							if (It.Value.Type == EShaderParameterType::LooseData)
							{
								--NumLooseParameters;
								ErrorString += FString::Printf(TEXT("%s%s"), *It.Key, NumLooseParameters ? TEXT(", ") : TEXT("."));
							}
						}
					}

					FilteredErrors.Add(ErrorString);
					Output.bSucceeded = false;
				}
			}
			else
			{
				UE_LOG(LogD3D12ShaderCompiler, Fatal, TEXT("Failed to find required points in the shader library."));
				Output.bSucceeded = false;
			}
		}
		else
		{
			TRefCountPtr<ID3D12ShaderReflection> ShaderReflection;
			const HRESULT CreateReflectionResult = Utils->CreateReflection(&ReflBuffer, IID_PPV_ARGS(ShaderReflection.GetInitReference()));
			if (FAILED(CreateReflectionResult))
			{
				UE_LOG(LogD3D12ShaderCompiler, Fatal, TEXT("CreateReflection failed: Result=%08x"), CreateReflectionResult);
			}

			D3D12_SHADER_DESC ShaderDesc = {};
			ShaderReflection->GetDesc(&ShaderDesc);

			ShaderRequiresFlags = ShaderReflection->GetRequiresFlags();

			ExtractParameterMapFromD3DShader<ID3D12ShaderReflection, D3D12_SHADER_DESC, D3D12_SHADER_INPUT_BIND_DESC,
				ID3D12ShaderReflectionConstantBuffer, D3D12_SHADER_BUFFER_DESC,
				ID3D12ShaderReflectionVariable, D3D12_SHADER_VARIABLE_DESC>(
					Input, ShaderParameterParser,
					AutoBindingSpace, ShaderReflection, ShaderDesc,
					bGlobalUniformBufferUsed, bDiagnosticBufferUsed,
					NumSamplers, NumSRVs, NumCBs, NumUAVs,
					Output, UniformBufferNames, UsedUniformBufferSlots, VendorExtensions);

			NumInstructions = ShaderDesc.InstructionCount;
			Output.bSucceeded = true;
		}

		if (!ValidateResourceCounts(NumSRVs, NumSamplers, NumUAVs, NumCBs, FilteredErrors))
		{
			Output.bSucceeded = false;
		}

		FShaderCodePackedResourceCounts PackedResourceCounts{};

		if (Output.bSucceeded)
		{
			if (bGlobalUniformBufferUsed)
			{
				PackedResourceCounts.UsageFlags |= EShaderResourceUsageFlags::GlobalUniformBuffer;
			}

			if (ShaderRequiresFlags & D3D_SHADER_REQUIRES_RESOURCE_DESCRIPTOR_HEAP_INDEXING)
			{
				PackedResourceCounts.UsageFlags |= EShaderResourceUsageFlags::BindlessResources;
			}

			if (ShaderRequiresFlags & D3D_SHADER_REQUIRES_SAMPLER_DESCRIPTOR_HEAP_INDEXING)
			{
				PackedResourceCounts.UsageFlags |= EShaderResourceUsageFlags::BindlessSamplers;
			}

			PackedResourceCounts.NumSamplers = static_cast<uint8>(NumSamplers);
			PackedResourceCounts.NumSRVs = static_cast<uint8>(NumSRVs);
			PackedResourceCounts.NumCBs = static_cast<uint8>(NumCBs);
			PackedResourceCounts.NumUAVs = static_cast<uint8>(NumUAVs);

			Output.bSucceeded = UE::ShaderCompilerCommon::ValidatePackedResourceCounts(Output, PackedResourceCounts);
		}

		// Save results if compilation and reflection succeeded
		if (Output.bSucceeded)
		{
			auto PostSRTWriterCallback = [&](FMemoryWriter& Ar)
			{
				if (bIsRayTracingShader)
				{
					Ar << RayEntryPoint;
					Ar << RayAnyHitEntryPoint;
					Ar << RayIntersectionEntryPoint;
				}
			};

			auto AddOptionalDataCallback = [&](FShaderCode& ShaderCode)
			{
				FShaderCodeFeatures CodeFeatures;

				if ((ShaderRequiresFlags & D3D_SHADER_REQUIRES_WAVE_OPS) != 0)
				{
					EnumAddFlags(CodeFeatures.CodeFeatures, EShaderCodeFeatures::WaveOps);
				}

				if ((ShaderRequiresFlags & D3D_SHADER_REQUIRES_NATIVE_16BIT_OPS) != 0)
				{
					EnumAddFlags(CodeFeatures.CodeFeatures, EShaderCodeFeatures::SixteenBitTypes);
				}

				if ((ShaderRequiresFlags & D3D_SHADER_REQUIRES_TYPED_UAV_LOAD_ADDITIONAL_FORMATS) != 0)
				{
					EnumAddFlags(CodeFeatures.CodeFeatures, EShaderCodeFeatures::TypedUAVLoadsExtended);
				}

				if ((ShaderRequiresFlags & (D3D_SHADER_REQUIRES_ATOMIC_INT64_ON_TYPED_RESOURCE| D3D_SHADER_REQUIRES_ATOMIC_INT64_ON_GROUP_SHARED)) != 0)
				{
					EnumAddFlags(CodeFeatures.CodeFeatures, EShaderCodeFeatures::Atomic64);
				}

				if (bDiagnosticBufferUsed)
				{
					EnumAddFlags(CodeFeatures.CodeFeatures, EShaderCodeFeatures::DiagnosticBuffer);
				}

				if ((ShaderRequiresFlags & D3D_SHADER_REQUIRES_RESOURCE_DESCRIPTOR_HEAP_INDEXING) != 0)
				{
					EnumAddFlags(CodeFeatures.CodeFeatures, EShaderCodeFeatures::BindlessResources);
				}

				if ((ShaderRequiresFlags & D3D_SHADER_REQUIRES_SAMPLER_DESCRIPTOR_HEAP_INDEXING) != 0)
				{
					EnumAddFlags(CodeFeatures.CodeFeatures, EShaderCodeFeatures::BindlessSamplers);
				}

				// We only need this to appear when using a DXC shader
				ShaderCode.AddOptionalData<FShaderCodeFeatures>(CodeFeatures);

				if (Language != ELanguage::SM6)
				{
					uint8 IsSM6 = 1;
					ShaderCode.AddOptionalData('6', &IsSM6, 1);
				}
			};

			// Return a fraction of the number of instructions as DXIL is more verbose than DXBC.
			// Ratio 119:307 was estimated by gathering average instruction count for D3D11 and D3D12 shaders in ShooterGame with result being ~ 357:921.
			constexpr uint32 DxbcToDxilInstructionRatio[2] = { 119, 307 };
			NumInstructions = NumInstructions * DxbcToDxilInstructionRatio[0] / DxbcToDxilInstructionRatio[1];

			//#todo-rco: Should compress ShaderCode?

			GenerateFinalOutput(ShaderBlob,
				Input, VendorExtensions,
				UsedUniformBufferSlots, UniformBufferNames,
				bProcessingSecondTime, ShaderInputs,
				PackedResourceCounts, NumInstructions,
				Output,
				PostSRTWriterCallback,
				AddOptionalDataCallback);
		}
	}
	else
	{
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, UE_ARRAY_COUNT(ErrorMsg), (int)D3DCompileToDxilResult);
		const bool bKnownError = ErrorMsg[0] != TEXT('\0');

		FString ErrorString = FString::Printf(TEXT("D3DCompileToDxil failed. Error code: %s (0x%08X)."), bKnownError ? ErrorMsg : TEXT("Unknown error"), (int)D3DCompileToDxilResult);

		FilteredErrors.Add(ErrorString);
	}

	return Output.bSucceeded;
}

#undef VERIFYHRESULT
