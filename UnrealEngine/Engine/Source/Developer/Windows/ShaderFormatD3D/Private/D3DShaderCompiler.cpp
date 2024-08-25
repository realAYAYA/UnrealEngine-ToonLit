// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHI.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RayTracingDefinitions.h"
#include "Serialization/MemoryWriter.h"
#include "ShaderFormatD3D.h"
#include "ShaderCompilerCommon.h"
#include "ShaderCompilerDefinitions.h"
#include "ShaderMinifier.h"
#include "ShaderParameterParser.h"
#include "ShaderPreprocessTypes.h"
#include "SpirvCommon.h"

DEFINE_LOG_CATEGORY_STATIC(LogD3D11ShaderCompiler, Log, All);

#define DEBUG_SHADERS 0

// D3D doesn't define a mask for this, so we do so here
#define SHADER_OPTIMIZATION_LEVEL_MASK (D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_OPTIMIZATION_LEVEL1 | D3DCOMPILE_OPTIMIZATION_LEVEL2 | D3DCOMPILE_OPTIMIZATION_LEVEL3)

// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition

#include "Windows/AllowWindowsPlatformTypes.h"
	#include <D3D11.h>
	#include <D3Dcompiler.h>
	#include <d3d11Shader.h>
#include "Windows/HideWindowsPlatformTypes.h"
#undef DrawText

#include "D3DShaderCompiler.inl"

#pragma warning(pop)

static const uint32 GD3DMaximumNumUAVs = 8; // Limit for feature level 11.0

static int32 GD3DCheckForDoubles = 1;
static int32 GD3DDumpAMDCodeXLFile = 0;

/**
 * TranslateCompilerFlag - translates the platform-independent compiler flags into D3DX defines
 * @param CompilerFlag - the platform-independent compiler flag to translate
 * @return uint32 - the value of the appropriate D3DX enum
 */
static uint32 TranslateCompilerFlagD3D11(ECompilerFlags CompilerFlag)
{
	switch(CompilerFlag)
	{
	case CFLAG_PreferFlowControl: return D3DCOMPILE_PREFER_FLOW_CONTROL;
	case CFLAG_AvoidFlowControl: return D3DCOMPILE_AVOID_FLOW_CONTROL;
	case CFLAG_WarningsAsErrors: return D3DCOMPILE_WARNINGS_ARE_ERRORS;
	default: return 0;
	};
}

/*
 * Turns invalid absolute paths that FXC generated back into a virtual file paths,
 * e.g. "D:\\Engine\\Private\\Common.ush" into "/Engine/Private/Common.ush"
 */
static void D3D11SanitizeErrorVirtualFilePath(FString& ErrorLine)
{
	if (ErrorLine.Len() > 3 && ErrorLine[1] == TEXT(':') && ErrorLine[2] == TEXT('\\'))
	{
		const int32 EndOfFilePath = ErrorLine.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 3);
		if (EndOfFilePath != INDEX_NONE)
		{
			for (int32 ErrorLineStringPosition = 2; ErrorLineStringPosition < EndOfFilePath; ++ErrorLineStringPosition)
			{
				if (ErrorLine[ErrorLineStringPosition] == TEXT('\\'))
				{
					ErrorLine[ErrorLineStringPosition] = TEXT('/');
				}
			}
			ErrorLine.RightChopInline(2);
		}
	}
}

/**
 * Filters out unwanted shader compile warnings
 */
static void D3D11FilterShaderCompileWarnings(const FString& CompileWarnings, TArray<FString>& FilteredWarnings)
{
	TArray<FString> WarningArray;
	FString OutWarningString = TEXT("");
	CompileWarnings.ParseIntoArray(WarningArray, TEXT("\n"), true);
	
	//go through each warning line
	for (int32 WarningIndex = 0; WarningIndex < WarningArray.Num(); WarningIndex++)
	{
		//suppress "warning X3557: Loop only executes for 1 iteration(s), forcing loop to unroll"
		if (!WarningArray[WarningIndex].Contains(TEXT("X3557"))
			// "warning X3205: conversion from larger type to smaller, possible loss of data"
			// Gets spammed when converting from float to half
			&& !WarningArray[WarningIndex].Contains(TEXT("X3205")))
		{
			D3D11SanitizeErrorVirtualFilePath(WarningArray[WarningIndex]);
			FilteredWarnings.AddUnique(WarningArray[WarningIndex]);
		}
	}
}

// @return 0 if not recognized
static const TCHAR* GetShaderProfileName(const FShaderCompilerInput& Input, ED3DShaderModel ShaderModel)
{
	if (ShaderModel == ED3DShaderModel::SM6_6)
	{
		switch (Input.Target.GetFrequency())
		{
		case SF_Pixel:         return TEXT("ps_6_6");
		case SF_Vertex:        return TEXT("vs_6_6");
		case SF_Mesh:          return TEXT("ms_6_6");
		case SF_Amplification: return TEXT("as_6_6");
		case SF_Geometry:      return TEXT("gs_6_6");
		case SF_Compute:       return TEXT("cs_6_6");
		case SF_RayGen:
		case SF_RayMiss:
		case SF_RayHitGroup:
		case SF_RayCallable:   return TEXT("lib_6_6");
		}
	}
	else if (ShaderModel == ED3DShaderModel::SM6_0)
	{
		//set defines and profiles for the appropriate shader paths
		switch (Input.Target.GetFrequency())
		{
		case SF_Pixel:    return TEXT("ps_6_0");
		case SF_Vertex:   return TEXT("vs_6_0");
		case SF_Geometry: return TEXT("gs_6_0");
		case SF_Compute:  return TEXT("cs_6_0");
		}
	}
	else
	{
		//set defines and profiles for the appropriate shader paths
		switch (Input.Target.GetFrequency())
		{
		case SF_Pixel:    return TEXT("ps_5_0");
		case SF_Vertex:   return TEXT("vs_5_0");
		case SF_Geometry: return TEXT("gs_5_0");
		case SF_Compute:  return TEXT("cs_5_0");
		}
	}

	checkfSlow(false, TEXT("Unexpected shader frequency"));
	return nullptr;
}

/**
 * D3D11CreateShaderCompileCommandLine - takes shader parameters used to compile with the DX11
 * compiler and returns an fxc command to compile from the command line
 */
