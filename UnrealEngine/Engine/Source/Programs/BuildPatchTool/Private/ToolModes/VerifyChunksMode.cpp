// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolModes/VerifyChunksMode.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Algo/Find.h"

using namespace BuildPatchTool;

class FVerifyChunksToolMode : public IToolMode
{
public:
	FVerifyChunksToolMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FVerifyChunksToolMode()
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
			UE_LOG(LogBuildPatchTool, Display, TEXT("VERIFY CHUNKS MODE"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("This tool mode allows you to verify the integrity of patch data. It will load chunk or chunkdb files to verify they are not corrupt."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			UE_LOG(LogBuildPatchTool, Display, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -mode=VerifyChunks  Must be specified to launch the tool in verify chunks mode."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -SearchPath=\"\"      Specifies in quotes the directory path which contains data to verify."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			UE_LOG(LogBuildPatchTool, Display, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -OutputFile=\"\"      When specified, full file path for each bad data will be saved to this file as \\r\\n seperated list."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			UE_LOG(LogBuildPatchTool, Display, TEXT("NB: All checks are logged, normal log for good data, error log for any bad data found."));
			return EReturnCode::OK;
		}

		// Run the enumeration routine.
		bool bSuccess = BpsInterface.VerifyChunkData(SearchPath, OutputFile);
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:

	bool ProcessCommandline()
	{
#define HAS_SWITCH(Switch) (Algo::FindByPredicate(Switches, [](const FString& Elem){ return Elem.StartsWith(TEXT(#Switch "="));}) != nullptr)
#define PARSE_SWITCH(Switch) ParseSwitch(TEXT(#Switch "="), Switch, Switches)
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		bHelp = ParseOption(TEXT("help"), Switches);
		if (bHelp)
		{
			return true;
		}

		// Get all required parameters.
		if (!PARSE_SWITCH(SearchPath))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("SearchPath is a required parameter"));
			return false;
		}
		NormalizeUriPath(SearchPath);

		// Get optional parameters.
		PARSE_SWITCH(OutputFile);
		NormalizeUriFile(OutputFile);

		return true;
#undef PARSE_SWITCH
#undef HAS_SWITCH
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString SearchPath;
	FString OutputFile;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FVerifyChunksToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FVerifyChunksToolMode(BpsInterface));
}
