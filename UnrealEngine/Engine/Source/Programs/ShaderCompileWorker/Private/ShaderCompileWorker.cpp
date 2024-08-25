// Copyright Epic Games, Inc. All Rights Reserved.


// ShaderCompileWorker.cpp : Defines the entry point for the console application.
//

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "Misc/Compression.h"
#include "RequiredProgramMainCPPInclude.h"
#include "ShaderCompilerCore.h"
#include "ShaderCompilerCommon.h"
#include "ShaderCompilerJobTypes.h"
#include "ShaderCore.h"
#include "HAL/ExceptionHandling.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "RHIShaderFormatDefinitions.inl"
#include "ShaderCompilerCommon.h"
#include "Serialization/MemoryReader.h"
#include "SocketSubsystem.h"

#define DEBUG_USING_CONSOLE	0

static double LastCompileTime = 0.0;
static int32 GNumProcessedJobs = 0;

enum class EXGEMode
{
	None,
	Xml,
	Intercept
};

static EXGEMode GXGEMode = EXGEMode::None;

inline bool IsUsingXGE()
{
	return GXGEMode != EXGEMode::None;
}

static FSCWErrorCode::ECode GFailedErrorCode = FSCWErrorCode::Success;

static void OnXGEJobCompleted(const TCHAR* WorkingDirectory)
{
	if (GXGEMode == EXGEMode::Xml)
	{
		// To signal compilation completion, create a zero length file in the working directory.
		// This is only required in Xml mode.
		delete IFileManager::Get().CreateFileWriter(*FString::Printf(TEXT("%s/Success"), WorkingDirectory), FILEWRITE_EvenIfReadOnly);
	}
}

#if PLATFORM_WINDOWS // Currently only implemented for windows
HMODULE GetUbaModule()
{
	static HMODULE UbaDetoursModule = GetModuleHandleW(L"UbaDetours.dll");
	return UbaDetoursModule;
}
#endif

inline bool IsUsingUBA()
{
#if PLATFORM_WINDOWS // Currently only implemented for windows
	return GetUbaModule() != nullptr;
#else
	return false;
#endif
}

#if USING_CODE_ANALYSIS
	[[noreturn]] static inline void ExitWithoutCrash(FSCWErrorCode::ECode ErrorCode, const FString& Message);
#endif

static inline void ExitWithoutCrash(FSCWErrorCode::ECode ErrorCode, const FString& Message)
{
	GFailedErrorCode = ErrorCode;
	FCString::Snprintf(GErrorExceptionDescription, sizeof(GErrorExceptionDescription), TEXT("%s"), *Message);
	UE_LOG(LogShaders, Fatal, TEXT("%s"), *Message);
}

static const TArray<const IShaderFormat*>& GetShaderFormats()
{
	static bool bInitialized = false;
	static TArray<const IShaderFormat*> Results;

	if (!bInitialized)
	{
		bInitialized = true;
		Results.Empty(Results.Num());

		TArray<FName> Modules;
		FModuleManager::Get().FindModules(SHADERFORMAT_MODULE_WILDCARD, Modules);

		if (!Modules.Num())
		{
			ExitWithoutCrash(FSCWErrorCode::NoTargetShaderFormatsFound, TEXT("No target shader formats found!"));
		}

		for (int32 Index = 0; Index < Modules.Num(); Index++)
		{
			IShaderFormat* Format = FModuleManager::LoadModuleChecked<IShaderFormatModule>(Modules[Index]).GetShaderFormat();
			if (Format != nullptr)
			{
				Results.Add(Format);
			}
		}
	}
	return Results;
}


static void UpdateFileSize(FArchive& OutputFile, int64 FileSizePosition)
{
	int64 Current = OutputFile.Tell();
	OutputFile.Seek(FileSizePosition);
	OutputFile << Current;
	OutputFile.Seek(Current);
};

static const TCHAR* GetLocalHostname(int32* OutHostnameLength = nullptr)
{
	static FString Hostname;
	if (Hostname.IsEmpty())
	{
		ISocketSubsystem::Get()->GetHostName(Hostname);
	}
	if (OutHostnameLength)
	{
		*OutHostnameLength = Hostname.Len();
	}
	return *Hostname;
}

static int64 WriteOutputFileHeader(FArchive& OutputFile, int32 ErrorCode, int32 CallstackLength, const TCHAR* Callstack,
	int32 ExceptionInfoLength, const TCHAR* ExceptionInfo)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WriteOutputFileHeader);

	int64 FileSizePosition = 0;
	int32 OutputVersion = ShaderCompileWorkerOutputVersion;
	OutputFile << OutputVersion;

	int64 FileSize = 0;
	// Get the position of the Size value to be patched in as the shader progresses
	FileSizePosition = OutputFile.Tell();
	OutputFile << FileSize;

	OutputFile << ErrorCode;

	OutputFile << GNumProcessedJobs;

	// Note: Can't use FStrings here as SEH can't be used with destructors
	OutputFile << CallstackLength;

	OutputFile << ExceptionInfoLength;

	int32 HostnameLength = 0;
	const TCHAR* Hostname = GetLocalHostname(&HostnameLength);
	OutputFile << HostnameLength;

	if (ErrorCode != FSCWErrorCode::Success)
	{
		if (CallstackLength > 0)
		{
			OutputFile.Serialize((void*)Callstack, CallstackLength * sizeof(TCHAR));
		}

		if (ExceptionInfoLength > 0)
		{
			OutputFile.Serialize((void*)ExceptionInfo, ExceptionInfoLength * sizeof(TCHAR));
		}

		if (HostnameLength > 0)
		{
			OutputFile.Serialize((void*)Hostname, HostnameLength * sizeof(TCHAR));
		}

		// Store available and used physical memory of host machine on OOM error
		if (ErrorCode == FSCWErrorCode::OutOfMemory)
		{
			FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();

			OutputFile
				<< MemoryStats.AvailablePhysical
				<< MemoryStats.AvailableVirtual
				<< MemoryStats.UsedPhysical
				<< MemoryStats.PeakUsedPhysical
				<< MemoryStats.UsedVirtual
				<< MemoryStats.PeakUsedVirtual
				;
		}
	}

	// Reset error code as it can be receive a new value now
	FSCWErrorCode::Reset();

	UpdateFileSize(OutputFile, FileSizePosition);
	return FileSizePosition;
}