static FString D3D11CreateShaderCompileCommandLine(
	const FString& ShaderPath, 
	const TCHAR* EntryFunction, 
	const TCHAR* ShaderProfile, 
	uint32 CompileFlags,
	FShaderCompilerOutput& Output
	)
{
	// fxc is our command line compiler
	FString FXCCommandline = FString(TEXT("\"%FXC%\" ")) + ShaderPath;

	// add the entry point reference
	FXCCommandline += FString(TEXT(" /E ")) + EntryFunction;

	// go through and add other switches
	if(CompileFlags & D3DCOMPILE_PREFER_FLOW_CONTROL)
	{
		CompileFlags &= ~D3DCOMPILE_PREFER_FLOW_CONTROL;
		FXCCommandline += FString(TEXT(" /Gfp"));
	}

	if(CompileFlags & D3DCOMPILE_DEBUG)
	{
		CompileFlags &= ~D3DCOMPILE_DEBUG;
		FXCCommandline += FString(TEXT(" /Zi"));
	}

	if(CompileFlags & D3DCOMPILE_SKIP_OPTIMIZATION)
	{
		CompileFlags &= ~D3DCOMPILE_SKIP_OPTIMIZATION;
		FXCCommandline += FString(TEXT(" /Od"));
	}

	if (CompileFlags & D3DCOMPILE_SKIP_VALIDATION)
	{
		CompileFlags &= ~D3DCOMPILE_SKIP_VALIDATION;
		FXCCommandline += FString(TEXT(" /Vd"));
	}

	if(CompileFlags & D3DCOMPILE_AVOID_FLOW_CONTROL)
	{
		CompileFlags &= ~D3DCOMPILE_AVOID_FLOW_CONTROL;
		FXCCommandline += FString(TEXT(" /Gfa"));
	}

	if(CompileFlags & D3DCOMPILE_PACK_MATRIX_ROW_MAJOR)
	{
		CompileFlags &= ~D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
		FXCCommandline += FString(TEXT(" /Zpr"));
	}

	if(CompileFlags & D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY)
	{
		CompileFlags &= ~D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
		FXCCommandline += FString(TEXT(" /Gec"));
	}

	if (CompileFlags & D3DCOMPILE_WARNINGS_ARE_ERRORS)
	{
		CompileFlags &= ~D3DCOMPILE_WARNINGS_ARE_ERRORS;
		FXCCommandline += FString(TEXT(" /WX"));
	}

	switch (CompileFlags & SHADER_OPTIMIZATION_LEVEL_MASK)
	{
		case D3DCOMPILE_OPTIMIZATION_LEVEL2:
		CompileFlags &= ~D3DCOMPILE_OPTIMIZATION_LEVEL2;
		FXCCommandline += FString(TEXT(" /O2"));
			break;

		case D3DCOMPILE_OPTIMIZATION_LEVEL3:
		CompileFlags &= ~D3DCOMPILE_OPTIMIZATION_LEVEL3;
		FXCCommandline += FString(TEXT(" /O3"));
			break;

		case D3DCOMPILE_OPTIMIZATION_LEVEL1:
		CompileFlags &= ~D3DCOMPILE_OPTIMIZATION_LEVEL1;
		FXCCommandline += FString(TEXT(" /O1"));
			break;

		case D3DCOMPILE_OPTIMIZATION_LEVEL0:
			CompileFlags &= ~D3DCOMPILE_OPTIMIZATION_LEVEL0;
			break;

		default:
			Output.Errors.Emplace(TEXT("Unknown D3DCOMPILE optimization level"));
			break;
	}

	checkf(CompileFlags == 0, TEXT("Unhandled d3d11 shader compiler flag!"));

	// add the target instruction set
	FXCCommandline += FString(TEXT(" /T ")) + ShaderProfile;

	// Assembly instruction numbering
	FXCCommandline += TEXT(" /Ni");

	// Output to ShaderPath.d3dasm
	if (FPaths::GetExtension(ShaderPath) == TEXT("usf"))
	{
		FXCCommandline += FString::Printf(TEXT(" /Fc%sd3dasm"), *ShaderPath.LeftChop(3));
	}

	// add a pause on a newline
	FXCCommandline += FString(TEXT(" \r\n pause"));

	// Batch file header:
	/*
	@ECHO OFF
		SET FXC="C:\Program Files (x86)\Windows Kits\10\bin\x64\fxc.exe"
		IF EXIST %FXC% (
			REM
			) ELSE (
				ECHO Couldn't find Windows 10 SDK, falling back to DXSDK...
				SET FXC="%DXSDK_DIR%\Utilities\bin\x86\fxc.exe"
				IF EXIST %FXC% (
					REM
					) ELSE (
						ECHO Couldn't find DXSDK! Exiting...
						GOTO END
					)
			)
	*/
	const FString BatchFileHeader = TEXT(
		"@ECHO OFF\n"\
		"IF \"%FXC%\" == \"\" SET \"FXC=C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\fxc.exe\"\n"\
		"IF NOT EXIST \"%FXC%\" (\n"\
		"\t" "ECHO Couldn't find Windows 10 SDK, falling back to DXSDK...\n"\
		"\t" "SET \"FXC=%DXSDK_DIR%\\Utilities\\bin\\x86\\fxc.exe\"\n"\
		"\t" "IF NOT EXIST \"%FXC%\" (\n"\
		"\t" "\t" "ECHO Couldn't find DXSDK! Exiting...\n"\
		"\t" "\t" "GOTO END\n"\
		"\t)\n"\
		")\n"
	);
	return BatchFileHeader + FXCCommandline + TEXT("\n:END\nREM\n");
}


// Validate that we are not going over to maximum amount of resource bindings support by the default root signature on DX12
// Currently limited for hard-coded root signature setup (see: FD3D12Adapter::StaticGraphicsRootSignature)
// In theory this limitation is only required for DX12, but we don't want a shader to compile on DX11 while not working on DX12.
// (DX11 has an API limit on 128 SRVs, 16 Samplers, 8 UAVs and 14 CBs but if you go over these values then the shader won't compile)
bool ValidateResourceCounts(uint32 NumSRVs, uint32 NumSamplers, uint32 NumUAVs, uint32 NumCBs, TArray<FString>& OutFilteredErrors)
{
	if (NumSRVs > MAX_SRVS || NumSamplers > MAX_SAMPLERS || NumUAVs > MAX_UAVS || NumCBs > MAX_CBS)
	{
		if (NumSRVs > MAX_SRVS)
		{
			OutFilteredErrors.Add(FString::Printf(TEXT("Shader is using too many SRVs: %d (only %d supported)"), NumSRVs, MAX_SRVS));
		}

		if (NumSamplers > MAX_SAMPLERS)
		{
			OutFilteredErrors.Add(FString::Printf(TEXT("Shader is using too many Samplers: %d (only %d supported)"), NumSamplers, MAX_SAMPLERS));
		}

		if (NumUAVs > MAX_UAVS)
		{
			OutFilteredErrors.Add(FString::Printf(TEXT("Shader is using too many UAVs: %d (only %d supported)"), NumUAVs, MAX_UAVS));
		}

		if (NumCBs > MAX_CBS)
		{
			OutFilteredErrors.Add(FString::Printf(TEXT("Shader is using too many Constant Buffers: %d (only %d supported)"), NumCBs, MAX_CBS));
		}

		return false;
	}

	return true;
}

/** Creates a batch file string to call the AMD shader analyzer. */
static FString CreateAMDCodeXLCommandLine(
	const FString& ShaderPath, 
	const TCHAR* EntryFunction, 
	const TCHAR* ShaderProfile,
	uint32 DXFlags
	)
{
	// Hardcoded to the default install path since there's no Env variable or addition to PATH
	FString Commandline = FString(TEXT("\"C:\\Program Files (x86)\\AMD\\CodeXL\\CodeXLAnalyzer.exe\" -c Pitcairn")) 
		+ TEXT(" -f ") + EntryFunction
		+ TEXT(" -s HLSL")
		+ TEXT(" -p ") + ShaderProfile
		+ TEXT(" -a AnalyzerStats.csv")
		+ TEXT(" --isa ISA.txt")
		+ *FString::Printf(TEXT(" --DXFlags %u "), DXFlags)
		+ ShaderPath;

	// add a pause on a newline
	Commandline += FString(TEXT(" \r\n pause"));
	return Commandline;
}

// D3Dcompiler.h has function pointer typedefs for some functions, but not all
typedef HRESULT(WINAPI *pD3DReflect)
	(__in_bcount(SrcDataSize) LPCVOID pSrcData,
	 __in SIZE_T  SrcDataSize,
	 __in  REFIID pInterface,
	 __out void** ppReflector);

typedef HRESULT(WINAPI *pD3DStripShader)
	(__in_bcount(BytecodeLength) LPCVOID pShaderBytecode,
	 __in SIZE_T     BytecodeLength,
	 __in UINT       uStripFlags,
	__out ID3DBlob** ppStrippedBlob);

#define DEFINE_GUID_FOR_CURRENT_COMPILER(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

// ShaderReflection IIDs may change between SDK versions if the reflection API changes.
// Define a GUID below that matches the desired IID for the DLL in CompilerPath. For example,
// look for IID_ID3D11ShaderReflection in d3d11shader.h for the SDK matching the compiler DLL.
DEFINE_GUID_FOR_CURRENT_COMPILER(IID_ID3D11ShaderReflectionForCurrentCompiler, 0x8d536ca1, 0x0cca, 0x4956, 0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84);

