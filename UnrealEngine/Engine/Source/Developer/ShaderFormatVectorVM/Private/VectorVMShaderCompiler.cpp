// Copyright Epic Games, Inc. All Rights Reserved.
// .

#include "ShaderFormatVectorVM.h"
#include "VectorVMBackend.h"
#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "ShaderCore.h"
#include "CrossCompilerCommon.h"
#include "ShaderCompilerCommon.h"
#include "ShaderPreprocessor.h"

#include "VectorVM.h"
#include "Serialization/MemoryWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogVectorVMShaderCompiler, Log, All); 

DECLARE_STATS_GROUP(TEXT("VectorVM"), STATGROUP_VectorVM, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("VectorVM - Compiler - CompileShader_VectorVM"), STAT_VectorVM_Compiler_CompileShader_VectorVM, STATGROUP_VectorVM);
DECLARE_CYCLE_STAT(TEXT("VectorVM - Compiler - PreprocessShader"), STAT_VectorVM_Compiler_CompileShader_VectorVMPreprocessShader, STATGROUP_VectorVM);
DECLARE_CYCLE_STAT(TEXT("VectorVM - Compiler - CrossCompilerContextRun"), STAT_VectorVM_Compiler_CompileShader_CrossCompilerContextRun, STATGROUP_VectorVM);


/**
 * Compile a shader for the VectorVM
 * @param Input - The input shader code and environment.
 * @param Output - Contains shader compilation results upon return.
 */
bool CompileShader_VectorVM(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory, uint8 Version)
{
	FVectorVMCompilationOutput CompilationOutput;
	bool Result = CompileShader_VectorVM(Input, Output, WorkingDirectory, Version, CompilationOutput, Input.Environment.CompilerFlags.Contains(CFLAG_SkipOptimizations));
	
	if (Result)
	{
		FMemoryWriter Ar(Output.ShaderCode.GetWriteAccess(), true);
		Ar << CompilationOutput;
		Output.bSucceeded = true;
	}
	else if (CompilationOutput.Errors.Len() > 0)
	{
		Output.Errors.Add(FShaderCompilerError(*CompilationOutput.Errors));
	}
	
	return Result;
}

//TODO: Move to this output living in the shader eco-system with the compute shaders too but for now just do things more directly.
bool CompileShader_VectorVM(const FShaderCompilerInput& Input, FShaderCompilerOutput& Output, const FString& WorkingDirectory, uint8 Version, FVectorVMCompilationOutput& VMCompilationOutput, bool bSkipBackendOptimizations)
{
	SCOPE_CYCLE_COUNTER(STAT_VectorVM_Compiler_CompileShader_VectorVM);

	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HCT_FeatureLevelSM5;
	ECompilerFlags PlatformFlowControl = CFLAG_AvoidFlowControl;

	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1);
	AdditionalDefines.SetDefine(TEXT("COMPILER_VECTORVM"), 1);
	AdditionalDefines.SetDefine(TEXT("VECTORVM_PROFILE"), 1);
	
	const bool bDumpDebugInfo = (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath));

	AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);

	if (Input.bSkipPreprocessedCache)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShader, *Input.VirtualSourceFilePath))
		{
			return false;
		}

		// Remove const as we are on debug-only mode
		CrossCompiler::CreateEnvironmentFromResourceTable(PreprocessedShader, (FShaderCompilerEnvironment&)Input.Environment);
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_VectorVM_Compiler_CompileShader_VectorVMPreprocessShader);

		// Don't include shader definitions since it creates shader compilation errors.
		if (!PreprocessShader(PreprocessedShader, Output, Input, AdditionalDefines, EDumpShaderDefines::DontIncludeDefines))
		{
			// The preprocessing stage will add any relevant errors.
			if (Output.Errors.Num() != 0)
			{
				FString Errors;
				for (const FShaderCompilerError& Error : Output.Errors)
				{
					Errors += Error.GetErrorString() + TEXT("\r\n");
				}
				VMCompilationOutput.Errors = Errors;
			}
			return false;
		}
	}

	//TODO: Need to remove any unsupported features here?

	char* ShaderSource = NULL;
	char* ErrorLog = NULL;

	const EHlslShaderFrequency Frequency = HSF_VertexShader;

 		// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (bDumpDebugInfo)
	{
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.GetSourceFilename()));
		if (FileWriter)
		{
			auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShader);
			FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
			{
				FString Line = CrossCompiler::CreateResourceTableFromEnvironment(Input.Environment);
				FileWriter->Serialize(TCHAR_TO_ANSI(*Line), Line.Len());
			}
			FileWriter->Close();
			delete FileWriter;
		}

		if (Input.bGenerateDirectCompileFile)
		{
			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *(Input.DumpDebugInfoPath / TEXT("DirectCompile.txt")));
		}
	}

	//Is stuff like this needed? What others?
	uint32 CCFlags = HLSLCC_NoPreprocess;
		//CCFlags |= HLSLCC_PrintAST;
	//CCFlags |= HLSLCC_UseFullPrecisionInPS;

	if (bSkipBackendOptimizations)
	{
		CCFlags |= HLSLCC_DisableBackendOptimizations;
	}

	//TODO: Do this later when we implement the rest of the shader plumbing stuff.
