// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class DatasmithSketchUpRuby2019Target : DatasmithSketchUpRubyBaseTarget
{
	public DatasmithSketchUpRuby2019Target(TargetInfo Target)
		: base(Target)
	{
		LaunchModuleName = "DatasmithSketchUpRuby2019";
		ExeBinariesSubFolder = @"SketchUpRuby/2019";

		AddCopyPostBuildStep(Target);
	}
}