// Helper class to load the engine-packaged FXC DLL and retrieve function pointers for the various FXC functions from it.
class FxcCompilerFunctions
{
public:

	static pD3DCompile GetCompile() { return Instance().Compile; }
	static pD3DReflect GetReflect() { return Instance().Reflect; }
	static pD3DDisassemble GetDisassemble() { return Instance().Disassemble; }
	static pD3DStripShader GetStripShader() { return Instance().StripShader; }

private:
	FxcCompilerFunctions()
	{
		FString CompilerPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/Windows/DirectX/x64/d3dcompiler_47.dll");
		CompilerDLL = LoadLibrary(*CompilerPath);
		if (!CompilerDLL)
		{
			UE_LOG(LogD3D11ShaderCompiler, Fatal, TEXT("Cannot find the compiler DLL '%s'"), *CompilerPath);
		}
		Compile = (pD3DCompile)(void*)GetProcAddress(CompilerDLL, "D3DCompile");
		Reflect = (pD3DReflect)(void*)GetProcAddress(CompilerDLL, "D3DReflect");
		Disassemble = (pD3DDisassemble)(void*)GetProcAddress(CompilerDLL, "D3DDisassemble");
		StripShader = (pD3DStripShader)(void*)GetProcAddress(CompilerDLL, "D3DStripShader");
	}

	static FxcCompilerFunctions& Instance()
	{
		static FxcCompilerFunctions Instance;
		return Instance;
	}

	HMODULE CompilerDLL = 0;
	pD3DCompile Compile = nullptr;
	pD3DReflect Reflect = nullptr;
	pD3DDisassemble Disassemble = nullptr;
	pD3DStripShader StripShader = nullptr;
};

static int D3DExceptionFilter(bool bCatchException)
{
	return bCatchException ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}

static HRESULT D3DCompileWrapper(
	pD3DCompile				D3DCompileFunc,
	LPCVOID					pSrcData,
	SIZE_T					SrcDataSize,
	LPCSTR					pFileName,
	CONST D3D_SHADER_MACRO*	pDefines,
	ID3DInclude*			pInclude,
	LPCSTR					pEntrypoint,
	LPCSTR					pTarget,
	uint32					Flags1,
	uint32					Flags2,
	ID3DBlob**				ppCode,
	ID3DBlob**				ppErrorMsgs,
	bool					bCatchException = false
	)
{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__try
#endif
	{
		return D3DCompileFunc(
			pSrcData,
			SrcDataSize,
			pFileName,
			pDefines,
			pInclude,
			pEntrypoint,
			pTarget,
			Flags1,
			Flags2,
			ppCode,
			ppErrorMsgs
		);
	}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__except(D3DExceptionFilter(bCatchException))
	{
		FSCWErrorCode::Report(FSCWErrorCode::CrashInsidePlatformCompiler);
		return E_FAIL;
	}
#endif
}

inline bool IsCompatibleBinding(const D3D11_SHADER_INPUT_BIND_DESC& BindDesc, uint32 BindingSpace)
{
	return true;
}

static void PatchSpirvForPrecompilation(FSpirv& Spirv)
{
	// Remove [unroll] loop hints from SPIR-V as this can fail on infinite loops
	for (FSpirvIterator SpirvInstruction : Spirv)
	{
		if (SpirvInstruction.Opcode() == SpvOpLoopMerge && SpirvInstruction.Operand(3) == SpvLoopControlUnrollMask)
		{
			(*SpirvInstruction)[3] = SpvLoopControlMaskNone;
		}
	}
}

// @param StageVariablesStorageClass Must be SpvStorageClassOutput for vertex shaders and SpvStorageClassInput for pixel shaders.
static bool PatchHlslWithReorderedIOVariables(
	FString& HlslSourceString,
	const FString& OriginalShaderSource,
	const FString& OriginalEntryPoint,
	SpvStorageClass StageVariablesStorageClass,
	TArray<FShaderCompilerError>& OutErrors)
{
	check(StageVariablesStorageClass == SpvStorageClassInput || StageVariablesStorageClass == SpvStorageClassOutput);

	// Find declaration struct for stage variables
	const FStringView StageVariableDeclarationName = (StageVariablesStorageClass == SpvStorageClassInput ? TEXT("SPIRV_Cross_Input") : TEXT("SPIRV_Cross_Output"));
	const int32 StageVariableDeclarationBegin = HlslSourceString.Find(StageVariableDeclarationName, ESearchCase::CaseSensitive);
	if (StageVariableDeclarationBegin == INDEX_NONE)
	{
		return false;
	}

	const int32 StageVariableDelcarationBlockBegin = HlslSourceString.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StageVariableDeclarationBegin + StageVariableDeclarationName.Len());
	if (StageVariableDelcarationBlockBegin == INDEX_NONE)
	{
		return false;
	}

	const int32 StageVariableDelcarationBlockEnd = HlslSourceString.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StageVariableDelcarationBlockBegin + 1);
	if (StageVariableDelcarationBlockEnd == INDEX_NONE)
	{
		return false;
	}

	// Parse declaration struct for stage variables into array of individual lines
	const FString StageVariableDeclarationSource = HlslSourceString.Mid(StageVariableDelcarationBlockBegin + 1, StageVariableDelcarationBlockEnd - (StageVariableDelcarationBlockBegin + 1));

	TArray<FString> StageVariableDeclarationLines;
	StageVariableDeclarationSource.ParseIntoArrayLines(StageVariableDeclarationLines);

	// Parse variable names from SPIR-V input
	TArray<FString> Variables, ParsingErrors;
	const EShaderParameterStorageClass ParameterStorageClass = (StageVariablesStorageClass == SpvStorageClassOutput ? EShaderParameterStorageClass::Output : EShaderParameterStorageClass::Input);
	if (!FindEntryPointParameters(OriginalShaderSource, OriginalEntryPoint, ParameterStorageClass, Variables, ParsingErrors))
	{
		for (FString& Error : ParsingErrors)
		{
			OutErrors.Add(FShaderCompilerError(MoveTemp(Error)));
		}
		return false;
	}

	if (Variables.Num() != StageVariableDeclarationLines.Num())
	{
		// Failed to match SPIR-V variables to SPIRV-Cross generated source
		return false;
	}

	// Re-arrange source lines of stage variable declarations
	FString SortedStageVariableDeclarationSource = TEXT("\n");

	for (const FString& Variable : Variables)
	{
		for (FString& SourceLine : StageVariableDeclarationLines)
		{
			// Search for semantic name (always case insensitive) in current stage variable source line
			if (SourceLine.Find(Variable, ESearchCase::IgnoreCase) != INDEX_NONE)
			{
				// Append source line for current variable at the end of sorted declaration string.
				// Then empty this source line to avoid unnecessary string comparisons for next variables.
				SortedStageVariableDeclarationSource += SourceLine;
				SortedStageVariableDeclarationSource += TEXT('\n');
				SourceLine.Empty();
				break;
			}
		}
	}

	// Replace old declaration with sorted one
	HlslSourceString.RemoveAt(StageVariableDelcarationBlockBegin + 1, StageVariableDelcarationBlockEnd - (StageVariableDelcarationBlockBegin + 1));
	HlslSourceString.InsertAt(StageVariableDelcarationBlockBegin + 1, SortedStageVariableDeclarationSource);

	return true;
}