class FWorkLoop
{
public:
	// If we have been idle for 20 seconds then exit. Can be overriden from the cmd line with -TimeToLive=N where N is in seconds (and a float value)
	float TimeToLive = 20.0f;
	int32 NumberToProcess = -1;
	bool DisableFileWrite = false;
	bool KeepInput = false;

	FWorkLoop(const TCHAR* ParentProcessIdText,const TCHAR* InWorkingDirectory,const TCHAR* InInputFilename,const TCHAR* InOutputFilename, TMap<FString, uint32>& InFormatVersionMap)
	:	ParentProcessId(FCString::Atoi(ParentProcessIdText))
	,	WorkingDirectory(InWorkingDirectory)
	,	InputFilename(InInputFilename)
	,	OutputFilename(InOutputFilename)
	,	InputFilePath(FString(InWorkingDirectory) / InInputFilename)
	,	OutputFilePath(FString(InWorkingDirectory) / InOutputFilename)
	,	FormatVersionMap(InFormatVersionMap)
	{
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
		for (FString& Switch : Switches)
		{
			if (Switch.StartsWith(TEXT("TimeToLive=")))
			{
				TimeToLive = FCString::Atof(Switch.GetCharArray().GetData() + 11);
			}
			else if (Switch.Equals(TEXT("DisableFileWrite")))
			{
				DisableFileWrite = true;
			}
			else if (Switch.Equals(TEXT("KeepInput")))
			{
				KeepInput = true;
			}
			else if (Switch.StartsWith(TEXT("NumJobs=")))
			{
				NumberToProcess = FCString::Atoi(Switch.GetCharArray().GetData() + 8);
			}
		}
	}

	void Loop(FString& CrashOutputFile)
	{
		UE_LOG(LogShaders, Log, TEXT("Entering job loop"));
		TRACE_CPUPROFILER_EVENT_SCOPE(Loop);

		int32 NumberProcessed = 0;

		while(true)
		{
			TArray<FShaderCompileJob> SingleJobs;
			TArray<FShaderPipelineCompileJob> PipelineJobs;
			TArray<FString> PipelineJobNames;

			// Read & Process Input
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ReadInput);

				FArchive* InputFilePtr = OpenInputFile();
				if(!InputFilePtr)
				{
					break;
				}

				UE_LOG(LogShaders, Log, TEXT("Processing shader"));

				ProcessInputFromArchive(InputFilePtr, SingleJobs, PipelineJobs, PipelineJobNames);

				LastCompileTime = FPlatformTime::Seconds();

				// Close the input file.
				delete InputFilePtr;
			}

			// Prepare for output
			if (DisableFileWrite)
			{
				// write to in-memory bytestream instead for debugging purposes
				TArray<uint8> MemBlock;
				FMemoryWriter MemWriter(MemBlock);
				WriteToOutputArchive(&MemWriter, SingleJobs, PipelineJobs, PipelineJobNames);
			}
			else
			{
				FArchive* OutputFilePtr = CreateOutputArchive();
				check(OutputFilePtr);
				WriteToOutputArchive(OutputFilePtr, SingleJobs, PipelineJobs, PipelineJobNames);
				// Close the output file.
				delete OutputFilePtr;

				// Change the output file name to requested one
				IFileManager::Get().Move(*OutputFilePath, *TempFilePath);
			}

			if (IsUsingXGE())
			{
				// To signal compilation completion, create a zero length file in the working directory.
				OnXGEJobCompleted(*WorkingDirectory);

				// We only do one pass per process when using XGE.
				break;
			}

#if PLATFORM_WINDOWS // Currently only implemented for windows
			if (HMODULE UbaDetoursModule = GetUbaModule())
			{
				using UbaRequestNextProcessFunc = bool(uint32 prevExitCode, TCHAR* outArguments, uint32 outArgumentsCapacity);
				static UbaRequestNextProcessFunc* RequestNextProcess = (UbaRequestNextProcessFunc*)(void*)GetProcAddress(UbaDetoursModule, "UbaRequestNextProcess");

				// Request new process
				TCHAR Arguments[1024];
				if (!RequestNextProcess(0, Arguments, 1024))
				{
					break; // No process available, exit loop
				}

				// We got a new process, change inputs and outputs and run again
				
				TArray<FString> Tokens;
				TArray<FString> Switches;
				FCommandLine::Parse(Arguments, Tokens, Switches);

				WorkingDirectory = Tokens[0];
				InputFilename = Tokens[3];
				OutputFilename = Tokens[4];

				InputFilePath = WorkingDirectory / InputFilename;
				OutputFilePath = WorkingDirectory / OutputFilename;

				CrashOutputFile = OutputFilePath;
				continue;
			}
#endif

			if (TimeToLive == 0)
			{
				UE_LOG(LogShaders, Log, TEXT("TimeToLive set to 0, exiting after single job"));
				break;
			}

			NumberProcessed++;
			if (NumberToProcess > 0 && NumberProcessed > NumberToProcess)
			{
				UE_LOG(LogShaders, Log, TEXT("NumJobs limit hit"));
				break;
			}

#if ENABLE_LOW_LEVEL_MEM_TRACKER
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UpdateStatsPerFrame);

				FLowLevelMemTracker::Get().UpdateStatsPerFrame();
			}
#endif
		}

		UE_LOG(LogShaders, Log, TEXT("Exiting job loop"));
	}

