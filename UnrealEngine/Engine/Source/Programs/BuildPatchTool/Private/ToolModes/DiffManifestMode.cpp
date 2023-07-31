// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolModes/DiffManifestMode.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

using namespace BuildPatchTool;

class FDiffManifestToolMode : public IToolMode
{
public:
	FDiffManifestToolMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FDiffManifestToolMode()
	{}

	virtual EReturnCode Execute() override
	{
		// Parse commandline.
		if (ProcessCommandLine() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested.
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Display, TEXT("DIFF MANIFEST MODE"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("This tool mode reports the changes between two existing manifest files."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			UE_LOG(LogBuildPatchTool, Display, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -mode=DiffManifests    Must be specified to launch the tool in diff manifests mode."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -ManifestA=\"\"          Specifies in quotes the file path to the base manifest."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -ManifestB=\"\"          Specifies in quotes the file path to the update manifest."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			UE_LOG(LogBuildPatchTool, Display, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -InstallTagsA=\"\"       Specifies in quotes a comma seperated list of install tags used on ManifestA. You should include empty string if you want to count untagged files."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("                           Leaving the parameter out will use all files."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("                           -InstallTagsA=\"\" will be untagged files only."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("                           -InstallTagsA=\",tag\" will be untagged files plus files tagged with 'tag'."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("                           -InstallTagsA=\"tag\" will be files tagged with 'tag' only."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -InstallTagsB=\"\"       Specifies in quotes a comma seperated list of install tags used on ManifestB. Same rules apply as InstallTagsA."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -CompareTagSet=\"\"      Specifies in quotes a comma seperated list of install tags used to calculate differential statistics betweeen the manifests. Multiple lists are allowed. Same rules apply as InstallTagsA."));
			UE_LOG(LogBuildPatchTool, Display, TEXT("  -OutputFile=\"\"         Specifies in quotes the file path where the diff will be exported as a JSON object."));
			UE_LOG(LogBuildPatchTool, Display, TEXT(""));
			return EReturnCode::OK;
		}

		// Calc desired tags.
		TSet<FString> TagSetA, TagSetB;
		if (bHasTagsA)
		{
			TagSetA = ProcessTagList(InstallTagsA);
		}
		if (bHasTagsB)
		{
			TagSetB = ProcessTagList(InstallTagsB);
		}

		TArray<TSet<FString>> CompareTagSets;

		for (const FString& CompareTagsList : CompareTagsArray)
		{
			CompareTagSets.Add(ProcessTagList(CompareTagsList));
		}

		// Setup and run.
		BuildPatchServices::FDiffManifestsConfiguration Configuration;
		Configuration.ManifestAUri = ManifestA;
		Configuration.ManifestBUri = ManifestB;
		Configuration.TagSetA = TagSetA;
		Configuration.TagSetB = TagSetB;
		Configuration.CompareTagSets = CompareTagSets;
		Configuration.OutputFilePath = OutputFile;

		// Run the merge manifest routine.
		bool bSuccess = BpsInterface.DiffManifests(Configuration);
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:

	bool ProcessCommandLine()
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
		if (!(PARSE_SWITCH(ManifestA)
		   && PARSE_SWITCH(ManifestB)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("ManifestA and ManifestB are required parameters."));
			return false;
		}
		NormalizeUriFile(ManifestA);
		NormalizeUriFile(ManifestB);

		// Get optional parameters
		bHasTagsA = PARSE_SWITCH(InstallTagsA);
		bHasTagsB = PARSE_SWITCH(InstallTagsB);
		PARSE_SWITCH(OutputFile);
		NormalizeUriFile(OutputFile);

		ParseSwitches(TEXT("CompareTagSet="), CompareTagsArray, Switches);

		return true;
#undef PARSE_SWITCH
	}

	TSet<FString> ProcessTagList(const FString& TagCommandLine) const
	{
		TArray<FString> TagArray;
		TagCommandLine.ParseIntoArray(TagArray, TEXT(","), false);
		for (FString& Tag : TagArray)
		{
			Tag.TrimStartAndEndInline();
		}
		if (TagArray.Num() == 0)
		{
			TagArray.Add(TEXT(""));
		}
		return TSet<FString>(MoveTemp(TagArray));
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString ManifestA;
	FString ManifestB;
	TArray<FString> CompareTagsArray;
	bool bHasTagsA;
	bool bHasTagsB;
	FString InstallTagsA;
	FString InstallTagsB;
	FString OutputFile;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FDiffManifestToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FDiffManifestToolMode(BpsInterface));
}