// @todo-lh: use ANSI string class whenever UE core gets one
static void PatchHlslForPrecompilation(
	TArray<ANSICHAR>& HlslSource,
	const EShaderFrequency Frequency,
	const FString& OriginalShaderSource,
	const FString& OriginalEntryPoint,
	TArray<FShaderCompilerError>& OutErrors)
{
	FString HlslSourceString;

	// Disable some warnings that might be introduced by cross-compiled HLSL, we only want to see those warnings from the original source and not from intermediate high-level source
	HlslSourceString += TEXT("#pragma warning(disable : 3571) // pow() intrinsic suggested to be used with abs()\n");

	// Append original cross-compiled source code
	HlslSourceString += ANSI_TO_TCHAR(HlslSource.GetData());

	// Patch SPIRV-Cross renaming to retain original member names in RootShaderParameters cbuffer
	const int32 RootShaderParameterSourceLocation = HlslSourceString.Find("cbuffer RootShaderParameters");
	if (RootShaderParameterSourceLocation != INDEX_NONE)
	{
		HlslSourceString.ReplaceInline(TEXT("cbuffer RootShaderParameters"), TEXT("cbuffer _RootShaderParameters"), ESearchCase::CaseSensitive);
		HlslSourceString.ReplaceInline(TEXT("_RootShaderParameters_"), TEXT(""), ESearchCase::CaseSensitive);
	}

	// Patch separation of atomic counters: replace declarations of all counter_var_... declarations by their original buffer resource.
	const FString CounterPrefix = TEXT("counter_var_");
	const FString CounterDeclPrefix = TEXT("RWByteAddressBuffer ") + CounterPrefix;

	for (int32 ReadPos = 0, NextReadPos = 0;
		 (NextReadPos = HlslSourceString.Find(CounterDeclPrefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, ReadPos)) != INDEX_NONE;
		 ReadPos = NextReadPos)
	{
		// Find original resource name without "counter_var_" prefix
		const int32 ResourceNameStartPos = NextReadPos + CounterDeclPrefix.Len();
		const int32 ResourceNameEndPos = HlslSourceString.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ResourceNameStartPos);
		if (ResourceNameEndPos != INDEX_NONE)
		{
			const FString ResourceName = HlslSourceString.Mid(NextReadPos + CounterDeclPrefix.Len(), ResourceNameEndPos - ResourceNameStartPos);
			const FString ResourceCounterName = HlslSourceString.Mid(NextReadPos + CounterDeclPrefix.Len() - CounterPrefix.Len(), ResourceNameEndPos - ResourceNameStartPos + CounterPrefix.Len());

			// Remove current "RWByteAddressBuffer counter_var_*;" resource declaration line
			HlslSourceString.RemoveAt(NextReadPos, ResourceNameEndPos - NextReadPos + 1);

			// Remove all "counter_var_" prefixes for the current resource
			HlslSourceString.ReplaceInline(*ResourceCounterName, *ResourceName, ESearchCase::CaseSensitive);
		}
	}

	if (Frequency == SF_Vertex)
	{
		// Ensure order of output variables remains the same as declared in original shader source
		PatchHlslWithReorderedIOVariables(HlslSourceString, OriginalShaderSource, OriginalEntryPoint, SpvStorageClassOutput, OutErrors);
	}
	else if (Frequency == SF_Pixel)
	{
		// Patch internal error when SV_DepthLessEqual or SV_DepthGreaterEqual is specified in a pixel shader output. This is to prevent the following internal error:
		//	error X8000 : D3D11 Internal Compiler Error : Invalid Bytecode : Interpolation mode for PS input position must be
		//				  linear_noperspective_centroid or linear_noperspective_sample when outputting oDepthGE or oDepthLE and
		//				  not running at sample frequency(which is forced by inputting SV_SampleIndex or declaring an input linear_sample or linear_noperspective_sample).
		if (HlslSourceString.Find(TEXT("SV_DepthLessEqual"), ESearchCase::CaseSensitive) != INDEX_NONE ||
			HlslSourceString.Find(TEXT("SV_DepthGreaterEqual"), ESearchCase::CaseSensitive) != INDEX_NONE)
		{
			// Ensure the interpolation mode is linear_noperspective_sample by adding "sample" specifier to one of the input-interpolators that have a floating-point type
			const int32 FragCoordStringPosition = HlslSourceString.Find(TEXT("float4 gl_FragCoord : SV_Position"), ESearchCase::CaseSensitive);
			if (FragCoordStringPosition != INDEX_NONE)
			{
				HlslSourceString.InsertAt(FragCoordStringPosition, TEXT("sample "));
			}
		}

		// Ensure order of input variables remains the same as declared in original shader source
		PatchHlslWithReorderedIOVariables(HlslSourceString, OriginalShaderSource, OriginalEntryPoint, SpvStorageClassInput, OutErrors);
	}

	// Return new HLSL source
	HlslSource.SetNum(HlslSourceString.Len() + 1);
	FMemory::Memcpy(HlslSource.GetData(), TCHAR_TO_ANSI(*HlslSourceString), HlslSourceString.Len());
	HlslSource[HlslSourceString.Len()] = '\0';
}

// Returns whether the specified D3D compiler error buffer contains any internal error messages, e.g. "internal error: out of memory"
static bool CompileErrorsContainInternalError(ID3DBlob* Errors)
{
	if (Errors)
	{
		if (void* ErrorBuffer = Errors->GetBufferPointer())
		{
			const ANSICHAR* ErrorString = reinterpret_cast<const ANSICHAR*>(ErrorBuffer);
			return
				FCStringAnsi::Strstr(ErrorString, "internal error:") != nullptr ||
				FCStringAnsi::Strstr(ErrorString, "Internal Compiler Error:") != nullptr;
		}
	}
	return false;
}

static bool D3DCompileErrorContainsValidationErrors(ID3DBlob* ErrorBlob)
{
	if (ErrorBlob != nullptr)
	{
		const FAnsiStringView ErrorString((const ANSICHAR*)ErrorBlob->GetBufferPointer(), (int32)ErrorBlob->GetBufferSize());
		return (ErrorString.Find(ANSITEXTVIEW("error X8000: Validation Error:")) != INDEX_NONE);
	}
	return false;
}