private:
	const int32 ParentProcessId;
	FString WorkingDirectory;
	FString InputFilename;
	FString OutputFilename;

	FString InputFilePath;
	FString OutputFilePath;
	TMap<FString, uint32> FormatVersionMap;
	FString TempFilePath;

	/** Opens an input file, trying multiple times if necessary. */
	FArchive* OpenInputFile()
	{
		FArchive* InputFile = nullptr;
		bool bFirstOpenTry = true;
		while(!InputFile && !IsEngineExitRequested())
		{
			// Try to open the input file that we are going to process
			InputFile = IFileManager::Get().CreateFileReader(*InputFilePath,FILEREAD_Silent);

			if(!InputFile && !bFirstOpenTry)
			{
				CheckExitConditions();
				// Give up CPU time while we are waiting
				FPlatformProcess::Sleep(0.01f);
			}
			bFirstOpenTry = false;
		}
		return InputFile;
	}

	void VerifyFormatVersions(TMap<FString, uint32>& ReceivedFormatVersionMap)
	{
		for (auto Pair : ReceivedFormatVersionMap)
		{
			auto* Found = FormatVersionMap.Find(Pair.Key);
			if (Found)
			{
				if (Pair.Value != *Found)
				{
					ExitWithoutCrash(FSCWErrorCode::BadShaderFormatVersion, FString::Printf(TEXT("Mismatched shader version for format %s: Found version %u but expected %u; did you forget to build ShaderCompilerWorker?"), *Pair.Key, *Found, Pair.Value));
				}
			}
		}
	}

	void ProcessInputFromArchive(FArchive* InputFilePtr, TArray<FShaderCompileJob>& OutSingleJobs, TArray<FShaderPipelineCompileJob>& OutPipelineJobs, TArray<FString>& OutPipelineNames)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ProcessInputFromArchive);

		int32 InputVersion;
		*InputFilePtr << InputVersion;
		if (ShaderCompileWorkerInputVersion != InputVersion)
		{
			ExitWithoutCrash(FSCWErrorCode::BadInputVersion, FString::Printf(TEXT("Exiting due to ShaderCompilerWorker expecting input version %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerInputVersion, InputVersion));
		}

		FString CompressionFormatString;
		*InputFilePtr << CompressionFormatString;
		FName CompressionFormat(*CompressionFormatString);

		bool bWasCompressed = (CompressionFormat != NAME_None);

		TArray<uint8> UncompressedData;
		if (bWasCompressed)
		{
			int32 UncompressedDataSize = 0;
			*InputFilePtr << UncompressedDataSize;

			if (UncompressedDataSize == 0)
			{
				ExitWithoutCrash(FSCWErrorCode::BadInputFile, TEXT("Exiting due to bad input file to ShaderCompilerWorker (uncompressed size is 0)! Did you forget to build ShaderCompilerWorker?"));
				// unreachable
				return;
			}

			UncompressedData.SetNumUninitialized(UncompressedDataSize);
			TArray<uint8> CompressedData;
			*InputFilePtr << CompressedData;
			if (!FCompression::UncompressMemory(CompressionFormat, UncompressedData.GetData(), UncompressedDataSize, CompressedData.GetData(), CompressedData.Num()))
			{
				ExitWithoutCrash(FSCWErrorCode::BadInputFile, FString::Printf(TEXT("Exiting due to bad input file to ShaderCompilerWorker (cannot uncompress with the format %s)! Did you forget to build ShaderCompilerWorker?"), *CompressionFormatString));
				// unreachable
				return;
			}
		}
		FMemoryReader InputMemory(UncompressedData);
		FArchive& InputFile = bWasCompressed ? InputMemory : *InputFilePtr;

		TMap<FString, uint32> ReceivedFormatVersionMap;
		InputFile << ReceivedFormatVersionMap;

		VerifyFormatVersions(ReceivedFormatVersionMap);
		
		// Apply shader source directory mappings.
		{
			TMap<FString, FString> DirectoryMappings;
			InputFile << DirectoryMappings;

			ResetAllShaderSourceDirectoryMappings();
			for (TPair<FString, FString>& MappingEntry : DirectoryMappings)
			{
				FPaths::NormalizeDirectoryName(MappingEntry.Value);
				AddShaderSourceDirectoryMapping(MappingEntry.Key, MappingEntry.Value);
			}
		}

		// Initialize shader hash cache before reading any includes.
		InitializeShaderHashCache();

		// Array of string used as const TCHAR* during compilation process.
		TArray<TUniquePtr<FString>> AllocatedStrings;
		auto DeserializeConstTCHAR = [&AllocatedStrings](FArchive& Archive)
		{
			FString Name;
			Archive << Name;

			const TCHAR* CharName = nullptr;
			if (Name.Len() != 0)
			{
				if (AllocatedStrings.GetSlack() == 0)
				{
					AllocatedStrings.Reserve(AllocatedStrings.Num() + 1024);
				}

				AllocatedStrings.Add(MakeUnique<FString>(Name));
				CharName = **AllocatedStrings.Last();
			}
			return CharName;
		};

		// Array of string used as const ANSICHAR* during compilation process.
		TArray<TUniquePtr<TArray<ANSICHAR>>> AllocatedAnsiStrings;
		auto DeserializeConstANSICHAR = [&AllocatedAnsiStrings](FArchive& Archive)
		{
			FString Name;
			Archive << Name;

			const ANSICHAR* CharName = nullptr;
			if (Name.Len() != 0)
			{
				if (AllocatedAnsiStrings.GetSlack() == 0)
				{
					AllocatedAnsiStrings.Reserve(AllocatedAnsiStrings.Num() + 1024);
				}

				TArray<ANSICHAR> AnsiString;
				AnsiString.SetNumZeroed(Name.Len() + 1);
				ANSICHAR* Dest = &AnsiString[0];
				FCStringAnsi::Strcpy(Dest, Name.Len() + 1, TCHAR_TO_ANSI(*Name));

				AllocatedAnsiStrings.Add(MakeUnique<TArray<ANSICHAR>>(AnsiString));
				CharName = &(*AllocatedAnsiStrings.Last())[0];
			}
			return CharName;
		};

		// Shared inputs
		TMap<FString, FThreadSafeSharedAnsiStringPtr> ExternalIncludes;
		{
			int32 NumExternalIncludes = 0;
			InputFile << NumExternalIncludes;
			ExternalIncludes.Reserve(NumExternalIncludes);

			for (int32 IncludeIndex = 0; IncludeIndex < NumExternalIncludes; IncludeIndex++)
			{
				FString NewIncludeName;
				InputFile << NewIncludeName;
				TArray<ANSICHAR>* NewIncludeContents = new TArray<ANSICHAR>;
				InputFile << (*NewIncludeContents);
				ExternalIncludes.Add(NewIncludeName, MakeShareable(NewIncludeContents));
			}
		}

		// Shared environments
		TArray<FShaderCompilerEnvironment> SharedEnvironments;
		{
			int32 NumSharedEnvironments = 0;
			InputFile << NumSharedEnvironments;
			SharedEnvironments.Empty(NumSharedEnvironments);
			SharedEnvironments.AddDefaulted(NumSharedEnvironments);

			for (int32 EnvironmentIndex = 0; EnvironmentIndex < NumSharedEnvironments; EnvironmentIndex++)
			{
				InputFile << SharedEnvironments[EnvironmentIndex];
			}
		}

		// All the shader parameter structures
		// Note: this is a bit more complicated, purposefully to avoid switch const TCHAR* to FString in runtime FShaderParametersMetadata.
		TArray<TUniquePtr<FShaderParametersMetadata>> ParameterStructures;
		{
			int32 NumParameterStructures = 0;
			InputFile << NumParameterStructures;
			ParameterStructures.Reserve(NumParameterStructures);

			for (int32 StructIndex = 0; StructIndex < NumParameterStructures; StructIndex++)
			{
				const TCHAR* LayoutName;
				const TCHAR* StructTypeName;
				const TCHAR* ShaderVariableName;
				FShaderParametersMetadata::EUseCase UseCase;
				const ANSICHAR* StructFileName;
				int32 StructFileLine;
				uint32 Size;
				int32 MemberCount;

				LayoutName = DeserializeConstTCHAR(InputFile);
				StructTypeName = DeserializeConstTCHAR(InputFile);
				ShaderVariableName = DeserializeConstTCHAR(InputFile);
				InputFile << UseCase;
				StructFileName = DeserializeConstANSICHAR(InputFile);
				InputFile << StructFileLine;
				InputFile << Size;
				InputFile << MemberCount;

				TArray<FShaderParametersMetadata::FMember> Members;
				Members.Reserve(MemberCount);

				for (int32 MemberIndex = 0; MemberIndex < MemberCount; MemberIndex++)
				{
					const TCHAR* Name;
					const TCHAR* ShaderType;
					int32 FileLine;
					uint32 Offset;
					uint8 BaseType;
					uint8 PrecisionModifier;
					uint32 NumRows;
					uint32 NumColumns;
					uint32 NumElements;
					int32 StructMetadataIndex;

					static_assert(sizeof(BaseType) == sizeof(EUniformBufferBaseType), "Cast failure.");
					static_assert(sizeof(PrecisionModifier) == sizeof(EShaderPrecisionModifier::Type), "Cast failure.");

					Name = DeserializeConstTCHAR(InputFile);
					ShaderType = DeserializeConstTCHAR(InputFile);
					InputFile << FileLine;
					InputFile << Offset;
					InputFile << BaseType;
					InputFile << PrecisionModifier;
					InputFile << NumRows;
					InputFile << NumColumns;
					InputFile << NumElements;
					InputFile << StructMetadataIndex;

					if (ShaderType == nullptr)
					{
						ShaderType = TEXT("");
					}

					const FShaderParametersMetadata* StructMetadata = nullptr;
					if (StructMetadataIndex != INDEX_NONE)
					{
						StructMetadata = ParameterStructures[StructMetadataIndex].Get();
					}

					FShaderParametersMetadata::FMember Member(
						Name,
						ShaderType,
						FileLine,
						Offset,
						EUniformBufferBaseType(BaseType),
						EShaderPrecisionModifier::Type(PrecisionModifier),
						NumRows,
						NumColumns,
						NumElements,
						StructMetadata);
					Members.Add(Member);
				}

				ParameterStructures.Add(MakeUnique<FShaderParametersMetadata>(
					UseCase,
					EUniformBufferBindingFlags::Shader,
					/* InLayoutName = */ LayoutName,
					/* InStructTypeName = */ StructTypeName,
					/* InShaderVariableName = */ ShaderVariableName,
					/* InStaticSlotName = */ nullptr,
					StructFileName,
					StructFileLine,
					Size,
					Members,
					/* bCompleteInitialization = */ true));
			}
		}

		GNumProcessedJobs = 0;

		// Individual jobs
		{
			int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
			InputFile << SingleJobHeader;
			if (ShaderCompileWorkerSingleJobHeader != SingleJobHeader)
			{
				ExitWithoutCrash(FSCWErrorCode::BadSingleJobHeader, FString::Printf(TEXT("Exiting due to ShaderCompilerWorker expecting job header %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerSingleJobHeader, SingleJobHeader));
			}

			int32 NumBatches = 0;
			InputFile << NumBatches;

			FlushShaderFileCache();

			OutSingleJobs.Reserve(NumBatches);
			
			for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
			{
				FShaderCompileJob& Job = OutSingleJobs.AddDefaulted_GetRef();
				// Deserialize the job's inputs.
				Job.SerializeWorkerInput(InputFile);
				Job.Input.DeserializeSharedInputs(InputFile, ExternalIncludes, SharedEnvironments, ParameterStructures);

				// SCW doesn't run DDPI, GShaderHasCache Initialize is run  at start with no knowledge of the CustomPlatforms
				// CustomPlatforms are known when we parse the WorkerInput so we populate the Directory here
				if (IsCustomPlatform((EShaderPlatform)Job.Input.Target.Platform))
				{
					const EShaderPlatform ShaderPlatform = ShaderFormatNameToShaderPlatform(Job.Input.ShaderFormat);
					UpdateIncludeDirectoryForPreviewPlatform((EShaderPlatform)Job.Input.Target.Platform, ShaderPlatform);
				}

				// Process the job.
				CompileShader(GetShaderFormats(), Job, WorkingDirectory, &GNumProcessedJobs); 
			}
		}

		// Shader pipeline jobs
		{
			int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
			InputFile << PipelineJobHeader;
			if (ShaderCompileWorkerPipelineJobHeader != PipelineJobHeader)
			{
				ExitWithoutCrash(FSCWErrorCode::BadPipelineJobHeader, FString::Printf(TEXT("Exiting due to ShaderCompilerWorker expecting pipeline job header %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerSingleJobHeader, PipelineJobHeader));
			}

			int32 NumPipelines = 0;
			InputFile << NumPipelines;

			OutPipelineNames.Reserve(NumPipelines);
			OutPipelineJobs.Reserve(NumPipelines);

			for (int32 Index = 0; Index < NumPipelines; ++Index)
			{
				FString& PipelineName = OutPipelineNames.AddDefaulted_GetRef();
				InputFile << PipelineName;
				
				int32 NumStages = 0;
				InputFile << NumStages;
				FShaderPipelineCompileJob& PipelineJob = OutPipelineJobs.Emplace_GetRef(NumStages);

				for (int32 StageIndex = 0; StageIndex < NumStages; ++StageIndex)
				{
					// Deserialize the job's inputs.
					FShaderCompileJob* Job = PipelineJob.StageJobs[StageIndex]->GetSingleShaderJob();
					Job->SerializeWorkerInput(InputFile);
					Job->Input.DeserializeSharedInputs(InputFile, ExternalIncludes, SharedEnvironments, ParameterStructures);

					// SCW doesn't run DDPI, GShaderHasCache Initialize is run  at start with no knowledge of the CustomPlatforms
					// CustomPlatforms are known when we parse the WorkerInput so we populate the Directory here
					if (IsCustomPlatform((EShaderPlatform)Job->Input.Target.Platform))
					{
						const EShaderPlatform ShaderPlatform = ShaderFormatNameToShaderPlatform(Job->Input.ShaderFormat);
						UpdateIncludeDirectoryForPreviewPlatform((EShaderPlatform)Job->Input.Target.Platform, ShaderPlatform);
					}
				}

				CompileShaderPipeline(GetShaderFormats(), &PipelineJob, WorkingDirectory, &GNumProcessedJobs);
			}
		}
	}

	FArchive* CreateOutputArchive()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CreateOutputArchive);

		FArchive* OutputFilePtr = nullptr;
		const double StartTime = FPlatformTime::Seconds();
		bool bResult = false;

		// It seems XGE does not support deleting files.
		// Don't delete the input file if we are running under Incredibuild (or if the cmdline args explicitly told us to keep it).
		// In xml mode, we signal completion by creating a zero byte "Success" file after the output file has been fully written.
		// In intercept mode, completion is signaled by this process terminating.
		// For UBA we can't delete the file when running remotely because there might be a crash or disconnect happening before result is sent back and then we can't retry
		if (!IsUsingXGE() && !KeepInput && !IsUsingUBA())
		{
			do 
			{
				// Remove the input file so that it won't get processed more than once
				bResult = IFileManager::Get().Delete(*InputFilePath);
			} 
			while (!bResult && (FPlatformTime::Seconds() - StartTime < 2));

			if (!bResult)
			{
				ExitWithoutCrash(FSCWErrorCode::CantDeleteInputFile, FString::Printf(TEXT("Couldn't delete input file %s, is it readonly?"), *InputFilePath));
			}
		}

		// To make sure that the process waiting for results won't read unfinished output file,
		// we use a temp file name during compilation.
		do
		{
			FGuid Guid;
			FPlatformMisc::CreateGuid(Guid);
			TempFilePath = WorkingDirectory + Guid.ToString();
		} while (IFileManager::Get().FileSize(*TempFilePath) != INDEX_NONE);

		const double StartTime2 = FPlatformTime::Seconds();

		do 
		{
			// Create the output file.
			OutputFilePtr = IFileManager::Get().CreateFileWriter(*TempFilePath,FILEWRITE_EvenIfReadOnly);
		} 
		while (!OutputFilePtr && (FPlatformTime::Seconds() - StartTime2 < 2));
			
		if (!OutputFilePtr)
		{
			ExitWithoutCrash(FSCWErrorCode::CantSaveOutputFile, FString::Printf(TEXT("Couldn't save output file %s"), *TempFilePath));
		}

		return OutputFilePtr;
	}

	void WriteToOutputArchive(FArchive* OutputFilePtr, TArray<FShaderCompileJob>& SingleJobs, TArray<FShaderPipelineCompileJob>& PipelineJobs, TArray<FString>& PipelineNames)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WriteToOutputArchive);

		FArchive& OutputFile = *OutputFilePtr;
		const int64 FileSizePosition = FSCWErrorCode::IsSet()
			? WriteOutputFileHeader(OutputFile, FSCWErrorCode::Get(), 0, nullptr, FSCWErrorCode::GetInfo().Len(), *FSCWErrorCode::GetInfo())
			: WriteOutputFileHeader(OutputFile, FSCWErrorCode::Success, 0, nullptr, 0, nullptr);

		{
			int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
			OutputFile << SingleJobHeader;

			int32 NumBatches = SingleJobs.Num();
			OutputFile << NumBatches;

			for (int32 JobIndex = 0; JobIndex < SingleJobs.Num(); JobIndex++)
			{
				SingleJobs[JobIndex].SerializeWorkerOutput(OutputFile);
				UpdateFileSize(OutputFile, FileSizePosition);
			}
		}

		{
			int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
			OutputFile << PipelineJobHeader;
			int32 NumBatches = PipelineJobs.Num();
			OutputFile << NumBatches;

			for (int32 JobIndex = 0; JobIndex < PipelineJobs.Num(); JobIndex++)
			{
				auto& PipelineJob = PipelineJobs[JobIndex];
				OutputFile << PipelineNames[JobIndex];
				bool bSucceeded = (bool)PipelineJob.bSucceeded;
				OutputFile << bSucceeded;
				int32 NumStageJobs = PipelineJob.StageJobs.Num();
				OutputFile << NumStageJobs;

				for (int32 Index = 0; Index < NumStageJobs; ++Index)
				{
					PipelineJob.StageJobs[Index]->SerializeWorkerOutput(OutputFile);
					UpdateFileSize(OutputFile, FileSizePosition);
				}
			}
		}
	}

	/** Called in the idle loop, checks for conditions under which the helper should exit */
	void CheckExitConditions()
	{
		if (!InputFilename.Contains(TEXT("Only")))
		{
			UE_LOG(LogShaders, Log, TEXT("InputFilename did not contain 'Only', exiting after one job."));
			FPlatformMisc::RequestExit(false);
		}

#if PLATFORM_MAC || PLATFORM_LINUX
		if (!FPlatformMisc::IsDebuggerPresent() && ParentProcessId > 0)
		{
			// If the parent process is no longer running, exit
			if (!FPlatformProcess::IsApplicationRunning(ParentProcessId))
			{
				FString FilePath = FString(WorkingDirectory) + InputFilename;
				checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to the parent process no longer running and the input file is present!"));
				UE_LOG(LogShaders, Log, TEXT("Parent process no longer running, exiting"));
				FPlatformMisc::RequestExit(false);
			}
		}

		const double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastCompileTime > TimeToLive)
		{
			UE_LOG(LogShaders, Log, TEXT("No jobs found for %f seconds, exiting"), (float)(CurrentTime - LastCompileTime));
			FPlatformMisc::RequestExit(false);
		}
#else
		// Don't do these if the debugger is present
		//@todo - don't do these if Unreal is being debugged either
		if (!IsDebuggerPresent())
		{
			if (ParentProcessId > 0)
			{
				FString FilePath = FString(WorkingDirectory) + InputFilename;

				bool bParentStillRunning = true;
				HANDLE ParentProcessHandle = OpenProcess(SYNCHRONIZE, false, ParentProcessId);
				// If we couldn't open the process then it is no longer running, exit
				if (ParentProcessHandle == nullptr)
				{
					checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to OpenProcess(ParentProcessId) failing and the input file is present!"));
					UE_LOG(LogShaders, Log, TEXT("Couldn't OpenProcess, Parent process no longer running, exiting"));
					FPlatformMisc::RequestExit(false);
				}
				else
				{
					// If we did open the process, that doesn't mean it is still running
					// The process object stays alive as long as there are handles to it
					// We need to check if the process has signaled, which indicates that it has exited
					uint32 WaitResult = WaitForSingleObject(ParentProcessHandle, 0);
					if (WaitResult != WAIT_TIMEOUT)
					{
						checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to WaitForSingleObject(ParentProcessHandle) signaling and the input file is present!"));
						UE_LOG(LogShaders, Log, TEXT("WaitForSingleObject signaled, Parent process no longer running, exiting"));
						FPlatformMisc::RequestExit(false);
					}
					CloseHandle(ParentProcessHandle);
				}
			}

			const double CurrentTime = FPlatformTime::Seconds();
			// If we have been idle for 20 seconds then exit
			if (CurrentTime - LastCompileTime > TimeToLive)
			{
				UE_LOG(LogShaders, Log, TEXT("No jobs found for %f seconds, exiting"), (float)(CurrentTime - LastCompileTime));
				FPlatformMisc::RequestExit(false);
			}
		}
#endif
	}
};

static void DirectCompile(const TArray<const class IShaderFormat*>& ShaderFormats)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DirectCompile);

	// Find all the info required for compiling a single shader
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

	FString InputFile;

	FName FormatName;
	FName ShaderPlatformName;
	FString Entry = TEXT("Main");
	uint32 SupportedHardwareMask = 0;
	FString DumpDebugInfoPath;
	bool bPipeline = false;
	EShaderFrequency Frequency = SF_Pixel;
	TArray<FString> UsedOutputs;
	bool bIncludeUsedOutputs = false;
	uint64 CFlags = 0;
	for (const FString& Token : Tokens)
	{
		if (Switches.Contains(Token))
		{
			if (Token.StartsWith(TEXT("format=")))
			{
				FormatName = FName(*Token.RightChop(7));
			}
			else if (Token.StartsWith(TEXT("entry=")))
			{
				Entry = Token.RightChop(6);

				// Remove quotations marks at beginning and end; happens when multiple entry points are specified, e.g. -entry="closesthit=A anyhit=B"
				if (Entry.Len() >= 2 && Entry[0] == TEXT('\"') && Entry[Entry.Len() - 1] == TEXT('\"'))
				{
					Entry = Entry.Mid(1, Entry.Len() - 2);
				}
			}
			else if (Token.StartsWith(TEXT("shaderPlatformName=")))
			{
				ShaderPlatformName = FName(*Token.RightChop(19));
			}
			else if (Token.StartsWith(TEXT("supportedHardwareMask=")))
			{
				SupportedHardwareMask = (uint32)FCString::Atoi64(*Token.RightChop(22));
			}
			else if (Token.StartsWith(TEXT("DebugInfoPath=")))
			{
				DumpDebugInfoPath = Token.RightChop(14);
			}
			else if (!FCString::Strcmp(*Token, TEXT("ps")))
			{
				Frequency = SF_Pixel;
			}
			else if (!FCString::Strcmp(*Token, TEXT("vs")))
			{
				Frequency = SF_Vertex;
			}
			else if (!FCString::Strcmp(*Token, TEXT("ms")))
			{
				Frequency = SF_Mesh;
			}
			else if (!FCString::Strcmp(*Token, TEXT("as")))
			{
				Frequency = SF_Amplification;
			}
			else if (!FCString::Strcmp(*Token, TEXT("gs")))
			{
				Frequency = SF_Geometry;
			}
			else if (!FCString::Strcmp(*Token, TEXT("cs")))
			{
				Frequency = SF_Compute;
			}
#if RHI_RAYTRACING
			else if (!FCString::Strcmp(*Token, TEXT("rgs")))
			{
				Frequency = SF_RayGen;
			}
			else if (!FCString::Strcmp(*Token, TEXT("rms")))
			{
				Frequency = SF_RayMiss;
			}
			else if (!FCString::Strcmp(*Token, TEXT("rhs")))
			{
				Frequency = SF_RayHitGroup;
			}
			else if (!FCString::Strcmp(*Token, TEXT("rcs")))
			{
				Frequency = SF_RayCallable;
			}
#endif // RHI_RAYTRACING
			else if (!FCString::Strcmp(*Token, TEXT("pipeline")))
			{
				bPipeline = true;
			}
			else if (Token.StartsWith(TEXT("usedoutputs=")))
			{
				FString Outputs = Token.RightChop(12);
				bIncludeUsedOutputs = true;
				FString LHS, RHS;
				while (Outputs.Split(TEXT("+"), &LHS, &RHS))
				{
					Outputs = RHS;
					UsedOutputs.Add(LHS);
				}
				UsedOutputs.Add(Outputs);
			}
		}
		else
		{
			if (InputFile.Len() == 0)
			{
				InputFile = Token;
			}
		}
	}

	FString Dir = FPlatformProcess::UserTempDir();

	FShaderCompileJob Job;
	Job.Input.EntryPointName = Entry;
	Job.Input.ShaderFormat = FormatName;
	Job.Input.ShaderPlatformName = ShaderPlatformName;
	Job.Input.SupportedHardwareMask = SupportedHardwareMask;
	Job.Input.VirtualSourceFilePath = InputFile;
	Job.Input.Target.Platform =  ShaderFormatNameToShaderPlatform(FormatName);
	Job.Input.Target.Frequency = Frequency;
	Job.Input.DumpDebugInfoPath = DumpDebugInfoPath;
	Job.Input.bCompilingForShaderPipeline = bPipeline;
	Job.Input.bIncludeUsedOutputs = bIncludeUsedOutputs;
	Job.Input.UsedOutputs = UsedOutputs;
	Job.Input.DebugInfoFlags = EShaderDebugInfoFlags::CompileFromDebugUSF;

	FShaderCompilerOutput Output;
	CompileShader(GetShaderFormats(), Job, Dir, &GNumProcessedJobs);
}