// 		if (bDumpDebugInfo)
// 		{
// 			const FString VVMFile = (Input.DumpDebugInfoPath / TEXT("Output.vvm"));
// 			const FString USFFile = (Input.DumpDebugInfoPath / Input.GetSourceFilename());
// 			const FString CCBatchFileContents = CreateCrossCompilerBatchFile(USFFile, VVMFile, *Input.EntryPointName, Frequency, Version, CCFlags);
// 			if (!CCBatchFileContents.IsEmpty())
// 			{
// 				const TCHAR * ScriptName = PLATFORM_WINDOWS ? TEXT("CrossCompile.bat") : TEXT("CrossCompile.sh");
// 				FFileHelper::SaveStringToFile(CCBatchFileContents, *(Input.DumpDebugInfoPath / ScriptName));
// 			}
// 		}

	//NEEDED?
	// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
	//CCFlags &= ~HLSLCC_NoPreprocess;

	FVectorVMCodeBackend VVMBackEnd(CCFlags, HlslCompilerTarget, VMCompilationOutput);
		FVectorVMLanguageSpec VVMLanguageSpec; 

	bool Result = false;
	FHlslCrossCompilerContext CrossCompilerContext(CCFlags, Frequency, HlslCompilerTarget);
	if (CrossCompilerContext.Init(TCHAR_TO_ANSI(*Input.VirtualSourceFilePath), &VVMLanguageSpec))
	{
		SCOPE_CYCLE_COUNTER(STAT_VectorVM_Compiler_CompileShader_CrossCompilerContextRun);

		Result = CrossCompilerContext.Run(
			TCHAR_TO_ANSI(*PreprocessedShader),
			TCHAR_TO_ANSI(*Input.EntryPointName),
			&VVMBackEnd,
			&ShaderSource,
			&ErrorLog
			) ? 1 : 0;
	}

	if (ErrorLog)
	{
		int32 SrcLen = FPlatformString::Strlen(ErrorLog);
		int32 DestLen = FPlatformString::ConvertedLength<TCHAR, ANSICHAR>(ErrorLog, SrcLen);
		TArray<TCHAR> Converted;
		Converted.AddUninitialized(DestLen);

		FPlatformString::Convert<ANSICHAR, TCHAR>(Converted.GetData(), DestLen, ErrorLog, SrcLen);
		Converted.Add(0);

		VMCompilationOutput.Errors = Converted.GetData();

		TArray<FString> OutputByLines;
		PreprocessedShader.ParseIntoArrayLines(OutputByLines, false);

		UE_LOG(LogVectorVMShaderCompiler, Warning, TEXT("Warnings while processing %s"), *Input.DebugGroupName);

		FString OutputHlsl;
		for (int32 i = 0; i < OutputByLines.Num(); i++)
		{
			UE_LOG(LogVectorVMShaderCompiler, Display, TEXT("/*%d*/%s"), i, *OutputByLines[i]);
		}
	}
	else
	{
		VMCompilationOutput.Errors.Empty();
	}

	//TODO: Try to get rid of the CompilationOutput and have the vm bytecode life in the shader eco-system as the compute shader version will.