// Generate the dumped usf file; call the D3D compiler, gather reflection information and generate the output data
static bool CompileAndProcessD3DShaderFXCExt(
	uint32 CompileFlags,
	const FShaderCompilerInput& Input,
	const FString& PreprocessedShaderSource,
	const FString& EntryPointName,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile, bool bSecondPassAferUnusedInputRemoval,
	TArray<FString>& FilteredErrors, FShaderCompilerOutput& Output)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CompileAndProcessD3DShaderFXCExt);

	auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShaderSource);

	bool bDumpDebugInfo = Input.DumpDebugInfoEnabled();
	if (bDumpDebugInfo)
	{
		FString BatchFileContents;
		FString Filename = Input.GetSourceFilename();
		BatchFileContents = D3D11CreateShaderCompileCommandLine(Filename, *EntryPointName, ShaderProfile, CompileFlags, Output);

		if (GD3DDumpAMDCodeXLFile)
		{
			const FString BatchFileContents2 = CreateAMDCodeXLCommandLine(Filename, *EntryPointName, ShaderProfile, CompileFlags);
			FFileHelper::SaveStringToFile(BatchFileContents2, *(Input.DumpDebugInfoPath / TEXT("CompileAMD.bat")));
		}

		FFileHelper::SaveStringToFile(BatchFileContents, *(Input.DumpDebugInfoPath / TEXT("CompileFXC.bat")));
	}

	TRefCountPtr<ID3DBlob> Shader;

	HRESULT Result = S_OK;
	pD3DCompile D3DCompileFunc = FxcCompilerFunctions::GetCompile();
	pD3DReflect D3DReflectFunc = FxcCompilerFunctions::GetReflect();
	pD3DDisassemble D3DDisassembleFunc = FxcCompilerFunctions::GetDisassemble();
	pD3DStripShader D3DStripShaderFunc = FxcCompilerFunctions::GetStripShader();

	TRefCountPtr<ID3DBlob> Errors;

	if (D3DCompileFunc)
	{
		const bool bHlslVersion2021 = Input.Environment.CompilerFlags.Contains(CFLAG_HLSL2021);
		const bool bPrecompileWithDXC = bHlslVersion2021 || Input.Environment.CompilerFlags.Contains(CFLAG_PrecompileWithDXC);
		if (!bPrecompileWithDXC)
		{
			Result = D3DCompileWrapper(
				D3DCompileFunc,
				AnsiSourceFile.Get(),
				AnsiSourceFile.Length(),
				TCHAR_TO_ANSI(*Input.VirtualSourceFilePath),
				/*pDefines=*/ NULL,
				/*pInclude=*/ NULL,
				TCHAR_TO_ANSI(*EntryPointName),
				TCHAR_TO_ANSI(ShaderProfile),
				CompileFlags,
				0,
				Shader.GetInitReference(),
				Errors.GetInitReference(),
				// We only want to catch the exception on initial FXC compiles so we can retry with a 
				// DXC precompilation step. If it fails again on the second attempt then we let
				// ShaderCompileWorker handle the exception and log an error.
				/* bCatchException */ true
			);
		}

		// Some materials give FXC a hard time to optimize and the compiler fails with an internal error.
		if (bPrecompileWithDXC || Result == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) || Result == E_OUTOFMEMORY || Result == E_FAIL || (Result != S_OK && CompileErrorsContainInternalError(Errors.GetReference())))
		{
			// If we ran out of memory, it's likely the next attempt will crash, too.
			// Report the error now in case CompileHlslToSpirv throws an exception.
			if (Result == E_OUTOFMEMORY)
			{
				FSCWErrorCode::Report(FSCWErrorCode::OutOfMemory);
			}

			CrossCompiler::FShaderConductorContext CompilerContext;

			// Load shader source into compiler context
			const EShaderFrequency Frequency = (EShaderFrequency)Input.Target.Frequency;
			CompilerContext.LoadSource(PreprocessedShaderSource, Input.VirtualSourceFilePath, EntryPointName, Frequency);

			// Compile HLSL source to SPIR-V binary
			CrossCompiler::FShaderConductorOptions Options;

			Options.bPreserveStorageInput = true; // Input/output stage variables must match
			if (bHlslVersion2021)
			{
				Options.HlslVersion = 2021;
			}

			FSpirv Spirv;
			if (!CompilerContext.CompileHlslToSpirv(Options, Spirv.Data))
			{
				CompilerContext.FlushErrors(Output.Errors);
				return false;
			}

			// Cross-compile back to HLSL
			CrossCompiler::FShaderConductorTarget TargetDesc;
			TargetDesc.Language = CrossCompiler::EShaderConductorLanguage::Hlsl;
			TargetDesc.Version = 50;

			PRAGMA_DISABLE_DEPRECATION_WARNINGS		// FShaderCompilerDefinitions will be made internal in the future, marked deprecated until then
			TargetDesc.CompileFlags->SetDefine(TEXT("implicit_resource_binding"), 1);
			TargetDesc.CompileFlags->SetDefine(TEXT("reconstruct_global_uniforms"), 1);
			TargetDesc.CompileFlags->SetDefine(TEXT("reconstruct_cbuffer_names"), 1);
			TargetDesc.CompileFlags->SetDefine(TEXT("reconstruct_semantics"), 1);
			TargetDesc.CompileFlags->SetDefine(TEXT("force_zero_initialized_variables"), 1);
			TargetDesc.CompileFlags->SetDefine(TEXT("relax_nan_checks"), 1);
			TargetDesc.CompileFlags->SetDefine(TEXT("preserve_structured_buffers"), 1);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			// Patch SPIR-V for workarounds to prevent potential additional FXC failures
			PatchSpirvForPrecompilation(Spirv);

			TArray<ANSICHAR> CrossCompiledSource;
			if (!CompilerContext.CompileSpirvToSourceAnsi(Options, TargetDesc, Spirv.GetByteData(), Spirv.GetByteSize(), CrossCompiledSource))
			{
				CompilerContext.FlushErrors(Output.Errors);
				return false;
			}

			// Patch HLSL for workarounds to prevent potential additional FXC failures
			PatchHlslForPrecompilation(CrossCompiledSource, Frequency, PreprocessedShaderSource, EntryPointName, Output.Errors);

			if (bDumpDebugInfo && CrossCompiledSource.Num() > 1)
			{
				DumpDebugShaderDisassembledSpirv(Input, Spirv.GetByteData(), Spirv.GetByteSize(), TEXT("intermediate.spvasm"));
				DumpDebugShaderText(Input, CrossCompiledSource.GetData(), CrossCompiledSource.Num() - 1, TEXT("intermediate.hlsl"));
			}

			// Generates an virtual source file path with the ".intermediate." suffix injected.
			auto MakeIntermediateVirtualSourceFilePath = [](const FString& InVirtualSourceFilePath) -> FString
			{
				FString PathPart, FilenamePart, ExtensionPart;
				FPaths::Split(InVirtualSourceFilePath, PathPart, FilenamePart, ExtensionPart);
				return FPaths::Combine(PathPart, FilenamePart) + TEXT(".intermediate.") + ExtensionPart;
			};

			const FString CrossCompiledSourceFilename = MakeIntermediateVirtualSourceFilePath(Input.VirtualSourceFilePath);
			auto ShaderProfileAnsi = StringCast<ANSICHAR>(ShaderProfile);
			auto CrossCompiledSourceFilenameAnsi = StringCast<ANSICHAR>(*CrossCompiledSourceFilename);

			// SPIRV-Cross will have generated the new shader with "main" as the new entry point.
			auto CompileCrossCompiledHlsl = [&D3DCompileFunc, &CrossCompiledSourceFilenameAnsi, &Shader, &Errors, &ShaderProfileAnsi](const TArray<ANSICHAR>& Source, uint32 CompileFlags, const ANSICHAR* EntryPoint = "main") -> HRESULT
			{
				checkf(Source.Num() > 0, TEXT("TArray<ANSICHAR> of cross-compiled HLSL source must have at least one element including the NUL-terminator"));
				return D3DCompileWrapper(
					D3DCompileFunc,
					Source.GetData(),
					static_cast<SIZE_T>(Source.Num() - 1),
					CrossCompiledSourceFilenameAnsi.Get(),
					/*pDefines=*/ NULL,
					/*pInclude=*/ NULL,
					EntryPoint,
					ShaderProfileAnsi.Get(),
					CompileFlags,
					0,
					Shader.GetInitReference(),
					Errors.GetInitReference()
				);
			};

			// Compile again with FXC - 1st try
			const uint32 CompileFlagsNoWarningsAsErrors = CompileFlags & (~D3DCOMPILE_WARNINGS_ARE_ERRORS);
			Result = CompileCrossCompiledHlsl(CrossCompiledSource, CompileFlagsNoWarningsAsErrors);

			// If FXC compilation failed with a validation error, assume bug in FXC's optimization passes
			// Compile again with FXC and disable special compiler rule to simplify control flow - 2nd try
			if (Result == E_FAIL && D3DCompileErrorContainsValidationErrors(Errors.GetReference()))
			{
				Output.Errors.Add(FShaderCompilerError(TEXT("Validation error in FXC encountered: Compiling intermediate HLSL a second time with simplified control flow")));

				// Rule 0x08024065 is described as "simplify flow control that writes the same value in each flow control path"
				const FAnsiStringView PragmaDirectiveCode = "#pragma ruledisable 0x08024065\n";
				CrossCompiledSource.Insert(PragmaDirectiveCode.GetData(), PragmaDirectiveCode.Len(), 0);

				Result = CompileCrossCompiledHlsl(CrossCompiledSource, CompileFlagsNoWarningsAsErrors);

				// If FXC compilation still fails with a validation error, compile again and skip optimizations entirely as a last resort - 3rd try
				if (Result == E_FAIL && D3DCompileErrorContainsValidationErrors(Errors.GetReference()))
				{
					Output.Errors.Add(FShaderCompilerError(TEXT("Validation error in FXC encountered: Compiling intermediate HLSL a third time without optimization (D3DCOMPILE_SKIP_OPTIMIZATION)")));

					const uint32 CompileFlagsSkipOptimizations = CompileFlagsNoWarningsAsErrors | D3DCOMPILE_SKIP_OPTIMIZATION;
					Result = CompileCrossCompiledHlsl(CrossCompiledSource, CompileFlagsSkipOptimizations);
				}
			}

			if (!bPrecompileWithDXC && SUCCEEDED(Result))
			{
				// Reset our previously set error code
				FSCWErrorCode::Reset();

				// Let the user know this shader had to be cross-compiled due to a crash in FXC. Only shows up if CVar 'r.ShaderDevelopmentMode' is enabled.
				Output.Errors.Add(FShaderCompilerError(TEXT("Cross-compiled shader to intermediate HLSL after first attempt crashed FXC")));
			}
		}
	}
	else
	{
		FilteredErrors.Add(TEXT("Couldn't find D3D shader compiler DLL"));
		Result = E_FAIL;
	}

	// Filter any errors.
	void* ErrorBuffer = Errors ? Errors->GetBufferPointer() : NULL;
	if (ErrorBuffer)
	{
		D3D11FilterShaderCompileWarnings(ANSI_TO_TCHAR(ErrorBuffer), FilteredErrors);
	}

	// Fail the compilation if certain extended features are being used, since those are not supported on all D3D11 cards.
	if (SUCCEEDED(Result) && D3DDisassembleFunc)
	{
		const bool bCheckForTypedUAVs = !Input.Environment.CompilerFlags.Contains(CFLAG_AllowTypedUAVLoads);
		if (GD3DCheckForDoubles || bCheckForTypedUAVs || bDumpDebugInfo)
		{
			TRefCountPtr<ID3DBlob> Disassembly;
			if (SUCCEEDED(D3DDisassembleFunc(Shader->GetBufferPointer(), Shader->GetBufferSize(), 0, "", Disassembly.GetInitReference())))
			{
				ANSICHAR* DisassemblyString = new ANSICHAR[Disassembly->GetBufferSize() + 1];
				FMemory::Memcpy(DisassemblyString, Disassembly->GetBufferPointer(), Disassembly->GetBufferSize());
				DisassemblyString[Disassembly->GetBufferSize()] = 0;
				FString DisassemblyStringW(DisassemblyString);
				delete[] DisassemblyString;

				if (bDumpDebugInfo)
				{
					FFileHelper::SaveStringToFile(DisassemblyStringW, *(Input.DumpDebugInfoPath / TEXT("Output.d3dasm")));
				}

				if (GD3DCheckForDoubles)
				{
					// dcl_globalFlags will contain enableDoublePrecisionFloatOps when the shader uses doubles, even though the docs on dcl_globalFlags don't say anything about this
					if (DisassemblyStringW.Contains(TEXT("enableDoublePrecisionFloatOps")))
					{
						FilteredErrors.Add(TEXT("Shader uses double precision floats, which are not supported on all D3D11 hardware!"));
						return false;
					}
				}
					
				if (bCheckForTypedUAVs)
				{
					// Disassembly will contain this text with typed loads from UAVs are used where the format and dimension are not fully supported
					// across all versions of Windows (like Windows 7/8.1).
					// https://microsoft.github.io/DirectX-Specs/d3d/UAVTypedLoad.html
					// https://docs.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
					// https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/format-support-for-direct3d-11-0-feature-level-hardware
					if (DisassemblyStringW.Contains(TEXT("Typed UAV Load Additional Formats")))
					{
						FilteredErrors.Add(TEXT("Shader uses UAV loads from additional typed formats, which are not supported on all D3D11 hardware! Set r.D3D.CheckedForTypedUAVs=0 if you want to allow typed UAV loads for your project, or individual shaders can opt-in by specifying CFLAG_AllowTypedUAVLoads."));
						return false;
					}
				}
			}
		}
	}

	// Gather reflection information
	TArray<FString> ShaderInputs;
	TArray<FShaderCodeVendorExtension> VendorExtensions;

	if (SUCCEEDED(Result))
	{
		bool bGlobalUniformBufferUsed = false;
		bool bDiagnosticBufferUsed = false;
		uint32 NumInstructions = 0;
		uint32 NumSamplers = 0;
		uint32 NumSRVs = 0;
		uint32 NumCBs = 0;
		uint32 NumUAVs = 0;

		TArray<FString> UniformBufferNames;

		TBitArray<> UsedUniformBufferSlots;
		UsedUniformBufferSlots.Init(false, 32);

		if (D3DReflectFunc)
		{
			Output.bSucceeded = true;
			TRefCountPtr<ID3D11ShaderReflection> Reflector;
			
			Result = D3DReflectFunc(Shader->GetBufferPointer(), Shader->GetBufferSize(), IID_ID3D11ShaderReflectionForCurrentCompiler, (void**)Reflector.GetInitReference());
			if (FAILED(Result))
			{
				UE_LOG(LogD3D11ShaderCompiler, Fatal, TEXT("D3DReflect failed: Result=%08x"), Result);
			}

			// Read the constant table description.
			D3D11_SHADER_DESC ShaderDesc;
			Reflector->GetDesc(&ShaderDesc);

			if (Input.Target.Frequency == SF_Pixel)
			{
				bool bFoundUnused = false;
				for (uint32 Index = 0; Index < ShaderDesc.InputParameters; ++Index)
				{
					// VC++ horrible hack: Runtime ESP checks get confused and fail for some reason calling Reflector->GetInputParameterDesc() (because it comes from another DLL?)
					// so "guard it" using the middle of an array; it's been confirmed NO corruption is really happening.
					D3D11_SIGNATURE_PARAMETER_DESC ParamDescs[3];
					D3D11_SIGNATURE_PARAMETER_DESC& ParamDesc = ParamDescs[1];
					Reflector->GetInputParameterDesc(Index, &ParamDesc);
					if (ParamDesc.SystemValueType == D3D_NAME_UNDEFINED)
					{
						if (ParamDesc.ReadWriteMask != 0)
						{
							FString SemanticName = ANSI_TO_TCHAR(ParamDesc.SemanticName);

							ShaderInputs.AddUnique(SemanticName);

							// Add the number (for the case of TEXCOORD)
							FString SemanticIndexName = FString::Printf(TEXT("%s%d"), *SemanticName, ParamDesc.SemanticIndex);
							ShaderInputs.AddUnique(SemanticIndexName);

							// Add _centroid
							ShaderInputs.AddUnique(SemanticName + TEXT("_centroid"));
							ShaderInputs.AddUnique(SemanticIndexName + TEXT("_centroid"));
						}
						else
						{
							bFoundUnused = true;
						}
					}
					else
					{
						//if (ParamDesc.ReadWriteMask != 0)
						{
							// Keep system values
							ShaderInputs.AddUnique(FString(ANSI_TO_TCHAR(ParamDesc.SemanticName)));
						}
					}
				}

				if (Input.Environment.CompilerFlags.Contains(CFLAG_ForceRemoveUnusedInterpolators) && Input.bCompilingForShaderPipeline && bFoundUnused && !bSecondPassAferUnusedInputRemoval)
				{
					// Rewrite the source removing the unused inputs so the bindings will match.
					// We may need to do this more than once if unused inputs change after the removal. Ie. for complex shaders, what can happen is:
					// pass1 detects that input A is not used, but input B and C are. Input A is removed, and we recompile (pass2). After the recompilation, we see that Input B is now also unused in pass2 
					// (it became simpler and the compiler could see through that).
					// Since unused inputs are passed to the next stage, that will cause us to generate a vertex shader that does not output B, but our pixel shader will still be expecting B on input,
					// as it was rewritten based on the pass1 results.

					FShaderCompilerOutput OriginalOutput = Output;
					const int kMaxReasonableAttempts = 64;
					for (int32 Attempt = 0; Attempt < kMaxReasonableAttempts; ++Attempt)
					{
						TArray<FString> RemoveErrors;
						FString ModifiedShaderSource = PreprocessedShaderSource;
						FString ModifiedEntryPointName = Input.EntryPointName;
						if (RemoveUnusedInputs(ModifiedShaderSource, ShaderInputs, ModifiedEntryPointName, RemoveErrors))
						{
							Output = OriginalOutput;
							if (!CompileAndProcessD3DShaderFXCExt(CompileFlags, Input, ModifiedShaderSource, ModifiedEntryPointName, ShaderParameterParser, ShaderProfile, true, FilteredErrors, Output))
							{
								// if we failed to compile the shader, propagate the error up
								return false;
							}

							// check if the ShaderInputs changed - if not, we're done here
							if (Output.UsedAttributes.Num() == ShaderInputs.Num())
							{
								Output.ModifiedShaderSource = MoveTemp(ModifiedShaderSource);
								Output.ModifiedEntryPointName = MoveTemp(ModifiedEntryPointName);

								return true;
							}

							// second pass cannot use more attributes than previously
							if (Output.UsedAttributes.Num() > ShaderInputs.Num())
							{
								UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("Second pass had more used attributes (%d) than first pass (%d)"), Output.UsedAttributes.Num(), ShaderInputs.Num());
								FShaderCompilerError NewError;
								NewError.StrippedErrorMessage = FString::Printf(TEXT("Second pass had more used attributes (%d) than first pass (%d)"), Output.UsedAttributes.Num(), ShaderInputs.Num());
								Output = OriginalOutput;
								Output.Errors.Add(NewError);
								break;
							}

							// if we're about to run out of attempts, report
							if (Attempt >= kMaxReasonableAttempts - 1)
							{
								UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("Unable to determine unused inputs after %d attempts (last number of used attributes: %d, previous step:%d)!"), 
									Attempt + 1,
									Output.UsedAttributes.Num(),
									ShaderInputs.Num()
									);
								FShaderCompilerError NewError;
								NewError.StrippedErrorMessage = FString::Printf(TEXT("Unable to determine unused inputs after %d attempts (last number of used attributes: %d, previous step:%d)!"),
									Attempt + 1,
									Output.UsedAttributes.Num(),
									ShaderInputs.Num()
									);
								Output = OriginalOutput;
								Output.Errors.Add(NewError);
								break;
							}

							ShaderInputs = Output.UsedAttributes;
							// go around to remove newly identified unused inputs
						}
						else
						{
							UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("Failed to remove unused inputs from shader: %s"), *Input.GenerateShaderName());
							for (const FString& ErrorMessage : RemoveErrors)
							{
								// Add error to shader output but also make sure the error shows up on build farm by emitting a log entry
								UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("%s"), *ErrorMessage);
								FShaderCompilerError NewError;
								NewError.StrippedErrorMessage = ErrorMessage;
								Output.Errors.Add(NewError);
							}
							break;
						}
					}
				}
			}

			const uint32 BindingSpace = 0; // Default binding space for D3D11 shaders
			ExtractParameterMapFromD3DShader<
				ID3D11ShaderReflection, D3D11_SHADER_DESC, D3D11_SHADER_INPUT_BIND_DESC,
				ID3D11ShaderReflectionConstantBuffer, D3D11_SHADER_BUFFER_DESC,
				ID3D11ShaderReflectionVariable, D3D11_SHADER_VARIABLE_DESC>(
					Input, ShaderParameterParser,
					BindingSpace, Reflector, ShaderDesc,
					bGlobalUniformBufferUsed, bDiagnosticBufferUsed,
					NumSamplers, NumSRVs, NumCBs, NumUAVs,
					Output, UniformBufferNames, UsedUniformBufferSlots, VendorExtensions);

			NumInstructions = ShaderDesc.InstructionCount;
		}
		else
		{
			FilteredErrors.Add(TEXT("Couldn't find shader reflection function in D3D Compiler DLL"));
			Result = E_FAIL;
			Output.bSucceeded = false;
		}
		
		if (!ValidateResourceCounts(NumSRVs, NumSamplers, NumUAVs, NumCBs, FilteredErrors))
		{
			Result = E_FAIL;
			Output.bSucceeded = false;
		}

		// Check for resource limits for feature level 11.0
		if (NumUAVs > GD3DMaximumNumUAVs)
		{
			FilteredErrors.Add(FString::Printf(TEXT("Number of UAVs exceeded limit: %d slots used, but limit is %d due to maximum feature level 11.0"), NumUAVs, GD3DMaximumNumUAVs));
			Result = E_FAIL;
			Output.bSucceeded = false;
		}

		// Save results if compilation and reflection succeeded
		if (Output.bSucceeded)
		{
			TRefCountPtr<ID3DBlob> CompressedData;

			if (Input.Environment.CompilerFlags.Contains(CFLAG_GenerateSymbols))
			{
				CompressedData = Shader;
			}
			else if (D3DStripShaderFunc)
			{
				// Strip shader reflection and debug info
				D3D_SHADER_DATA ShaderData;
				ShaderData.pBytecode = Shader->GetBufferPointer();
				ShaderData.BytecodeLength = Shader->GetBufferSize();
				Result = D3DStripShaderFunc(Shader->GetBufferPointer(),
					Shader->GetBufferSize(),
					D3DCOMPILER_STRIP_REFLECTION_DATA | D3DCOMPILER_STRIP_DEBUG_INFO | D3DCOMPILER_STRIP_TEST_BLOBS,
					CompressedData.GetInitReference());

				if (FAILED(Result))
				{
					UE_LOG(LogD3D11ShaderCompiler, Fatal, TEXT("D3DStripShader failed: Result=%08x"), Result);
				}
			}
			else
			{
				// D3DStripShader is not guaranteed to exist
				// e.g. the open-source DXIL shader compiler does not currently implement it
				CompressedData = Shader;
			}

			// Add resource masks before the parameters are pulled for the uniform buffers
			FShaderCodeResourceMasks ResourceMasks{};
			for (const auto& Param : Output.ParameterMap.GetParameterMap())
			{
				const FParameterAllocation& ParamAlloc = Param.Value;
				if (ParamAlloc.Type == EShaderParameterType::UAV)
				{
					ResourceMasks.UAVMask |= 1u << ParamAlloc.BaseIndex;
				}
			}

			auto AddOptionalDataCallback = [&](FShaderCode& ShaderCode)
			{
				Output.ShaderCode.AddOptionalData(ResourceMasks);
			};

			FShaderCodePackedResourceCounts PackedResourceCounts{};
			if (bGlobalUniformBufferUsed)
			{
				PackedResourceCounts.UsageFlags |= EShaderResourceUsageFlags::GlobalUniformBuffer;
			}

			PackedResourceCounts.NumSamplers = static_cast<uint8>(NumSamplers);
			PackedResourceCounts.NumSRVs = static_cast<uint8>(NumSRVs);
			PackedResourceCounts.NumCBs = static_cast<uint8>(NumCBs);
			PackedResourceCounts.NumUAVs = static_cast<uint8>(NumUAVs);

			GenerateFinalOutput(CompressedData,
				Input, VendorExtensions,
				UsedUniformBufferSlots, UniformBufferNames,
				bSecondPassAferUnusedInputRemoval, ShaderInputs,
				PackedResourceCounts, NumInstructions,
				Output,
				[](FMemoryWriter&){},
				AddOptionalDataCallback);
		}
	}

	return SUCCEEDED(Result);
}

