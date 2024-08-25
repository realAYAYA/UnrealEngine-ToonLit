// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class DatasmithSketchUpRuby2024Target : DatasmithSketchUpRubyBaseTarget
{
	public DatasmithSketchUpRuby2024Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithSketchUpRuby2024";
		ExeBinariesSubFolder = @"SketchUpRuby/2024";

		AddCopyPostBuildStep(Target);
	}
}
