// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DotNetPerforceLibTarget : TargetRules
{
	public DotNetPerforceLibTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "DotNetPerforceLib";

		bShouldCompileAsDLL = true;

		bBuildDeveloperTools = false;
		bUseMallocProfiler = false;
		bCompileICU = false;
		bBuildWithEditorOnlyData = false;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;

		string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Shipping) ? "Release" : "Debug";
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			OutputFile = $"Binaries/DotNET/EpicGames.Perforce.Native/win-x64/{ConfigFolder}/EpicGames.Perforce.Native.dll";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			OutputFile = $"Binaries/DotNET/EpicGames.Perforce.Native/mac-x64/{ConfigFolder}/EpicGames.Perforce.Native.dylib";
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			OutputFile = $"Binaries/DotNET/EpicGames.Perforce.Native/linux-x64/{ConfigFolder}/EpicGames.Perforce.Native.so";
		}
	}
}
