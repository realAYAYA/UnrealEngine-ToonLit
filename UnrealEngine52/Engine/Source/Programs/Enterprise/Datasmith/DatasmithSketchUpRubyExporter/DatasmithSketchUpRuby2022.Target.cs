// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class DatasmithSketchUpRuby2022Target : DatasmithSketchUpRubyBaseTarget
{
	public DatasmithSketchUpRuby2022Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithSketchUpRuby2022";
		ExeBinariesSubFolder = @"SketchUpRuby/2022";

		AddCopyPostBuildStep(Target);
	}
}
