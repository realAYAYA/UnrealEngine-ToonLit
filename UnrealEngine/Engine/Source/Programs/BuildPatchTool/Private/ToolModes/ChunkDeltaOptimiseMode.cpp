// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolModes/ChunkDeltaOptimiseMode.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"

using namespace BuildPatchTool;

class FChunkDeltaOptimiseMode : public IToolMode
{
public:
	FChunkDeltaOptimiseMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
		, ScanWindowSize(8191)
		, OutputChunkSize(1024 * 1024)
	{}

	virtual ~FChunkDeltaOptimiseMode()
	{}

	virtual EReturnCode Execute() override
	{
		// Parse commandline.
		if (ProcessCommandline() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested.
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Display, TEXT("CHUNK DELTA OPTIMISE MODE"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("This tool supports optimising chunk based patches to reduce the number of chunks required to download when patching between specific versions."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			UE_LOG(LogBuildPatchTool, Display, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -mode=ChunkDeltaOptimise  Must be specified to launch the tool in chunk delta optimise mode."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -ManifestA=\"\"             Specifies in quotes the file path to the base manifest."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -ManifestB=\"\"             Specifies in quotes the file path to the update manifest."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			UE_LOG(LogBuildPatchTool, Display, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -CloudDir=\"\"                     Specifies in quotes the cloud directory where existing data will be recognized from, and new data added to. If not provided, location of ManifestB will be used."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -ScanWindowSize=1000000          Specifies in bytes, the scan window to use. Range accepted is 128KiB >= n >= 8Kb, defaults to 8191 (Closest prime to 8KiB)."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -OutputChunkSize=1000000         Specifies in bytes, the chunk size to save out unknown data with. Range accepted is 10MiB >= n >= 1MB, defaults to 1048576 (1MiB)."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -DiffAbortThreshold=10000000000  Specifies in bytes, an upper limit for original diffs to try to enhance."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("                                   This allows short circuiting lengthy optimisation attempts on large diffs which may not benefit. Range accepted is n >= 1GB, defaults to never aborting."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			return EReturnCode::OK;
		}

		// Setup and run.
		BuildPatchServices::FChunkDeltaOptimiserConfiguration Configuration;
		Configuration.ManifestAUri = ManifestA;
		Configuration.ManifestBUri = ManifestB;
		Configuration.CloudDirectory = CloudDir;
		Configuration.ScanWindowSize = ScanWindowSize;
		Configuration.OutputChunkSize = OutputChunkSize;
		Configuration.DiffAbortThreshold = DiffAbortThreshold;

		// Run the build generation.
		bool bSuccess = BpsInterface.OptimiseChunkDelta(Configuration);
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:
	bool ProcessCommandline()
	{
#define PARSE_SWITCH(Switch) ParseSwitch(TEXT(#Switch "="), Switch, Switches)
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		bHelp = ParseOption(TEXT("help"), Switches);
		if (bHelp)
		{
			return true;
		}

		// Get all required parameters.
		if (!(PARSE_SWITCH(ManifestA) && PARSE_SWITCH(ManifestB)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("ManifestA and ManifestB are required parameters"));
			return false;
		}
		NormalizeUriFile(ManifestA);
		NormalizeUriFile(ManifestB);

		// Get optional values.
		const bool bHasCloudDir = PARSE_SWITCH(CloudDir);
		PARSE_SWITCH(ScanWindowSize);
		PARSE_SWITCH(OutputChunkSize);
		if (!bHasCloudDir)
		{
			CloudDir = FPaths::GetPath(ManifestB);
		}
		NormalizeUriPath(CloudDir);
		if (!PARSE_SWITCH(DiffAbortThreshold))
		{
			DiffAbortThreshold = TNumericLimits<int64>::Max();
		}

		// Clamp ScanWindowSize to sane range.
		const uint32 RequestedScanWindowSize = ScanWindowSize;
		ScanWindowSize = FMath::Clamp<uint32>(ScanWindowSize, 8000, 128*1024);
		if (RequestedScanWindowSize != ScanWindowSize)
		{
			UE_LOG(LogBuildPatchTool, Warning, TEXT("Requested -ScanWindowSize=%u is outside of allowed range 128KiB >= n >= 8Kb. Please update your arg to be within range. Continuing with %u."), RequestedScanWindowSize, ScanWindowSize);
		}

		// Clamp OutputChunkSize to sane range.
		const uint32 RequestedOutputChunkSize = OutputChunkSize;
		OutputChunkSize = FMath::Clamp<uint32>(OutputChunkSize, 1000000, 10*1024*1024);
		if (RequestedOutputChunkSize != OutputChunkSize)
		{
			UE_LOG(LogBuildPatchTool, Warning, TEXT("Requested -OutputChunkSize=%u is outside of allowed range 10MiB >= n >= 1MB. Please update your arg to be within range. Continuing with %u."), RequestedOutputChunkSize, OutputChunkSize);
		}

		// Clamp DiffAbortThreshold to sane min.
		const uint64 RequestedDiffAbortThreshold = DiffAbortThreshold;
		DiffAbortThreshold = FMath::Max<uint64>(DiffAbortThreshold, 1000000000);
		if (RequestedDiffAbortThreshold != DiffAbortThreshold)
		{
			UE_LOG(LogBuildPatchTool, Warning, TEXT("Requested -DiffAbortThreshold=%llu is below allowed minimum n >= 1GB. Please update your arg to be above limit. Continuing with %llu."), RequestedDiffAbortThreshold, DiffAbortThreshold);
		}

		return true;
#undef PARSE_SWITCH
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString ManifestA;
	FString ManifestB;
	FString CloudDir;
	uint32 ScanWindowSize;
	uint32 OutputChunkSize;
	uint64 DiffAbortThreshold;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FChunkDeltaOptimiseToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FChunkDeltaOptimiseMode(BpsInterface));
}