bool CompileAndProcessD3DShaderFXC(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	const FString& InEntryPointName,
	const FShaderParameterParser& ShaderParameterParser,
	const TCHAR* ShaderProfile,
	bool bSecondPassAferUnusedInputRemoval,
	FShaderCompilerOutput& Output)
{
	// @TODO - implement different material path to allow us to remove backwards compat flag on sm5 shaders
	uint32 CompileFlags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY
		// Unpack uniform matrices as row-major to match the CPU layout.
		| D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

	if (Input.Environment.CompilerFlags.Contains(CFLAG_GenerateSymbols))
	{
		CompileFlags |= D3DCOMPILE_DEBUG;
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_Debug))
	{
		CompileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
	}
	else
	{
		if (Input.Environment.CompilerFlags.Contains(CFLAG_StandardOptimization))
		{
			CompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
		}
		else
		{
			CompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
		}
	}

	Input.Environment.CompilerFlags.Iterate([&CompileFlags](uint32 Flag)
		{
			CompileFlags |= TranslateCompilerFlagD3D11((ECompilerFlags)Flag);
		});

	TArray<FString> FilteredErrors;
	const bool bSuccess = CompileAndProcessD3DShaderFXCExt(CompileFlags, Input, InPreprocessedSource, InEntryPointName, ShaderParameterParser, ShaderProfile, false, FilteredErrors, Output);

	// Process errors
	for (int32 ErrorIndex = 0; ErrorIndex < FilteredErrors.Num(); ErrorIndex++)
	{
		const FString& CurrentError = FilteredErrors[ErrorIndex];
		FShaderCompilerError NewError;

		// Extract filename and line number from FXC output with format:
		// "d:\Project\Binaries\BasePassPixelShader(30,7): error X3000: invalid target or usage string"
		int32 FirstParenIndex = CurrentError.Find(TEXT("("));
		int32 LastParenIndex = CurrentError.Find(TEXT("):"));
		if (FirstParenIndex != INDEX_NONE &&
			LastParenIndex != INDEX_NONE &&
			LastParenIndex > FirstParenIndex)
		{
			// Extract and store error message with source filename
			NewError.ErrorVirtualFilePath = CurrentError.Left(FirstParenIndex);
			NewError.ErrorLineString = CurrentError.Mid(FirstParenIndex + 1, LastParenIndex - FirstParenIndex - FCString::Strlen(TEXT("(")));
			NewError.StrippedErrorMessage = CurrentError.Right(CurrentError.Len() - LastParenIndex - FCString::Strlen(TEXT("):")));
		}
		else
		{
			NewError.StrippedErrorMessage = CurrentError;
		}
		Output.Errors.Add(NewError);
	}

	return bSuccess;
}

