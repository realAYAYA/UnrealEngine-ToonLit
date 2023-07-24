// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class DatasmithSketchUpRuby2023Target : DatasmithSketchUpRubyBaseTarget
{
	public DatasmithSketchUpRuby2023Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithSketchUpRuby2023";
		ExeBinariesSubFolder = @"SketchUpRuby/2023";

		AddCopyPostBuildStep(Target);
	}
}