/** 
 * Main entrypoint, guarded by a try ... except.
 * This expects 4 parameters:
 *		The image path and name
 *		The working directory path, which has to be unique to the instigating process and thread.
 *		The parent process Id
 *		The thread Id corresponding to this worker
 */
static int32 GuardedMain(int32 argc, TCHAR* argv[], FString& CrashOutputFile, bool bDirectMode)
{
	FString ExtraCmdLine = TEXT("-NOPACKAGECACHE -ReduceThreadUsage -cpuprofilertrace -nocrashreports");

	// When executing tasks remotely through XGE, enumerating files requires tcp/ip round-trips with
	// the initiator, which can slow down engine initialization quite drastically.
	// The idea here is to save the Ini and Modules manager state and reuse them on the workers
	// to avoid all those directory enumeration during engine init.
	FString IniBootstrapFilename;
	FString ModulesBootstrapFilename;

	// Register out-of-memory delegate to report error code on exit
	FCoreDelegates::GetOutOfMemoryDelegate().AddLambda(
		[]()
		{
			FSCWErrorCode::Report(FSCWErrorCode::OutOfMemory);
		}
	);

	if (IsUsingXGE())
	{
		// Tie the bootstrap filenames to the xge job id to refresh bootstraps state every time a new build starts
		// This allows the ini/modules and shadercompilerworker binaries to change between builds.
		FGuid XGJobID;
		if (FGuid::Parse(FPlatformMisc::GetEnvironmentVariable(TEXT("xgJobID")), XGJobID))
		{
			FString XGJobIDString = XGJobID.ToString(EGuidFormats::DigitsWithHyphens);
			IniBootstrapFilename = FString::Printf(TEXT("%s/Bootstrap-%s.inis"), argv[1], *XGJobIDString);
			ModulesBootstrapFilename = FString::Printf(TEXT("%s/Bootstrap-%s.modules"), argv[1], *XGJobIDString);

			ExtraCmdLine.Appendf(TEXT(" -IniBootstrap=\"%s\" -ModulesBootstrap=\"%s\""), *IniBootstrapFilename, *ModulesBootstrapFilename);

			// Use Windows API directly because required CreateFile flags are not supported by our current OS abstraction
#if PLATFORM_WINDOWS
			// This is advantageous to have only a single worker do the init work instead of having all workers
			// do a stampede of the initiator's machine all trying to enumerate directories at the same time.
			// I've seen incoming TCP connections going through the roof (350 connections for 150 virtual CPUs)
			// coming from workers doing all the same directory enumerations.
			// This is not strictly required, but will improve performance when successful.
			// Most likely a local worker will win the race and do a fast init.
			FString MutexFilename = FString::Printf(TEXT("%s/Bootstrap-%s.mutex"), argv[1], *XGJobIDString);

			// We need to implement a mutex scheme through a file for it to work with XGE's file virtualization layer.
			// The first process to successfully create this file will have the honor of doing the complete initialization.
			HANDLE MutexHandle =
				CreateFileW(
					*MutexFilename,
					GENERIC_WRITE,
					0,
					nullptr,
					CREATE_NEW,
					FILE_ATTRIBUTE_NORMAL,
					nullptr);

			if (MutexHandle != INVALID_HANDLE_VALUE)
			{
				// We won the race, proceed to initialization.
				CloseHandle(MutexHandle);
			}
			else
			{
				// Wait until the race winner writes the last bootstrap file
				// Due to a bug in XGE, some workers might never see the new file appear, we must proceed after some timeout value.
				for (int32 Index = 0; Index < 10 && !FPaths::FileExists(ModulesBootstrapFilename); ++Index)
				{
					Sleep(100);
				}
			}
#endif
		}
	}

	GEngineLoop.PreInit(argc, argv, *ExtraCmdLine);
#if DEBUG_USING_CONSOLE
	GLogConsole->Show( true );
#endif

	TRACE_CPUPROFILER_EVENT_SCOPE(Main);

	auto AtomicSave = 
		[](const FString& Filename, TFunctionRef<void (const FString& TmpFile)> SaveFunction)
		{
			if (!Filename.IsEmpty() && !FPaths::FileExists(Filename))
			{
				// Use a tmp file for atomic publication and avoid reading incomplete state from other workers
				FString TmpFile = FString::Printf(TEXT("%s-%s"), *Filename, *FGuid::NewGuid().ToString());
				SaveFunction(TmpFile);
				const bool bReplace = false;
				const bool bDoNotRetryOrError = true;
				const bool bEvenIfReadOnly = false;
				const bool bAttributes = false;
				IFileManager::Get().Move(*Filename, *TmpFile, bReplace, bEvenIfReadOnly, bAttributes, bDoNotRetryOrError);
				// In case this process lost the race and wasn't able to move the file, discard the tmp file.
				IFileManager::Get().Delete(*TmpFile);
			}
		};

	AtomicSave(IniBootstrapFilename,     [](const FString& TmpFile) { GConfig->SaveCurrentStateForBootstrap(*TmpFile); });
	AtomicSave(ModulesBootstrapFilename, [](const FString& TmpFile) { FModuleManager::Get().SaveCurrentStateForBootstrap(*TmpFile); });

	// Explicitly load ShaderPreprocessor module so it will run its initialization step
	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("ShaderPreprocessor"));

	// We just enumerate the shader formats here for debugging.
	const TArray<const class IShaderFormat*>& ShaderFormats = GetShaderFormats();
	check(ShaderFormats.Num());
	TMap<FString, uint32> FormatVersionMap;
	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ShaderFormat);

		TArray<FName> OutFormats;
		ShaderFormats[Index]->GetSupportedFormats(OutFormats);
		check(OutFormats.Num());
		for (int32 InnerIndex = 0; InnerIndex < OutFormats.Num(); InnerIndex++)
		{
			UE_LOG(LogShaders, Display, TEXT("Available Shader Format %s"), *OutFormats[InnerIndex].ToString());
			uint32 Version = ShaderFormats[Index]->GetVersion(OutFormats[InnerIndex]);
			FormatVersionMap.Add(OutFormats[InnerIndex].ToString(), Version);
		}
	}

	LastCompileTime = FPlatformTime::Seconds();

	if (bDirectMode)
	{
		DirectCompile(ShaderFormats);
	}
	else
	{
#if PLATFORM_WINDOWS
		//@todo - would be nice to change application name or description to have the ThreadId in it for debugging purposes
		SetConsoleTitle(argv[3]);
#endif

		TRACE_CPUPROFILER_EVENT_SCOPE(FWorkLoop);

		FWorkLoop WorkLoop(argv[2], argv[1], argv[4], argv[5], FormatVersionMap);
		WorkLoop.Loop(CrashOutputFile);
	}

	return 0;
}