//		int32 SourceLen = VVMBackEnd.ByteCode.Num();//ShaderSource ? FCStringAnsi::Strlen(ShaderSource) : 0;
// 		if (SourceLen > 0)
// 		{
// 			if (bDumpDebugInfo)
// 			{
// 				const FString GLSLFile = (Input.DumpDebugInfoPath / TEXT("Output.vvm"));
// 
// 				if (SourceLen > 0)
// 				{
// 					uint32 Len = FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.VirtualSourceFilePath)) + FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.EntryPointName)) + FCStringAnsi::Strlen(ShaderSource) + 20;
// 					char* Dest = (char*)malloc(Len);
// 					FCStringAnsi::Snprintf(Dest, Len, "// ! %s:%s\n%s", (const char*)TCHAR_TO_ANSI(*Input.VirtualSourceFilePath), (const char*)TCHAR_TO_ANSI(*Input.EntryPointName), (const char*)ShaderSource);
// 					free(ShaderSource);
// 					ShaderSource = Dest;
// 					SourceLen = FCStringAnsi::Strlen(ShaderSource);
// 					
// 					FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.VirtualSourceFilePath + TEXT(".vvm")));
// 					if (FileWriter)
// 					{
// 						FileWriter->Serialize(ShaderSource,SourceLen+1);
// 						FileWriter->Close();
// 						delete FileWriter;
// 					}
// 				}
// 			}

		//HMMMM....?
// #if VALIDATE_GLSL_WITH_DRIVER
// 			PrecompileShader(Output, Input, ShaderSource, Version, Frequency);
// #else // VALIDATE_GLSL_WITH_DRIVER
		//SourceLen = FCStringAnsi::Strlen(ShaderSource);
//			Output.Target = Input.Target;
		//BuildShaderOutput(Output, Input, ShaderSource, SourceLen, Version);

//			Output.bSucceeded = true;
		//Should we add optional data to define the register allocations etc so that someone can just write hlsl directly and have it become a niagara sim?
//			Output.ShaderCode.GetWriteAccess().Append((uint8*)VVMBackEnd.ByteCode.GetData(), VVMBackEnd.ByteCode.Num());
//			Output.ShaderCode.FinalizeShaderCode();
//#endif // VALIDATE_GLSL_WITH_DRIVER
// 		}
// 		else
// 		{
		//ES2 Command line is throwing me off. Is this needed?

// 			if (bDumpDebugInfo)
// 			{
// 				// Generate the batch file to help track down cross-compiler issues if necessary
// 				const FString VVMFile = (Input.DumpDebugInfoPath / TEXT("Output.vvm"));
// 				const FString VVMBatchFileContents = CreateCommandLineGLSLES2(VVMFile, (Input.DumpDebugInfoPath / TEXT("Output.asm")), Version, Frequency, false);
// 				if (!VVMBatchFileContents.IsEmpty())
// 				{
// 					FFileHelper::SaveStringToFile(VVMBatchFileContents, *(Input.DumpDebugInfoPath / TEXT("VVMCompile.bat")));
// 				}
// 			}
// 
// 			Output.bSucceeded = false;
// 
// 			FString Tmp = ANSI_TO_TCHAR(ErrorLog);
// 			TArray<FString> ErrorLines;
// 			Tmp.ParseIntoArray(ErrorLines, TEXT("\n"), true);
// 			for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
// 			{
// 				const FString& Line = ErrorLines[LineIndex];
// 				CrossCompiler::ParseHlslccError(Output.Errors, Line);
// 			}
// 		}

	if (ShaderSource)
	{
		UE_LOG(LogVectorVMShaderCompiler, Warning, TEXT("%s"), (const char*)ShaderSource);
		free(ShaderSource);
	}
	if (VMCompilationOutput.Errors.Len() != 0)
	{
		UE_LOG(LogVectorVMShaderCompiler, Warning, TEXT("%s"), *VMCompilationOutput.Errors);
		free(ErrorLog);
	}

	return Result;
}

