// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class DatasmithSketchUpRuby2021Target : DatasmithSketchUpRubyBaseTarget
{
	public DatasmithSketchUpRuby2021Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithSketchUpRuby2021";
		ExeBinariesSubFolder = @"SketchUpRuby/2021";

		AddCopyPostBuildStep(Target);
	}
}