static int32 GuardedMainWrapper(int32 ArgC, TCHAR* ArgV[], FString& CrashOutputFile, bool bDirectMode)
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	// We need to know whether we are using XGE now, in case an exception
	// is thrown before we parse the command line inside GuardedMain.
	if ((ArgC > 6) && FCString::Strcmp(ArgV[6], TEXT("-xge_int")) == 0)
	{
		GXGEMode = EXGEMode::Intercept;
	}
	else if ((ArgC > 6) && FCString::Strcmp(ArgV[6], TEXT("-xge_xml")) == 0)
	{
		GXGEMode = EXGEMode::Xml;
	}
	else
	{
		GXGEMode = EXGEMode::None;
	}

	int32 ReturnCode = 0;
#if PLATFORM_WINDOWS
	if (FPlatformMisc::IsDebuggerPresent())
#endif
	{
		ReturnCode = GuardedMain(ArgC, ArgV, CrashOutputFile, bDirectMode);
	}
#if PLATFORM_WINDOWS
	else
	{
		// Don't want 32 dialogs popping up when SCW fails
		GUseCrashReportClient = false;
		FString ExceptionMsg;
		FString ExceptionCallStack;
		__try
		{
			GIsGuarded = 1;
			ReturnCode = GuardedMain(ArgC, ArgV, CrashOutputFile, bDirectMode);
			GIsGuarded = 0;
		}
		__except(HandleShaderCompileException(GetExceptionInformation(), ExceptionMsg, ExceptionCallStack))
		{
			FArchive& OutputFile = *IFileManager::Get().CreateFileWriter(*CrashOutputFile, FILEWRITE_EvenIfReadOnly);

			if (GFailedErrorCode == FSCWErrorCode::Success)
			{
				if (FSCWErrorCode::IsSet())
				{
					// Use the value set inside the shader format
					GFailedErrorCode = FSCWErrorCode::Get();
				}
				else
				{
					// Something else failed before we could set the error code, so mark it as a General Crash
					GFailedErrorCode = FSCWErrorCode::GeneralCrash;
				}
			}

			int64 FileSizePosition = WriteOutputFileHeader(OutputFile, GFailedErrorCode, ExceptionCallStack.Len(), *ExceptionCallStack,
				ExceptionMsg.Len(), *ExceptionMsg);

			int32 NumBatches = 0;
			OutputFile << NumBatches;
			OutputFile << NumBatches;

			UpdateFileSize(OutputFile, FileSizePosition);

			// Close the output file.
			delete &OutputFile;

			if (IsUsingXGE())
			{
				ReturnCode = 1;
				OnXGEJobCompleted(ArgV[1]);
			}
			else if (GetUbaModule())
			{
				ReturnCode = GFailedErrorCode;
			}
		}
	}