struct FD3DShaderParameterParserPlatformConfiguration : public FShaderParameterParser::FPlatformConfiguration
{
	FD3DShaderParameterParserPlatformConfiguration()
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

void CompileD3DShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& InPreprocessOutput, FShaderCompilerOutput& Output, const FString& WorkingDirectory, ED3DShaderModel ShaderModel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CompileD3DShader);

	const TCHAR* ShaderProfile = GetShaderProfileName(Input, ShaderModel);

	if (!ShaderProfile)
	{
		Output.Errors.Add(FShaderCompilerError(*FString::Printf(TEXT("Unrecognized shader frequency %s"), GetShaderFrequencyString((EShaderFrequency)Input.Target.Frequency))));
		return;
	}

	FString EntryPointName = Input.EntryPointName;
	FString PreprocessedSource(InPreprocessOutput.GetSourceViewWide());

	FD3DShaderParameterParserPlatformConfiguration PlatformConfiguration;
	FShaderParameterParser ShaderParameterParser(PlatformConfiguration);
	if (!ShaderParameterParser.ParseAndModify(Input, Output.Errors, PreprocessedSource))
	{
		// The FShaderParameterParser will add any relevant errors.
		return;
	}

	if (ShaderParameterParser.DidModifyShader())
	{
		Output.ModifiedShaderSource = PreprocessedSource;
	}

	if (Input.Environment.CompilerFlags.Contains(CFLAG_ForceRemoveUnusedInterpolators) && Input.Target.Frequency == SF_Vertex && Input.bCompilingForShaderPipeline)
	{
		// Always add SV_Position
		TArray<FStringView> UsedOutputs;
		for (const FString& UsedOutput : Input.UsedOutputs)
		{
			UsedOutputs.Emplace(UsedOutput);
		}
		UsedOutputs.Emplace(TEXTVIEW("SV_POSITION"));
		UsedOutputs.Emplace(TEXTVIEW("SV_ViewPortArrayIndex"));

		// We can't remove any of the output-only system semantics
		//@todo - there are a bunch of tessellation ones as well
		const FStringView Exceptions[] =
		{
			TEXTVIEW("SV_ClipDistance"),
			TEXTVIEW("SV_ClipDistance0"),
			TEXTVIEW("SV_ClipDistance1"),
			TEXTVIEW("SV_ClipDistance2"),
			TEXTVIEW("SV_ClipDistance3"),
			TEXTVIEW("SV_ClipDistance4"),
			TEXTVIEW("SV_ClipDistance5"),
			TEXTVIEW("SV_ClipDistance6"),
			TEXTVIEW("SV_ClipDistance7"),

			TEXTVIEW("SV_CullDistance"),
			TEXTVIEW("SV_CullDistance0"),
			TEXTVIEW("SV_CullDistance1"),
			TEXTVIEW("SV_CullDistance2"),
			TEXTVIEW("SV_CullDistance3"),
			TEXTVIEW("SV_CullDistance4"),
			TEXTVIEW("SV_CullDistance5"),
			TEXTVIEW("SV_CullDistance6"),
			TEXTVIEW("SV_CullDistance7"),
		};

		TArray<FScopedDeclarations> ScopedDeclarations;
		const FStringView GlobalSymbols[] =
		{
			TEXTVIEW("RayDesc"),
		};
		ScopedDeclarations.Emplace(TConstArrayView<FStringView>(), GlobalSymbols);

		TArray<FString> Errors;
		if (!RemoveUnusedOutputs(PreprocessedSource, UsedOutputs, Exceptions, ScopedDeclarations, EntryPointName, Errors))
		{
			UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("Failed to remove unused outputs from shader: %s"), *Input.GenerateShaderName());
			for (const FString& ErrorReport : Errors)
			{
				// Add error to shader output but also make sure the error shows up on build farm by emitting a log entry
				UE_LOG(LogD3D11ShaderCompiler, Warning, TEXT("%s"), *ErrorReport);
				FShaderCompilerError NewError;
				NewError.StrippedErrorMessage = ErrorReport;
				Output.Errors.Add(NewError);
			}
		}
		else
		{
			Output.ModifiedEntryPointName = EntryPointName;
			Output.ModifiedShaderSource = PreprocessedSource;
		}
	}

	const bool bSuccess = DoesShaderModelRequireDXC(ShaderModel)
		? CompileAndProcessD3DShaderDXC(Input, PreprocessedSource, EntryPointName, ShaderParameterParser, ShaderProfile, ShaderModel, false, Output)
		: CompileAndProcessD3DShaderFXC(Input, PreprocessedSource, EntryPointName, ShaderParameterParser, ShaderProfile, false, Output);

	if (!bSuccess && !Output.Errors.Num())
	{
		Output.Errors.Add(TEXT("Compile failed without errors!"));
	}

	ShaderParameterParser.ValidateShaderParameterTypes(Input, Output);

	if (EnumHasAnyFlags(Input.DebugInfoFlags, EShaderDebugInfoFlags::CompileFromDebugUSF))
	{
		for (const FShaderCompilerError& Error : Output.Errors)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), *Error.GetErrorStringWithLineMarker());
		}
	}
}
