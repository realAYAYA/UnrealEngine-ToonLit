// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UnrealPackageToolTarget : TargetRules
{
	public UnrealPackageToolTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		LaunchModuleName = "UnrealPackageTool";

		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;

		bCompileAgainstApplicationCore = false;
		bCompileAgainstCoreUObject = true;
		bCompileAgainstEngine = false;
		bCompileICU = false;

		bIsBuildingConsoleApplication = true;

		GlobalDefinitions.Add("UE_TRACE_ENABLED=1");
	}
}