#endif

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();

	return ReturnCode;
}

IMPLEMENT_APPLICATION(ShaderCompileWorker, "ShaderCompileWorker")


/**
 * Application entry point
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	// Redirect for special XGE utilities...
	extern bool XGEMain(int ArgC, TCHAR* ArgV[], int32& ReturnCode);
	{
		int32 ReturnCode;
		if (XGEMain(ArgC, ArgV, ReturnCode))
		{
			return ReturnCode;
		}
	}

	FString OutputFilePath;

	bool bDirectMode = false;
	for (int32 Index = 1; Index < ArgC; ++Index)
	{
		if (FCString::Strcmp(ArgV[Index], TEXT("-directcompile")) == 0)
		{
			bDirectMode = true;
			break;
		}
	}

	if (!bDirectMode)
	{
		if (ArgC < 6)
		{
			printf("ShaderCompileWorker (v%d) is called by UnrealEditor, it requires specific command line arguments.\n", ShaderCompileWorkerOutputVersion);
			return -1;
		}

		// Game exe can pass any number of parameters through with appGetSubprocessCommandline
		// so just make sure we have at least the minimum number of parameters.
		check(ArgC >= 6);

		OutputFilePath = ArgV[1];
		OutputFilePath += ArgV[5];
	}

	return GuardedMainWrapper(ArgC, ArgV, OutputFilePath, bDirectMode);
}
